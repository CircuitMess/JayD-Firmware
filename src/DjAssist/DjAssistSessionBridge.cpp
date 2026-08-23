#include "DjAssistSessionBridge.h"
#include "DjAssistScoring.h"
#include <string.h>

namespace DjAssistBridge {

namespace {
bool isNonZero16(const uint8_t bytes[16]){
	for(uint8_t i = 0; i < 16; ++i){
		if(bytes[i] != 0) return true;
	}
	return false;
}
} // namespace

DjTrackIdentity buildTrackIdentity(const uint8_t fingerprint[16], const uint8_t sourceId[16]){
	DjTrackIdentity identity;
	if(isNonZero16(fingerprint)){
		identity.flags |= DJ_TRACK_IDENTITY_FINGERPRINT;
		memcpy(identity.fingerprint, fingerprint, 16);
	}
	if(isNonZero16(sourceId)){
		identity.flags |= DJ_TRACK_IDENTITY_SOURCE;
		memcpy(identity.sourceId, sourceId, 16);
	}
	return identity;
}

DjAssistLibraryEntry buildLibraryEntry(
	uint32_t libraryIndex,
	const DjTrackIdentity& identity,
	DjMetadataState state,
	uint32_t sampleRate,
	uint64_t durationFrames,
	uint32_t bpmMilli,
	uint16_t key,
	uint8_t rating,
	uint32_t cueCount,
	uint32_t gridCount,
	uint32_t phraseCount
){
	DjAssistLibraryEntry entry;
	entry.libraryIndex = libraryIndex;
	entry.identity = identity;
	entry.state = state;
	entry.bpmMilli = bpmMilli;
	entry.key = key;
	entry.rating = rating;
	entry.durationFrames = durationFrames;
	entry.sampleRate = sampleRate;

	uint16_t capabilities = 0;
	if(sampleRate && durationFrames) capabilities |= DJ_METADATA_HAS_SOURCE_FRAMES;
	if(bpmMilli) capabilities |= DJ_METADATA_HAS_BPM;
	if(key) capabilities |= DJ_METADATA_HAS_KEY;
	if(rating != 255) capabilities |= DJ_METADATA_HAS_RATING;
	if(cueCount) capabilities |= DJ_METADATA_HAS_CUES;
	if(gridCount) capabilities |= DJ_METADATA_HAS_GRID;
	if(phraseCount) capabilities |= DJ_METADATA_HAS_PHRASES;
	entry.capabilities = capabilities;

	return entry;
}

uint8_t computeCrossfadeMix(
	uint8_t toDeck,
	uint64_t elapsedMicros,
	uint8_t crossfadeBeats,
	uint32_t bpmMilli
){
	const uint8_t targetEndpoint = toDeck == 0 ? 0 : 255;
	if(bpmMilli == 0 || crossfadeBeats == 0) return targetEndpoint;

	// microsPerBeat = 60,000,000 us/min * 1000 (bpmMilli scale) / bpmMilli.
	const uint64_t microsPerBeat = 60000000000ULL / bpmMilli;
	if(microsPerBeat == 0) return targetEndpoint;

	const uint64_t elapsedBeats64 = elapsedMicros / microsPerBeat;
	const uint16_t totalSteps = crossfadeBeats;
	const uint16_t stepIndex = elapsedBeats64 > 0xFFFFU ? uint16_t(0xFFFFU) : uint16_t(elapsedBeats64);
	const uint8_t curve = DjAssistScoring::crossfadeCurve(stepIndex, totalSteps);
	// crossfadeCurve ramps 0->255 as stepIndex advances; that already
	// matches the toDeck==1 direction (0=deck0, 255=deck1). toDeck==0 needs
	// the mirror image (255 -> 0 as the transition progresses).
	return toDeck == 0 ? uint8_t(255 - curve) : curve;
}

DjAssistOwnershipReconciliation reconcileOwnership(
	bool priorStartOwnedByPlan,
	bool priorSyncOwnedByPlan,
	uint32_t livePlayGeneration,
	uint32_t armedPlayGeneration,
	uint32_t liveSyncGeneration,
	uint32_t armedSyncGeneration
){
	DjAssistOwnershipReconciliation result;
	result.toDeckStartOwnedByPlan = priorStartOwnedByPlan && (livePlayGeneration == armedPlayGeneration);
	result.toDeckSyncOwnedByPlan = priorSyncOwnedByPlan && (liveSyncGeneration == armedSyncGeneration);
	return result;
}

DjAssistRollbackPhase nextRollbackPhase(
	DjAssistRollbackPhase phase,
	bool crossfadeSubmitted,
	bool manualMixOccurred,
	bool syncOwnedByPlan,
	bool startDeckOwnedByPlan
){
	if(phase == DJ_ASSIST_ROLLBACK_IDLE) phase = DJ_ASSIST_ROLLBACK_MIX;
	if(phase == DJ_ASSIST_ROLLBACK_MIX && (!crossfadeSubmitted || manualMixOccurred)) phase = DJ_ASSIST_ROLLBACK_SYNC;
	if(phase == DJ_ASSIST_ROLLBACK_SYNC && !syncOwnedByPlan) phase = DJ_ASSIST_ROLLBACK_STOP_DECK;
	if(phase == DJ_ASSIST_ROLLBACK_STOP_DECK && !startDeckOwnedByPlan) phase = DJ_ASSIST_ROLLBACK_DONE;
	return phase;
}

DjAssistCommandOutcome evaluateCommandOutcome(DjCommandStatus status){
	switch(status){
		case DJ_COMMAND_APPLIED:
			return DJ_ASSIST_COMMAND_DONE;
		case DJ_COMMAND_REJECTED:
		case DJ_COMMAND_FAILED:
			return DJ_ASSIST_COMMAND_FAILED;
		case DJ_COMMAND_SUPERSEDED:
			return DJ_ASSIST_COMMAND_RESUBMIT;
		case DJ_COMMAND_ACCEPTED:
		case DJ_COMMAND_PENDING:
		default:
			return DJ_ASSIST_COMMAND_WAIT;
	}
}

DjAssistBoundaryArrival evaluateBoundaryArrival(uint64_t currentFrame, uint64_t targetFrame){
	if(currentFrame < targetFrame) return DJ_ASSIST_BOUNDARY_NOT_YET;
	const uint64_t lateFrames = currentFrame - targetFrame;
	return lateFrames <= DJ_QUANTIZE_TOLERANCE_FRAMES ? DJ_ASSIST_BOUNDARY_REACHED : DJ_ASSIST_BOUNDARY_MISSED;
}

bool phraseCacheNeedsRescan(
	const DjAssistPhraseCacheState& cache,
	uint8_t deck,
	uint64_t currentFrame,
	const DjTrackIdentity& identity,
	uint32_t metadataGeneration,
	DjMetadataState metadataState
){
	if(!cache.valid || cache.deck != deck) return true;
	if(!DjAssistScoring::identityMatches(cache.identity, identity)) return true;
	if(cache.metadataGeneration != metadataGeneration || cache.metadataState != metadataState) return true;
	if(currentFrame < cache.lastFrame) return true; // backward seek
	if(!cache.terminal && currentFrame >= cache.frame) return true; // cached boundary passed
	return false;
}

void updatePhraseCache(
	DjAssistPhraseCacheState& cache,
	uint8_t deck,
	uint64_t currentFrame,
	const DjTrackIdentity& identity,
	uint32_t metadataGeneration,
	DjMetadataState metadataState,
	bool found,
	uint64_t phraseFrame
){
	cache.deck = deck;
	cache.valid = true;
	cache.terminal = !found;
	cache.frame = found ? phraseFrame : 0;
	cache.lastFrame = currentFrame;
	cache.identity = identity;
	cache.metadataGeneration = metadataGeneration;
	cache.metadataState = metadataState;
}

bool candidateGenerationCurrent(uint32_t loadedGeneration, uint32_t observedGeneration){
	return loadedGeneration == observedGeneration;
}

} // namespace DjAssistBridge
