#include "DjAssistController.h"

#include "DjAssistScoring.h"
#include "DjAssistSessionBridge.h"

#include <Arduino.h>

namespace {

// Drives the semantic actions DjAssistEngine's transition state machine
// needs from the real Sync/quantize/mix primitives. START_DECK/STOP_DECK
// map directly to setPlaying(); LOCK_TEMPO and ENABLE_SYNC are two separate
// engine steps but both resolve to the same idempotent setSync(deck, true,
// ...) call (re-arming an already-armed deck is a harmless no-op in
// DjSession::apply()); RELEASE_SYNC maps to setSync(deck, false, ...);
// SET_MIX (rollback-only) is a single exact-value setMix() tracked like any
// other real DjCommand. CROSSFADE has no single DjCommand up front - it's a
// continuous ramp driven by repeated, supersedable DJ_COMMAND_SET_MIX
// submissions each poll (intermediate values are fire-and-forget; their
// supersession by the next ramp tick is normal). Completion is NOT inferred
// from the computed ramp value alone: once DjAssistBridge::computeCrossfadeMix()
// reaches the target endpoint, the actuator submits that exact value as the
// one *tracked* final command and only reports APPLIED once DjSession
// confirms it (REJECTED/FAILED -> failed; SUPERSEDED -> resubmit once more,
// e.g. if unrelated traffic raced it out of the queue).
class DjAssistSessionActuator : public DjAssistActuator {
public:
	explicit DjAssistSessionActuator(DjAssistSessionPort* session) : session_(session){
	}

	bool submit(const DjAssistTransitionStep& step, uint32_t& outCommandId) override{
		switch(step.action){
			case DJ_ASSIST_ACTION_START_DECK:
				return acceptedResult(session_->setPlaying(step.deck, true, DJ_ORIGIN_SYSTEM), outCommandId);
			case DJ_ASSIST_ACTION_LOCK_TEMPO:
			case DJ_ASSIST_ACTION_ENABLE_SYNC: {
				const int8_t otherDeck = step.deck == 0 ? 1 : 0;
				return acceptedResult(
					session_->setSync(step.deck, true, otherDeck, DJ_ORIGIN_SYSTEM), outCommandId
				);
			}
			case DJ_ASSIST_ACTION_CROSSFADE:
				return beginCrossfade(step, outCommandId);
			case DJ_ASSIST_ACTION_STOP_DECK:
				return acceptedResult(session_->setPlaying(step.deck, false, DJ_ORIGIN_SYSTEM), outCommandId);
			case DJ_ASSIST_ACTION_RELEASE_SYNC:
				return acceptedResult(session_->setSync(step.deck, false, -1, DJ_ORIGIN_SYSTEM), outCommandId);
			case DJ_ASSIST_ACTION_SET_MIX: {
				const uint8_t mixValue = step.param > 255 ? 255 : uint8_t(step.param);
				return acceptedResult(session_->setMix(mixValue, DJ_ORIGIN_SYSTEM), outCommandId);
			}
			case DJ_ASSIST_ACTION_WAIT_BOUNDARY:
				break; // engine never submits this action to the actuator.
		}
		return false;
	}

	DjCommandStatus poll(uint32_t commandId) const override{
		if(commandId >= CrossfadeIdBase) return pollCrossfade(commandId);
		return session_->assistTrackedStatus(commandId);
	}

private:
	static const uint32_t CrossfadeIdBase = 0x80000000UL;

	DjAssistSessionPort* session_;
	uint32_t crossfadeId_ = CrossfadeIdBase;
	uint8_t crossfadeToDeck_ = 0;
	uint8_t crossfadeBeats_ = 16;
	uint64_t crossfadeStartMicros_ = 0;
	// True once the ramp's exact endpoint value has been submitted as a
	// single tracked command (crossfadeFinalCommandId_); before that,
	// intermediate ramp ticks are fire-and-forget.
	mutable bool crossfadeFinalSubmitted_ = false;
	mutable uint32_t crossfadeFinalCommandId_ = 0;

	// Records the submitted command as the ONE durable tracked outcome
	// DjSession will resolve authoritatively when it applies - never
	// inferred from the bounded/evictable recentResults ring (see
	// DjSession::assistTrackCommand()/assistTrackedStatus()).
	bool acceptedResult(const DjSubmitResult& result, uint32_t& outCommandId){
		if(result.status == DJ_COMMAND_REJECTED) return false;
		outCommandId = result.id;
		session_->assistTrackCommand(result.id);
		return true;
	}

	bool beginCrossfade(const DjAssistTransitionStep& step, uint32_t& outCommandId){
		if(crossfadeId_ == 0xFFFFFFFFUL) crossfadeId_ = CrossfadeIdBase; // guard wrap (never reached in practice)
		++crossfadeId_;
		crossfadeToDeck_ = step.deck;
		crossfadeBeats_ = (step.param > 0 && step.param <= 255) ? uint8_t(step.param) : 16;
		crossfadeStartMicros_ = micros();
		crossfadeFinalSubmitted_ = false;
		crossfadeFinalCommandId_ = 0;
		outCommandId = crossfadeId_;
		return true;
	}

	DjCommandStatus pollCrossfade(uint32_t commandId) const{
		if(commandId != crossfadeId_) return DJ_COMMAND_FAILED;

		if(crossfadeFinalSubmitted_){
			const DjCommandStatus status = session_->assistTrackedStatus(crossfadeFinalCommandId_);
			switch(DjAssistBridge::evaluateCommandOutcome(status)){
				case DjAssistBridge::DJ_ASSIST_COMMAND_DONE:
					return DJ_COMMAND_APPLIED;
				case DjAssistBridge::DJ_ASSIST_COMMAND_FAILED:
					return DJ_COMMAND_FAILED;
				case DjAssistBridge::DJ_ASSIST_COMMAND_RESUBMIT:
					// Overtaken by unrelated mix traffic before we saw it
					// applied - fall through and resubmit the exact
					// endpoint value once more below.
					crossfadeFinalSubmitted_ = false;
					break;
				case DjAssistBridge::DJ_ASSIST_COMMAND_WAIT:
				default:
					return DJ_COMMAND_PENDING;
			}
		}

		DjSnapshot snapshot;
		session_->copySnapshot(snapshot);
		const uint32_t bpmMilli = snapshot.decks[crossfadeToDeck_].metadata.bpmMilli;
		const uint64_t elapsedMicros = micros() - crossfadeStartMicros_;
		const uint8_t mixValue = DjAssistBridge::computeCrossfadeMix(
			crossfadeToDeck_, elapsedMicros, crossfadeBeats_, bpmMilli
		);
		const uint8_t targetEndpoint = crossfadeToDeck_ == 0 ? 0 : 255;

		if(mixValue == targetEndpoint){
			const DjSubmitResult result = session_->setMix(mixValue, DJ_ORIGIN_SYSTEM);
			if(result.status != DJ_COMMAND_REJECTED){
				crossfadeFinalSubmitted_ = true;
				crossfadeFinalCommandId_ = result.id;
				session_->assistTrackCommand(result.id);
			}
			// Rejected: retry next tick without marking submitted.
			return DJ_COMMAND_PENDING;
		}

		// Intermediate ramp value: fire-and-forget, superseded freely - not
		// tracked (only the final endpoint command is durably watched).
		session_->setMix(mixValue, DJ_ORIGIN_SYSTEM);
		return DJ_COMMAND_PENDING;
	}
};

} // namespace

DjAssistController::DjAssistController(){
}

DjAssistController::~DjAssistController(){
	end();
}

void DjAssistController::begin(DjAssistSessionPort* session){
	session_ = session;
	entryCapacity_ = DJ_ASSIST_MAX_INDEX_ENTRIES;
	entries_ = static_cast<DjAssistLibraryEntry*>(ps_malloc(sizeof(DjAssistLibraryEntry) * entryCapacity_));
	if(!entries_){
		allocationFailed_ = true;
		entryCapacity_ = 0;
	}
	actuator_ = new DjAssistSessionActuator(session_);

	if(!allocationFailed_){
		// See DjAssistFillWorker's doc comment for why this is not
		// CircuitOS's own Util/Task.h: that class's start()/stop(true)
		// pair deadlocks forever if the underlying task creation ever
		// fails to launch. A failed launch here instead simply leaves
		// the candidate table permanently empty (suggestions/advice stay
		// off) without affecting the rest of the session, and end()
		// below is guaranteed to return immediately in that case.
		fillWorker_.begin(&DjAssistController::fillWorkerStepTrampoline, this, "DjAssistFill", 4096);
	}
}

void DjAssistController::end(){
	// No-op (returns immediately) unless a worker was actually launched -
	// see DjAssistFillWorker::end(). Otherwise blocks until it has
	// actually exited before entries_ is freed below.
	fillWorker_.end();
	delete actuator_;
	actuator_ = nullptr;
	free(entries_);
	entries_ = nullptr;
	session_ = nullptr;
}

void DjAssistController::fillWorkerStepTrampoline(void* self){
	static_cast<DjAssistController*>(self)->fillWorkerStep();
}

// Background-thread-only: performs the bounded, possibly SD-backed
// candidate-table fill entirely off DjSession::loop()/the audio thread (see
// review requirement to remove synchronous SD scanning from the session
// loop). Reads one metadata record at a time OUTSIDE the lock (the actual
// I/O never happens while candidateMutex_ is held), then takes the lock
// only for the brief array write + bookkeeping update - so the main thread
// is never blocked waiting on a card read, only on a few field writes.
//
// assistMetadataRevision() is a lock-free atomic read (see DjSession.h),
// so every commit point below re-loads the LIVE revision a second time
// immediately adjacent to (inside) the candidateMutex_ critical section it
// commits under, rather than trusting a value captured before the lock was
// acquired - there is no way for a refresh landing in that gap to go
// unnoticed, because the very last thing checked before every mutation is
// a fresh, in-lock read of the same lock-free accessor a concurrent
// refreshLibraryMetadata()/invalidateLibraryMetadata()/shutdown() bumps
// before it swaps/closes the reader. Deliberately keyed on
// assistMetadataRevision(), NOT assistLibraryGeneration() - the latter is
// an externally-meaningful semantic library identity that can stay
// unchanged across a real underlying content change (same-generation
// sidecar replacement) or a loss+reopen cycle landing back on the same
// value; assistMetadataRevision() is a dedicated counter guaranteed to
// advance on every such case.
void DjAssistController::fillWorkerStep(){
	if(!session_ || allocationFailed_) return;

	const uint32_t currentGeneration = session_->assistMetadataRevision();

	candidateMutex_.lock();
	bool freshGeneration = !generationSeen_ || currentGeneration != loadedGeneration_;
	if(freshGeneration){
		generationSeen_ = true;
		loadedGeneration_ = currentGeneration;
		fillCursor_ = 0;
		entryTotal_ = 0;
		fillComplete_ = false;
	}
	const bool complete = fillComplete_;
	const uint16_t cursor = fillCursor_;
	candidateMutex_.unlock();
	if(complete) return;

	const uint32_t rawTotal = session_->assistTrackCount();
	const uint16_t cappedTotal = rawTotal > entryCapacity_ ? entryCapacity_ : uint16_t(rawTotal);
	if(cappedTotal == 0) return; // reader not ready yet, or an empty library.
	if(cursor >= cappedTotal){
		candidateMutex_.lock();
		// Live revision re-loaded HERE, inside the lock, immediately
		// before the commit - not the currentGeneration captured at the
		// top of this call, and not a pre-lock probe either - so a
		// refresh landing in the gap between counting cappedTotal and
		// this exact instant is still caught.
		const uint32_t liveGeneration = session_->assistMetadataRevision();
		if(DjAssistBridge::candidateGenerationCurrent(loadedGeneration_, liveGeneration)){
			entryTotal_ = cappedTotal;
			fillComplete_ = true;
		}
		candidateMutex_.unlock();
		return;
	}

	// The actual (possibly slow) read happens into a local, unlocked -
	// assistTrackEntry() returns the exact metadata revision it was
	// performed under, captured atomically (single lock) with the read
	// itself inside DjSession, rather than via a separate before/after
	// generation probe here. That closes the previous check-then-lock gap:
	// there is no window between "read the generation" and "read the
	// entry" for a concurrent refresh to swap the reader in, because both
	// happen under one DjSession-side lock acquisition.
	DjAssistLibraryEntry entry;
	uint32_t entryRevision = 0;
	if(!session_->assistTrackEntry(cursor, entry, entryRevision)){
		// Missing/unreadable record: still occupies a slot (so indices
		// stay stable) but carries no capabilities/state, which the
		// scorer treats as reduced confidence rather than a rejection.
		entry = DjAssistLibraryEntry();
		entry.libraryIndex = cursor;
	}

	candidateMutex_.lock();
	// Two independent re-checks immediately before the write: entryRevision
	// (captured together with the read itself, inside DjSession's own
	// lock) AND a fresh in-lock live-generation reload right here (in case
	// a refresh completed after the read but before this lock was
	// acquired). Either mismatch discards the record rather than commits
	// it; fillCursor_ stays put so the same slot is retried on the next
	// (now-current-generation) pass.
	const uint32_t liveGeneration = session_->assistMetadataRevision();
	if(DjAssistBridge::candidateGenerationCurrent(loadedGeneration_, entryRevision) &&
	   DjAssistBridge::candidateGenerationCurrent(loadedGeneration_, liveGeneration) &&
	   fillCursor_ == cursor){
		entries_[cursor] = entry;
		++fillCursor_;
		if(fillCursor_ >= cappedTotal){
			entryTotal_ = cappedTotal;
			fillComplete_ = true;
		}
	}
	candidateMutex_.unlock();
}

// Bounded, lock-protected readiness check - never blocks on I/O (the fill
// task never holds candidateMutex_ across a read). Retained for tests/
// diagnostics (see DjAssistIntegrationSelfCheck::tableReady()), but
// tickSuggestions() no longer gates its scan off this function's separately-
// acquired result - see tickSuggestions()'s doc comment for why that used
// to be a TOCTOU gap. Safe only because assistMetadataRevision() is
// lock-free (see DjSession.h): calling a metadataMutex-guarded accessor
// from inside candidateMutex_ would require reasoning about lock ordering
// across two independently-locked classes.
bool DjAssistController::candidateTableReady(uint32_t& outGeneration){
	candidateMutex_.lock();
	const uint32_t liveGeneration = session_ ? session_->assistMetadataRevision() : 0;
	const bool ready = fillComplete_ && loadedGeneration_ == liveGeneration;
	outGeneration = loadedGeneration_;
	candidateMutex_.unlock();
	return ready;
}

void DjAssistController::updateRecentTracks(const DjSnapshot& snapshot){
	for(uint8_t d = 0; d < DJ_DECK_COUNT; ++d){
		const bool loadedNow = snapshot.decks[d].loaded && snapshot.decks[d].metadata.state == DJ_METADATA_VALID;
		if(loadedNow){
			const DjTrackIdentity& identity = snapshot.decks[d].identity;
			const bool changed = !lastDeckLoaded_[d] ||
				!DjAssistScoring::identityMatches(lastDeckIdentity_[d], identity);
			if(changed){
				if(lastDeckLoaded_[d]){
					// The track that was previously loaded on this deck just
					// got replaced; remember it so a fresh scan doesn't
					// immediately re-suggest what was just played.
					recentTracks_[recentNext_] = lastDeckIdentity_[d];
					recentNext_ = uint8_t((recentNext_ + 1) % DJ_ASSIST_MAX_RECENT_TRACKS);
					if(recentCount_ < DJ_ASSIST_MAX_RECENT_TRACKS) ++recentCount_;
				}
				lastDeckIdentity_[d] = identity;
			}
		}
		lastDeckLoaded_[d] = loadedNow;
	}
}

DjAssistDeckContext DjAssistController::buildDeckContext(const DjSnapshot& snapshot, uint8_t deck) const{
	DjAssistDeckContext ctx;
	if(deck >= DJ_DECK_COUNT) return ctx;
	const DjDeckSnapshot& d = snapshot.decks[deck];
	if(!d.loaded || d.metadata.state != DJ_METADATA_VALID) return ctx;

	ctx.valid = true;
	ctx.bpmMilli = d.metadata.bpmMilli;
	ctx.key = d.metadata.key;
	ctx.sampleRate = d.metadata.sourceSampleRate;
	// Coarse (whole-second) remaining-time estimate from the already-
	// published elapsed/duration fields - deliberately not frame-accurate
	// (no per-tick frame-level snapshot field exists), which is sufficient
	// for a bounded "duration/remaining-time fit" scoring signal.
	const uint64_t elapsedFrames = ctx.sampleRate ? uint64_t(d.elapsed) * ctx.sampleRate : 0;
	const uint64_t totalFrames = d.metadata.sourceDurationFrames;
	ctx.remainingFrames = elapsedFrames < totalFrames ? totalFrames - elapsedFrames : 0;
	return ctx;
}

DjAssistGuardSnapshot DjAssistController::buildGuard(const DjSnapshot& snapshot) const{
	DjAssistGuardSnapshot guard;
	guard.recording = snapshot.recordingInfo.state == DJ_RECORDING_STARTING ||
					  snapshot.recordingInfo.state == DJ_RECORDING_ACTIVE ||
					  snapshot.recordingInfo.state == DJ_RECORDING_STOPPING;
	guard.mediaPresent = session_ ? session_->mediaPresent() : true;
	guard.mix = snapshot.mix;
	for(uint8_t d = 0; d < DJ_DECK_COUNT; ++d){
		guard.deckLoaded[d] = snapshot.decks[d].loaded;
		guard.deckPlaying[d] = snapshot.decks[d].playing;
		guard.loopActive[d] = snapshot.decks[d].loop.state != DJ_LOOP_INACTIVE;
		guard.metadataValid[d] = snapshot.decks[d].metadata.state == DJ_METADATA_VALID;
		guard.rateMilli[d] = uint32_t((uint64_t(snapshot.decks[d].sync.targetRate) * 1000ULL) / DJ_RATE_SCALE);
		guard.syncActive[d] = snapshot.decks[d].sync.state != DJ_SYNC_OFF;
		guard.deckIdentity[d] = snapshot.decks[d].identity;
	}
	// Live values for the non-system intent generations - see
	// DjAssistGuardSnapshot's doc comment and
	// DjSession::assistIntentGenerationsSnapshot(). guardOk()/armTransition()
	// compare these against the plan's armed*Generation baseline; this
	// call itself never compares against anything, so it is safe (and
	// correct) to populate unconditionally even before any transition has
	// armed.
	if(session_){
		const DjAssistIntentGenerations generations = session_->assistIntentGenerationsSnapshot();
		guard.mixIntentGeneration = generations.mix;
		for(uint8_t d = 0; d < DJ_DECK_COUNT; ++d){
			guard.playIntentGeneration[d] = generations.playing[d];
			guard.syncIntentGeneration[d] = generations.sync[d];
		}
	}
	return guard;
}

bool DjAssistController::resolveBoundary(const DjSnapshot& snapshot, uint8_t deck, DjAssistBoundaryHint& hint){
	hint = DjAssistBoundaryHint();
	if(!session_ || deck >= DJ_DECK_COUNT || !snapshot.decks[deck].loaded) return false;

	const uint64_t currentFrame = session_->deckElapsedFrames(deck);

	uint64_t downbeatFrame = 0;
	if(session_->nextDownbeatFrame(deck, currentFrame, downbeatFrame)){
		hint.hasDownbeat = true;
		hint.downbeatFrame = downbeatFrame;
	}

	// Phrase lookups do a bounded but real SD read; only actually perform
	// one when the cache says it's needed (identity change, backward seek,
	// or a previously *found* boundary now passed). A cached *terminal*
	// ("no future phrase") result is deliberately not rescanned every
	// tick just because it's still terminal - without this, a track
	// nearing its end would trigger a full phrase-table rescan on every
	// single Coach/transition tick.
	const DjTrackIdentity& identity = snapshot.decks[deck].identity;
	const uint32_t metadataGeneration = snapshot.decks[deck].metadata.libraryGeneration;
	const DjMetadataState metadataState = snapshot.decks[deck].metadata.state;
	if(DjAssistBridge::phraseCacheNeedsRescan(phraseCache_, deck, currentFrame, identity, metadataGeneration, metadataState)){
		uint64_t phraseFrame = 0;
		const bool found = session_->nextPhraseFrame(deck, currentFrame, phraseFrame);
		DjAssistBridge::updatePhraseCache(
			phraseCache_, deck, currentFrame, identity, metadataGeneration, metadataState, found, phraseFrame);
	}
	if(phraseCache_.valid && !phraseCache_.terminal){
		hint.hasPhrase = true;
		hint.phraseFrame = phraseCache_.frame;
	}
	return hint.hasDownbeat || hint.hasPhrase;
}

void DjAssistController::tickSuggestions(const DjSnapshot& snapshot){
	const DjAssistMode mode = engine_.mode();
	if(mode == DJ_ASSIST_MODE_OFF){
		suggestionCount_ = 0;
		lastAdvice_ = DjAssistCoachAdvice();
		return;
	}

	uint8_t referenceDeck = 0xFF;
	for(uint8_t d = 0; d < DJ_DECK_COUNT; ++d){
		if(snapshot.decks[d].playing && snapshot.decks[d].metadata.state == DJ_METADATA_VALID){
			referenceDeck = d;
			break;
		}
	}
	if(referenceDeck == 0xFF){
		suggestionCount_ = 0;
		lastAdvice_ = DjAssistCoachAdvice();
		return;
	}

	const DjAssistDeckContext deckCtx = buildDeckContext(snapshot, referenceDeck);

	DjTrackIdentity loaded[DJ_DECK_COUNT];
	uint8_t loadedCount = 0;
	for(uint8_t d = 0; d < DJ_DECK_COUNT; ++d){
		if(snapshot.decks[d].loaded && snapshot.decks[d].metadata.state == DJ_METADATA_VALID){
			loaded[loadedCount++] = snapshot.decks[d].identity;
		}
	}

	// Readiness (fillComplete_ + live metadata-revision match), the
	// revision-keyed scanCursor_/suggestionCount_ reset, AND the actual
	// scan now all happen inside ONE candidateMutex_ acquisition - this
	// closes the review's readiness/consumption TOCTOU. Previously
	// tick() called candidateTableReady() (its own separate, already-
	// unlocked-by-the-time-it-returns lock acquisition) to decide
	// whether/how to reset, THEN called this function, which re-locked
	// separately just to run the scan - a background fillWorkerStep()
	// refresh landing in the gap between those two independent lock
	// acquisitions could leave the reset decision stale relative to what
	// the table actually looked like by the time it was scanned, or
	// expose a scan over a table that had just been partially
	// invalidated/is mid-refill for a newer revision. There is no such
	// gap now: the live revision is read exactly once, under this same
	// lock, and used immediately to decide both the reset and the scan
	// (or to skip the scan and clear suggestions if not ready).
	// entries_[]/entryTotal_ are written by the background fill task (see
	// fillWorkerStep()); this is the one place the main thread reads them.
	candidateMutex_.lock();
	const uint32_t liveGeneration = session_ ? session_->assistMetadataRevision() : 0;
	const bool ready = fillComplete_ && loadedGeneration_ == liveGeneration;
	if(!ready){
		// Not ready this tick - either still filling, or a refresh has
		// moved the live revision past what has actually been filled so
		// far. Report no suggestions rather than scan/publish against a
		// table that is incomplete or no longer matches the live
		// revision: never expose a stale or partially-overwritten table.
		scanGenerationSeen_ = false;
		scanCursor_ = 0;
		suggestionCount_ = 0;
	}else{
		if(!scanGenerationSeen_ || liveGeneration != scanGeneration_){
			scanGenerationSeen_ = true;
			scanGeneration_ = liveGeneration;
			scanCursor_ = 0;
			suggestionCount_ = 0;
		}

		// Test-only seam (no-op in production - see the member's doc
		// comment in DjAssistController.h).
		if(testHookBeforeScanConsume_) testHookBeforeScanConsume_(testHookBeforeScanConsumeArg_);

		// Re-confirm the live revision immediately before actually
		// consuming/scanning the table, rather than trusting the
		// `liveGeneration` read captured a few statements above: this is
		// the exact "under-lock live-revision check" the review asked
		// for at the real consumption point, closing even a same-tick
		// gap between the readiness decision above and this instant (see
		// testSuggestionsDiscardWhenRevisionChangesBetweenReadinessAndConsume).
		// On real firmware this can only actually differ if this whole
		// critical section were ever entered without truly holding
		// candidateMutex_ across both reads - which it does - so this is
		// defense in depth, not a reachable production path.
		const uint32_t confirmGeneration = session_ ? session_->assistMetadataRevision() : liveGeneration;
		if(confirmGeneration != liveGeneration){
			scanGenerationSeen_ = false;
			scanCursor_ = 0;
			suggestionCount_ = 0;
		}else{
			DjAssistScoring::scanTick(
				entries_, entryTotal_, scanCursor_, DJ_ASSIST_DEFAULT_SCAN_BUDGET,
				deckCtx, loaded, loadedCount, recentTracks_, recentCount_,
				suggestions_, suggestionCount_, DJ_ASSIST_MAX_SUGGESTIONS
			);
		}
	}
	candidateMutex_.unlock();

	if(mode == DJ_ASSIST_MODE_COACH){
		const uint8_t otherDeck = referenceDeck == 0 ? 1 : 0;
		const DjAssistDeckContext candidateCtx = buildDeckContext(snapshot, otherDeck);
		DjAssistBoundaryHint hint;
		resolveBoundary(snapshot, referenceDeck, hint);
		const DjAssistGuardSnapshot guard = buildGuard(snapshot);
		lastAdvice_ = engine_.coachAdvice(referenceDeck, deckCtx, candidateCtx, hint, guard);
	}else{
		lastAdvice_ = DjAssistCoachAdvice();
	}
}

// Capture-once-then-compare arrival check for the WAIT_BOUNDARY transition
// step, using the established quantize tolerance
// (DJ_QUANTIZE_TOLERANCE_FRAMES) so a boundary is never reported reached
// arbitrarily late. The first tick this step is current, latches ONE
// target boundary frame (prefer phrase, matching Coach's own preference,
// else the next downbeat); every subsequent tick compares the live
// playhead against that fixed target via
// DjAssistBridge::evaluateBoundaryArrival(): NOT_YET keeps waiting,
// REACHED (at or within tolerance past target) fires the hint, and MISSED
// (more than tolerance past target - e.g. this tick's own scheduling
// jitter, or the actuator stalled on a prior step) re-captures the next
// boundary ahead of the current position and keeps waiting rather than
// ever releasing on a stale target.
bool DjAssistController::resolveWaitBoundaryReached(const DjSnapshot& snapshot, uint8_t deck){
	if(!session_ || deck >= DJ_DECK_COUNT || !snapshot.decks[deck].loaded) return false;

	if(!waitBoundaryCaptured_){
		DjAssistBoundaryHint fresh;
		if(!resolveBoundary(snapshot, deck, fresh)) return false; // no grid yet - retry next tick.
		waitBoundaryTargetFrame_ = fresh.hasPhrase ? fresh.phraseFrame : fresh.downbeatFrame;
		waitBoundaryCaptured_ = true;
	}

	const uint64_t currentFrame = session_->deckElapsedFrames(deck);
	switch(DjAssistBridge::evaluateBoundaryArrival(currentFrame, waitBoundaryTargetFrame_)){
		case DjAssistBridge::DJ_ASSIST_BOUNDARY_REACHED:
			return true;
		case DjAssistBridge::DJ_ASSIST_BOUNDARY_MISSED: {
			DjAssistBoundaryHint rescheduled;
			if(resolveBoundary(snapshot, deck, rescheduled)){
				waitBoundaryTargetFrame_ = rescheduled.hasPhrase ? rescheduled.phraseFrame : rescheduled.downbeatFrame;
			}
			// If no fresh boundary is available, stay latched on the
			// stale target; the transition's other guards (metadata/media)
			// fail the transition safely if this persists.
			return false;
		}
		case DjAssistBridge::DJ_ASSIST_BOUNDARY_NOT_YET:
		default:
			return false;
	}
}

void DjAssistController::tickTransition(const DjSnapshot& snapshot){
	if(!actuator_) return;
	const DjAssistMode mode = engine_.mode();
	if(mode != DJ_ASSIST_MODE_TRANSITION_ARMED && mode != DJ_ASSIST_MODE_TRANSITION_RUNNING) return;

	const DjAssistGuardSnapshot guard = buildGuard(snapshot);

	DjAssistBoundaryHint hint;
	const DjAssistTransitionPlan& p = engine_.plan();
	if(p.currentStep < p.stepCount && p.steps[p.currentStep].action == DJ_ASSIST_ACTION_WAIT_BOUNDARY){
		// Boundary timing is anchored to the currently-audible outgoing
		// deck (fromDeck), matching Coach's own "next safe window" advice;
		// the step's `deck` field (toDeck) only tags which deck is being
		// prepared, not which grid the wait is measured against.
		hint.reached = resolveWaitBoundaryReached(snapshot, p.fromDeck);
	}
	engine_.tick(*actuator_, guard, hint);
}

namespace {
bool planStepSubmitted(const DjAssistTransitionPlan& plan, DjAssistTransitionAction action){
	for(uint8_t i = 0; i < plan.stepCount; ++i){
		if(plan.steps[i].action == action) return plan.steps[i].submitted;
	}
	return false;
}
} // namespace

// Bounded rollback for a terminally-failed/cancelled transition: undoes
// only the mutations THIS plan actually introduced (mix/sync/started-deck),
// in mix -> sync -> stop-deck order, one actuator submit/poll per tick,
// waiting for each command's real applied/failed/superseded result before
// advancing (never assumes success from elapsed time). Deliberately never
// attempts to restart fromDeck once STOP_DECK has run - by that point the
// transition is essentially complete and un-stopping the new deck would be
// more disruptive than leaving it playing.
//
// Sync/start-deck ownership is read directly from the plan's
// toDeckSyncOwnedByPlan/toDeckStartOwnedByPlan fields, which the engine set
// only at the instant it observed the corresponding step's command reach
// DJ_COMMAND_APPLIED (see DjAssistEngine::tick()) - never recomputed here
// from "was this step type submitted", since a step can be submitted and
// still be in flight (or have failed) without ever having actually mutated
// target-deck state. A manual mix change at any point after arming means
// the MIX phase must never restore armedMix over it.
void DjAssistController::tickRollback(){
	if(!actuator_) return;
	if(engine_.mode() != DJ_ASSIST_MODE_TRANSITION_FAILED) return;

	// Re-reconcile toDeck play/sync ownership against LIVE intent
	// generations at the START of every tick this runs - not just once, at
	// the single tick guardOk() first observed a divergence.
	// DjAssistEngine::tick() (and thus guardOk()) no longer runs once the
	// transition has already reached FAILED (via a guard failure OR an
	// explicit cancelTransition(), which never calls guardOk() at all), so
	// without this, a user re-asserting play/sync on the target deck AFTER
	// that point - but before rollback has finished acting - would never
	// be caught, and rollback would go on to stop/release state the user
	// re-established after the transition was already terminal. See
	// DjAssistEngine::reconcileOwnershipOnDivergence()/
	// DjAssistBridge::reconcileOwnership().
	if(session_){
		const DjAssistIntentGenerations live = session_->assistIntentGenerationsSnapshot();
		const uint8_t toDeck = engine_.plan().toDeck;
		if(toDeck < DJ_DECK_COUNT){
			engine_.reconcileOwnershipOnDivergence(live.playing[toDeck], live.sync[toDeck]);
		}
	}

	const DjAssistTransitionPlan& p = engine_.plan();

	// Purge exactly once per failure/cancel episode, before computing/
	// advancing rollback phases: any still-queued mix/play/sync command
	// this plan itself submitted (or that raced admission just before
	// cancel) must never be allowed to apply after rollback has already
	// restored safe state.
	if(!pendingPurgeDone_){
		pendingPurgeDone_ = true;
		if(session_){
			session_->assistPurgePendingSystemCommands(p.fromDeck);
			session_->assistPurgePendingSystemCommands(p.toDeck);
		}
	}

	if(rollbackPhase_ == DjAssistBridge::DJ_ASSIST_ROLLBACK_DONE) return;

	const bool crossfadeSubmitted = planStepSubmitted(p, DJ_ASSIST_ACTION_CROSSFADE);
	const bool manualMixOccurred = session_ &&
		session_->assistIntentGenerationsSnapshot().mix != p.armedMixGeneration;

	rollbackPhase_ = DjAssistBridge::nextRollbackPhase(
		rollbackPhase_, crossfadeSubmitted, manualMixOccurred,
		p.toDeckSyncOwnedByPlan, p.toDeckStartOwnedByPlan
	);
	if(rollbackPhase_ == DjAssistBridge::DJ_ASSIST_ROLLBACK_DONE) return;

	DjAssistTransitionStep step;
	if(rollbackPhase_ == DjAssistBridge::DJ_ASSIST_ROLLBACK_MIX){
		step.action = DJ_ASSIST_ACTION_SET_MIX;
		step.deck = p.fromDeck;
		step.param = p.armedMix;
	}else if(rollbackPhase_ == DjAssistBridge::DJ_ASSIST_ROLLBACK_SYNC){
		step.action = DJ_ASSIST_ACTION_RELEASE_SYNC;
		step.deck = p.toDeck;
	}else{ // DJ_ASSIST_ROLLBACK_STOP_DECK
		step.action = DJ_ASSIST_ACTION_STOP_DECK;
		step.deck = p.toDeck;
	}

	if(!rollbackSubmitted_){
		uint32_t commandId = 0;
		if(actuator_->submit(step, commandId)){
			rollbackCommandId_ = commandId;
			rollbackSubmitted_ = true;
		}
		// Rejected: retry the same bounded submit next tick.
		return;
	}

	switch(DjAssistBridge::evaluateCommandOutcome(actuator_->poll(rollbackCommandId_))){
		case DjAssistBridge::DJ_ASSIST_COMMAND_DONE:
			rollbackSubmitted_ = false;
			if(rollbackPhase_ == DjAssistBridge::DJ_ASSIST_ROLLBACK_MIX){
				rollbackPhase_ = DjAssistBridge::DJ_ASSIST_ROLLBACK_SYNC;
			}else if(rollbackPhase_ == DjAssistBridge::DJ_ASSIST_ROLLBACK_SYNC){
				rollbackPhase_ = DjAssistBridge::DJ_ASSIST_ROLLBACK_STOP_DECK;
			}else{
				rollbackPhase_ = DjAssistBridge::DJ_ASSIST_ROLLBACK_DONE;
			}
			break;
		case DjAssistBridge::DJ_ASSIST_COMMAND_RESUBMIT:
			rollbackSubmitted_ = false; // retry this same phase next tick.
			break;
		case DjAssistBridge::DJ_ASSIST_COMMAND_FAILED:
			// No further automated recovery is safe to attempt here (undoing
			// an undo is out of scope); stop rather than spin forever.
			rollbackPhase_ = DjAssistBridge::DJ_ASSIST_ROLLBACK_DONE;
			break;
		case DjAssistBridge::DJ_ASSIST_COMMAND_WAIT:
		default:
			break; // keep waiting.
	}
}

void DjAssistController::tick(){
	if(!session_) return;

	DjSnapshot snapshot;
	if(!session_->copySnapshot(snapshot)) return;

	updateRecentTracks(snapshot);

	// Readiness/generation-reset now happens INSIDE tickSuggestions()'s own
	// candidateMutex_ acquisition (see its doc comment) - no separate
	// candidateTableReady() pre-check/gate here anymore, closing the
	// previous readiness/consumption TOCTOU between this call site and the
	// scan.
	tickSuggestions(snapshot);

	tickTransition(snapshot);
	tickRollback();
}

bool DjAssistController::setCoachEnabled(bool enabled){
	engine_.setCoachEnabled(enabled);
	return true;
}

bool DjAssistController::armTransition(
	uint8_t fromDeck,
	uint8_t toDeck,
	uint32_t libraryIndex,
	const DjTrackIdentity& targetIdentity,
	uint8_t crossfadeBeats,
	bool startAtBoundary,
	bool tempoLock
){
	if(!session_) return false;
	DjSnapshot snapshot;
	session_->copySnapshot(snapshot);
	const DjAssistGuardSnapshot guard = buildGuard(snapshot);
	const bool armed = engine_.armTransition(
		fromDeck, toDeck, libraryIndex, targetIdentity, crossfadeBeats, startAtBoundary, tempoLock, guard
	);
	// A fresh plan must never reuse a WAIT_BOUNDARY target or rollback
	// state latched by a previous transition (cancelled/failed/completed).
	if(armed){
		waitBoundaryCaptured_ = false;
		rollbackPhase_ = DjAssistBridge::DJ_ASSIST_ROLLBACK_IDLE;
		rollbackSubmitted_ = false;
		pendingPurgeDone_ = false;
	}
	return armed;
}

void DjAssistController::cancelTransition(){
	engine_.cancelTransition();
}

void DjAssistController::copySnapshot(DjAssistSnapshot& snapshot) const{
	snapshot.mode = engine_.mode();
	snapshot.plan = engine_.plan();
	snapshot.suggestionCount = suggestionCount_;
	for(uint8_t i = 0; i < suggestionCount_ && i < DJ_ASSIST_MAX_SUGGESTIONS; ++i){
		snapshot.suggestions[i] = suggestions_[i];
	}
	snapshot.advice = lastAdvice_;
}
