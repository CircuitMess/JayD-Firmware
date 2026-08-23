#include <assert.h>
#include <string.h>

#include "../src/DjAssist/DjAssistEngine.h"
#include "../src/DjAssist/DjAssistScoring.h"

using namespace DjAssistScoring;

namespace {

DjTrackIdentity fingerprintIdentity(uint8_t seed){
	DjTrackIdentity identity = {};
	identity.flags = DJ_TRACK_IDENTITY_FINGERPRINT;
	memset(identity.fingerprint, seed, sizeof(identity.fingerprint));
	return identity;
}

DjAssistLibraryEntry fullEntry(uint32_t libraryIndex, uint32_t bpmMilli, uint16_t key, uint8_t rating){
	DjAssistLibraryEntry entry;
	entry.libraryIndex = libraryIndex;
	entry.identity = fingerprintIdentity(static_cast<uint8_t>(libraryIndex + 1));
	entry.state = DJ_METADATA_VALID;
	entry.capabilities = DJ_METADATA_HAS_SOURCE_FRAMES | DJ_METADATA_HAS_BPM | DJ_METADATA_HAS_KEY |
		DJ_METADATA_HAS_RATING | DJ_METADATA_HAS_GRID | DJ_METADATA_HAS_DOWNBEATS | DJ_METADATA_HAS_PHRASES;
	entry.bpmMilli = bpmMilli;
	entry.key = key;
	entry.rating = rating;
	entry.durationFrames = 44100ULL * 180ULL; // 180s
	entry.sampleRate = 44100;
	return entry;
}

DjAssistDeckContext deckContext(uint32_t bpmMilli, uint16_t key){
	DjAssistDeckContext deck;
	deck.valid = true;
	deck.bpmMilli = bpmMilli;
	deck.key = key;
	deck.remainingFrames = 44100ULL * 120ULL;
	deck.sampleRate = 44100;
	return deck;
}

// -- key/tempo boundary checks -------------------------------------------

void testKeyRelationships(){
	assert(classifyKeyRelationship(0, 0x101) == DJ_ASSIST_KEY_UNKNOWN);
	assert(classifyKeyRelationship(0x101, 0) == DJ_ASSIST_KEY_UNKNOWN);
	assert(classifyKeyRelationship(5, 5) == DJ_ASSIST_KEY_SAME);          // 5B vs 5B
	assert(classifyKeyRelationship(0x105, 0x105) == DJ_ASSIST_KEY_SAME);  // 5A vs 5A
	assert(classifyKeyRelationship(5, 0x105) == DJ_ASSIST_KEY_RELATIVE);  // 5B vs 5A (relative)
	assert(classifyKeyRelationship(5, 6) == DJ_ASSIST_KEY_ADJACENT);      // 5B vs 6B
	assert(classifyKeyRelationship(5, 4) == DJ_ASSIST_KEY_ADJACENT);      // 5B vs 4B
	// wheel wrap boundary: 12 and 1 are adjacent.
	assert(classifyKeyRelationship(12, 1) == DJ_ASSIST_KEY_ADJACENT);
	assert(classifyKeyRelationship(1, 12) == DJ_ASSIST_KEY_ADJACENT);
	assert(classifyKeyRelationship(5, 8) == DJ_ASSIST_KEY_INCOMPATIBLE);
	assert(classifyKeyRelationship(5, 0x108) == DJ_ASSIST_KEY_INCOMPATIBLE);
}

void testRequiredRateBoundaries(){
	uint32_t rate = 0;
	assert(requiredRateMilli(0, 128000, rate) == false);
	assert(rate == DJ_ASSIST_RATE_UNITY_MILLI);
	assert(requiredRateMilli(128000, 0, rate) == false);

	assert(requiredRateMilli(128000, 128000, rate) == true);
	assert(rate == 1000);

	// exact narrow-band edges are inclusive.
	assert(requiredRateMilli(920, 1000, rate) == true && rate == 920);
	assert(requiredRateMilli(1080, 1000, rate) == true && rate == 1080);

	// checked math on large values must not overflow/crash: an out-of-range
	// ratio is rejected (false) rather than silently wrapping.
	assert(requiredRateMilli(4000000000u, 1, rate) == false);
	assert(rate == DJ_ASSIST_RATE_UNITY_MILLI);
	assert(requiredRateMilli(1, 4000000000u, rate) == false); // integer division underflows to 0
	assert(rate == DJ_ASSIST_RATE_UNITY_MILLI);
	assert(requiredRateMilli(200000, 100000, rate) == true && rate == 2000); // sane 2x, well-formed
}

void testTempoScoreBoundaries(){
	DjAssistDeckContext deck = deckContext(128000, 0);

	// Exactly at the narrow band edge (rate 920) -> NARROW flag, full 400 tempo pts.
	DjAssistLibraryEntry narrowEdge = fullEntry(1, 139130, 0, 255); // 128000*1000/139130 ~= 920
	narrowEdge.capabilities &= ~(DJ_METADATA_HAS_KEY | DJ_METADATA_HAS_RATING);
	DjAssistSuggestion narrow = scoreEntry(narrowEdge, deck, false, false);
	assert(narrow.reasonFlags & DJ_ASSIST_REASON_TEMPO_NARROW);
	assert(!(narrow.reasonFlags & DJ_ASSIST_REASON_TEMPO_OUT_OF_RANGE));

	// Just inside full range but outside narrow band (rate ~1200) -> in-range, reduced score.
	DjAssistLibraryEntry wideRange = fullEntry(2, 106667, 0, 255); // 128000*1000/106667 ~= 1200
	wideRange.capabilities &= ~(DJ_METADATA_HAS_KEY | DJ_METADATA_HAS_RATING);
	DjAssistSuggestion wide = scoreEntry(wideRange, deck, false, false);
	assert(wide.reasonFlags & DJ_ASSIST_REASON_TEMPO_IN_RANGE);
	assert(!(wide.reasonFlags & DJ_ASSIST_REASON_TEMPO_NARROW));

	// Just outside the full 0.5x-1.5x rate window -> excluded from tempo score, flagged.
	DjAssistLibraryEntry outOfRange = fullEntry(3, 85000, 0, 255); // 128000/85000 ~= 1.506x
	outOfRange.capabilities &= ~(DJ_METADATA_HAS_KEY | DJ_METADATA_HAS_RATING);
	DjAssistSuggestion out = scoreEntry(outOfRange, deck, false, false);
	assert(out.reasonFlags & DJ_ASSIST_REASON_TEMPO_OUT_OF_RANGE);
	assert(!(out.reasonFlags & DJ_ASSIST_REASON_TEMPO_IN_RANGE));
	assert(out.score < narrow.score);
}

// -- missing/stale metadata reduces confidence, never blanket-rejects ----

void testMissingAndStaleMetadata(){
	DjAssistDeckContext deck = deckContext(128000, 5);

	DjAssistLibraryEntry bare;
	bare.libraryIndex = 10;
	bare.identity = fingerprintIdentity(50);
	bare.state = DJ_METADATA_ABSENT;
	bare.capabilities = 0; // nothing known at all
	DjAssistSuggestion bareSuggestion = scoreEntry(bare, deck, false, false);
	assert(bareSuggestion.excludeReason == DJ_ASSIST_EXCLUDE_NONE);
	assert(bareSuggestion.confidence < 400);
	assert(bareSuggestion.reasonFlags & DJ_ASSIST_REASON_LOW_CONFIDENCE);
	assert(bareSuggestion.reasonFlags & DJ_ASSIST_REASON_KEY_UNKNOWN);

	// Stale state with fully valid capabilities is still scored on its merits -
	// state alone does not blanket-reject, only corrupt/unsupported does.
	DjAssistLibraryEntry stale = fullEntry(11, 128000, 5, 5);
	stale.state = DJ_METADATA_STALE;
	DjAssistSuggestion staleSuggestion = scoreEntry(stale, deck, false, false);
	assert(staleSuggestion.excludeReason == DJ_ASSIST_EXCLUDE_NONE);
	assert(staleSuggestion.score > bareSuggestion.score);

	DjAssistLibraryEntry corrupt = fullEntry(12, 128000, 5, 5);
	corrupt.state = DJ_METADATA_CORRUPT;
	DjAssistSuggestion corruptSuggestion = scoreEntry(corrupt, deck, false, false);
	assert(corruptSuggestion.excludeReason == DJ_ASSIST_EXCLUDE_UNSUPPORTED_METADATA);

	DjAssistLibraryEntry unsupported = fullEntry(13, 128000, 5, 5);
	unsupported.state = DJ_METADATA_UNSUPPORTED;
	DjAssistSuggestion unsupportedSuggestion = scoreEntry(unsupported, deck, false, false);
	assert(unsupportedSuggestion.excludeReason == DJ_ASSIST_EXCLUDE_UNSUPPORTED_METADATA);
}

// -- loaded/recent exclusion ----------------------------------------------

void testExclusionByIdentity(){
	DjTrackIdentity loadedId = fingerprintIdentity(9);
	DjTrackIdentity recentId = fingerprintIdentity(20);
	DjTrackIdentity unrelated = fingerprintIdentity(99);
	DjTrackIdentity noEvidence = {}; // flags == 0, no fingerprint/source

	assert(identityMatches(loadedId, loadedId));
	assert(!identityMatches(loadedId, unrelated));
	assert(!identityMatches(noEvidence, noEvidence)); // no evidence never claims a match

	DjAssistDeckContext deck = deckContext(128000, 5);
	DjAssistLibraryEntry loadedEntry = fullEntry(20, 128000, 5, 5);
	loadedEntry.identity = loadedId;
	DjAssistSuggestion loaded = scoreEntry(loadedEntry, deck, /*isLoaded=*/true, false);
	assert(loaded.excludeReason == DJ_ASSIST_EXCLUDE_LOADED);

	DjAssistLibraryEntry recentEntry = fullEntry(21, 128000, 5, 5);
	recentEntry.identity = recentId;
	DjAssistSuggestion recent = scoreEntry(recentEntry, deck, false, /*isRecent=*/true);
	assert(recent.excludeReason == DJ_ASSIST_EXCLUDE_RECENT);
}

// -- deterministic ranking, ties, and scan-budget chunking ---------------

void testDeterministicRankingAndScanBudget(){
	const uint16_t total = 20;
	DjAssistLibraryEntry entries[total];
	for(uint16_t i = 0; i < total; i++){
		// Two ties at the top score (indices 3 and 7 both perfect matches);
		// ascending libraryIndex must win the tie.
		bool topTier = (i == 3 || i == 7);
		entries[i] = fullEntry(i, topTier ? 128000u : (128000u - (i + 1) * 5000u), 5, topTier ? 5 : 1);
	}

	DjAssistDeckContext deck = deckContext(128000, 5);

	// Full single-shot ranking.
	DjAssistSuggestion fullPass[DJ_ASSIST_MAX_SUGGESTIONS] = {};
	uint8_t fullCount = 0;
	uint16_t fullCursor = 0;
	uint16_t processed = scanTick(entries, total, fullCursor, total, deck, nullptr, 0, nullptr, 0,
		fullPass, fullCount, DJ_ASSIST_MAX_SUGGESTIONS);
	assert(processed == total);
	assert(fullCount == DJ_ASSIST_MAX_SUGGESTIONS);
	assert(fullPass[0].libraryIndex == 3); // tie winner: lower libraryIndex first
	assert(fullPass[1].libraryIndex == 7);
	for(uint8_t i = 1; i < fullCount; i++){
		assert(fullPass[i - 1].score >= fullPass[i].score);
	}

	// Chunked ranking (small, uneven budgets, wrapping cursor) must match exactly.
	DjAssistSuggestion chunked[DJ_ASSIST_MAX_SUGGESTIONS] = {};
	uint8_t chunkedCount = 0;
	uint16_t cursor = 0;
	uint16_t totalProcessed = 0;
	const uint16_t budgets[] = { 3, 5, 1, 4, 7 }; // sums to exactly one full pass (20)
	for(uint8_t b = 0; b < 5; b++){
		totalProcessed += scanTick(entries, total, cursor, budgets[b], deck, nullptr, 0, nullptr, 0,
			chunked, chunkedCount, DJ_ASSIST_MAX_SUGGESTIONS);
	}
	assert(totalProcessed == total);
	assert(chunkedCount == fullCount);
	for(uint8_t i = 0; i < fullCount; i++){
		assert(chunked[i].libraryIndex == fullPass[i].libraryIndex);
		assert(chunked[i].score == fullPass[i].score);
	}

	// Bounded work per call: a small budget never processes more than requested.
	uint16_t smallCursor = 0;
	uint8_t smallCount = 0;
	DjAssistSuggestion smallOut[DJ_ASSIST_MAX_SUGGESTIONS] = {};
	assert(scanTick(entries, total, smallCursor, 2, deck, nullptr, 0, nullptr, 0, smallOut, smallCount, DJ_ASSIST_MAX_SUGGESTIONS) == 2);
	assert(smallCursor == 2);
}

void testMergeSuggestionCapacityBoundary(){
	DjAssistSuggestion suggestions[3] = {};
	uint8_t count = 0;
	for(uint32_t i = 0; i < 5; i++){
		DjAssistSuggestion s;
		s.libraryIndex = i;
		s.score = static_cast<uint16_t>(100 + i); // strictly increasing
		mergeSuggestion(suggestions, count, 3, s);
	}
	assert(count == 3);
	// Only the top 3 scores (indices 2,3,4) survive, highest first.
	assert(suggestions[0].libraryIndex == 4);
	assert(suggestions[1].libraryIndex == 3);
	assert(suggestions[2].libraryIndex == 2);

	// Excluded candidates never enter the ranking, regardless of score.
	DjAssistSuggestion excluded;
	excluded.libraryIndex = 99;
	excluded.score = 5000;
	excluded.excludeReason = DJ_ASSIST_EXCLUDE_LOADED;
	mergeSuggestion(suggestions, count, 3, excluded);
	assert(count == 3);
	assert(suggestions[0].libraryIndex == 4);

	// Re-merging the same libraryIndex again (e.g. an overlapping scan
	// chunk) must replace its entry, never duplicate it.
	DjAssistSuggestion rescored;
	rescored.libraryIndex = 3;
	rescored.score = 500; // now clearly the best
	mergeSuggestion(suggestions, count, 3, rescored);
	assert(count == 3);
	assert(suggestions[0].libraryIndex == 3);
	assert(suggestions[0].score == 500);
	uint8_t occurrences = 0;
	for(uint8_t i = 0; i < count; i++){
		if(suggestions[i].libraryIndex == 3) occurrences++;
	}
	assert(occurrences == 1);
}

// A track that WAS ranked (e.g. because it was free/valid on an earlier
// scan pass) but has since become excluded (loaded/recent/unsupported) must
// be removed from the ranking, not left stale.
void testMergeSuggestionExcludedRemovesStaleEntry(){
	// Single-entry case: insert score 900 at index 3, then re-merge the same
	// index as excluded - the list must end up empty.
	{
		DjAssistSuggestion suggestions[5] = {};
		uint8_t count = 0;

		DjAssistSuggestion first;
		first.libraryIndex = 3;
		first.score = 900;
		mergeSuggestion(suggestions, count, 5, first);
		assert(count == 1);
		assert(suggestions[0].libraryIndex == 3);

		DjAssistSuggestion nowExcluded;
		nowExcluded.libraryIndex = 3;
		nowExcluded.score = 900; // score is irrelevant once excluded
		nowExcluded.excludeReason = DJ_ASSIST_EXCLUDE_LOADED;
		mergeSuggestion(suggestions, count, 5, nowExcluded);
		assert(count == 0);
	}

	// Full-list case: a full top-N list where a middle entry becomes
	// excluded must shrink by exactly one, preserving the relative order of
	// the remaining entries (no stale duplicate, no leftover slot at old
	// score/position).
	{
		DjAssistSuggestion suggestions[4] = {};
		uint8_t count = 0;
		for(uint32_t i = 0; i < 4; i++){
			DjAssistSuggestion s;
			s.libraryIndex = i;
			s.score = static_cast<uint16_t>(400 - i * 10); // 0 > 1 > 2 > 3 in score
			mergeSuggestion(suggestions, count, 4, s);
		}
		assert(count == 4);
		assert(suggestions[0].libraryIndex == 0);
		assert(suggestions[1].libraryIndex == 1);
		assert(suggestions[2].libraryIndex == 2);
		assert(suggestions[3].libraryIndex == 3);

		DjAssistSuggestion recentNow;
		recentNow.libraryIndex = 2; // was ranked third, now excluded
		recentNow.score = 999;
		recentNow.excludeReason = DJ_ASSIST_EXCLUDE_RECENT;
		mergeSuggestion(suggestions, count, 4, recentNow);
		assert(count == 3);
		assert(suggestions[0].libraryIndex == 0);
		assert(suggestions[1].libraryIndex == 1);
		assert(suggestions[2].libraryIndex == 3);
		for(uint8_t i = 0; i < count; i++){
			assert(suggestions[i].libraryIndex != 2);
		}

		// A now-freed slot must accept a fresh candidate normally afterwards.
		DjAssistSuggestion fresh;
		fresh.libraryIndex = 9;
		fresh.score = 50; // lowest score, still fits since count < capacity
		mergeSuggestion(suggestions, count, 4, fresh);
		assert(count == 4);
		uint8_t occurrences9 = 0;
		for(uint8_t i = 0; i < count; i++){
			if(suggestions[i].libraryIndex == 9) occurrences9++;
		}
		assert(occurrences9 == 1);
	}
}

// -- crossfade curve endpoints / overflow-wrap guards ---------------------

void testCrossfadeCurveAndOverflowGuards(){
	assert(crossfadeCurve(0, 16) == 0);
	assert(crossfadeCurve(16, 16) == 255);
	assert(crossfadeCurve(17, 16) == 255); // past-the-end saturates, never overflows
	assert(crossfadeCurve(8, 16) == 127);
	assert(crossfadeCurve(0, 0) == 255); // checked divide-by-zero guard

	uint8_t previous = 0;
	for(uint16_t step = 0; step <= 32; step++){
		uint8_t value = crossfadeCurve(step, 32);
		assert(value >= previous); // monotonic, no wraparound
		previous = value;
	}
}

// -- fake actuator + full transition state machine ------------------------

class FakeActuator : public DjAssistActuator {
public:
	bool rejectNext = false;
	uint8_t completeAfterPolls = 1;
	uint32_t nextId = 1;
	// When set, poll() returns this status unconditionally instead of the
	// normal poll-count-driven ACCEPTED/APPLIED progression - used to
	// exercise terminal statuses (SUPERSEDED/FAILED/REJECTED) a real
	// command could reach without needing that many fake polls.
	bool forceStatusEnabled = false;
	DjCommandStatus forceStatus = DJ_COMMAND_ACCEPTED;

	bool submit(const DjAssistTransitionStep&, uint32_t& outCommandId) override{
		if(rejectNext) return false;
		outCommandId = nextId++;
		pollCounts[outCommandId] = 0;
		return true;
	}

	DjCommandStatus poll(uint32_t commandId) const override{
		if(forceStatusEnabled) return forceStatus;
		uint8_t& polls = const_cast<FakeActuator*>(this)->pollCounts[commandId];
		polls++;
		return polls >= completeAfterPolls ? DJ_COMMAND_APPLIED : DJ_COMMAND_ACCEPTED;
	}

private:
	mutable uint8_t pollCounts[64] = {};
};

DjAssistGuardSnapshot readyGuard(uint8_t fromDeck, uint8_t toDeck, DjTrackIdentity targetIdentity = fingerprintIdentity(1)){
	DjAssistGuardSnapshot guard;
	guard.recording = false;
	guard.mediaPresent = true;
	guard.mix = 127;
	guard.deckLoaded[fromDeck] = true;
	guard.deckLoaded[toDeck] = true;
	guard.deckPlaying[fromDeck] = true;
	guard.deckPlaying[toDeck] = false;
	guard.metadataValid[fromDeck] = true;
	guard.metadataValid[toDeck] = true;
	guard.rateMilli[fromDeck] = DJ_ASSIST_RATE_UNITY_MILLI;
	guard.rateMilli[toDeck] = DJ_ASSIST_RATE_UNITY_MILLI;
	guard.deckIdentity[toDeck] = targetIdentity;
	return guard;
}

void driveUntilCommandVisible(DjAssistEngine& engine, FakeActuator& actuator, const DjAssistGuardSnapshot& guard, uint16_t maxTicks = 200){
	DjAssistBoundaryHint boundary;
	boundary.hasDownbeat = true;
	boundary.downbeatFrame = 1000;
	// This helper drives a transition all the way through, so the
	// WAIT_BOUNDARY step must actually be allowed to advance - the specific
	// "hasDownbeat true but not yet reached" waiting behaviour is covered
	// separately by testTransitionWaitsForBoundaryStep().
	boundary.reached = true;
	for(uint16_t i = 0; i < maxTicks; i++){
		if(engine.mode() != DJ_ASSIST_MODE_TRANSITION_ARMED && engine.mode() != DJ_ASSIST_MODE_TRANSITION_RUNNING) return;
		engine.tick(actuator, guard, boundary);
	}
}

void testArmRequiresValidPreconditions(){
	DjAssistEngine engine;
	DjTrackIdentity target = fingerprintIdentity(1);

	DjAssistGuardSnapshot notLoaded = readyGuard(0, 1);
	notLoaded.deckLoaded[1] = false;
	assert(!engine.armTransition(0, 1, 5, target, 16, true, true, notLoaded));
	assert(engine.mode() == DJ_ASSIST_MODE_OFF);

	DjAssistGuardSnapshot recording = readyGuard(0, 1);
	recording.recording = true;
	assert(!engine.armTransition(0, 1, 5, target, 16, true, true, recording));

	DjAssistGuardSnapshot badBeats = readyGuard(0, 1);
	assert(!engine.armTransition(0, 1, 5, target, 15, true, true, badBeats)); // must be 4/8/16/32

	assert(!engine.armTransition(0, 0, 5, target, 16, true, true, badBeats)); // same deck
}

// armTransition() requires the target deck to be stopped and sync-off at
// arm time (simplest safe contract - see DjAssistTransitionPlan::
// toDeckStartOwnedByPlan/toDeckSyncOwnedByPlan): if the target were already
// playing/synced, the plan's START_DECK/ENABLE_SYNC steps would be
// idempotent no-ops against pre-existing user state that a rollback must
// never touch, so arming is refused outright rather than trying to infer
// ownership from a baseline later.
void testArmRejectsAlreadyPlayingOrSyncedTarget(){
	DjTrackIdentity target = fingerprintIdentity(1);

	{
		DjAssistEngine engine;
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		guard.deckPlaying[1] = true;
		assert(!engine.armTransition(0, 1, 5, target, 16, true, true, guard));
		assert(engine.mode() == DJ_ASSIST_MODE_OFF);
	}
	{
		DjAssistEngine engine;
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		guard.syncActive[1] = true;
		assert(!engine.armTransition(0, 1, 5, target, 16, true, true, guard));
		assert(engine.mode() == DJ_ASSIST_MODE_OFF);
	}
	// Idle/not-synced target at arm time: arm succeeds, and neither
	// ownership flag is set yet (only set once the corresponding step is
	// observed applied - see testTransitionOwnershipTrackedFromAppliedStep).
	{
		DjAssistEngine engine;
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		assert(engine.armTransition(0, 1, 5, target, 16, true, true, guard));
		assert(!engine.plan().toDeckStartOwnedByPlan);
		assert(!engine.plan().toDeckSyncOwnedByPlan);
	}
}

// Ownership must be recorded from the exact applied mutation, not merely
// from "this step type was submitted" - it starts false and flips to true
// only once the engine observes the corresponding command reach
// DJ_COMMAND_APPLIED.
void testTransitionOwnershipTrackedFromAppliedStep(){
	DjAssistEngine engine;
	FakeActuator actuator;
	actuator.completeAfterPolls = 1;
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
	assert(engine.armTransition(0, 1, 5, target, 16, false, true, guard));

	DjAssistBoundaryHint boundary;
	// Step 0 is START_DECK (startAtBoundary=false, tempoLock=true): submit,
	// then poll to APPLIED.
	engine.tick(actuator, guard, boundary); // submit START_DECK
	assert(!engine.plan().toDeckStartOwnedByPlan);
	guard.deckPlaying[1] = true; // reflects the just-applied START_DECK
	engine.tick(actuator, guard, boundary); // poll -> APPLIED
	assert(engine.plan().toDeckStartOwnedByPlan);
	assert(!engine.plan().toDeckSyncOwnedByPlan);

	// Step 1 is LOCK_TEMPO (both LOCK_TEMPO and ENABLE_SYNC ultimately call
	// setSync(true) - see buildSteps()/DjAssistSessionActuator - so both
	// must set toDeckSyncOwnedByPlan the moment either one applies.
	// Previously only ENABLE_SYNC set this flag: if LOCK_TEMPO's own
	// setSync(true) applied first (as it always does in the default plan)
	// and something failed the transition before ENABLE_SYNC ever
	// submitted, rollback would see toDeckSyncOwnedByPlan still false and
	// skip releasing sync entirely, leaking it on indefinitely.
	engine.tick(actuator, guard, boundary); // submit LOCK_TEMPO
	assert(!engine.plan().toDeckSyncOwnedByPlan);
	guard.syncActive[1] = true; // reflects LOCK_TEMPO's own setSync(true)
	engine.tick(actuator, guard, boundary); // poll -> APPLIED
	assert(engine.plan().toDeckSyncOwnedByPlan);

	// Step 2 is ENABLE_SYNC - a no-op idempotent re-application of the
	// same setSync(true); ownership was already true from LOCK_TEMPO above
	// and must remain true (not itself require re-detection).
	engine.tick(actuator, guard, boundary); // submit ENABLE_SYNC
	assert(engine.plan().toDeckSyncOwnedByPlan);
	engine.tick(actuator, guard, boundary); // poll -> APPLIED
	assert(engine.plan().toDeckSyncOwnedByPlan);
}

// A user command (play/sync on the target deck, or mix) admitted at any
// point after arm - whether it is admitted before the plan's own
// corresponding step ever submits, or admitted/applied after that step has
// already submitted - must abort the transition with MANUAL_OVERRIDE. This
// is the core regression for issue #2: the previous "has our own step
// submitted yet" gating went blind to a user command that raced in after
// submission (queued behind it, or applied while the plan's own command was
// still in flight); the monotonic intent-generation baseline captured at
// arm() is instead compared unconditionally every tick, so ordering can
// never matter. Bumping guard.playIntentGeneration/syncIntentGeneration
// here stands in for DjSession::admitAssistCommand() incrementing
// DjAssistIntentGenerations the instant a non-system command is admitted -
// see DjSessionState.h.
void testTransitionAbortsOnTargetIntentDivergence(){
	// Target play intent diverges before START_DECK is ever submitted.
	{
		DjAssistEngine engine;
		FakeActuator actuator;
		DjTrackIdentity target = fingerprintIdentity(1);
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		assert(engine.armTransition(0, 1, 5, target, 16, true, true, guard));

		DjAssistGuardSnapshot diverged = guard;
		diverged.playIntentGeneration[1] = guard.playIntentGeneration[1] + 1;
		DjAssistBoundaryHint boundary;
		engine.tick(actuator, diverged, boundary);
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_MANUAL_OVERRIDE);
	}
	// Target sync intent diverges before ENABLE_SYNC is ever submitted.
	{
		DjAssistEngine engine;
		FakeActuator actuator;
		DjTrackIdentity target = fingerprintIdentity(1);
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		assert(engine.armTransition(0, 1, 5, target, 16, true, true, guard));

		DjAssistGuardSnapshot diverged = guard;
		diverged.syncIntentGeneration[1] = guard.syncIntentGeneration[1] + 1;
		DjAssistBoundaryHint boundary;
		engine.tick(actuator, diverged, boundary);
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_MANUAL_OVERRIDE);
	}
	// Same divergence, but admitted only AFTER the plan's own START_DECK
	// step has already submitted (still in flight, not yet applied) - the
	// exact ordering the old stepSubmitted-gated check went blind to.
	{
		DjAssistEngine engine;
		FakeActuator actuator;
		actuator.completeAfterPolls = 5; // stays ACCEPTED for several polls
		DjTrackIdentity target = fingerprintIdentity(1);
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		assert(engine.armTransition(0, 1, 5, target, 16, false, false, guard));

		DjAssistBoundaryHint boundary;
		engine.tick(actuator, guard, boundary); // submit START_DECK
		assert(engine.plan().steps[0].submitted);
		assert(!engine.plan().steps[0].applied);

		DjAssistGuardSnapshot diverged = guard;
		diverged.playIntentGeneration[1] = guard.playIntentGeneration[1] + 1;
		engine.tick(actuator, diverged, boundary);
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_MANUAL_OVERRIDE);
	}
}

// A command superseded before it ever applies must fail the transition
// rather than let the generic step-poll loop wait on it forever - only the
// crossfade actuator's own internal resubmit policy may treat SUPERSEDED as
// anything other than terminal (see DjAssistSessionActuator::pollCrossfade,
// exercised only by the real controller/actuator, not this pure engine).
void testTransitionFailsOnSupersededNormalStep(){
	DjAssistEngine engine;
	FakeActuator actuator;
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
	assert(engine.armTransition(0, 1, 4, target, 4, false, false, guard));

	DjAssistBoundaryHint boundary;
	engine.tick(actuator, guard, boundary); // submit first step
	actuator.forceStatus = DJ_COMMAND_SUPERSEDED;
	actuator.forceStatusEnabled = true;
	engine.tick(actuator, guard, boundary); // poll -> SUPERSEDED
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
	assert(engine.plan().failure == DJ_ASSIST_FAIL_COMMAND_REJECTED);
}

void testTransitionHappyPath(){
	DjAssistEngine engine;
	FakeActuator actuator;
	actuator.completeAfterPolls = 2;
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);

	assert(engine.armTransition(0, 1, 5, target, 16, true, true, guard));
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_ARMED);
	assert(engine.plan().stepCount > 0);

	driveUntilCommandVisible(engine, actuator, guard);
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_COMPLETE);
	assert(engine.plan().failure == DJ_ASSIST_FAIL_NONE);
	for(uint8_t i = 0; i < engine.plan().stepCount; i++){
		assert(engine.plan().steps[i].applied);
	}
}

void testTransitionWaitsForAppliedResult(){
	DjAssistEngine engine;
	FakeActuator actuator;
	actuator.completeAfterPolls = 5; // stays ACCEPTED for several polls
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);
	assert(engine.armTransition(0, 1, 5, target, 8, false, false, guard));

	DjAssistBoundaryHint boundary;
	engine.tick(actuator, guard, boundary); // submits step 0
	assert(engine.plan().steps[0].submitted);
	assert(!engine.plan().steps[0].applied);
	assert(engine.plan().currentStep == 0);

	for(int i = 0; i < 3; i++) engine.tick(actuator, guard, boundary);
	assert(!engine.plan().steps[0].applied); // still waiting - not enough polls yet
	assert(engine.plan().currentStep == 0);
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_RUNNING);
}

void testTransitionCancel(){
	DjAssistEngine engine;
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);
	assert(engine.armTransition(0, 1, 5, target, 4, true, true, guard));
	engine.cancelTransition();
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
	assert(engine.plan().failure == DJ_ASSIST_FAIL_CANCELLED);

	// Cancelling again (or an OFF engine) is a no-op, not a crash.
	engine.cancelTransition();
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
}

void testTransitionCommandRejected(){
	DjAssistEngine engine;
	FakeActuator actuator;
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);
	assert(engine.armTransition(0, 1, 5, target, 4, false, false, guard));

	actuator.rejectNext = true;
	DjAssistBoundaryHint boundary;
	engine.tick(actuator, guard, boundary);
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
	assert(engine.plan().failure == DJ_ASSIST_FAIL_COMMAND_REJECTED);
}

void testTransitionMediaAndMetadataLoss(){
	DjAssistEngine engine;
	FakeActuator actuator;
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);
	assert(engine.armTransition(0, 1, 5, target, 4, false, false, guard));

	DjAssistGuardSnapshot removed = guard;
	removed.mediaPresent = false;
	DjAssistBoundaryHint boundary;
	engine.tick(actuator, removed, boundary);
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
	assert(engine.plan().failure == DJ_ASSIST_FAIL_MEDIA_REMOVED);

	DjAssistEngine engine2;
	assert(engine2.armTransition(0, 1, 5, target, 4, false, false, guard));
	DjAssistGuardSnapshot staleMeta = guard;
	staleMeta.metadataValid[0] = false;
	engine2.tick(actuator, staleMeta, boundary);
	assert(engine2.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
	assert(engine2.plan().failure == DJ_ASSIST_FAIL_METADATA_LOST);
}

void testTransitionManualOverride(){
	// Manual play override: source deck's play intent diverges before the
	// STOP_DECK step is ever submitted (see testTransitionAbortsOnTarget-
	// IntentDivergence's doc comment - a real user PLAY/PAUSE command is
	// what bumps guard.playIntentGeneration[fromDeck] on admission).
	{
		DjAssistEngine engine;
		FakeActuator actuator;
		DjTrackIdentity target = fingerprintIdentity(1);
		DjAssistGuardSnapshot guard = readyGuard(0, 1);
		assert(engine.armTransition(0, 1, 5, target, 4, false, false, guard));

		DjAssistGuardSnapshot manualStop = guard;
		manualStop.playIntentGeneration[0] = guard.playIntentGeneration[0] + 1;
		DjAssistBoundaryHint boundary;
		engine.tick(actuator, manualStop, boundary);
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_MANUAL_OVERRIDE);
	}

	// Manual rate override on the still-playing source deck: rate has no
	// discrete origin-tagged command to generation-track, so this remains
	// a direct state comparison (guardOk() keeps this gated on
	// !stopSubmitted - see below for the post-STOP_DECK case).
	{
		DjAssistEngine engine;
		FakeActuator actuator;
		DjTrackIdentity target = fingerprintIdentity(1);
		DjAssistGuardSnapshot guard = readyGuard(0, 1);
		assert(engine.armTransition(0, 1, 5, target, 4, false, false, guard));

		DjAssistGuardSnapshot manualRate = guard;
		manualRate.rateMilli[0] = 1100;
		DjAssistBoundaryHint boundary;
		engine.tick(actuator, manualRate, boundary);
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_MANUAL_OVERRIDE);
	}

	// Manual mix override: a durable comparison of DjSession's monotonic
	// non-system mix-intent generation counter against the value captured
	// at arm() time (DjAssistTransitionPlan::armedMixGeneration - see
	// DjSession::assistIntentGenerationsSnapshot()), latched at command
	// ADMISSION rather than apply. This is checked unconditionally every
	// tick - NOT skipped once the crossfade step has submitted, which is
	// exactly the gap the review flagged: a manual override during an
	// active programmatic ramp must not be silently overwritten by the
	// next system tick. Drive the transition up through the CROSSFADE
	// step's submission first, then prove the divergence still aborts.
	{
		DjAssistEngine engine;
		FakeActuator actuator; // default completeAfterPolls == 1
		DjTrackIdentity target = fingerprintIdentity(1);
		DjAssistGuardSnapshot guard = readyGuard(0, 1);
		assert(engine.armTransition(0, 1, 5, target, 4, false, false, guard));

		DjAssistBoundaryHint boundary;
		engine.tick(actuator, guard, boundary); // submit START_DECK
		guard.deckPlaying[1] = true;
		engine.tick(actuator, guard, boundary); // poll -> APPLIED, advances to CROSSFADE
		engine.tick(actuator, guard, boundary); // submit CROSSFADE (not yet polled)
		assert(engine.plan().steps[engine.plan().currentStep].action == DJ_ASSIST_ACTION_CROSSFADE);
		assert(engine.plan().steps[engine.plan().currentStep].submitted);
		assert(!engine.plan().steps[engine.plan().currentStep].applied);

		DjAssistGuardSnapshot manualOverride = guard;
		manualOverride.mix = 200;
		manualOverride.mixIntentGeneration = guard.mixIntentGeneration + 1;
		engine.tick(actuator, manualOverride, boundary);
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_MANUAL_OVERRIDE);
	}
}

void testTransitionRecordingConflictDuringRun(){
	DjAssistEngine engine;
	FakeActuator actuator;
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);
	assert(engine.armTransition(0, 1, 5, target, 4, false, false, guard));

	DjAssistGuardSnapshot recordingStarted = guard;
	recordingStarted.recording = true;
	DjAssistBoundaryHint boundary;
	engine.tick(actuator, recordingStarted, boundary);
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
	assert(engine.plan().failure == DJ_ASSIST_FAIL_CONFLICT);
}

void testTransitionWaitsForBoundaryStep(){
	DjAssistEngine engine;
	FakeActuator actuator;
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);
	assert(engine.armTransition(0, 1, 5, target, 4, /*startAtBoundary=*/true, false, guard));
	assert(engine.plan().steps[0].action == DJ_ASSIST_ACTION_WAIT_BOUNDARY);

	DjAssistBoundaryHint noBoundary;
	for(int i = 0; i < 5; i++) engine.tick(actuator, guard, noBoundary);
	assert(engine.plan().currentStep == 0); // still waiting, no boundary hint yet
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_RUNNING);

	// Regression for the review finding: a "next boundary exists" hint
	// (hasPhrase/hasDownbeat true, as resolveBoundary() reports almost
	// every tick once any grid exists) must NOT by itself advance the
	// step - only `reached` (the playhead having actually arrived at the
	// ONE captured target frame) may.
	DjAssistBoundaryHint futureBoundaryNotReached;
	futureBoundaryNotReached.hasPhrase = true;
	futureBoundaryNotReached.phraseFrame = 4410;
	futureBoundaryNotReached.reached = false;
	for(int i = 0; i < 5; i++) engine.tick(actuator, guard, futureBoundaryNotReached);
	assert(engine.plan().currentStep == 0); // still waiting - hasPhrase alone must not advance
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_RUNNING);

	DjAssistBoundaryHint reached;
	reached.hasPhrase = true;
	reached.phraseFrame = 4410;
	reached.reached = true;
	engine.tick(actuator, guard, reached);
	assert(engine.plan().currentStep == 1); // advanced past WAIT_BOUNDARY
}

// A target-deck swap (re-load or deck-swap) after arming - but before the
// engine has actually acted on that deck - must be caught every tick, not
// just at arm() time, so the transition never proceeds onto an unconfirmed
// track.
void testTransitionTargetSwapDetected(){
	DjTrackIdentity target = fingerprintIdentity(1);
	DjTrackIdentity swappedIn = fingerprintIdentity(7);

	// Swap while still waiting for the boundary (before any command has
	// been submitted at all).
	{
		DjAssistEngine engine;
		FakeActuator actuator;
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		assert(engine.armTransition(0, 1, 5, target, 4, /*startAtBoundary=*/true, false, guard));
		assert(engine.plan().steps[0].action == DJ_ASSIST_ACTION_WAIT_BOUNDARY);
		assert(engine.plan().currentStep == 0);

		DjAssistGuardSnapshot swapped = guard;
		swapped.deckIdentity[1] = swappedIn; // still "loaded", just a different track
		DjAssistBoundaryHint withPhrase;
		withPhrase.hasPhrase = true;
		withPhrase.phraseFrame = 4410;
		engine.tick(actuator, swapped, withPhrase);
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_TARGET_CHANGED);
		assert(engine.plan().currentStep == 0); // never advanced onto the swapped track
	}

	// Swap after the first (real, non-boundary) step has already been
	// submitted - still must be caught before any further action.
	{
		DjAssistEngine engine;
		FakeActuator actuator;
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		assert(engine.armTransition(0, 1, 5, target, 4, /*startAtBoundary=*/false, false, guard));

		DjAssistBoundaryHint boundary;
		engine.tick(actuator, guard, boundary); // submits START_DECK
		assert(engine.plan().steps[0].submitted);
		assert(!engine.plan().steps[0].applied);

		DjAssistGuardSnapshot swapped = guard;
		swapped.deckIdentity[1] = swappedIn;
		engine.tick(actuator, swapped, boundary);
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_TARGET_CHANGED);
	}

	// Cancel semantics still apply cleanly on top of a swapped/unconfirmed
	// target - cancelling never crashes and always reports CANCELLED, not
	// some other failure code racing with the guard check.
	{
		DjAssistEngine engine;
		DjAssistGuardSnapshot guard = readyGuard(0, 1, target);
		assert(engine.armTransition(0, 1, 5, target, 4, /*startAtBoundary=*/true, false, guard));
		engine.cancelTransition();
		assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_FAILED);
		assert(engine.plan().failure == DJ_ASSIST_FAIL_CANCELLED);
	}

	// A toDeck that reports loaded=true but never matches the confirmed
	// target identity must not even be armable.
	{
		DjAssistEngine engine;
		DjAssistGuardSnapshot guard = readyGuard(0, 1, swappedIn); // wrong track loaded
		assert(!engine.armTransition(0, 1, 5, target, 4, false, false, guard));
		assert(engine.mode() == DJ_ASSIST_MODE_OFF);
	}
}

// -- coach advice: phrase preferred over downbeat, warnings ----------------

void testCoachAdvicePhraseWindow(){
	DjAssistEngine engine;
	DjAssistDeckContext playing = deckContext(128000, 5);
	DjAssistDeckContext candidate = deckContext(128000, 5);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);

	DjAssistBoundaryHint both;
	both.hasDownbeat = true;
	both.downbeatFrame = 100;
	both.hasPhrase = true;
	both.phraseFrame = 900;
	DjAssistCoachAdvice adviceBoth = engine.coachAdvice(0, playing, candidate, both, guard);
	assert(adviceBoth.valid);
	assert(adviceBoth.boundaryIsPhrase);
	assert(adviceBoth.boundaryFrame == 900);
	assert(adviceBoth.suggestedDeck == 1);
	assert(adviceBoth.crossfaderDirection == 1);
	assert(!(adviceBoth.warningFlags & DJ_ASSIST_WARN_NO_GRID));

	DjAssistBoundaryHint downbeatOnly;
	downbeatOnly.hasDownbeat = true;
	downbeatOnly.downbeatFrame = 200;
	DjAssistCoachAdvice adviceDownbeat = engine.coachAdvice(1, playing, candidate, downbeatOnly, guard);
	assert(adviceDownbeat.valid);
	assert(!adviceDownbeat.boundaryIsPhrase);
	assert(adviceDownbeat.boundaryFrame == 200);
	assert(adviceDownbeat.suggestedDeck == 0);
	assert(adviceDownbeat.crossfaderDirection == -1);

	DjAssistBoundaryHint neither;
	DjAssistCoachAdvice adviceNone = engine.coachAdvice(0, playing, candidate, neither, guard);
	assert(adviceNone.warningFlags & DJ_ASSIST_WARN_NO_GRID);

	// Coach advice never mutates engine mode - it stays whatever it was.
	assert(engine.mode() == DJ_ASSIST_MODE_OFF);
}

void testCoachAdviceWarnings(){
	DjAssistEngine engine;
	DjAssistDeckContext playing = deckContext(128000, 5);
	playing.remainingFrames = 44100ULL * 5; // 5s left -> ending soon
	DjAssistDeckContext outOfRangeCandidate = deckContext(60000, 5); // rate way outside 0.5-1.5x
	DjAssistGuardSnapshot guard = readyGuard(0, 1);
	guard.recording = true;
	guard.loopActive[0] = true;

	DjAssistBoundaryHint boundary;
	boundary.hasDownbeat = true;
	DjAssistCoachAdvice advice = engine.coachAdvice(0, playing, outOfRangeCandidate, boundary, guard);
	assert(advice.warningFlags & DJ_ASSIST_WARN_ENDING_SOON);
	assert(advice.warningFlags & DJ_ASSIST_WARN_OUT_OF_RANGE);
	assert(advice.warningFlags & DJ_ASSIST_WARN_RECORDING_ACTIVE);
	assert(advice.warningFlags & DJ_ASSIST_WARN_LOOP_ACTIVE);
}

void testModeToggle(){
	DjAssistEngine engine;
	assert(engine.mode() == DJ_ASSIST_MODE_OFF);
	engine.setCoachEnabled(true);
	assert(engine.mode() == DJ_ASSIST_MODE_COACH);
	engine.setCoachEnabled(false);
	assert(engine.mode() == DJ_ASSIST_MODE_OFF);

	// setCoachEnabled must never disturb an active transition.
	DjTrackIdentity target = fingerprintIdentity(1);
	DjAssistGuardSnapshot guard = readyGuard(0, 1);
	assert(engine.armTransition(0, 1, 5, target, 4, false, false, guard));
	engine.setCoachEnabled(false);
	assert(engine.mode() == DJ_ASSIST_MODE_TRANSITION_ARMED);
}

} // namespace

int main(){
	testKeyRelationships();
	testRequiredRateBoundaries();
	testTempoScoreBoundaries();
	testMissingAndStaleMetadata();
	testExclusionByIdentity();
	testDeterministicRankingAndScanBudget();
	testMergeSuggestionCapacityBoundary();
	testMergeSuggestionExcludedRemovesStaleEntry();
	testCrossfadeCurveAndOverflowGuards();
	testArmRequiresValidPreconditions();
	testArmRejectsAlreadyPlayingOrSyncedTarget();
	testTransitionOwnershipTrackedFromAppliedStep();
	testTransitionAbortsOnTargetIntentDivergence();
	testTransitionFailsOnSupersededNormalStep();
	testTransitionHappyPath();
	testTransitionWaitsForAppliedResult();
	testTransitionCancel();
	testTransitionCommandRejected();
	testTransitionMediaAndMetadataLoss();
	testTransitionManualOverride();
	testTransitionRecordingConflictDuringRun();
	testTransitionWaitsForBoundaryStep();
	testTransitionTargetSwapDetected();
	testCoachAdvicePhraseWindow();
	testCoachAdviceWarnings();
	testModeToggle();
	return 0;
}
