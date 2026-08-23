#ifndef JAYD_FIRMWARE_DJASSISTSESSIONBRIDGE_H
#define JAYD_FIRMWARE_DJASSISTSESSIONBRIDGE_H

#include "DjAssistTypes.h"

// Pure, bounded, host-testable glue between DjSession's authoritative state
// and the Coach/transition engine's POD inputs. Deliberately free of
// FS/Arduino dependencies (no JaydMetadata.h, no MixSystem.h) so the exact
// arithmetic used by the real DjAssistController/DjAssistSessionActuator can
// be exercised by a host self-check, matching DjAssistScoring's bar. Callers
// on the ESP32 side pass already-resolved scalars (from Track/snapshot
// fields); this module never touches storage or hardware.
namespace DjAssistBridge {

// Builds a DjTrackIdentity from raw fingerprint/sourceId bytes as read from
// JaydMetadata::Track. Each evidence flag is gated on its array being
// non-all-zero, so an unpopulated field is treated as "no evidence" rather
// than a spurious 16-zero-byte value that could falsely match another
// unresolved identity. Used both for building candidate-table entries and
// for DjSession::publishSnapshot()'s per-deck loaded identity.
DjTrackIdentity buildTrackIdentity(const uint8_t fingerprint[16], const uint8_t sourceId[16]);

// Builds one candidate-table entry's capability bitmask from cheap,
// already-resolved integer fields (mirrors DjSession::resolveMetadata()'s
// capability formula, minus the per-grid/per-phrase downbeat-count/
// confidence enumeration - deliberately out of scope for the bulk candidate
// table; see DjAssistController for the disclosed tradeoff). Missing fields
// simply leave the corresponding bit unset (reduces scoring confidence
// rather than rejecting the track).
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
);

// Deterministic crossfade mix value in [0,255] for a fromDeck->toDeck
// transition, `elapsedMicros` into a `crossfadeBeats`-long ramp at
// `bpmMilli`. Checked against bpmMilli==0/crossfadeBeats==0 (falls back to
// the immediate target endpoint rather than dividing by zero); saturates at
// the endpoint once elapsed time reaches/exceeds the ramp duration. Mirrors
// Mixer::setMixRatio semantics: 0 == full deck0, 255 == full deck1.
uint8_t computeCrossfadeMix(
	uint8_t toDeck,
	uint64_t elapsedMicros,
	uint8_t crossfadeBeats,
	uint32_t bpmMilli
);

// Rollback phases for a failed/cancelled one-shot transition, owned by the
// controller (not the pure DjAssistEngine) - see
// DjAssistController::tickRollback(). Ordered mix -> sync -> stop-deck,
// matching the review's required undo order.
enum DjAssistRollbackPhase : uint8_t {
	DJ_ASSIST_ROLLBACK_IDLE,
	DJ_ASSIST_ROLLBACK_MIX,
	DJ_ASSIST_ROLLBACK_SYNC,
	DJ_ASSIST_ROLLBACK_STOP_DECK,
	DJ_ASSIST_ROLLBACK_DONE
};

// Result of reconcileOwnership() below: which of the plan's two toDeck
// applied-mutation ownership flags (START_DECK, LOCK_TEMPO/ENABLE_SYNC) this
// plan is STILL the sole author of, after comparing live vs. armed intent
// generations for BOTH properties independently.
struct DjAssistOwnershipReconciliation {
	bool toDeckStartOwnedByPlan;
	bool toDeckSyncOwnedByPlan;
};

// Independently re-derives toDeck play/sync rollback ownership from live-
// vs-armed intent generations, for BOTH properties in one pass - never as a
// chain of early-return checks that could relinquish only the FIRST
// divergent property and leave a second, simultaneously-diverged property's
// ownership flag stale (e.g. a user command that touches both play and sync
// on the target deck in one action). A property already relinquished (or
// never owned) stays relinquished; ownership is only ever taken away here,
// never granted (granting happens solely at the instant DjAssistEngine::
// tick() observes the corresponding step's command reach
// DJ_COMMAND_APPLIED). Called from BOTH DjAssistEngine::guardOk() (every
// tick while ARMED/RUNNING) and DjAssistController::tickRollback() (every
// tick while FAILED, via DjAssistEngine::reconcileOwnershipOnDivergence() -
// guardOk() no longer runs once the transition has already failed/been
// cancelled, so without this second call site a user re-asserting play/
// sync AFTER that point, but before rollback finishes, would never be
// caught and rollback could stop/release the user's own newer intent).
DjAssistOwnershipReconciliation reconcileOwnership(
	bool priorStartOwnedByPlan,
	bool priorSyncOwnedByPlan,
	uint32_t livePlayGeneration,
	uint32_t armedPlayGeneration,
	uint32_t liveSyncGeneration,
	uint32_t armedSyncGeneration
);

// Given the current rollback phase, whether a manual (non-system) mix
// change has occurred since arm, and whether the sync/start-deck steps were
// both submitted AND actually plan-owned (i.e. the engine observed the
// corresponding command reach DJ_COMMAND_APPLIED while armed - see
// DjAssistTransitionPlan::toDeckStartOwnedByPlan/toDeckSyncOwnedByPlan),
// returns the next phase to attempt - skipping any phase with nothing to
// undo, or that would clobber state the plan never introduced. Pure and
// deterministic; the caller does the actual actuator submit/poll for
// whichever phase this returns and re-normalizes after each completed
// phase.
//
// The MIX phase is unconditionally skipped once a manual mix change has
// occurred: restoring armedMix over a user's own subsequent action would
// silently overwrite it, which the caller must never do. The SYNC and
// STOP_DECK phases are skipped whenever the corresponding step was never
// submitted, OR the target deck was already in that state at arm time (an
// idempotent no-op the plan did not actually introduce, so rollback must
// not undo pre-existing user state).
DjAssistRollbackPhase nextRollbackPhase(
	DjAssistRollbackPhase phase,
	bool crossfadeSubmitted,
	bool manualMixOccurred,
	bool syncOwnedByPlan,
	bool startDeckOwnedByPlan
);

// Outcome of polling a single in-flight command's status - used by both the
// crossfade actuator's final-mix confirmation and the rollback state
// machine so the same status->outcome mapping isn't duplicated.
// DJ_COMMAND_APPLIED -> DONE, REJECTED/FAILED -> FAILED, SUPERSEDED ->
// RESUBMIT (bounded retry - e.g. a rollback mix command overtaken by
// unrelated traffic), anything else (ACCEPTED, or not yet observed) ->
// WAIT.
enum DjAssistCommandOutcome : uint8_t {
	DJ_ASSIST_COMMAND_WAIT,
	DJ_ASSIST_COMMAND_RESUBMIT,
	DJ_ASSIST_COMMAND_DONE,
	DJ_ASSIST_COMMAND_FAILED
};
DjAssistCommandOutcome evaluateCommandOutcome(DjCommandStatus status);

// Result of comparing a live position against a captured WAIT_BOUNDARY
// target using the established quantize tolerance
// (DJ_QUANTIZE_TOLERANCE_FRAMES, DjBeatEngine.h) - the same "one audio
// output block" window already used for quantize scheduling elsewhere, so a
// boundary is never treated as reached arbitrarily late.
enum DjAssistBoundaryArrival : uint8_t {
	DJ_ASSIST_BOUNDARY_NOT_YET,
	DJ_ASSIST_BOUNDARY_REACHED,
	DJ_ASSIST_BOUNDARY_MISSED
};

// Pure arrival decision for a captured WAIT_BOUNDARY target: NOT_YET while
// still approaching, REACHED once at or within tolerance past the target,
// or MISSED once more than tolerance has elapsed past it - in which case
// the caller must re-capture the next boundary ahead of currentFrame rather
// than ever releasing on a stale, long-passed target.
DjAssistBoundaryArrival evaluateBoundaryArrival(uint64_t currentFrame, uint64_t targetFrame);

// Cached result of a (possibly SD-backed) phrase lookup for one deck, used
// to throttle DjAssistController::resolveBoundary() so it does not rescan
// up to the metadata reader's whole phrase table every tick. `terminal`
// records a confirmed "no future phrase exists for this identity/position"
// result so that case is cached too, not just a found boundary - without
// this, a track nearing its end (no phrase left ahead) would trigger a
// full rescan every single tick. metadataGeneration/metadataState mirror
// DjTrackMetadataSnapshot's own revision fields so a metadata refresh that
// leaves the same track *identity* loaded (e.g. re-resolved grid/phrase
// data, or a transient VALID->PENDING->VALID cycle) still invalidates a
// cached result instead of silently reusing phrase data computed against
// the prior metadata state.
struct DjAssistPhraseCacheState {
	uint8_t deck = 0xFF; // 0xFF = unset/never cached
	bool valid = false;
	bool terminal = false;
	uint64_t frame = 0;       // cached phrase frame, only meaningful if valid && !terminal
	uint64_t lastFrame = 0;   // playback position at last (re)cache, to detect backward seeks
	DjTrackIdentity identity = {};
	uint32_t metadataGeneration = 0;
	DjMetadataState metadataState = DJ_METADATA_ABSENT;
};

// True when the cache must be refreshed via a real lookup rather than
// reused: no cache yet for this deck, the loaded identity changed, the
// metadata generation/state for this deck's slot changed (a refresh
// re-resolved the same identity's metadata), playback seeked backward
// (lastFrame > currentFrame), or a previously *found* cached boundary has
// now been passed (currentFrame >= cached frame). A cached *terminal* ("no
// future phrase") result is deliberately NOT rescanned just because it is
// still terminal, and is not invalidated by forward playback alone - only
// by an identity/metadata change or a backward seek. The caller performs
// the actual lookup and calls updatePhraseCache() with the result whenever
// this returns true.
bool phraseCacheNeedsRescan(
	const DjAssistPhraseCacheState& cache,
	uint8_t deck,
	uint64_t currentFrame,
	const DjTrackIdentity& identity,
	uint32_t metadataGeneration,
	DjMetadataState metadataState
);

// Records the outcome of a (possibly skipped) phrase lookup into the cache.
// Pure/no I/O - the caller already performed the real lookup, if any.
void updatePhraseCache(
	DjAssistPhraseCacheState& cache,
	uint8_t deck,
	uint64_t currentFrame,
	const DjTrackIdentity& identity,
	uint32_t metadataGeneration,
	DjMetadataState metadataState,
	bool found,
	uint64_t phraseFrame
);

// True when a background-filled candidate-table entry (or a fill pass'
// completion) may be safely committed. `loadedGeneration` is the library
// generation this fill pass is currently loading; `observedGeneration` is
// either (a) the exact metadata revision an entry read was performed under -
// captured atomically with the read itself, inside the same reader-lifetime
// lock, by DjSession::assistTrackEntry() - or (b) a fresh immediate re-read
// of the live generation taken right before the completion commit. Either
// way this is a single equality check against a value that cannot have
// drifted from what was actually read/observed, closing the previous
// check-then-lock gap where two independent generation probes (one before,
// one after an unlocked read) could still both match a stale value while a
// refresh completed unnoticed in between.
bool candidateGenerationCurrent(uint32_t loadedGeneration, uint32_t observedGeneration);

} // namespace DjAssistBridge

#endif
