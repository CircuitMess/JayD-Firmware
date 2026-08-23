#include "DjSession.h"
#include "DjRecordingStorage.h"
#include "../DjAssist/DjAssistSessionBridge.h"
#include <Arduino.h>
#include <AudioLib/EffectType.hpp>
#include <AudioLib/SpeedModifier.h>
#include <Loop/LoopManager.h>
#include <SD.h>
#include <esp_system.h>

// DjBeatEngine.h's DJ_RATE_* constants are plain values (kept dependency-free
// for host testing) but must exactly mirror SpeedModifier's Q16.16 rate
// range/scale, since DjSession feeds targetRate straight into
// SpeedModifier::setRate()/nudgeRate(). Assert that at compile time so any
// future change to either side's constants fails the build instead of
// silently desyncing the sync controller's math from the real clamp range.
static_assert(DJ_RATE_SCALE == SpeedModifier::RateScale, "DJ_RATE_SCALE must match SpeedModifier::RateScale");
static_assert(DJ_RATE_MIN == SpeedModifier::MinRate, "DJ_RATE_MIN must match SpeedModifier::MinRate");
static_assert(DJ_RATE_NEUTRAL == SpeedModifier::NeutralRate, "DJ_RATE_NEUTRAL must match SpeedModifier::NeutralRate");
static_assert(DJ_RATE_MAX == SpeedModifier::MaxRate, "DJ_RATE_MAX must match SpeedModifier::MaxRate");

DjSession* DjSession::instance = nullptr;
uint64_t DjSession::bootId = 0;
uint32_t DjSession::sessionCounter = 0;
bool DjSession::orphanRecoveryDone = false;
static const char* metadataPath = "/library.jydm";

static uint64_t metadataIdentity(fs::File& file){
	if(!file) return 0;
	uint64_t key = file.size();
	uint8_t crc[4] = {};
	if(file.size() >= 48 && file.seek(44) && file.read(crc, sizeof(crc)) == sizeof(crc)){
		for(uint8_t byte : crc) key = (key ^ byte) * 1099511628211ULL;
	}
	return key;
}

DjSession* DjSession::begin(uint8_t leftGain, uint8_t rightGain, uint8_t initialMix){
	if(instance) return instance;
	if(bootId == 0){
		bootId = (static_cast<uint64_t>(esp_random()) << 32) | esp_random();
	}
	instance = new DjSession(leftGain, rightGain, initialMix);
	LoopManager::addListener(instance);
	return instance;
}

DjSession* DjSession::get(){
	return instance;
}

void DjSession::end(){
	if(!instance) return;
	DjSession* session = instance;
	instance = nullptr;
	LoopManager::removeListener(session);
	session->shutdown();
	delete session;
}

DjSession::DjSession(uint8_t leftGain, uint8_t rightGain, uint8_t initialMix) :
		gains{ leftGain, rightGain }, mix(initialMix), sessionId(++sessionCounter){
	system = new MixSystem();
	if(!orphanRecoveryDone){
		orphanRecoveryDone = true;
		const DjRecordingStorage::RecoveryResult recovery =
				DjRecordingStorage::recoverOrphan(MixSystem::recordPath);
		recordingSnapshot.orphansRepaired = recovery.repaired;
		recordingSnapshot.orphansFailed = recovery.failed;
	}
	assistController.begin(this);
	publishSnapshot();
}

DjSession::~DjSession(){
	delete system;
	system = nullptr;
}

DjSubmitResult DjSession::submit(DjCommand command){
	commandMutex.lock();
#if defined(JAYD_ENABLE_WIRELESS)
	if(command.origin == DJ_ORIGIN_HTTP && command.clientCommandId[0] != '\0'){
		DjCommandResult duplicate;
		if(commandResults.findClientCommand(command.clientId, command.clientCommandId, duplicate)){
			commandMutex.unlock();
			return { duplicate.id, duplicate.status, duplicate.error, true };
		}
	}
#endif
	command.id = ++nextCommandId;
	if(command.id == 0) command.id = ++nextCommandId;

	DjCommandError error = ending ? DJ_COMMAND_ERROR_SESSION_ENDING : validate(command);
	if(error != DJ_COMMAND_ERROR_NONE){
		commandResults.record(command, DJ_COMMAND_REJECTED, error);
		commandMutex.unlock();
		return { command.id, DJ_COMMAND_REJECTED, error };
	}

	const DjSubmitResult result = admitAssistCommand(
		command, commandQueue, commandResults, assistTracked, assistIntentGenerations);
	if(result.error == DJ_COMMAND_ERROR_QUEUE_FULL) queueDrops++;
	commandMutex.unlock();
	return result;
}

DjSubmitResult DjSession::loadDeck(
	uint8_t deck,
	const char* path,
	DjCommandOrigin origin,
	const DjTrackIdentity* identity
){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_LOAD_DECK;
	command.deck = deck;
	metadataMutex.lock();
	command.libraryGeneration = libraryGeneration;
	command.libraryKey = libraryKey;
	metadataMutex.unlock();
	if(identity) command.trackIdentity = *identity;
	if(path){
		const size_t length = strlen(path);
		if(length < DJ_PATH_CAPACITY) memcpy(command.path, path, length + 1);
	}
	return submit(command);
}

DjSubmitResult DjSession::setPlaying(uint8_t deck, bool playing, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_PLAYING;
	command.deck = deck;
	command.value = playing;
	return submit(command);
}

DjSubmitResult DjSession::seek(uint8_t deck, uint16_t seconds, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SEEK;
	command.deck = deck;
	command.value = seconds;
	return submit(command);
}

DjSubmitResult DjSession::setGain(uint8_t deck, uint8_t gain, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_GAIN;
	command.deck = deck;
	command.value = gain;
	return submit(command);
}

DjSubmitResult DjSession::setMix(uint8_t value, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_MIX;
	command.value = value;
	return submit(command);
}

DjSubmitResult DjSession::setEffectType(uint8_t deck, uint8_t slot, uint8_t type, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_EFFECT_TYPE;
	command.deck = deck;
	command.slot = slot;
	command.value = type;
	return submit(command);
}

DjSubmitResult DjSession::setEffectIntensity(uint8_t deck, uint8_t slot, uint8_t intensity, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_EFFECT_INTENSITY;
	command.deck = deck;
	command.slot = slot;
	command.value = intensity;
	return submit(command);
}

DjSubmitResult DjSession::setRecording(bool recording, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_RECORDING;
	command.value = recording;
	return submit(command);
}

DjSubmitResult DjSession::setQuantize(uint8_t deck, DjQuantizeResolution resolution, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_QUANTIZE;
	command.deck = deck;
	command.value = resolution;
	return submit(command);
}

DjSubmitResult DjSession::loopEngage(uint8_t deck, DjLoopLength length, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_LOOP_ENGAGE;
	command.deck = deck;
	command.value = length;
	return submit(command);
}

DjSubmitResult DjSession::loopDisengage(uint8_t deck, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_LOOP_DISENGAGE;
	command.deck = deck;
	return submit(command);
}

DjSubmitResult DjSession::loopReloop(uint8_t deck, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_LOOP_RELOOP;
	command.deck = deck;
	return submit(command);
}

DjSubmitResult DjSession::setSync(uint8_t deck, bool armed, int8_t masterDeck, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_SYNC;
	command.deck = deck;
	command.value = armed;
	// 0 = auto-master, 1..DJ_DECK_COUNT = explicit deck index + 1.
	command.slot = masterDeck < 0 ? 0 : uint8_t(masterDeck) + 1;
	return submit(command);
}

DjSubmitResult DjSession::setCue(uint8_t deck, uint8_t cue, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_SET_CUE;
	command.deck = deck;
	command.slot = cue;
	return submit(command);
}

DjSubmitResult DjSession::triggerCue(uint8_t deck, uint8_t cue, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_TRIGGER_CUE;
	command.deck = deck;
	command.slot = cue;
	return submit(command);
}

DjSubmitResult DjSession::clearCue(uint8_t deck, uint8_t cue, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_CLEAR_CUE;
	command.deck = deck;
	command.slot = cue;
	return submit(command);
}

DjSubmitResult DjSession::assistSetMode(bool coachEnabled, DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_ASSIST_SET_MODE;
	command.value = coachEnabled ? 1 : 0;
	return submit(command);
}

DjSubmitResult DjSession::assistArmTransition(
	uint8_t fromDeck,
	uint8_t toDeck,
	uint32_t libraryIndex,
	const DjTrackIdentity& targetIdentity,
	uint8_t crossfadeBeats,
	bool startAtBoundary,
	bool tempoLock,
	DjCommandOrigin origin
){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_ASSIST_ARM_TRANSITION;
	command.deck = fromDeck;
	command.slot = toDeck;
	command.libraryIndex = libraryIndex;
	command.trackIdentity = targetIdentity;
	command.value = uint16_t(
		crossfadeBeats |
		(startAtBoundary ? (1 << 8) : 0) |
		(tempoLock ? (1 << 9) : 0)
	);
	return submit(command);
}

DjSubmitResult DjSession::assistCancelTransition(DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_ASSIST_CANCEL_TRANSITION;
	return submit(command);
}

#if defined(JAYD_ENABLE_WIRELESS)
DjSubmitResult DjSession::requestPairing(DjCommandOrigin origin){
	DjCommand command = {};
	command.origin = origin;
	command.type = DJ_COMMAND_OPEN_PAIRING;
	return submit(command);
}
#endif

DjCommandError DjSession::validate(const DjCommand& command) const{
#if defined(JAYD_ENABLE_WIRELESS)
	if(command.type > DJ_COMMAND_OPEN_PAIRING) return DJ_COMMAND_ERROR_INVALID_VALUE;
	if(command.type == DJ_COMMAND_OPEN_PAIRING){
		return command.origin == DJ_ORIGIN_PHYSICAL || command.origin == DJ_ORIGIN_LOCAL_UI ?
			DJ_COMMAND_ERROR_NONE : DJ_COMMAND_ERROR_INVALID_VALUE;
	}
	if(command.origin == DJ_ORIGIN_HTTP &&
	   (command.clientId[0] == '\0' || command.clientCommandId[0] == '\0')){
		return DJ_COMMAND_ERROR_CLIENT_ID_REQUIRED;
	}
	if(!djCommandIdentityMatches(command, bootId, sessionId)){
		return DJ_COMMAND_ERROR_STALE_IDENTITY;
	}
#else
	if(command.type > DJ_COMMAND_ASSIST_CANCEL_TRANSITION) return DJ_COMMAND_ERROR_INVALID_VALUE;
#endif

	const bool deckCommand = command.type == DJ_COMMAND_LOAD_DECK ||
							 command.type == DJ_COMMAND_SET_PLAYING ||
							 command.type == DJ_COMMAND_SEEK ||
							 command.type == DJ_COMMAND_SET_GAIN ||
							 command.type == DJ_COMMAND_SET_EFFECT_TYPE ||
							 command.type == DJ_COMMAND_SET_EFFECT_INTENSITY ||
							 command.type == DJ_COMMAND_SET_QUANTIZE ||
							 command.type == DJ_COMMAND_LOOP_ENGAGE ||
							 command.type == DJ_COMMAND_LOOP_DISENGAGE ||
							 command.type == DJ_COMMAND_LOOP_RELOOP ||
							 command.type == DJ_COMMAND_SET_SYNC ||
							 command.type == DJ_COMMAND_SET_CUE ||
							 command.type == DJ_COMMAND_TRIGGER_CUE ||
							 command.type == DJ_COMMAND_CLEAR_CUE;
	if(deckCommand && command.deck >= DJ_DECK_COUNT) return DJ_COMMAND_ERROR_INVALID_DECK;
	if((command.type == DJ_COMMAND_SET_EFFECT_TYPE ||
		command.type == DJ_COMMAND_SET_EFFECT_INTENSITY) &&
	   command.slot >= DJ_EFFECT_SLOT_COUNT) return DJ_COMMAND_ERROR_INVALID_SLOT;
	if((command.type == DJ_COMMAND_SET_CUE ||
		command.type == DJ_COMMAND_TRIGGER_CUE ||
		command.type == DJ_COMMAND_CLEAR_CUE) &&
	   command.slot >= DJ_CUE_COUNT) return DJ_COMMAND_ERROR_INVALID_SLOT;
	if(command.type == DJ_COMMAND_SET_EFFECT_TYPE && command.value >= DJ_EFFECT_COUNT){
		return DJ_COMMAND_ERROR_INVALID_VALUE;
	}
	if((command.type == DJ_COMMAND_SET_PLAYING || command.type == DJ_COMMAND_SET_RECORDING) &&
	   command.value > 1) return DJ_COMMAND_ERROR_INVALID_VALUE;
	if((command.type == DJ_COMMAND_SET_GAIN ||
		command.type == DJ_COMMAND_SET_MIX ||
		command.type == DJ_COMMAND_SET_EFFECT_INTENSITY) &&
	   command.value > 255) return DJ_COMMAND_ERROR_INVALID_VALUE;
	if(command.type == DJ_COMMAND_LOAD_DECK){
		if(command.path[0] == '\0' || memchr(command.path, '\0', DJ_PATH_CAPACITY) == nullptr){
			return DJ_COMMAND_ERROR_INVALID_PATH;
		}
	}
	if(command.type == DJ_COMMAND_SET_QUANTIZE && command.value >= DJ_QUANTIZE_RESOLUTION_COUNT){
		return DJ_COMMAND_ERROR_INVALID_VALUE;
	}
	if(command.type == DJ_COMMAND_LOOP_ENGAGE && command.value >= DJ_LOOP_LENGTH_COUNT){
		return DJ_COMMAND_ERROR_INVALID_VALUE;
	}
	if(command.type == DJ_COMMAND_SET_SYNC){
		if(command.value > 1) return DJ_COMMAND_ERROR_INVALID_VALUE;
		if(command.slot > DJ_DECK_COUNT) return DJ_COMMAND_ERROR_INVALID_SLOT;
		if(command.slot != 0 && command.slot - 1 == command.deck) return DJ_COMMAND_ERROR_INVALID_MASTER;
	}
	if(command.type == DJ_COMMAND_SET_RECORDING && system){
		const DjRecordingState mapped = mapRecordingState(system->getRecordingStatus().state);
		if(djRecordingStartBusy(command.value != 0, mapped)){
			return DJ_COMMAND_ERROR_RECORDING_BUSY;
		}
	}
	if(command.type == DJ_COMMAND_ASSIST_SET_MODE && command.value > 1){
		return DJ_COMMAND_ERROR_INVALID_VALUE;
	}
	if(command.type == DJ_COMMAND_ASSIST_ARM_TRANSITION){
		if(command.deck >= DJ_DECK_COUNT || command.slot >= DJ_DECK_COUNT || command.deck == command.slot){
			return DJ_COMMAND_ERROR_INVALID_DECK;
		}
		const uint8_t crossfadeBeats = uint8_t(command.value & 0xFF);
		if(crossfadeBeats != 4 && crossfadeBeats != 8 && crossfadeBeats != 16 && crossfadeBeats != 32){
			return DJ_COMMAND_ERROR_INVALID_VALUE;
		}
	}
	return DJ_COMMAND_ERROR_NONE;
}

bool DjSession::hasDeck(uint8_t deck) const{
	return system && system->hasChannel(deck);
}

bool DjSession::copySnapshot(DjSnapshot& snapshot){
	snapshotMutex.lock();
	snapshots.copy(snapshot);
	snapshotMutex.unlock();
	return snapshot.sessionActive;
}

bool DjSession::hasPendingLoad(){
	commandMutex.lock();
	const bool found = commandQueue.contains(DJ_COMMAND_LOAD_DECK);
	commandMutex.unlock();
	return found;
}

bool DjSession::libraryWorkAllowed(){
	commandMutex.lock();
	const bool playbackChangePending =
		commandQueue.contains(DJ_COMMAND_LOAD_DECK) ||
		commandQueue.contains(DJ_COMMAND_SET_PLAYING) ||
		commandQueue.contains(DJ_COMMAND_SET_RECORDING);
	commandMutex.unlock();
	if(playbackChangePending) return false;

	DjSnapshot snapshot;
	copySnapshot(snapshot);
	return djAllowsLibraryWork(snapshot);
}

DjMetadataState DjSession::metadataState(JaydMetadata::Status status){
	switch(status){
		case JaydMetadata::Status::Ready: return DJ_METADATA_VALID;
		case JaydMetadata::Status::Stale: return DJ_METADATA_STALE;
		case JaydMetadata::Status::Corrupt: return DJ_METADATA_CORRUPT;
		case JaydMetadata::Status::Unsupported: return DJ_METADATA_UNSUPPORTED;
		case JaydMetadata::Status::Missing: return DJ_METADATA_ABSENT;
	}
	return DJ_METADATA_CORRUPT;
}

DjMetadataState DjSession::refreshLibraryMetadata(uint32_t generation, uint64_t key){
	metadataMutex.lock();
	fs::File file = SD.open(metadataPath);
	const uint64_t fileKey = metadataIdentity(file);
	if(metadataInitialized && libraryGeneration == generation &&
	   libraryKey == key && metadataFileKey == fileKey){
		file.close();
		const DjMetadataState state = metadataState(metadataReaderStatus);
		metadataMutex.unlock();
		return state;
	}
	const bool hadAssociation[DJ_DECK_COUNT] = {
		deckMetadata[0].attached(),
		deckMetadata[1].attached()
	};
	// Bump the (atomic, lock-free-readable) metadataRevision BEFORE
	// swapping the reader below - any concurrent assistMetadataRevision()
	// caller sees the new value the instant it's visible, ahead of the
	// reader mutation it describes, closing the review's check-then-lock
	// gap for callers that re-check the revision immediately adjacent to
	// their own lock (see DjAssistController::fillWorkerStep()/
	// candidateTableReady()). libraryGeneration is bumped here too, but
	// purely for its own external-identity purpose - it is not what
	// candidate-table freshness keys off anymore.
	libraryGeneration = generation;
	libraryKey = key;
	metadataFileKey = fileKey;
	// metadataRevision is bumped here too, alongside libraryGeneration -
	// this is the ONLY place assistMetadataRevision() advances on a real
	// content change, and it must do so even when generation/key (the
	// externally-supplied semantic values) are unchanged but fileKey
	// isn't (a same-generation sidecar file replacement): the candidate
	// table must still be invalidated in that case, which libraryGeneration
	// alone cannot signal since it's about to be reassigned the SAME
	// external value.
	metadataRevision++;
	metadataInitialized = true;
	metadataReader.close();
	metadataReaderStatus = metadataReader.open(file);

	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; ++deck){
		DjMetadataState state = DJ_METADATA_ABSENT;
		if(hasDeck(deck)){
			state = hadAssociation[deck] || metadataReaderStatus == JaydMetadata::Status::Ready
				? DJ_METADATA_STALE
				: metadataState(metadataReaderStatus);
		}
		deckMetadata[deck].invalidate(state, generation);
	}
	const DjMetadataState state = metadataState(metadataReaderStatus);
	metadataMutex.unlock();
	publishSnapshot();
	return state;
}

void DjSession::invalidateLibraryMetadata(){
	metadataMutex.lock();
	// Same bump-before-mutation ordering as refreshLibraryMetadata() above.
	// metadataRevision is bumped unconditionally here too - a subsequent
	// fast reopen landing back on the SAME external generation/key must
	// still be distinguishable from "nothing ever changed" by anything
	// that samples assistMetadataRevision() across the invalidate+reopen
	// window.
	libraryGeneration = 0;
	libraryKey = 0;
	metadataFileKey = 0;
	metadataRevision++;
	metadataInitialized = false;
	metadataReader.close();
	metadataReaderStatus = JaydMetadata::Status::Missing;
	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; ++deck){
		deckMetadata[deck].invalidate(
			deckMetadata[deck].attached() ? DJ_METADATA_STALE : DJ_METADATA_ABSENT,
			0
		);
	}
	metadataMutex.unlock();
	publishSnapshot();
}

DjMetadataState DjSession::resolveMetadata(
	const char* path,
	uint32_t generation,
	uint64_t key,
	const DjTrackIdentity* identity,
	JaydMetadata::Track& track,
	DjTrackMetadataSnapshot& metadata
){
	metadata = {};
	metadata.libraryGeneration = libraryGeneration;
	metadata.state = metadataState(metadataReaderStatus);
	if(metadataReaderStatus != JaydMetadata::Status::Ready) return metadata.state;
	if(generation != libraryGeneration || key != libraryKey){
		metadata.state = DJ_METADATA_STALE;
		return metadata.state;
	}

	const char* normalized = path;
	if(normalized && normalized[0] == '/') ++normalized;
	const uint8_t* fingerprint = identity &&
		(identity->flags & DJ_TRACK_IDENTITY_FINGERPRINT)
		? identity->fingerprint : nullptr;
	const uint8_t* sourceId = identity &&
		(identity->flags & DJ_TRACK_IDENTITY_SOURCE)
		? identity->sourceId : nullptr;
	const JaydMetadata::Status match = metadataReader.trackByPath(
		normalized,
		track,
		fingerprint,
		sourceId
	);
	metadata.state = metadataState(match);
	if(match != JaydMetadata::Status::Ready) return metadata.state;

	if(!metadataReader.readStringHash(track.provenance, metadata.provenanceHash)){
		metadata.state = DJ_METADATA_CORRUPT;
		return metadata.state;
	}
	metadata.sourceSampleRate = track.sampleRate;
	metadata.sourceDurationFrames = track.durationFrames;
	metadata.bpmMilli = track.bpmMilli;
	metadata.key = track.key;
	metadata.rating = track.rating;
	metadata.cueCount = track.cueCount;
	metadata.gridCount = track.gridCount;
	metadata.phraseCount = track.phraseCount;
	if(track.sampleRate && track.durationFrames) metadata.capabilities |= DJ_METADATA_HAS_SOURCE_FRAMES;
	if(track.bpmMilli) metadata.capabilities |= DJ_METADATA_HAS_BPM;
	if(track.key) metadata.capabilities |= DJ_METADATA_HAS_KEY;
	if(track.rating != 255) metadata.capabilities |= DJ_METADATA_HAS_RATING;
	if(track.cueCount) metadata.capabilities |= DJ_METADATA_HAS_CUES;
	if(track.gridCount) metadata.capabilities |= DJ_METADATA_HAS_GRID;
	if(track.phraseCount) metadata.capabilities |= DJ_METADATA_HAS_PHRASES;

	for(uint32_t index = 0; index < track.gridCount; ++index){
		JaydMetadata::Grid grid;
		if(!metadataReader.readGrid(track, index, grid)){
			metadata = {};
			metadata.libraryGeneration = libraryGeneration;
			metadata.state = DJ_METADATA_CORRUPT;
			return metadata.state;
		}
		if(grid.beatNumber == 1) ++metadata.downbeatCount;
		if(grid.confidence > metadata.confidence) metadata.confidence = grid.confidence;
	}
	for(uint32_t index = 0; index < track.phraseCount; ++index){
		JaydMetadata::Phrase phrase;
		if(!metadataReader.readPhrase(track, index, phrase)){
			metadata = {};
			metadata.libraryGeneration = libraryGeneration;
			metadata.state = DJ_METADATA_CORRUPT;
			return metadata.state;
		}
		if(phrase.confidence > metadata.confidence) metadata.confidence = phrase.confidence;
	}
	if(metadata.downbeatCount) metadata.capabilities |= DJ_METADATA_HAS_DOWNBEATS;
	metadata.state = DJ_METADATA_VALID;
	return metadata.state;
}

DjMetadataState DjSession::lookupTrackMetadata(
	const char* path,
	DjTrackMetadataSnapshot& metadata,
	const DjTrackIdentity* identity
){
	metadataMutex.lock();
	JaydMetadata::Track track{};
	const DjMetadataState state = resolveMetadata(
		path,
		libraryGeneration,
		libraryKey,
		identity,
		track,
		metadata
	);
	metadataMutex.unlock();
	return state;
}

bool DjSession::copyCue(uint8_t deck, uint16_t index, JaydMetadata::Cue& cue){
	if(deck >= DJ_DECK_COUNT) return false;
	metadataMutex.lock();
	const bool copied = deckMetadata[deck].attached() &&
		deckMetadata[deck].snapshot().libraryGeneration == libraryGeneration &&
		metadataReader.readCue(metadataTracks[deck], index, cue);
	metadataMutex.unlock();
	return copied;
}

bool DjSession::copyGrid(uint8_t deck, uint16_t index, JaydMetadata::Grid& grid){
	if(deck >= DJ_DECK_COUNT) return false;
	metadataMutex.lock();
	const bool copied = deckMetadata[deck].attached() &&
		deckMetadata[deck].snapshot().libraryGeneration == libraryGeneration &&
		metadataReader.readGrid(metadataTracks[deck], index, grid);
	metadataMutex.unlock();
	return copied;
}

bool DjSession::copyPhrase(uint8_t deck, uint16_t index, JaydMetadata::Phrase& phrase){
	if(deck >= DJ_DECK_COUNT) return false;
	metadataMutex.lock();
	const bool copied = deckMetadata[deck].attached() &&
		deckMetadata[deck].snapshot().libraryGeneration == libraryGeneration &&
		metadataReader.readPhrase(metadataTracks[deck], index, phrase);
	metadataMutex.unlock();
	return copied;
}

bool DjSession::mediaPresent() const{
	return SD.cardType() != CARD_NONE;
}

uint64_t DjSession::deckElapsedFrames(uint8_t deck) const{
	if(deck >= DJ_DECK_COUNT || !system || !hasDeck(deck)) return 0;
	return system->getElapsedSourceFrames(deck);
}

bool DjSession::nextDownbeatFrame(uint8_t deck, uint64_t currentFrame, uint64_t& outFrame) const{
	if(deck >= DJ_DECK_COUNT || !grids[deck].valid()) return false;
	return grids[deck].nextBoundary(currentFrame, DJ_BEAT_QUARTER_BEATS, outFrame);
}

bool DjSession::nextPhraseFrame(uint8_t deck, uint64_t currentFrame, uint64_t& outFrame){
	if(deck >= DJ_DECK_COUNT) return false;
	metadataMutex.lock();
	bool found = false;
	if(deckMetadata[deck].attached() && deckMetadata[deck].snapshot().libraryGeneration == libraryGeneration){
		const uint32_t phraseCount = metadataTracks[deck].phraseCount;
		const uint32_t bound = phraseCount > JaydMetadata::Reader::MaxPhrasesPerTrack
			? JaydMetadata::Reader::MaxPhrasesPerTrack : phraseCount;
		uint64_t best = 0;
		for(uint32_t index = 0; index < bound; ++index){
			JaydMetadata::Phrase phrase;
			if(!metadataReader.readPhrase(metadataTracks[deck], index, phrase)) continue;
			if(phrase.positionFrames > currentFrame && (!found || phrase.positionFrames < best)){
				best = phrase.positionFrames;
				found = true;
			}
		}
		if(found) outFrame = best;
	}
	metadataMutex.unlock();
	return found;
}

// Lock-free: metadataRevision is std::atomic, and refreshLibraryMetadata()/
// invalidateLibraryMetadata()/shutdown() bump it BEFORE mutating
// metadataReader (see DjSession.h's doc comment on the member). No
// metadataMutex acquisition needed for this specific accessor, which is
// what lets a caller safely call it a SECOND time from inside an unrelated
// lock (e.g. DjAssistController's candidateMutex_ critical section, right
// before a commit/readiness decision) with zero lock-ordering/deadlock risk
// between the two independently locked classes. Deliberately independent
// of libraryGeneration (the externally-supplied semantic library identity):
// see metadataRevision's doc comment in DjSession.h for why the two must
// not be conflated.
uint32_t DjSession::assistMetadataRevision(){
	return metadataRevision;
}

uint32_t DjSession::assistTrackCount(){
	metadataMutex.lock();
	const uint32_t count = (metadataInitialized && metadataReaderStatus == JaydMetadata::Status::Ready)
		? metadataReader.trackCount() : 0;
	metadataMutex.unlock();
	return count;
}

bool DjSession::assistTrackEntry(uint32_t index, DjAssistLibraryEntry& outEntry, uint32_t& outRevision){
	metadataMutex.lock();
	// outRevision is captured under the SAME lock acquisition as the
	// trackByIndex() read below - not a separate before/after probe - so
	// it is guaranteed to describe the exact reader state this read used.
	// refreshLibraryMetadata()/invalidateLibraryMetadata() also take
	// metadataMutex around every metadataRevision bump and reader swap,
	// so there is no window in which a refresh can change the reader out
	// from under this read while still reporting the old revision (the
	// previously-possible check-then-lock gap between a generation probe
	// and this call).
	outRevision = metadataRevision;
	JaydMetadata::Track track;
	const bool ok = metadataInitialized && metadataReaderStatus == JaydMetadata::Status::Ready &&
		metadataReader.trackByIndex(index, track) == JaydMetadata::Status::Ready;
	if(ok){
		const DjTrackIdentity identity = DjAssistBridge::buildTrackIdentity(track.fingerprint, track.sourceId);
		outEntry = DjAssistBridge::buildLibraryEntry(
			index, identity, DJ_METADATA_VALID, track.sampleRate, track.durationFrames,
			track.bpmMilli, track.key, track.rating, track.cueCount, track.gridCount, track.phraseCount
		);
	}
	metadataMutex.unlock();
	return ok;
}

bool DjSession::copyAssistSnapshot(DjAssistSnapshot& snapshot) const{
	assistController.copySnapshot(snapshot);
	return true;
}

void DjSession::assistTrackCommand(uint32_t commandId){
	commandMutex.lock();
	assistTracked.id = commandId;
	assistTracked.tracked = true;
	assistTracked.status = DJ_COMMAND_ACCEPTED;
	commandMutex.unlock();
}

DjCommandStatus DjSession::assistTrackedStatus(uint32_t commandId){
	commandMutex.lock();
	DjCommandStatus status = DJ_COMMAND_PENDING;
	if(assistTracked.tracked && assistTracked.id == commandId){
		status = assistTracked.status;
	}
	commandMutex.unlock();
	return status;
}

DjAssistIntentGenerations DjSession::assistIntentGenerationsSnapshot(){
	commandMutex.lock();
	const DjAssistIntentGenerations generations = assistIntentGenerations;
	commandMutex.unlock();
	return generations;
}

void DjSession::assistPurgePendingSystemCommands(uint8_t deck){
	commandMutex.lock();
	DjCommand probe = {};
	// Only .type/.deck feed DjCommandQueue::sameTarget(); any non-system
	// origin works as the probe so removeSystemTargeting() matches every
	// SYSTEM-origin entry for that channel regardless of who ends up
	// "incoming" here (this call never actually admits `probe`).
	probe.origin = DJ_ORIGIN_LOCAL_UI;
	probe.deck = deck;

	const DjCommandType purgedTypes[3] = { DJ_COMMAND_SET_PLAYING, DJ_COMMAND_SET_SYNC, DJ_COMMAND_SET_MIX };
	for(uint8_t typeIndex = 0; typeIndex < 3; ++typeIndex){
		probe.type = purgedTypes[typeIndex];
		uint32_t removedIds[DJ_COMMAND_CAPACITY] = {};
		const uint8_t removedCount = commandQueue.removeSystemTargeting(probe, removedIds, DJ_COMMAND_CAPACITY);
		for(uint8_t i = 0; i < removedCount && i < DJ_COMMAND_CAPACITY; i++){
			commandResults.finish(removedIds[i], DJ_COMMAND_SUPERSEDED, DJ_COMMAND_ERROR_NONE);
			if(assistTracked.tracked && assistTracked.id == removedIds[i]) assistTracked.status = DJ_COMMAND_SUPERSEDED;
		}
	}
	commandMutex.unlock();
}

void DjSession::attachView(InfoGenerator* left, InfoGenerator* right, InfoGenerator* output){
	if(!system || !left || !right || !output) return;
	if(viewAttached && viewInfo[0] == left && viewInfo[1] == right && viewInfo[2] == output) return;
	viewAttached = true;
	if(viewInfo[0] == left && viewInfo[1] == right && viewInfo[2] == output) return;
	viewInfo[0] = left;
	viewInfo[1] = right;
	viewInfo[2] = output;
	system->setChannelInfo(0, left);
	system->setChannelInfo(1, right);
	system->setChannelInfo(2, output);
}

void DjSession::detachView(){
	viewAttached = false;
}

void DjSession::loop(uint micros){
	(void) micros;

	DjCommand command;
	commandMutex.lock();
	const bool available = commandQueue.pop(command);
	commandMutex.unlock();
	if(available){
		DjCommandError error = DJ_COMMAND_ERROR_NONE;
		DjCommandStatus status = DJ_COMMAND_APPLIED;
		DjCommandResult diagnostics = {};
		const bool applied = apply(command, error, status, diagnostics);
		if(!applied) status = DJ_COMMAND_FAILED;
		commandMutex.lock();
		commandResults.finishWithDiagnostics(command.id, status, error,
			diagnostics.targetFrame, diagnostics.lateFrames, diagnostics.missed);
		if(assistTracked.tracked && assistTracked.id == command.id){
			assistTracked.status = status;
		}
		commandMutex.unlock();
	}

	tickLoops();
	tickSync();
	pollRecording();
	// Publish before tickAssist(): assistController.tick() only ever reads
	// state via copySnapshot(), and a command applied above (e.g. a deck
	// reload) must be visible to it in the SAME iteration it was applied
	// in. Publishing here - after apply()/tickLoops()/tickSync()/
	// pollRecording() but before tickAssist() - closes that same-iteration
	// gap; a same-tick target-deck swap is now reflected in the guard
	// tickAssist() builds instead of leaking through on the stale,
	// previous-iteration snapshot. tickAssist() itself never mutates
	// DjSession's own live state directly (it only submits DjCommands,
	// applied on a later iteration), so nothing it does needs a second
	// publish this tick.
	publishSnapshot();
	tickAssist();
}

void DjSession::tickAssist(){
	assistController.tick();
}

bool DjSession::apply(const DjCommand& command, DjCommandError& error, DjCommandStatus& status, DjCommandResult& diagnostics){
	status = DJ_COMMAND_APPLIED;
	diagnostics = {};
	if(ending){
		error = DJ_COMMAND_ERROR_SESSION_ENDING;
		return false;
	}

	if(command.type == DJ_COMMAND_LOAD_DECK) return applyLoad(command, error);

	if((command.type == DJ_COMMAND_SET_PLAYING ||
		command.type == DJ_COMMAND_SEEK ||
		command.type == DJ_COMMAND_SET_QUANTIZE ||
		command.type == DJ_COMMAND_LOOP_ENGAGE ||
		command.type == DJ_COMMAND_LOOP_DISENGAGE ||
		command.type == DJ_COMMAND_LOOP_RELOOP ||
		command.type == DJ_COMMAND_SET_SYNC ||
		command.type == DJ_COMMAND_SET_CUE ||
		command.type == DJ_COMMAND_TRIGGER_CUE) &&
	   !hasDeck(command.deck)){
		error = DJ_COMMAND_ERROR_NO_DECK;
		return false;
	}

	switch(command.type){
		case DJ_COMMAND_SET_PLAYING: {
			if(command.value){
				// Quantized play-start only applies to a currently paused deck;
				// a deck that is already playing just keeps playing (no-op).
				if(system->isChannelPaused(command.deck) &&
				   quantizeResolution[command.deck] != DJ_QUANTIZE_OFF &&
				   grids[command.deck].valid()){
					const uint64_t currentFrame = system->getElapsedSourceFrames(command.deck);
					uint64_t targetFrame;
					if(djQuantizeTarget(grids[command.deck], currentFrame,
										 djQuantizeQuarterBeats(quantizeResolution[command.deck]), targetFrame)){
						if(targetFrame != currentFrame){
							system->seekChannelSourceFrame(command.deck, targetFrame);
						}
						diagnostics.targetFrame = targetFrame;
						diagnostics.lateFrames = targetFrame > currentFrame ? 0 : currentFrame - targetFrame;
					}
				}
				system->resumeChannel(command.deck);
			}else{
				system->pauseChannel(command.deck);
			}
			return true;
		}
		case DJ_COMMAND_SEEK:
			system->seekChannel(command.deck, command.value);
			return true;
		case DJ_COMMAND_SET_GAIN:
			gains[command.deck] = command.value;
			system->setVolume(command.deck, gains[command.deck]);
			return true;
		case DJ_COMMAND_SET_MIX:
			// assistIntentGenerations.mix is bumped at admission time in
			// admitAssistCommand() (called from submit()), not here - a
			// manual mix that is later superseded before ever applying
			// still needs to have registered so a system-origin mix
			// attempted while it was in flight sees the origin-priority
			// rejection instead of racing this apply.
			mix = command.value;
			system->setMix(mix);
			return true;
		case DJ_COMMAND_SET_EFFECT_TYPE: {
			if(command.value == DJ_EFFECT_SPEED && syncArmed[command.deck]){
				// The Speed effect and Sync share the single per-channel rate
				// resource; only one owner at a time.
				error = DJ_COMMAND_ERROR_SYNC_CONFLICT;
				return false;
			}
			DjEffectTransition transition;
			if(!effectState.setType(command.deck, command.slot, command.value,
									system->hasChannel(command.deck), transition)){
				error = DJ_COMMAND_ERROR_INVALID_VALUE;
				return false;
			}
			if(transition.removeSpeed) system->removeSpeed(command.deck);
			if(transition.clearEffect){
				system->setEffect(command.deck, command.slot, EffectType::NONE);
			}else{
				system->setEffect(command.deck, command.slot, static_cast<EffectType>(command.value));
			}
			if(transition.addSpeed) system->addSpeed(command.deck);
			if(transition.setSpeed){
				system->setSpeed(command.deck, effectState.get(command.deck, command.slot).intensity);
			}
			return true;
		}
		case DJ_COMMAND_SET_EFFECT_INTENSITY: {
			DjEffectTransition transition;
			if(!effectState.setIntensity(command.deck, command.slot, command.value, transition)){
				error = DJ_COMMAND_ERROR_NO_EFFECT;
				return false;
			}
			const DjEffectSnapshot& effect = effectState.get(command.deck, command.slot);
			if(transition.setSpeed){
				system->setSpeed(command.deck, effect.intensity);
			}else if(effect.type != DJ_EFFECT_SPEED){
				system->setEffectIntensity(command.deck, command.slot, effect.intensity);
			}
			return true;
		}
		case DJ_COMMAND_SET_RECORDING: {
			if(!hasDeck(0) && !hasDeck(1)){
				error = DJ_COMMAND_ERROR_NO_DECK;
				return false;
			}
			const bool ok = command.value ? system->startRecording() : system->stopRecording();
			if(!ok){
				error = DJ_COMMAND_ERROR_RECORDING_FAILED;
				return false;
			}
			return true;
		}
		case DJ_COMMAND_SET_QUANTIZE:
			if(command.value >= DJ_QUANTIZE_RESOLUTION_COUNT){
				error = DJ_COMMAND_ERROR_INVALID_VALUE;
				return false;
			}
			if(command.value != DJ_QUANTIZE_OFF && !grids[command.deck].valid()){
				error = DJ_COMMAND_ERROR_NO_GRID;
				return false;
			}
			quantizeResolution[command.deck] = static_cast<DjQuantizeResolution>(command.value);
			return true;
		case DJ_COMMAND_LOOP_ENGAGE: {
			if(!grids[command.deck].valid()){
				error = DJ_COMMAND_ERROR_NO_GRID;
				return false;
			}
			if(command.value >= DJ_LOOP_LENGTH_COUNT){
				error = DJ_COMMAND_ERROR_INVALID_VALUE;
				return false;
			}
			const uint64_t currentFrame = system->getElapsedSourceFrames(command.deck);
			const uint64_t durationFrames = system->getDurationSourceFrames(command.deck);
			if(!loopEngines[command.deck].engage(grids[command.deck], currentFrame, durationFrames,
												  static_cast<DjLoopLength>(command.value), command.id)){
				error = DJ_COMMAND_ERROR_LOOP_OUT_OF_RANGE;
				return false;
			}
			status = DJ_COMMAND_PENDING;
			diagnostics.targetFrame = loopEngines[command.deck].startFrame();
			return true;
		}
		case DJ_COMMAND_LOOP_RELOOP: {
			if(!loopEngines[command.deck].reloop(command.id)){
				error = DJ_COMMAND_ERROR_LOOP_BUSY;
				return false;
			}
			status = DJ_COMMAND_PENDING;
			diagnostics.targetFrame = loopEngines[command.deck].startFrame();
			return true;
		}
		case DJ_COMMAND_LOOP_DISENGAGE:
			loopEngines[command.deck].disengage();
			return true;
		case DJ_COMMAND_SET_SYNC: {
			if(command.value){
				if(!grids[command.deck].valid()){
					error = DJ_COMMAND_ERROR_SYNC_UNAVAILABLE;
					syncLastError[command.deck] = error;
					return false;
				}
				if(effectState.isSpeedActive(command.deck)){
					error = DJ_COMMAND_ERROR_SYNC_CONFLICT;
					syncLastError[command.deck] = error;
					return false;
				}
				const int8_t requestedMaster = command.slot == 0 ? int8_t(-1) : int8_t(command.slot - 1);
				if(requestedMaster >= 0 && (requestedMaster == command.deck || requestedMaster >= DJ_DECK_COUNT)){
					error = DJ_COMMAND_ERROR_INVALID_MASTER;
					syncLastError[command.deck] = error;
					return false;
				}
				// Deterministic anti-oscillation rule: with two decks, at most
				// one may be armed as a follower at a time.
				for(uint8_t other = 0; other < DJ_DECK_COUNT; ++other){
					if(other != command.deck && syncArmed[other]){
						error = DJ_COMMAND_ERROR_SYNC_CONFLICT;
						syncLastError[command.deck] = error;
						return false;
					}
				}
				syncArmed[command.deck] = true;
				syncMasterDeck[command.deck] = requestedMaster;
				syncLastError[command.deck] = DJ_COMMAND_ERROR_NONE;
				system->addSpeed(command.deck); // idempotent: no-ops if already present
			}else{
				syncArmed[command.deck] = false;
				syncMasterDeck[command.deck] = -1;
				syncLastError[command.deck] = DJ_COMMAND_ERROR_NONE;
				syncControllers[command.deck].reset();
				if(!effectState.isSpeedActive(command.deck)){
					system->setRate(command.deck, DJ_RATE_NEUTRAL);
					system->removeSpeed(command.deck);
				}
			}
			return true;
		}
		case DJ_COMMAND_SET_CUE:
			return cues.set(command.deck, command.slot, system->getElapsed(command.deck));
		case DJ_COMMAND_TRIGGER_CUE: {
			uint16_t position = 0;
			if(!cues.trigger(command.deck, command.slot, position)){
				error = DJ_COMMAND_ERROR_EMPTY_CUE;
				return false;
			}
			system->seekChannel(command.deck, position);
			return true;
		}
		case DJ_COMMAND_CLEAR_CUE:
			return cues.clear(command.deck, command.slot);
		case DJ_COMMAND_ASSIST_SET_MODE:
			assistController.setCoachEnabled(command.value != 0);
			return true;
		case DJ_COMMAND_ASSIST_ARM_TRANSITION: {
			const uint8_t crossfadeBeats = uint8_t(command.value & 0xFF);
			const bool startAtBoundary = (command.value & (1 << 8)) != 0;
			const bool tempoLock = (command.value & (1 << 9)) != 0;
			if(!assistController.armTransition(
				command.deck, command.slot, command.libraryIndex, command.trackIdentity,
				crossfadeBeats, startAtBoundary, tempoLock
			)){
				error = DJ_COMMAND_ERROR_ASSIST_REJECTED;
				return false;
			}
			return true;
		}
		case DJ_COMMAND_ASSIST_CANCEL_TRANSITION:
			assistController.cancelTransition();
			return true;
#if defined(JAYD_ENABLE_WIRELESS)
		case DJ_COMMAND_OPEN_PAIRING:
			pairingGeneration++;
			if(pairingGeneration == 0) pairingGeneration++;
			return true;
#endif
		default:
			error = DJ_COMMAND_ERROR_INVALID_VALUE;
			return false;
	}
}

bool DjSession::applyLoad(const DjCommand& command, DjCommandError& error){
	if(system){
		const RecordingState state = system->getRecordingStatus().state;
		if(state == RecordingState::STARTING ||
		   state == RecordingState::RECORDING ||
		   state == RecordingState::STOPPING){
			error = DJ_COMMAND_ERROR_RECORDING_ACTIVE;
			return false;
		}
	}
	fs::File file = SD.open(command.path);
	if(!file){
		error = DJ_COMMAND_ERROR_OPEN_FAILED;
		return false;
	}

	JaydMetadata::Track candidateTrack{};
	DjTrackMetadataSnapshot candidateMetadata{};
	metadataMutex.lock();
	resolveMetadata(
		command.path,
		command.libraryGeneration,
		command.libraryKey,
		&command.trackIdentity,
		candidateTrack,
		candidateMetadata
	);

	const bool hadLeft = hasDeck(0);
	const bool hadRight = hasDeck(1);
	const bool opened = system->openChannel(command.deck, file);
	if(!opened){
		deckMetadata[command.deck].commitIfLoaded(candidateMetadata, false, false);
		metadataMutex.unlock();
		file.close();
		error = DJ_COMMAND_ERROR_OPEN_FAILED;
		return false;
	}

	files[command.deck] = file;
	memcpy(paths[command.deck], command.path, strlen(command.path) + 1);
	metadataTracks[command.deck] = candidateTrack;
	deckMetadata[command.deck].commitIfLoaded(
		candidateMetadata,
		candidateMetadata.state == DJ_METADATA_VALID,
		true
	);
	metadataMutex.unlock();

	// Successful hot-load: old loop bounds/anchors belonged to the previous
	// track and are meaningless now, so always clear and rebuild from
	// scratch. A failed load (handled above, before this point) leaves both
	// untouched.
	loopEngines[command.deck].disengage();
	buildGrid(command.deck);
	cues.clearDeck(command.deck);

	system->setVolume(command.deck, gains[command.deck]);
	DjEffectTransition effectTransition;
	effectState.deckLoaded(command.deck, effectTransition);
	if(effectTransition.addSpeed) system->addSpeed(command.deck);
	if(effectTransition.setSpeed){
		for(uint8_t slot = 0; slot < DJ_EFFECT_SLOT_COUNT; slot++){
			const DjEffectSnapshot& effect = effectState.get(command.deck, slot);
			if(effect.type != DJ_EFFECT_SPEED) continue;
			system->setSpeed(command.deck, effect.intensity);
			break;
		}
	}

	if(!system->isRunning()){
		mix = command.deck == 0 ? 0 : 255;
		system->setMix(mix);
		system->start();
	}else if(!hadLeft && !hadRight){
		mix = command.deck == 0 ? 0 : 255;
		system->setMix(mix);
	}
	return true;
}

bool DjSession::buildGrid(uint8_t deck){
	if(deck >= DJ_DECK_COUNT) return false;
	grids[deck].reset();

	metadataMutex.lock();
	const bool attached = deckMetadata[deck].attached() &&
		deckMetadata[deck].snapshot().libraryGeneration == libraryGeneration;
	if(!attached){
		metadataMutex.unlock();
		return false;
	}
	const DjTrackMetadataSnapshot metadata = deckMetadata[deck].snapshot();
	const JaydMetadata::Track track = metadataTracks[deck];

	// Fail closed: any missing/insufficient capability disables the grid
	// entirely rather than risking a wrong quantize/loop/sync decision.
	if(!(metadata.capabilities & DJ_METADATA_HAS_GRID) ||
	   !(metadata.capabilities & DJ_METADATA_HAS_SOURCE_FRAMES) ||
	   !(metadata.capabilities & DJ_METADATA_HAS_BPM) ||
	   metadata.confidence < DJ_GRID_MIN_CONFIDENCE ||
	   track.gridCount == 0){
		metadataMutex.unlock();
		return false;
	}

	// Subsample the metadata reader's ordered grid section down to the
	// bounded anchor cache, keeping each surviving anchor's true sequential
	// index as its quarter-beat value (never renumbered), so sparse gaps
	// remain correct.
	DjGridAnchor anchors[DJ_GRID_ANCHOR_CAPACITY];
	uint16_t anchorCount = 0;
	const uint32_t stride = (track.gridCount + DJ_GRID_ANCHOR_CAPACITY - 1) / DJ_GRID_ANCHOR_CAPACITY;
	for(uint32_t index = 0; index < track.gridCount && anchorCount < DJ_GRID_ANCHOR_CAPACITY; index += stride){
		JaydMetadata::Grid grid;
		if(!metadataReader.readGrid(track, index, grid)){
			metadataMutex.unlock();
			return false;
		}
		if(grid.confidence < DJ_GRID_MIN_CONFIDENCE) continue;
		anchors[anchorCount].frame = grid.positionFrames;
		anchors[anchorCount].quarterBeat = int64_t(index) * DJ_BEAT_QUARTER_BEATS;
		anchorCount++;
	}
	metadataMutex.unlock();

	if(anchorCount == 0) return false;
	return grids[deck].build(metadata.sourceSampleRate, metadata.bpmMilli,
							  metadata.sourceDurationFrames, anchors, anchorCount);
}

uint8_t DjSession::resolveMasterDeck(uint8_t followerDeck) const{
	const int8_t explicitMaster = syncMasterDeck[followerDeck];
	if(explicitMaster >= 0 && explicitMaster < DJ_DECK_COUNT) return uint8_t(explicitMaster);
	return followerDeck == 0 ? 1 : 0; // auto-master: the other deck (DJ_DECK_COUNT == 2)
}

void DjSession::tickLoops(){
	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; ++deck){
		DjLoopEngine& engine = loopEngines[deck];
		if(engine.state() == DJ_LOOP_INACTIVE) continue;
		if(!hasDeck(deck)){
			engine.disengage();
			continue;
		}

		const uint64_t currentFrame = system->getElapsedSourceFrames(deck);
		uint64_t targetFrame;
		if(!engine.needsSeek(currentFrame, targetFrame)) continue;

		// Single seek in flight, never a busy-loop of repeated attempts.
		engine.beginSeek();
		const bool success = system->seekChannelSourceFrame(deck, targetFrame);
		const uint32_t commandId = engine.pendingCommandId();
		const uint64_t lateFramesWide = currentFrame > targetFrame ? currentFrame - targetFrame : 0;
		const int32_t lateFrames = lateFramesWide > uint64_t(INT32_MAX) ? INT32_MAX : int32_t(lateFramesWide);
		engine.completeSeek(success);

		// Only report a terminal outcome: success (now ACTIVE), or the retry
		// budget just got exhausted (completeSeek() disengaged us). A failed
		// attempt that still has retries left stays PENDING and silently
		// tries again next tick -- no result yet.
		const bool terminal = success || engine.state() == DJ_LOOP_INACTIVE;
		if(commandId != 0 && terminal){
			commandMutex.lock();
			commandResults.finishWithDiagnostics(commandId,
				success ? DJ_COMMAND_APPLIED : DJ_COMMAND_FAILED,
				success ? DJ_COMMAND_ERROR_NONE : DJ_COMMAND_ERROR_LOOP_OUT_OF_RANGE,
				targetFrame, lateFrames, !success);
			commandMutex.unlock();
			engine.clearPendingCommandId();
		}
	}
}

void DjSession::tickSync(){
	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; ++deck){
		if(!syncArmed[deck]){
			syncControllers[deck].reset();
			continue;
		}

		DjSyncInputs inputs;
		inputs.armed = true;

		const bool followerReady = hasDeck(deck) && grids[deck].valid();
		if(followerReady){
			const uint8_t masterDeck = resolveMasterDeck(deck);
			inputs.followerValid = true;
			inputs.followerPlaying = !system->isChannelPaused(deck);
			inputs.followerBpmMilli = grids[deck].bpmMilli();
			inputs.followerSampleRate = grids[deck].sampleRate();

			inputs.masterValid = hasDeck(masterDeck) && grids[masterDeck].valid();
			if(inputs.masterValid){
				inputs.masterPlaying = !system->isChannelPaused(masterDeck);
				inputs.masterBpmMilli = grids[masterDeck].bpmMilli();

				int64_t followerPhase = 0, masterPhase = 0;
				uint64_t followerFramesPerBeat = 0, masterFramesPerBeat = 0;
				const uint64_t followerFrame = system->getElapsedSourceFrames(deck);
				const uint64_t masterFrame = system->getElapsedSourceFrames(masterDeck);
				if(djBeatPhase(grids[deck], followerFrame, followerPhase, followerFramesPerBeat) &&
				   djBeatPhase(grids[masterDeck], masterFrame, masterPhase, masterFramesPerBeat)){
					inputs.followerPhaseFrames = followerPhase;
					inputs.followerFramesPerBeat = followerFramesPerBeat;
					inputs.masterPhaseFrames = masterPhase;
					inputs.masterFramesPerBeat = masterFramesPerBeat;
				}else{
					// Phase not currently computable (e.g. before the first
					// anchor): hold the rate rather than guessing.
					inputs.masterValid = false;
				}
			}
		}
		// else: deck unloaded or metadata/grid lost since arming -- leave
		// masterValid/followerValid false so the controller surfaces this as
		// an observable ERROR state instead of silently doing nothing.

		const DjSyncOutputs out = syncControllers[deck].tick(inputs);
		if(out.applyRate) system->setRate(deck, out.targetRate);
		if(out.applyNudge) system->nudgeRate(deck, out.nudgeAmount);
		if(out.hardAlign){
			const uint64_t followerFrame = system->getElapsedSourceFrames(deck);
			const int64_t corrected = int64_t(followerFrame) - out.hardAlignPhaseErrorFrames;
			if(corrected > 0) system->seekChannelSourceFrame(deck, uint64_t(corrected));
		}
	}
}

DjRecordingState DjSession::mapRecordingState(RecordingState state){
	switch(state){
		case RecordingState::IDLE: return DJ_RECORDING_IDLE;
		case RecordingState::STARTING: return DJ_RECORDING_STARTING;
		case RecordingState::RECORDING: return DJ_RECORDING_ACTIVE;
		case RecordingState::STOPPING: return DJ_RECORDING_STOPPING;
		case RecordingState::COMPLETE: return DJ_RECORDING_COMPLETE;
		case RecordingState::FAILED: return DJ_RECORDING_FAILED;
		default: return DJ_RECORDING_FAILED;
	}
}

DjRecordingError DjSession::mapRecordingError(RecordingError error){
	switch(error){
		case RecordingError::NONE: return DJ_RECORDING_ERROR_NONE;
		case RecordingError::SD_UNAVAILABLE: return DJ_RECORDING_ERROR_SD_UNAVAILABLE;
		case RecordingError::OPEN_FAILED: return DJ_RECORDING_ERROR_OPEN_FAILED;
		case RecordingError::WRITE_FAILED: return DJ_RECORDING_ERROR_WRITE_FAILED;
		case RecordingError::FINALIZE_FAILED: return DJ_RECORDING_ERROR_FINALIZE_FAILED;
		case RecordingError::BUFFER_OVERRUN: return DJ_RECORDING_ERROR_BUFFER_OVERRUN;
		case RecordingError::QUEUE_FULL: return DJ_RECORDING_ERROR_QUEUE_FULL;
		default: return DJ_RECORDING_ERROR_NONE;
	}
}

void DjSession::pollRecording(){
	if(!system) return;
	const RecordingStatus status = system->getRecordingStatus();
	const DjRecordingState mapped = mapRecordingState(status.state);

	if(mapped != lastRecordingState){
		if(mapped == DJ_RECORDING_STARTING){
			// fresh attempt: clear any stale path/error from a prior recording
			recordingSnapshot.path[0] = '\0';
			recordingSnapshot.error = DJ_RECORDING_ERROR_NONE;
			recordingSnapshot.valid = false;
			finalizeError = DJ_RECORDING_ERROR_NONE;
		}else if(mapped == DJ_RECORDING_COMPLETE && status.fileValid){
			// finalize exactly once, on the completion edge
			char finalPath[DJ_PATH_CAPACITY];
			const auto outcome = DjRecordingStorage::finalizeRecording(MixSystem::recordPath, finalPath, sizeof(finalPath));
			if(outcome == DjRecordingStorage::FinalizeOutcome::SUCCESS){
				memcpy(recordingSnapshot.path, finalPath, strlen(finalPath) + 1);
			}else{
				recordingSnapshot.path[0] = '\0';
				finalizeError = outcome == DjRecordingStorage::FinalizeOutcome::RENAME_FAILED
					? DJ_RECORDING_ERROR_RENAME_FAILED
					: DJ_RECORDING_ERROR_NAME_EXHAUSTED;
			}
		}
		lastRecordingState = mapped;
	}

	recordingSnapshot.state = mapped;
	// Freeze the terminal error/validity once the outcome is settled so a
	// subsequent idle poll can't clobber what the UI/API needs to show. A
	// storage-layer finalize failure (bounded naming exhausted or rename
	// failed) overrides a library-reported success: the recording captured
	// fine but could not be moved into permanent, collision-safe storage.
	if(mapped == DJ_RECORDING_COMPLETE || mapped == DJ_RECORDING_FAILED){
		if(finalizeError != DJ_RECORDING_ERROR_NONE){
			recordingSnapshot.error = finalizeError;
			recordingSnapshot.valid = false;
		}else{
			recordingSnapshot.error = mapRecordingError(status.error);
			recordingSnapshot.valid = mapped == DJ_RECORDING_COMPLETE && status.fileValid;
		}
	}else{
		recordingSnapshot.error = DJ_RECORDING_ERROR_NONE;
		recordingSnapshot.valid = false;
	}
	recordingSnapshot.bytes = status.bytes;
	recordingSnapshot.durationMs = status.durationMs;
}

void DjSession::publishSnapshot(){
	DjSnapshot snapshot = {};
	snapshot.seq = ++snapshotSeq;
	snapshot.bootId = bootId;
	snapshot.sessionId = sessionId;
	snapshot.sessionActive = !ending;
	snapshot.mixerRunning = system && system->isRunning();
	snapshot.mix = mix;
	snapshot.recordingInfo = recordingSnapshot;

	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; deck++){
		DjDeckSnapshot& deckSnapshot = snapshot.decks[deck];
		deckSnapshot.loaded = hasDeck(deck);
		deckSnapshot.playing = deckSnapshot.loaded && !system->isChannelPaused(deck);
		deckSnapshot.elapsed = deckSnapshot.loaded ? system->getElapsed(deck) : 0;
		deckSnapshot.duration = deckSnapshot.loaded ? system->getDuration(deck) : 0;
		deckSnapshot.timingQuality = deckSnapshot.loaded ? DJ_TIMING_COARSE : DJ_TIMING_UNAVAILABLE;
		deckSnapshot.gain = gains[deck];
		memcpy(deckSnapshot.path, paths[deck], DJ_PATH_CAPACITY);
		effectState.copyDeck(deck, deckSnapshot.effects);
		cues.copyDeck(deck, deckSnapshot.cues);

		deckSnapshot.grid.valid = grids[deck].valid();
		if(deckSnapshot.grid.valid){
			deckSnapshot.grid.confidence = deckMetadata[deck].snapshot().confidence;
			const uint64_t currentFrame = deckSnapshot.loaded ? system->getElapsedSourceFrames(deck) : 0;
			int64_t quarterBeat = 0;
			uint64_t boundaryFrame = 0;
			if(grids[deck].quarterBeatAtFrame(currentFrame, quarterBeat, boundaryFrame)){
				deckSnapshot.grid.currentQuarterBeat = quarterBeat;
			}
		}

		deckSnapshot.quantize.resolution = quantizeResolution[deck];
		deckSnapshot.quantize.pending = loopEngines[deck].state() == DJ_LOOP_PENDING;
		deckSnapshot.quantize.pendingTargetFrame = deckSnapshot.quantize.pending
			? loopEngines[deck].startFrame() : 0;

		const uint64_t durationFrames = deckSnapshot.loaded ? system->getDurationSourceFrames(deck) : 0;
		const uint64_t currentFrame = deckSnapshot.loaded ? system->getElapsedSourceFrames(deck) : 0;
		const uint8_t validLengthMask = grids[deck].valid()
			? djValidLoopLengthMask(grids[deck], currentFrame, durationFrames) : 0;
		deckSnapshot.loop = loopEngines[deck].snapshot(validLengthMask);

		deckSnapshot.sync.state = syncControllers[deck].state();
		deckSnapshot.sync.masterDeck = syncMasterDeck[deck];
		deckSnapshot.sync.targetRate = system ? system->getRate(deck) : DJ_RATE_NEUTRAL;
		deckSnapshot.sync.lastError = syncLastError[deck];
	}
	metadataMutex.lock();
	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; ++deck){
		snapshot.decks[deck].metadata = deckMetadata[deck].snapshot();
		snapshot.decks[deck].identity = deckMetadata[deck].attached()
			? DjAssistBridge::buildTrackIdentity(metadataTracks[deck].fingerprint, metadataTracks[deck].sourceId)
			: DjTrackIdentity();
	}
	metadataMutex.unlock();

	commandMutex.lock();
	snapshot.queueDepth = commandQueue.depth();
	snapshot.queueDrops = queueDrops;
#if defined(JAYD_ENABLE_WIRELESS)
	snapshot.pairingGeneration = pairingGeneration;
#endif
	commandResults.copyTo(snapshot.recentResults);
	commandMutex.unlock();

	snapshotMutex.lock();
	snapshots.publish(snapshot);
	snapshotMutex.unlock();
}

void DjSession::shutdown(){
	commandMutex.lock();
	ending = true;
	DjCommand command;
	while(commandQueue.pop(command)){
		commandResults.finish(command.id, DJ_COMMAND_FAILED, DJ_COMMAND_ERROR_SESSION_ENDING);
	}
	commandMutex.unlock();

	publishSnapshot();
	if(system) system->stop();

	// Stop (blocking join) the background candidate-fill task BEFORE
	// touching metadataReader below. DjAssistController::end() is already
	// idempotent and already joins its task before freeing anything -
	// see DjAssistController.cpp - but it was never being called here at
	// all: the fill task was previously only ever stopped as an
	// incidental side effect of assistController's OWN member destructor
	// running, which happens well AFTER shutdown() returns and the reader
	// is already closed below. A straggling in-flight fillWorkerStep()
	// iteration could then run assistTrackEntry()/assistMetadataRevision()
	// against an already-closed reader. Calling end() here first
	// guarantees no such iteration is still in flight by the time
	// metadataReader.close() runs. The destructor's later implicit
	// end() call (via ~DjAssistController()) remains a safe no-op.
	assistController.end();

	metadataMutex.lock();
	// Bump-before-mutate, same ordering as refreshLibraryMetadata()/
	// invalidateLibraryMetadata(): any assistMetadataRevision() reader
	// (there are none left in-flight after assistController.end() above,
	// but this keeps the invariant unconditional, not something a caller
	// must rely on ordering-with-end() to get right) observes the bump
	// strictly before the reader is actually closed.
	metadataRevision++;
	metadataInitialized = false;
	metadataReaderStatus = JaydMetadata::Status::Missing;
	metadataReader.close();
	metadataMutex.unlock();
}
