#ifndef JAYD_FIRMWARE_DJSESSIONSTATE_H
#define JAYD_FIRMWARE_DJSESSIONSTATE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "DjBeatEngine.h"

static constexpr uint8_t DJ_DECK_COUNT = 2;
static constexpr uint8_t DJ_EFFECT_SLOT_COUNT = 3;
static constexpr uint8_t DJ_CUE_COUNT = 8;
static constexpr uint8_t DJ_COMMAND_CAPACITY = 16;
#if defined(JAYD_ENABLE_WIRELESS)
static constexpr uint8_t DJ_RECENT_RESULT_COUNT = DJ_COMMAND_CAPACITY + 8;
#else
static constexpr uint8_t DJ_RECENT_RESULT_COUNT = 8;
#endif
static constexpr size_t DJ_PATH_CAPACITY = 128;

enum DjMetadataState : uint8_t {
	DJ_METADATA_ABSENT,
	DJ_METADATA_VALID,
	DJ_METADATA_STALE,
	DJ_METADATA_CORRUPT,
	DJ_METADATA_UNSUPPORTED
};

enum DjMetadataCapability : uint16_t {
	DJ_METADATA_HAS_SOURCE_FRAMES = 1 << 0,
	DJ_METADATA_HAS_BPM = 1 << 1,
	DJ_METADATA_HAS_KEY = 1 << 2,
	DJ_METADATA_HAS_RATING = 1 << 3,
	DJ_METADATA_HAS_CUES = 1 << 4,
	DJ_METADATA_HAS_GRID = 1 << 5,
	DJ_METADATA_HAS_DOWNBEATS = 1 << 6,
	DJ_METADATA_HAS_PHRASES = 1 << 7
};

enum DjTrackIdentityFlag : uint8_t {
	DJ_TRACK_IDENTITY_FINGERPRINT = 1 << 0,
	DJ_TRACK_IDENTITY_SOURCE = 1 << 1
};

struct DjTrackIdentity {
	uint8_t flags = 0;
	uint8_t fingerprint[16] = {};
	uint8_t sourceId[16] = {};
};

struct DjTrackMetadataSnapshot {
	DjMetadataState state = DJ_METADATA_ABSENT;
	uint16_t capabilities = 0;
	uint32_t libraryGeneration = 0;
	uint32_t provenanceHash = 0;
	uint16_t confidence = 0;
	uint32_t sourceSampleRate = 0;
	uint64_t sourceDurationFrames = 0;
	uint32_t bpmMilli = 0;
	uint16_t key = 0;
	uint8_t rating = 255;
	uint16_t cueCount = 0;
	uint16_t gridCount = 0;
	uint16_t downbeatCount = 0;
	uint16_t phraseCount = 0;
};

enum DjCommandOrigin : uint8_t {
	DJ_ORIGIN_LOCAL_UI,
	DJ_ORIGIN_PHYSICAL,
	DJ_ORIGIN_HTTP,
	DJ_ORIGIN_SYSTEM
};

enum DjCommandType : uint8_t {
	DJ_COMMAND_LOAD_DECK,
	DJ_COMMAND_SET_PLAYING,
	DJ_COMMAND_SEEK,
	DJ_COMMAND_SET_GAIN,
	DJ_COMMAND_SET_MIX,
	DJ_COMMAND_SET_EFFECT_TYPE,
	DJ_COMMAND_SET_EFFECT_INTENSITY,
	DJ_COMMAND_SET_RECORDING,
	DJ_COMMAND_SET_QUANTIZE,
	DJ_COMMAND_LOOP_ENGAGE,
	DJ_COMMAND_LOOP_DISENGAGE,
	DJ_COMMAND_LOOP_RELOOP,
	DJ_COMMAND_SET_SYNC,
	DJ_COMMAND_SET_CUE,
	DJ_COMMAND_TRIGGER_CUE,
	DJ_COMMAND_CLEAR_CUE,
	// Coach/one-shot-transition control surface. Always present (not gated
	// on wireless) since the physical Assist bank uses these too.
	DJ_COMMAND_ASSIST_SET_MODE,
	DJ_COMMAND_ASSIST_ARM_TRANSITION,
	DJ_COMMAND_ASSIST_CANCEL_TRANSITION
#if defined(JAYD_ENABLE_WIRELESS)
	,DJ_COMMAND_OPEN_PAIRING
#endif
};

enum DjCommandStatus : uint8_t {
	DJ_COMMAND_ACCEPTED,
	DJ_COMMAND_APPLIED,
	DJ_COMMAND_FAILED,
	DJ_COMMAND_SUPERSEDED,
	DJ_COMMAND_REJECTED,
	DJ_COMMAND_PENDING
};

enum DjCommandError : uint8_t {
	DJ_COMMAND_ERROR_NONE,
	DJ_COMMAND_ERROR_QUEUE_FULL,
	DJ_COMMAND_ERROR_INVALID_DECK,
	DJ_COMMAND_ERROR_INVALID_SLOT,
	DJ_COMMAND_ERROR_INVALID_VALUE,
	DJ_COMMAND_ERROR_INVALID_PATH,
	DJ_COMMAND_ERROR_NO_DECK,
	DJ_COMMAND_ERROR_NO_EFFECT,
	DJ_COMMAND_ERROR_OPEN_FAILED,
	DJ_COMMAND_ERROR_RECORDING_FAILED,
	DJ_COMMAND_ERROR_SESSION_ENDING,
	DJ_COMMAND_ERROR_NO_GRID,
	DJ_COMMAND_ERROR_LOOP_OUT_OF_RANGE,
	DJ_COMMAND_ERROR_LOOP_BUSY,
	DJ_COMMAND_ERROR_SYNC_UNAVAILABLE,
	DJ_COMMAND_ERROR_SYNC_CONFLICT,
	DJ_COMMAND_ERROR_INVALID_MASTER,
	DJ_COMMAND_ERROR_EMPTY_CUE,
	DJ_COMMAND_ERROR_RECORDING_ACTIVE,
	DJ_COMMAND_ERROR_RECORDING_BUSY,
	// Coach/one-shot-transition arm rejected: target not loaded, source deck
	// not playing, recording/loop conflict, or invalid plan arguments. See
	// DjAssistEngine::armTransition() for the exact guard rules.
	DJ_COMMAND_ERROR_ASSIST_REJECTED,
	// A system-origin mix/play/sync command (Assist's own ramp/rollback/
	// sync-lock step) was rejected because a still-queued manual
	// (non-system) command for that same control channel has not yet been
	// dequeued/applied - see admitAssistCommand()'s queue-wide origin
	// policy below. The manual command keeps its place; the caller (the
	// actuator's poll loop) simply retries the same submit next tick once
	// it drains.
	DJ_COMMAND_ERROR_ASSIST_OVERRIDE_PENDING
#if defined(JAYD_ENABLE_WIRELESS)
	,DJ_COMMAND_ERROR_STALE_IDENTITY,
	DJ_COMMAND_ERROR_CLIENT_ID_REQUIRED
#endif
};

enum DjTimingQuality : uint8_t {
	DJ_TIMING_UNAVAILABLE,
	DJ_TIMING_COARSE
};

enum DjEffectType : uint8_t {
	DJ_EFFECT_NONE,
	DJ_EFFECT_SPEED,
	DJ_EFFECT_LOWPASS,
	DJ_EFFECT_HIGHPASS,
	DJ_EFFECT_REVERB,
	DJ_EFFECT_BITCRUSHER,
	DJ_EFFECT_COUNT
};

// Recording-busy gate shared by DjSession::validate() and the host
// self-check: only a new *start* is rejected while a previous start/stop is
// still in flight (STARTING/ACTIVE/STOPPING). A stop is always allowed
// through, even while STARTING, so a stop issued before the library applies
// an accepted start is forwarded rather than silently rejected -- the
// library's own recording state machine is designed to accept a stop during
// STARTING and transition safely to STOPPING.
enum DjRecordingState : uint8_t {
	DJ_RECORDING_IDLE,
	DJ_RECORDING_STARTING,
	DJ_RECORDING_ACTIVE,
	DJ_RECORDING_STOPPING,
	DJ_RECORDING_COMPLETE,
	DJ_RECORDING_FAILED
};

enum DjRecordingError : uint8_t {
	DJ_RECORDING_ERROR_NONE,
	DJ_RECORDING_ERROR_SD_UNAVAILABLE,
	DJ_RECORDING_ERROR_OPEN_FAILED,
	DJ_RECORDING_ERROR_WRITE_FAILED,
	DJ_RECORDING_ERROR_FINALIZE_FAILED,
	DJ_RECORDING_ERROR_BUFFER_OVERRUN,
	DJ_RECORDING_ERROR_QUEUE_FULL,
	DJ_RECORDING_ERROR_NAME_EXHAUSTED,
	DJ_RECORDING_ERROR_RENAME_FAILED
};

inline bool djRecordingStartBusy(bool isStartCommand, DjRecordingState state){
	return isStartCommand &&
		   (state == DJ_RECORDING_STARTING || state == DJ_RECORDING_ACTIVE || state == DJ_RECORDING_STOPPING);
}

struct DjCommand {
	uint32_t id = 0;
	DjCommandOrigin origin = DJ_ORIGIN_SYSTEM;
	DjCommandType type = DJ_COMMAND_SET_PLAYING;
	uint8_t deck = 0;
	uint8_t slot = 0;
	uint16_t value = 0;
	uint32_t libraryGeneration = 0;
	uint64_t libraryKey = 0;
	DjTrackIdentity trackIdentity = {};
	char path[DJ_PATH_CAPACITY] = {};
	// DJ_COMMAND_ASSIST_ARM_TRANSITION only: candidate-table index of the
	// confirmed target track (paired with trackIdentity, which the engine
	// re-checks against the loaded deck every tick to catch a swap).
	uint32_t libraryIndex = 0;
#if defined(JAYD_ENABLE_WIRELESS)
	uint64_t requestBootId = 0;
	uint32_t requestSessionId = 0;
	char clientId[33] = {};
	char clientCommandId[33] = {};
#endif
};

struct DjSubmitResult {
	uint32_t id = 0;
	DjCommandStatus status = DJ_COMMAND_REJECTED;
	DjCommandError error = DJ_COMMAND_ERROR_NONE;
#if defined(JAYD_ENABLE_WIRELESS)
	bool duplicate = false;
#endif

	DjSubmitResult() = default;
	DjSubmitResult(
		uint32_t id,
		DjCommandStatus status,
		DjCommandError error
#if defined(JAYD_ENABLE_WIRELESS)
		,bool duplicate = false
#endif
	) : id(id), status(status), error(error)
#if defined(JAYD_ENABLE_WIRELESS)
		,duplicate(duplicate)
#endif
	{}

	bool accepted() const{
		return status == DJ_COMMAND_ACCEPTED;
	}
};

struct DjCommandResult {
	uint32_t id = 0;
	DjCommandOrigin origin = DJ_ORIGIN_SYSTEM;
	DjCommandType type = DJ_COMMAND_SET_PLAYING;
	DjCommandStatus status = DJ_COMMAND_REJECTED;
	DjCommandError error = DJ_COMMAND_ERROR_NONE;
	// Diagnostics for quantized/scheduled actions (play-start, loop
	// engage/reloop): the source frame the action targeted, how many frames
	// late it actually applied (0 if on time or not yet resolved), and
	// whether the original boundary was missed and the action fell back to
	// the next one. Zero/false for command types that do not schedule.
	uint64_t targetFrame = 0;
	int32_t lateFrames = 0;
	bool missed = false;
#if defined(JAYD_ENABLE_WIRELESS)
	char clientId[33] = {};
	char clientCommandId[33] = {};
	uint64_t sequence = 0;
#endif
};

// Durable single-slot command-outcome tracker for DjAssist (see
// DjSession::assistTrackCommand()/assistTrackedStatus()) - deliberately
// separate from DjCommandResults' bounded/evictable ring, and updated the
// instant a command is superseded at admission time (see
// admitAssistCommand() below), not only when it later pops/finishes in
// loop() - otherwise a command dropped from the queue before it was ever
// dequeued would leave this slot stuck reporting ACCEPTED forever.
struct DjAssistTrackedCommand {
	uint32_t id = 0;
	bool tracked = false;
	DjCommandStatus status = DJ_COMMAND_PENDING;
};

// Monotonic non-system ("user"/manual) intent generations for the three
// control channels Assist's guard/rollback logic must never silently run
// over: the global crossfader mix, and per-deck play/sync. Each counter is
// bumped in admitAssistCommand() the instant a non-system command for that
// channel is ADMITTED to the queue (fresh push or supersede-replace), not
// when it later applies - so a user command still only queued (racing the
// plan's own step either direction) is visible immediately. System-origin
// commands (Assist's own START_DECK/STOP_DECK/LOCK_TEMPO/ENABLE_SYNC/
// CROSSFADE/rollback steps) never bump these, which is also what makes
// LOCK_TEMPO and ENABLE_SYNC both mapping to the same underlying setSync()
// mutation harmless: neither one is a "user" event, so neither can ever
// trigger a false divergence against a baseline captured at arm time. See
// DjAssistGuardSnapshot/DjAssistTransitionPlan (DjAssistTypes.h) for the
// live/baseline comparison and DjSession::assistIntentGenerationsSnapshot().
struct DjAssistIntentGenerations {
	uint32_t mix = 0;
	uint32_t playing[DJ_DECK_COUNT] = {};
	uint32_t sync[DJ_DECK_COUNT] = {};
};

struct DjEffectSnapshot {
	uint8_t type = 0;
	uint8_t intensity = 0;
};

struct DjEffectTransition {
	bool addSpeed = false;
	bool removeSpeed = false;
	bool setSpeed = false;
	bool clearEffect = false;
};

class DjEffectState {
public:
	bool setType(uint8_t deck, uint8_t slot, uint8_t type, bool deckLoaded, DjEffectTransition& transition){
		if(deck >= DJ_DECK_COUNT || slot >= DJ_EFFECT_SLOT_COUNT || type >= DJ_EFFECT_COUNT) return false;

		DjEffectSnapshot& effect = effects[deck][slot];
		if(type == DJ_EFFECT_SPEED){
			for(uint8_t other = 0; other < DJ_EFFECT_SLOT_COUNT; other++){
				if(other == slot || effects[deck][other].type != DJ_EFFECT_SPEED) continue;
				effects[deck][other] = {};
			}
			effect.type = type;
			effect.intensity = 127;
			transition.clearEffect = true;
			if(deckLoaded && !speedActive[deck]){
				speedActive[deck] = true;
				transition.addSpeed = true;
			}
			transition.setSpeed = speedActive[deck];
			return true;
		}

		if(effect.type == DJ_EFFECT_SPEED && speedActive[deck]){
			speedActive[deck] = false;
			transition.removeSpeed = true;
		}
		effect.type = type;
		effect.intensity = 0;
		return true;
	}

	bool setIntensity(uint8_t deck, uint8_t slot, uint8_t intensity, DjEffectTransition& transition){
		if(deck >= DJ_DECK_COUNT || slot >= DJ_EFFECT_SLOT_COUNT ||
		   effects[deck][slot].type == DJ_EFFECT_NONE) return false;
		effects[deck][slot].intensity = intensity;
		transition.setSpeed = effects[deck][slot].type == DJ_EFFECT_SPEED && speedActive[deck];
		return true;
	}

	void deckLoaded(uint8_t deck, DjEffectTransition& transition){
		if(deck >= DJ_DECK_COUNT || speedActive[deck]) return;
		for(uint8_t slot = 0; slot < DJ_EFFECT_SLOT_COUNT; slot++){
			if(effects[deck][slot].type != DJ_EFFECT_SPEED) continue;
			speedActive[deck] = true;
			transition.addSpeed = true;
			transition.setSpeed = true;
			return;
		}
	}

	const DjEffectSnapshot& get(uint8_t deck, uint8_t slot) const{
		return effects[deck][slot];
	}

	const DjEffectSnapshot* getDeck(uint8_t deck) const{
		return effects[deck];
	}

	void copyDeck(uint8_t deck, DjEffectSnapshot* destination) const{
		memcpy(destination, effects[deck], sizeof(effects[deck]));
	}

	bool isSpeedActive(uint8_t deck) const{
		return deck < DJ_DECK_COUNT && speedActive[deck];
	}

private:
	DjEffectSnapshot effects[DJ_DECK_COUNT][DJ_EFFECT_SLOT_COUNT] = {};
	bool speedActive[DJ_DECK_COUNT] = {};
};

struct DjGridSnapshot {
	bool valid = false;
	uint16_t confidence = 0;
	int64_t currentQuarterBeat = 0;
};

struct DjQuantizeSnapshot {
	DjQuantizeResolution resolution = DJ_QUANTIZE_OFF;
	bool pending = false;
	uint64_t pendingTargetFrame = 0;
};

struct DjSyncSnapshot {
	DjSyncState state = DJ_SYNC_OFF;
	int8_t masterDeck = -1; // -1 = auto (the other deck) or unset
	DjRate targetRate = DJ_RATE_NEUTRAL;
	DjCommandError lastError = DJ_COMMAND_ERROR_NONE;
};

struct DjCueSnapshot {
	bool occupied = false;
	uint16_t position = 0;
};

struct DjDeckSnapshot {
	bool loaded = false;
	bool playing = false;
	uint16_t elapsed = 0;
	uint16_t duration = 0;
	DjTimingQuality timingQuality = DJ_TIMING_UNAVAILABLE;
	uint8_t gain = 255;
	char path[DJ_PATH_CAPACITY] = {};
	DjEffectSnapshot effects[DJ_EFFECT_SLOT_COUNT] = {};
	DjCueSnapshot cues[DJ_CUE_COUNT] = {};
	DjTrackMetadataSnapshot metadata = {};
	DjGridSnapshot grid = {};
	DjQuantizeSnapshot quantize = {};
	DjLoopSnapshot loop = {};
	DjSyncSnapshot sync = {};
	// Only meaningful when metadata.state == DJ_METADATA_VALID (see
	// DjSession::publishSnapshot()); all-zero/unattached otherwise so an
	// unresolved deck never falsely identity-matches another.
	DjTrackIdentity identity = {};
};

// Authoritative recording lifecycle snapshot: accepted (STARTING/STOPPING)
// vs applied (ACTIVE/COMPLETE/FAILED) state, stable error mapping, validity,
// byte/duration counters, the finalized file path once available, and
// boot-time orphan recovery diagnostics.
struct DjRecordingSnapshot {
	DjRecordingState state = DJ_RECORDING_IDLE;
	DjRecordingError error = DJ_RECORDING_ERROR_NONE;
	bool valid = false;
	uint32_t bytes = 0;
	uint32_t durationMs = 0;
	char path[DJ_PATH_CAPACITY] = {};
	uint32_t orphansRepaired = 0;
	uint32_t orphansFailed = 0;
};

struct DjSnapshot {
	uint64_t seq = 0;
	uint64_t bootId = 0;
	uint32_t sessionId = 0;
	bool sessionActive = false;
	bool mixerRunning = false;
	uint8_t mix = 127;
	DjRecordingSnapshot recordingInfo;
	DjDeckSnapshot decks[DJ_DECK_COUNT] = {};
	uint8_t queueDepth = 0;
	uint32_t queueDrops = 0;
#if defined(JAYD_ENABLE_WIRELESS)
	uint32_t pairingGeneration = 0;
#endif
	DjCommandResult recentResults[DJ_RECENT_RESULT_COUNT] = {};
};

// Recording is considered "busy" for library-work purposes across the same
// STARTING/ACTIVE/STOPPING span that djRecordingStartBusy() gates for new
// start commands; COMPLETE/FAILED/IDLE do not block library work.
inline bool djAllowsLibraryWork(const DjSnapshot& snapshot){
	if(djRecordingStartBusy(true, snapshot.recordingInfo.state)) return false;
	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; ++deck){
		if(snapshot.decks[deck].playing) return false;
	}
	return true;
}

class DjDeckMetadataState {
public:
	bool commitIfLoaded(
		const DjTrackMetadataSnapshot& metadata,
		bool attached,
		bool loadSucceeded
	){
		if(!loadSucceeded) return false;
		snapshot_ = metadata;
		attached_ = attached && metadata.state == DJ_METADATA_VALID;
		return true;
	}

	void invalidate(DjMetadataState state, uint32_t generation){
		snapshot_ = {};
		snapshot_.state = state;
		snapshot_.libraryGeneration = generation;
		attached_ = false;
	}

	bool attached() const{
		return attached_;
	}

	const DjTrackMetadataSnapshot& snapshot() const{
		return snapshot_;
	}

private:
	DjTrackMetadataSnapshot snapshot_ = {};
	bool attached_ = false;
};

class DjCueState {
public:
	bool set(uint8_t deck, uint8_t cue, uint16_t position){
		if(deck >= DJ_DECK_COUNT || cue >= DJ_CUE_COUNT) return false;
		cues[deck][cue].occupied = true;
		cues[deck][cue].position = position;
		return true;
	}

	bool trigger(uint8_t deck, uint8_t cue, uint16_t& position) const{
		if(deck >= DJ_DECK_COUNT || cue >= DJ_CUE_COUNT || !cues[deck][cue].occupied) return false;
		position = cues[deck][cue].position;
		return true;
	}

	bool clear(uint8_t deck, uint8_t cue){
		if(deck >= DJ_DECK_COUNT || cue >= DJ_CUE_COUNT) return false;
		cues[deck][cue] = {};
		return true;
	}

	void clearDeck(uint8_t deck){
		if(deck >= DJ_DECK_COUNT) return;
		memset(cues[deck], 0, sizeof(cues[deck]));
	}

	void copyDeck(uint8_t deck, DjCueSnapshot* destination) const{
		if(deck >= DJ_DECK_COUNT || !destination) return;
		memcpy(destination, cues[deck], sizeof(cues[deck]));
	}

private:
	DjCueSnapshot cues[DJ_DECK_COUNT][DJ_CUE_COUNT] = {};
};

#if defined(JAYD_ENABLE_WIRELESS)
inline bool djCommandIdentityMatches(const DjCommand& command, uint64_t bootId, uint32_t sessionId){
	return command.origin != DJ_ORIGIN_HTTP ||
		(command.requestBootId == bootId && command.requestSessionId == sessionId);
}
#endif

// Outcome of attempting to admit a supersedable command against whatever
// currently occupies the queue's tail slot for the same target. NONE means
// there is nothing to supersede (caller should push fresh); REPLACED means
// the tail command was overwritten (its id is reported via supersededId).
// Origin priority (never letting a system command silently clobber a still-
// queued manual one) is handled by admitAssistCommand()'s queue-wide scan
// below, not here - supersede() itself is origin-agnostic last-write-wins
// for every supersedable type, matching the tail-only "rapid dial changes
// coalesce" behavior for all of them uniformly.
enum DjSupersedeOutcome : uint8_t {
	DJ_SUPERSEDE_NONE,
	DJ_SUPERSEDE_REPLACED
};

class DjCommandQueue {
public:
	bool push(const DjCommand& command){
		if(count == DJ_COMMAND_CAPACITY) return false;
		commands[tail] = command;
		tail = (tail + 1) % DJ_COMMAND_CAPACITY;
		count++;
		return true;
	}

	bool pop(DjCommand& command){
		if(count == 0) return false;
		command = commands[head];
		head = (head + 1) % DJ_COMMAND_CAPACITY;
		count--;
		return true;
	}

	// Last-write-wins tail replacement for any supersedable command type
	// targeting the same thing as the current tail entry (rapid dial
	// changes on the same target coalesce into one queued command).
	DjSupersedeOutcome supersede(const DjCommand& command, uint32_t& supersededId){
		if(count == 0 || !isSupersedable(command.type)) return DJ_SUPERSEDE_NONE;
		const uint8_t index = (tail + DJ_COMMAND_CAPACITY - 1) % DJ_COMMAND_CAPACITY;
		if(!sameTarget(commands[index], command)) return DJ_SUPERSEDE_NONE;
		supersededId = commands[index].id;
		commands[index] = command;
		return DJ_SUPERSEDE_REPLACED;
	}

	// True when at least one currently-queued (pending, not yet dequeued)
	// command anywhere in the ring - not only the tail slot supersede()
	// can reach - is non-system-origin and targets the same channel as
	// `command` (per sameTarget()). Used to reject an incoming SYSTEM
	// command outright rather than let it jump ahead of/replace a still-
	// queued manual one; see admitAssistCommand()'s queue-wide origin
	// policy.
	bool hasNonSystemPending(const DjCommand& command) const{
		for(uint8_t offset = 0; offset < count; offset++){
			const DjCommand& queued = commands[(head + offset) % DJ_COMMAND_CAPACITY];
			if(queued.origin != DJ_ORIGIN_SYSTEM && sameTarget(queued, command)) return true;
		}
		return false;
	}

	// Removes every currently-queued SYSTEM-origin command matching
	// sameTarget() against `incoming` (a just-admitted non-system command)
	// wherever it sits in the ring, not only the tail - so a user command
	// can never be followed later by a stale Assist write for the same
	// channel re-appearing from further back in the queue. Bounded
	// in-place stable compaction (read/write two-pointer scan over the
	// logical head-relative sequence, <= DJ_COMMAND_CAPACITY iterations,
	// no heap, no auxiliary array - writeOffset <= readOffset is an
	// invariant, so copying the read slot into the write slot via a local
	// temporary is always safe even when they alias). Removed command ids
	// are reported (bounded by maxRemoved) so the caller can route each
	// through the same superseded-outcome bookkeeping (durable tracked
	// slot + presentation ring) used elsewhere; the return value is the
	// exact number removed even if that exceeds maxRemoved.
	uint8_t removeSystemTargeting(const DjCommand& incoming, uint32_t* removedIds, uint8_t maxRemoved){
		uint8_t removed = 0;
		uint8_t writeOffset = 0;
		for(uint8_t readOffset = 0; readOffset < count; readOffset++){
			const DjCommand entry = commands[(head + readOffset) % DJ_COMMAND_CAPACITY];
			if(entry.origin == DJ_ORIGIN_SYSTEM && sameTarget(entry, incoming)){
				if(removedIds && removed < maxRemoved) removedIds[removed] = entry.id;
				removed++;
				continue;
			}
			commands[(head + writeOffset) % DJ_COMMAND_CAPACITY] = entry;
			writeOffset++;
		}
		count = writeOffset;
		tail = (head + writeOffset) % DJ_COMMAND_CAPACITY;
		return removed;
	}

	void clear(){
		head = 0;
		tail = 0;
		count = 0;
	}

	uint8_t depth() const{
		return count;
	}

	bool contains(DjCommandType type) const{
		for(uint8_t offset = 0; offset < count; offset++){
			if(commands[(head + offset) % DJ_COMMAND_CAPACITY].type == type) return true;
		}
		return false;
	}

private:
	static bool isSupersedable(DjCommandType type){
		return type == DJ_COMMAND_SET_GAIN ||
			   type == DJ_COMMAND_SET_MIX ||
			   type == DJ_COMMAND_SET_EFFECT_TYPE ||
			   type == DJ_COMMAND_SET_EFFECT_INTENSITY ||
			   type == DJ_COMMAND_SET_QUANTIZE ||
			   type == DJ_COMMAND_SET_SYNC ||
			   type == DJ_COMMAND_SET_CUE ||
			   type == DJ_COMMAND_CLEAR_CUE;
	}

	static bool sameTarget(const DjCommand& first, const DjCommand& second){
		if(first.type != second.type) return false;
		if(first.type == DJ_COMMAND_SET_MIX) return true;
		if(first.deck != second.deck) return false;
		if(first.type == DJ_COMMAND_SET_EFFECT_TYPE ||
		   first.type == DJ_COMMAND_SET_EFFECT_INTENSITY ||
		   first.type == DJ_COMMAND_SET_CUE ||
		   first.type == DJ_COMMAND_CLEAR_CUE){
			return first.slot == second.slot;
		}
		return true;
	}

	DjCommand commands[DJ_COMMAND_CAPACITY] = {};
	uint8_t head = 0;
	uint8_t tail = 0;
	uint8_t count = 0;
};

class DjCommandResults {
public:
	void record(const DjCommand& command, DjCommandStatus status, DjCommandError error){
#if defined(JAYD_ENABLE_WIRELESS)
		uint8_t selected = next;
		for(uint8_t offset = 0; offset < DJ_RECENT_RESULT_COUNT; offset++){
			const uint8_t candidate = (next + offset) % DJ_RECENT_RESULT_COUNT;
			if(results[candidate].id == 0 || results[candidate].status != DJ_COMMAND_ACCEPTED){
				selected = candidate;
				break;
			}
		}
		DjCommandResult& result = results[selected];
#else
		DjCommandResult& result = results[next];
#endif
		result.id = command.id;
		result.origin = command.origin;
		result.type = command.type;
		result.status = status;
		result.error = error;
#if defined(JAYD_ENABLE_WIRELESS)
		memcpy(result.clientId, command.clientId, sizeof(result.clientId));
		memcpy(result.clientCommandId, command.clientCommandId, sizeof(result.clientCommandId));
		result.sequence = ++nextSequence;
		if(result.sequence == 0) result.sequence = ++nextSequence;
#endif
#if defined(JAYD_ENABLE_WIRELESS)
		next = (selected + 1) % DJ_RECENT_RESULT_COUNT;
#else
		next = (next + 1) % DJ_RECENT_RESULT_COUNT;
#endif
	}

	void finish(uint32_t id, DjCommandStatus status, DjCommandError error){
		for(auto& result : results){
			if(result.id != id) continue;
			result.status = status;
			result.error = error;
			return;
		}
	}

	// Overload for commands with scheduling diagnostics (quantized play-start,
	// loop engage/reloop). Leaves id/origin/type untouched; only updates the
	// status/error/diagnostics fields, mirroring finish() above.
	void finishWithDiagnostics(uint32_t id, DjCommandStatus status, DjCommandError error,
							   uint64_t targetFrame, int32_t lateFrames, bool missed){
		for(auto& result : results){
			if(result.id != id) continue;
			result.status = status;
			result.error = error;
			result.targetFrame = targetFrame;
			result.lateFrames = lateFrames;
			result.missed = missed;
			return;
		}
	}

	void copyTo(DjCommandResult* destination) const{
#if defined(JAYD_ENABLE_WIRELESS)
		bool copied[DJ_RECENT_RESULT_COUNT] = {};
		for(uint8_t output = 0; output < DJ_RECENT_RESULT_COUNT; output++){
			bool found = false;
			uint8_t selected = 0;
			for(uint8_t candidate = 0; candidate < DJ_RECENT_RESULT_COUNT; candidate++){
				if(copied[candidate] || results[candidate].sequence == 0) continue;
				if(!found || results[candidate].sequence > results[selected].sequence){
					selected = candidate;
					found = true;
				}
			}
			destination[output] = found ? results[selected] : DjCommandResult{};
			if(found) copied[selected] = true;
		}
#else
		for(uint8_t i = 0; i < DJ_RECENT_RESULT_COUNT; i++){
			const uint8_t index = (next + DJ_RECENT_RESULT_COUNT - 1 - i) % DJ_RECENT_RESULT_COUNT;
			destination[i] = results[index];
		}
#endif
	}

#if defined(JAYD_ENABLE_WIRELESS)
	bool findClientCommand(const char* clientId, const char* clientCommandId, DjCommandResult& destination) const{
		if(!clientId || clientId[0] == '\0' || !clientCommandId || clientCommandId[0] == '\0') return false;
		for(uint8_t i = 0; i < DJ_RECENT_RESULT_COUNT; i++){
			const DjCommandResult& result = results[i];
			if(strcmp(result.clientId, clientId) != 0) continue;
			if(strcmp(result.clientCommandId, clientCommandId) != 0) continue;
			destination = result;
			return true;
		}
		return false;
	}
#endif

private:
	DjCommandResult results[DJ_RECENT_RESULT_COUNT] = {};
	uint8_t next = 0;
#if defined(JAYD_ENABLE_WIRELESS)
	uint64_t nextSequence = 0;
#endif
};

// True for the three control channels Assist's guard/rollback logic tracks
// origin/intent generations for (see DjAssistIntentGenerations above).
// Every other command type (load, gain, effects, cues, quantize, recording,
// ...) is unaffected by the origin-priority policy below.
inline bool djIsIntentTrackedCommand(const DjCommand& command){
	return command.type == DJ_COMMAND_SET_MIX ||
		command.type == DJ_COMMAND_SET_PLAYING ||
		command.type == DJ_COMMAND_SET_SYNC;
}

inline void djBumpIntentGeneration(DjAssistIntentGenerations& generations, const DjCommand& command){
	if(command.origin == DJ_ORIGIN_SYSTEM) return; // Assist's own steps never move the arm-time baseline.
	if(command.type == DJ_COMMAND_SET_MIX){
		generations.mix++;
	}else if(command.deck < DJ_DECK_COUNT){
		if(command.type == DJ_COMMAND_SET_PLAYING) generations.playing[command.deck]++;
		else if(command.type == DJ_COMMAND_SET_SYNC) generations.sync[command.deck]++;
	}
}

// Single choke point for admitting an already-validated, already-ID-assigned
// command into the queue, used by DjSession::submit() for every command
// (including the setMix()/setPlaying()/etc. wrappers). Kept as a free
// function operating only on these primitive, host-testable types so the
// exact admission/bookkeeping semantics can be exercised directly by host
// tests without any Arduino/CircuitOS dependencies - see
// tests/dj_session_self_test.cpp. Related fixes live here:
//
//  - a command superseded/removed before it ever reaches the front of the
//    queue must mark BOTH the bounded/evictable presentation ring
//    (results) and, if it is the one the Coach transition/rollback state
//    machine is durably watching (tracked), the never-evictable tracked
//    slot too - otherwise that slot could report ACCEPTED forever for a
//    command that will never run.
//  - queue-wide origin priority for the three intent-tracked channels
//    (mix/play/sync): an incoming SYSTEM command (Assist's own step) is
//    rejected outright if ANY non-system command for that channel is
//    still pending anywhere in the queue (djCommandQueue::
//    hasNonSystemPending() - not only the tail slot supersede() can
//    reach), so a queued user intent is never silently skipped over. An
//    incoming non-system command instead purges every pending SYSTEM-
//    origin command for that channel wherever queued
//    (removeSystemTargeting()), so a cancelled/rolled-back plan can never
//    be followed by a stale Assist write re-appearing after the user's
//    own command.
//  - the corresponding DjAssistIntentGenerations counter is bumped the
//    instant a non-system command is itself admitted (supersede-replace
//    or fresh push), not when it later applies, so anything gated on "the
//    user touched this control" (guard snapshots, Assist's own arm-time
//    baseline) sees it immediately rather than racing apply().
inline DjSubmitResult admitAssistCommand(
	const DjCommand& command,
	DjCommandQueue& queue,
	DjCommandResults& results,
	DjAssistTrackedCommand& tracked,
	DjAssistIntentGenerations& generations
){
	if(djIsIntentTrackedCommand(command)){
		if(command.origin == DJ_ORIGIN_SYSTEM && queue.hasNonSystemPending(command)){
			results.record(command, DJ_COMMAND_REJECTED, DJ_COMMAND_ERROR_ASSIST_OVERRIDE_PENDING);
			return DjSubmitResult(command.id, DJ_COMMAND_REJECTED, DJ_COMMAND_ERROR_ASSIST_OVERRIDE_PENDING);
		}
		if(command.origin != DJ_ORIGIN_SYSTEM){
			uint32_t removedIds[DJ_COMMAND_CAPACITY] = {};
			const uint8_t removedCount = queue.removeSystemTargeting(command, removedIds, DJ_COMMAND_CAPACITY);
			for(uint8_t i = 0; i < removedCount && i < DJ_COMMAND_CAPACITY; i++){
				results.finish(removedIds[i], DJ_COMMAND_SUPERSEDED, DJ_COMMAND_ERROR_NONE);
				if(tracked.tracked && tracked.id == removedIds[i]) tracked.status = DJ_COMMAND_SUPERSEDED;
			}
		}
	}

	uint32_t supersededId = 0;
	const DjSupersedeOutcome outcome = queue.supersede(command, supersededId);
	if(outcome == DJ_SUPERSEDE_REPLACED){
		results.finish(supersededId, DJ_COMMAND_SUPERSEDED, DJ_COMMAND_ERROR_NONE);
		if(tracked.tracked && tracked.id == supersededId) tracked.status = DJ_COMMAND_SUPERSEDED;
		results.record(command, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
		djBumpIntentGeneration(generations, command);
		return DjSubmitResult(command.id, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	}
	if(!queue.push(command)){
		results.record(command, DJ_COMMAND_REJECTED, DJ_COMMAND_ERROR_QUEUE_FULL);
		return DjSubmitResult(command.id, DJ_COMMAND_REJECTED, DJ_COMMAND_ERROR_QUEUE_FULL);
	}
	results.record(command, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	djBumpIntentGeneration(generations, command);
	return DjSubmitResult(command.id, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
}

class DjSnapshotBuffers {
public:
	void publish(const DjSnapshot& snapshot){
		const uint8_t next = active == 0 ? 1 : 0;
		snapshots[next] = snapshot;
		active = next;
	}

	void copy(DjSnapshot& snapshot) const{
		snapshot = snapshots[active];
	}

private:
	DjSnapshot snapshots[2] = {};
	uint8_t active = 0;
};

#endif
