#include <assert.h>
#include <string.h>

#include "../src/DjAssist/DjAssistSessionBridge.h"

using namespace DjAssistBridge;

namespace {

DjTrackIdentity fingerprintIdentity(uint8_t seed){
	DjTrackIdentity identity = {};
	identity.flags = DJ_TRACK_IDENTITY_FINGERPRINT;
	memset(identity.fingerprint, seed, sizeof(identity.fingerprint));
	return identity;
}

// -- buildTrackIdentity: zero-array evidence gating -----------------------

void testBuildTrackIdentityGatesZeroEvidence(){
	uint8_t zero[16] = {};
	uint8_t fp[16];
	memset(fp, 0xAB, sizeof(fp));
	uint8_t src[16];
	memset(src, 0xCD, sizeof(src));

	// Both zero: no evidence at all.
	const DjTrackIdentity none = buildTrackIdentity(zero, zero);
	assert(none.flags == 0);

	// Only fingerprint real.
	const DjTrackIdentity fpOnly = buildTrackIdentity(fp, zero);
	assert(fpOnly.flags == DJ_TRACK_IDENTITY_FINGERPRINT);
	assert(memcmp(fpOnly.fingerprint, fp, 16) == 0);

	// Only sourceId real.
	const DjTrackIdentity srcOnly = buildTrackIdentity(zero, src);
	assert(srcOnly.flags == DJ_TRACK_IDENTITY_SOURCE);
	assert(memcmp(srcOnly.sourceId, src, 16) == 0);

	// Both real.
	const DjTrackIdentity both = buildTrackIdentity(fp, src);
	assert(both.flags == (DJ_TRACK_IDENTITY_FINGERPRINT | DJ_TRACK_IDENTITY_SOURCE));
	assert(memcmp(both.fingerprint, fp, 16) == 0);
	assert(memcmp(both.sourceId, src, 16) == 0);

	// Two independently-built all-zero identities must never claim to
	// match via DjAssistScoring::identityMatches (no comparable evidence).
	const DjTrackIdentity otherNone = buildTrackIdentity(zero, zero);
	assert(none.flags == otherNone.flags && none.flags == 0);
}

// -- buildLibraryEntry: capability bits mirror DjSession::resolveMetadata()
// -- exactly (minus the deliberately-out-of-scope downbeat enumeration).

void testBuildLibraryEntryFullCapabilities(){
	const DjTrackIdentity identity = fingerprintIdentity(7);
	const DjAssistLibraryEntry entry = buildLibraryEntry(
		42, identity, DJ_METADATA_VALID,
		44100, 44100ULL * 180ULL, 128000, 0x105, 4,
		/*cueCount*/ 3, /*gridCount*/ 64, /*phraseCount*/ 8
	);
	assert(entry.libraryIndex == 42);
	assert(entry.state == DJ_METADATA_VALID);
	assert(entry.bpmMilli == 128000);
	assert(entry.key == 0x105);
	assert(entry.rating == 4);
	assert(entry.durationFrames == 44100ULL * 180ULL);
	assert(entry.sampleRate == 44100);
	assert(memcmp(entry.identity.fingerprint, identity.fingerprint, 16) == 0);
	const uint16_t expected = DJ_METADATA_HAS_SOURCE_FRAMES | DJ_METADATA_HAS_BPM | DJ_METADATA_HAS_KEY |
		DJ_METADATA_HAS_RATING | DJ_METADATA_HAS_CUES | DJ_METADATA_HAS_GRID | DJ_METADATA_HAS_PHRASES;
	assert(entry.capabilities == expected);
	// Deliberately never set at the bulk-candidate-table level (see header
	// comment + DjAssistController): scoring never reads this bit.
	assert(!(entry.capabilities & DJ_METADATA_HAS_DOWNBEATS));
}

void testBuildLibraryEntryMissingFieldsClearBitsOnly(){
	// Every field absent/zero/sentinel: every bit clears, but the call still
	// returns a usable (lower-confidence, never rejected) entry - no field
	// individually forces exclusion.
	const DjAssistLibraryEntry entry = buildLibraryEntry(
		0, DjTrackIdentity{}, DJ_METADATA_STALE,
		0, 0, 0, 0, 255,
		0, 0, 0
	);
	assert(entry.capabilities == 0);
	assert(entry.state == DJ_METADATA_STALE);
	assert(entry.rating == 255);

	// Rating sentinel boundary: 255 must never set HAS_RATING; any real
	// 0-5 rating (including 0, a legitimate unrated-low value) must.
	const DjAssistLibraryEntry rated = buildLibraryEntry(
		0, DjTrackIdentity{}, DJ_METADATA_VALID,
		44100, 1000, 120000, 5, 0,
		0, 0, 0
	);
	assert(rated.capabilities & DJ_METADATA_HAS_RATING);

	// sampleRate/durationFrames must BOTH be present for HAS_SOURCE_FRAMES,
	// matching resolveMetadata()'s `track.sampleRate && track.durationFrames`.
	const DjAssistLibraryEntry halfSource = buildLibraryEntry(
		0, DjTrackIdentity{}, DJ_METADATA_VALID,
		44100, 0, 0, 0, 255,
		0, 0, 0
	);
	assert(!(halfSource.capabilities & DJ_METADATA_HAS_SOURCE_FRAMES));
}

// -- computeCrossfadeMix: endpoints, direction, checked math --------------

void testCrossfadeMixEndpoints(){
	// toDeck==1: mix ramps 0 (start) -> 255 (complete).
	assert(computeCrossfadeMix(1, 0, 16, 128000) == 0);
	// toDeck==0: mix ramps 255 (start) -> 0 (complete) - the mirror image.
	assert(computeCrossfadeMix(0, 0, 16, 128000) == 255);

	// microsPerBeat at 120.000 bpm == 500,000us. 16 beats == 8,000,000us.
	// Well past the end: saturate exactly at the target endpoint.
	assert(computeCrossfadeMix(1, 100000000ULL, 16, 120000) == 255);
	assert(computeCrossfadeMix(0, 100000000ULL, 16, 120000) == 0);

	// Exactly halfway (8 of 16 beats): curve(8,16) == 127 (255*8/16).
	const uint64_t halfwayMicros = 4000000ULL; // 8 beats * 500,000us
	assert(computeCrossfadeMix(1, halfwayMicros, 16, 120000) == 127);
	assert(computeCrossfadeMix(0, halfwayMicros, 16, 120000) == 255 - 127);
}

void testCrossfadeMixCheckedDivideByZero(){
	// bpmMilli == 0: no valid tempo, snap immediately to the target rather
	// than dividing by zero.
	assert(computeCrossfadeMix(1, 0, 16, 0) == 255);
	assert(computeCrossfadeMix(0, 0, 16, 0) == 0);
	// crossfadeBeats == 0: same fallback (an instant cut is a degenerate
	// but valid "0-beat crossfade").
	assert(computeCrossfadeMix(1, 12345, 0, 128000) == 255);
	assert(computeCrossfadeMix(0, 12345, 0, 128000) == 0);
}

void testCrossfadeMixOverflowGuard(){
	// Absurdly large elapsed time and a tiny bpm: elapsedBeats would
	// overflow uint16_t internally; must clamp/saturate, never wrap back
	// around to a small stepIndex.
	assert(computeCrossfadeMix(1, 0xFFFFFFFFFFFFFFFFULL, 32, 1) == 255);
	assert(computeCrossfadeMix(0, 0xFFFFFFFFFFFFFFFFULL, 32, 1) == 0);
}

// -- nextRollbackPhase: skips phases with nothing to undo/nothing plan-
// -- owned, deterministic --------------------------------------------------

void testNextRollbackPhaseFullSequence(){
	// Every side effect was submitted and plan-owned: mix -> sync ->
	// stop-deck -> done, never skipping a phase.
	DjAssistRollbackPhase phase = DJ_ASSIST_ROLLBACK_IDLE;
	phase = nextRollbackPhase(phase, /*crossfade*/true, /*manualMixOccurred*/false,
		/*syncOwned*/true, /*startDeckOwned*/true);
	assert(phase == DJ_ASSIST_ROLLBACK_MIX);
	phase = nextRollbackPhase(DJ_ASSIST_ROLLBACK_SYNC, true, false, true, true);
	assert(phase == DJ_ASSIST_ROLLBACK_SYNC);
	phase = nextRollbackPhase(DJ_ASSIST_ROLLBACK_STOP_DECK, true, false, true, true);
	assert(phase == DJ_ASSIST_ROLLBACK_STOP_DECK);
	phase = nextRollbackPhase(DJ_ASSIST_ROLLBACK_DONE, true, false, true, true);
	assert(phase == DJ_ASSIST_ROLLBACK_DONE);
}

void testNextRollbackPhaseSkipsUnsubmittedSteps(){
	// Nothing was ever submitted (e.g. failed during WAIT_BOUNDARY, before
	// any actuator action): every phase must be skipped straight to DONE -
	// there is nothing to undo.
	assert(nextRollbackPhase(DJ_ASSIST_ROLLBACK_IDLE, false, false, false, false) == DJ_ASSIST_ROLLBACK_DONE);

	// Only the start-deck step was submitted and plan-owned (failed right
	// after START_DECK, before LOCK_TEMPO/ENABLE_SYNC or CROSSFADE ever
	// ran): mix and sync phases must both be skipped, landing directly on
	// STOP_DECK.
	assert(nextRollbackPhase(DJ_ASSIST_ROLLBACK_IDLE, false, false, false, true) == DJ_ASSIST_ROLLBACK_STOP_DECK);

	// Mix (crossfade) was submitted but sync/start-deck were not (e.g. a
	// tempoLock=false, startAtBoundary=false plan that failed mid-ramp):
	// only the MIX phase runs, then falls straight to DONE.
	assert(nextRollbackPhase(DJ_ASSIST_ROLLBACK_IDLE, true, false, false, false) == DJ_ASSIST_ROLLBACK_MIX);
	assert(nextRollbackPhase(DJ_ASSIST_ROLLBACK_SYNC, true, false, false, false) == DJ_ASSIST_ROLLBACK_DONE);
}

void testNextRollbackPhaseSkipsOwnershipWhenAlreadyInThatState(){
	// The sync/start-deck steps were both submitted (buildSteps() always
	// includes them), but the target deck was ALREADY playing/synced
	// before the transition armed - those steps were idempotent no-ops the
	// plan did not actually introduce, so rollback must skip straight from
	// MIX to DONE without touching sync/playback state a user set up
	// beforehand.
	DjAssistRollbackPhase phase = nextRollbackPhase(
		DJ_ASSIST_ROLLBACK_IDLE, /*crossfade*/true, /*manualMixOccurred*/false,
		/*syncOwned*/false, /*startDeckOwned*/false
	);
	assert(phase == DJ_ASSIST_ROLLBACK_MIX);
	phase = nextRollbackPhase(DJ_ASSIST_ROLLBACK_SYNC, true, false, false, false);
	assert(phase == DJ_ASSIST_ROLLBACK_DONE);
}

void testNextRollbackPhaseSkipsMixOnManualOverride(){
	// A manual (non-system) mix change happened at any point after arming:
	// the MIX phase must be skipped entirely, even though crossfade WAS
	// submitted by the plan - restoring armedMix would silently overwrite
	// the user's own action.
	DjAssistRollbackPhase phase = nextRollbackPhase(
		DJ_ASSIST_ROLLBACK_IDLE, /*crossfade*/true, /*manualMixOccurred*/true,
		/*syncOwned*/true, /*startDeckOwned*/true
	);
	assert(phase == DJ_ASSIST_ROLLBACK_SYNC);
}

// -- evaluateCommandOutcome: exhaustive status -> outcome mapping ---------

void testEvaluateCommandOutcomeMapping(){
	assert(evaluateCommandOutcome(DJ_COMMAND_APPLIED) == DJ_ASSIST_COMMAND_DONE);
	assert(evaluateCommandOutcome(DJ_COMMAND_REJECTED) == DJ_ASSIST_COMMAND_FAILED);
	assert(evaluateCommandOutcome(DJ_COMMAND_FAILED) == DJ_ASSIST_COMMAND_FAILED);
	assert(evaluateCommandOutcome(DJ_COMMAND_SUPERSEDED) == DJ_ASSIST_COMMAND_RESUBMIT);
	assert(evaluateCommandOutcome(DJ_COMMAND_ACCEPTED) == DJ_ASSIST_COMMAND_WAIT);
	assert(evaluateCommandOutcome(DJ_COMMAND_PENDING) == DJ_ASSIST_COMMAND_WAIT);
}

// -- evaluateBoundaryArrival: tolerance window, never released late ------

void testEvaluateBoundaryArrivalNotYet(){
	assert(evaluateBoundaryArrival(0, 1000) == DJ_ASSIST_BOUNDARY_NOT_YET);
	assert(evaluateBoundaryArrival(999, 1000) == DJ_ASSIST_BOUNDARY_NOT_YET);
}

void testEvaluateBoundaryArrivalReachedWithinTolerance(){
	// Exactly on target, and up to DJ_QUANTIZE_TOLERANCE_FRAMES past it,
	// both count as reached.
	assert(evaluateBoundaryArrival(1000, 1000) == DJ_ASSIST_BOUNDARY_REACHED);
	assert(evaluateBoundaryArrival(1000 + DJ_QUANTIZE_TOLERANCE_FRAMES, 1000) == DJ_ASSIST_BOUNDARY_REACHED);
}

void testEvaluateBoundaryArrivalMissedBeyondTolerance(){
	assert(evaluateBoundaryArrival(1000 + DJ_QUANTIZE_TOLERANCE_FRAMES + 1, 1000) == DJ_ASSIST_BOUNDARY_MISSED);
	assert(evaluateBoundaryArrival(1000000, 1000) == DJ_ASSIST_BOUNDARY_MISSED);
}

// -- phraseCacheNeedsRescan / updatePhraseCache: terminal-aware throttle -

// Fixed metadata generation/state used by every call below except the
// dedicated metadata-invalidation test - keeps the existing scenarios
// focused on identity/frame behavior without threading a real generation
// counter through them.
static const uint32_t kGen = 7;
static const DjMetadataState kMetaState = DJ_METADATA_VALID;

void testPhraseCacheNeedsRescanWhenEmpty(){
	DjAssistPhraseCacheState cache;
	const DjTrackIdentity identity = fingerprintIdentity(1);
	assert(phraseCacheNeedsRescan(cache, 0, 0, identity, kGen, kMetaState));
}

void testPhraseCacheDoesNotRescanBeforeFoundBoundaryPassed(){
	DjAssistPhraseCacheState cache;
	const DjTrackIdentity identity = fingerprintIdentity(1);
	updatePhraseCache(cache, /*deck*/0, /*currentFrame*/100, identity, kGen, kMetaState, /*found*/true, /*phraseFrame*/5000);
	assert(!phraseCacheNeedsRescan(cache, 0, 200, identity, kGen, kMetaState)); // still well before 5000.
	assert(!phraseCacheNeedsRescan(cache, 0, 4999, identity, kGen, kMetaState));
	assert(phraseCacheNeedsRescan(cache, 0, 5000, identity, kGen, kMetaState)); // reached/passed - refresh.
}

void testPhraseCacheDoesNotRescanTerminalJustBecauseStillTerminal(){
	// No future phrase exists (terminal). Forward playback alone must
	// NOT force a rescan - this is exactly the up-to-128-record-rescan-
	// every-tick bug the cache exists to prevent.
	DjAssistPhraseCacheState cache;
	const DjTrackIdentity identity = fingerprintIdentity(1);
	updatePhraseCache(cache, 0, 1000, identity, kGen, kMetaState, /*found*/false, 0);
	assert(!phraseCacheNeedsRescan(cache, 0, 1001, identity, kGen, kMetaState));
	assert(!phraseCacheNeedsRescan(cache, 0, 50000, identity, kGen, kMetaState));
}

void testPhraseCacheRescansOnIdentityChangeOrDeckChange(){
	DjAssistPhraseCacheState cache;
	const DjTrackIdentity a = fingerprintIdentity(1);
	const DjTrackIdentity b = fingerprintIdentity(2);
	updatePhraseCache(cache, 0, 1000, a, kGen, kMetaState, true, 5000);
	assert(phraseCacheNeedsRescan(cache, 0, 1500, b, kGen, kMetaState)); // new track loaded on same deck.
	assert(phraseCacheNeedsRescan(cache, 1, 1500, a, kGen, kMetaState)); // different deck entirely.
}

void testPhraseCacheRescansOnBackwardSeek(){
	DjAssistPhraseCacheState cache;
	const DjTrackIdentity identity = fingerprintIdentity(1);
	updatePhraseCache(cache, 0, 5000, identity, kGen, kMetaState, true, 9000);
	assert(!phraseCacheNeedsRescan(cache, 0, 5500, identity, kGen, kMetaState)); // still forward, before cached target.
	assert(phraseCacheNeedsRescan(cache, 0, 1000, identity, kGen, kMetaState)); // seeked backward.
}

// A metadata refresh that leaves the same track identity loaded on the same
// deck (e.g. re-resolved grid/phrase data, or a transient state cycle) must
// still invalidate a cached phrase result - the identity alone is not
// sufficient, since the underlying metadata backing that identity can
// change without the identity itself changing.
void testPhraseCacheRescansOnMetadataGenerationOrStateChange(){
	DjAssistPhraseCacheState cache;
	const DjTrackIdentity identity = fingerprintIdentity(1);
	updatePhraseCache(cache, 0, 1000, identity, kGen, kMetaState, true, 5000);
	assert(!phraseCacheNeedsRescan(cache, 0, 1500, identity, kGen, kMetaState)); // unchanged - no rescan.
	assert(phraseCacheNeedsRescan(cache, 0, 1500, identity, kGen + 1, kMetaState)); // generation bumped.
	assert(phraseCacheNeedsRescan(cache, 0, 1500, identity, kGen, DJ_METADATA_STALE)); // state changed.
}

// -- candidateGenerationCurrent: closes the fillWorkerStep() check-then- ---
// -- lock gap (issue #4). It is now a single equality check against a -----
// -- value that was itself captured atomically with the read it guards ----
// -- (DjSession::assistTrackEntry()'s outRevision, or a fresh immediate ----
// -- re-read taken right before the completion commit) - there is no ------
// -- separate "before" vs "after" probe left to reconcile here. -----------
void testCandidateGenerationCurrentMatchesExactly(){
	assert(DjAssistBridge::candidateGenerationCurrent(5, 5));
	assert(!DjAssistBridge::candidateGenerationCurrent(6, 5));
	assert(!DjAssistBridge::candidateGenerationCurrent(5, 6));
}

} // namespace

int main(){
	testBuildTrackIdentityGatesZeroEvidence();
	testBuildLibraryEntryFullCapabilities();
	testBuildLibraryEntryMissingFieldsClearBitsOnly();
	testCrossfadeMixEndpoints();
	testCrossfadeMixCheckedDivideByZero();
	testCrossfadeMixOverflowGuard();
	testNextRollbackPhaseFullSequence();
	testNextRollbackPhaseSkipsUnsubmittedSteps();
	testNextRollbackPhaseSkipsOwnershipWhenAlreadyInThatState();
	testNextRollbackPhaseSkipsMixOnManualOverride();
	testEvaluateCommandOutcomeMapping();
	testEvaluateBoundaryArrivalNotYet();
	testEvaluateBoundaryArrivalReachedWithinTolerance();
	testEvaluateBoundaryArrivalMissedBeyondTolerance();
	testPhraseCacheNeedsRescanWhenEmpty();
	testPhraseCacheDoesNotRescanBeforeFoundBoundaryPassed();
	testPhraseCacheDoesNotRescanTerminalJustBecauseStillTerminal();
	testPhraseCacheRescansOnIdentityChangeOrDeckChange();
	testPhraseCacheRescansOnBackwardSeek();
	testPhraseCacheRescansOnMetadataGenerationOrStateChange();
	testCandidateGenerationCurrentMatchesExactly();
	return 0;
}
