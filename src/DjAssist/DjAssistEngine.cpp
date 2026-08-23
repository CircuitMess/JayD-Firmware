#include "DjAssistEngine.h"

#include "DjAssistScoring.h"
#include "DjAssistSessionBridge.h"

DjAssistEngine::DjAssistEngine() : mode_(DJ_ASSIST_MODE_OFF), plan_(){
}

DjAssistMode DjAssistEngine::mode() const{
	return mode_;
}

void DjAssistEngine::setCoachEnabled(bool enabled){
	if(mode_ == DJ_ASSIST_MODE_TRANSITION_ARMED || mode_ == DJ_ASSIST_MODE_TRANSITION_RUNNING) return;
	mode_ = enabled ? DJ_ASSIST_MODE_COACH : DJ_ASSIST_MODE_OFF;
}

DjAssistCoachAdvice DjAssistEngine::coachAdvice(
	uint8_t playingDeck,
	const DjAssistDeckContext& playing,
	const DjAssistDeckContext& candidate,
	const DjAssistBoundaryHint& boundary,
	const DjAssistGuardSnapshot& guard
) const{
	DjAssistCoachAdvice advice;
	if(playingDeck >= DJ_DECK_COUNT || !playing.valid || !candidate.valid) return advice;

	const uint8_t otherDeck = playingDeck == 0 ? 1 : 0;
	advice.suggestedDeck = otherDeck;

	if(boundary.hasPhrase){
		advice.boundaryIsPhrase = true;
		advice.boundaryFrame = boundary.phraseFrame;
	} else if(boundary.hasDownbeat){
		advice.boundaryIsPhrase = false;
		advice.boundaryFrame = boundary.downbeatFrame;
	} else {
		advice.warningFlags |= DJ_ASSIST_WARN_NO_GRID;
	}

	uint32_t rateMilli = DJ_ASSIST_RATE_UNITY_MILLI;
	const bool rateKnown = DjAssistScoring::requiredRateMilli(playing.bpmMilli, candidate.bpmMilli, rateMilli);
	advice.targetRateMilli = rateMilli;
	if(!rateKnown || rateMilli < DJ_ASSIST_RATE_MIN_MILLI || rateMilli > DJ_ASSIST_RATE_MAX_MILLI){
		advice.warningFlags |= DJ_ASSIST_WARN_OUT_OF_RANGE;
	}

	advice.crossfaderDirection = otherDeck == 1 ? 1 : -1;

	static const uint64_t minRemainingSeconds = 20;
	if(playing.sampleRate > 0 && (playing.remainingFrames / playing.sampleRate) < minRemainingSeconds){
		advice.warningFlags |= DJ_ASSIST_WARN_ENDING_SOON;
	}

	if(guard.recording) advice.warningFlags |= DJ_ASSIST_WARN_RECORDING_ACTIVE;
	if(guard.loopActive[playingDeck] || guard.loopActive[otherDeck]) advice.warningFlags |= DJ_ASSIST_WARN_LOOP_ACTIVE;
	if(!guard.metadataValid[playingDeck] || !guard.metadataValid[otherDeck]) advice.warningFlags |= DJ_ASSIST_WARN_NO_METADATA;

	advice.valid = true;
	return advice;
}

bool DjAssistEngine::armTransition(
	uint8_t fromDeck,
	uint8_t toDeck,
	uint32_t libraryIndex,
	const DjTrackIdentity& targetIdentity,
	uint8_t crossfadeBeats,
	bool startAtBoundary,
	bool tempoLock,
	const DjAssistGuardSnapshot& guard
){
	if(mode_ == DJ_ASSIST_MODE_TRANSITION_ARMED || mode_ == DJ_ASSIST_MODE_TRANSITION_RUNNING) return false;
	if(fromDeck >= DJ_DECK_COUNT || toDeck >= DJ_DECK_COUNT || fromDeck == toDeck) return false;
	if(crossfadeBeats != 4 && crossfadeBeats != 8 && crossfadeBeats != 16 && crossfadeBeats != 32) return false;
	if(guard.recording) return false;
	if(!guard.deckLoaded[toDeck]) return false;
	if(!DjAssistScoring::identityMatches(guard.deckIdentity[toDeck], targetIdentity)) return false;
	if(!guard.deckPlaying[fromDeck]) return false;
	if(guard.loopActive[fromDeck] || guard.loopActive[toDeck]) return false;
	if(!guard.metadataValid[fromDeck]) return false;
	// Target deck must be stopped and sync-off at arm time: this is what
	// makes rollback's applied-mutation ownership tracking safe below. If
	// the target were already playing/synced, the plan's START_DECK/
	// ENABLE_SYNC steps would be idempotent no-ops against pre-existing
	// user state that a rollback must never touch - simplest safe contract
	// is to just refuse to arm until the user (or a prior plan) returns the
	// target deck to that baseline.
	if(guard.deckPlaying[toDeck] || guard.syncActive[toDeck]) return false;

	plan_ = DjAssistTransitionPlan();
	plan_.fromDeck = fromDeck;
	plan_.toDeck = toDeck;
	plan_.libraryIndex = libraryIndex;
	plan_.targetIdentity = targetIdentity;
	plan_.crossfadeBeats = crossfadeBeats;
	plan_.startAtBoundary = startAtBoundary;
	plan_.tempoLock = tempoLock;
	plan_.armedMix = guard.mix;
	plan_.armedFromRateMilli = guard.rateMilli[fromDeck];
	plan_.armedMixGeneration = guard.mixIntentGeneration;
	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; deck++){
		plan_.armedPlayGeneration[deck] = guard.playIntentGeneration[deck];
		plan_.armedSyncGeneration[deck] = guard.syncIntentGeneration[deck];
	}

	buildSteps(plan_);
	mode_ = DJ_ASSIST_MODE_TRANSITION_ARMED;
	return true;
}

void DjAssistEngine::buildSteps(DjAssistTransitionPlan& plan) const{
	uint8_t n = 0;
	const uint8_t toDeck = plan.toDeck;
	const uint8_t fromDeck = plan.fromDeck;

	if(plan.startAtBoundary && n < DJ_ASSIST_MAX_TRANSITION_STEPS){
		plan.steps[n].action = DJ_ASSIST_ACTION_WAIT_BOUNDARY;
		plan.steps[n].deck = toDeck;
		n++;
	}
	if(n < DJ_ASSIST_MAX_TRANSITION_STEPS){
		plan.steps[n].action = DJ_ASSIST_ACTION_START_DECK;
		plan.steps[n].deck = toDeck;
		n++;
	}
	if(plan.tempoLock){
		if(n < DJ_ASSIST_MAX_TRANSITION_STEPS){
			plan.steps[n].action = DJ_ASSIST_ACTION_LOCK_TEMPO;
			plan.steps[n].deck = toDeck;
			n++;
		}
		if(n < DJ_ASSIST_MAX_TRANSITION_STEPS){
			plan.steps[n].action = DJ_ASSIST_ACTION_ENABLE_SYNC;
			plan.steps[n].deck = toDeck;
			n++;
		}
	}
	if(n < DJ_ASSIST_MAX_TRANSITION_STEPS){
		plan.steps[n].action = DJ_ASSIST_ACTION_CROSSFADE;
		plan.steps[n].deck = toDeck;
		plan.steps[n].param = plan.crossfadeBeats;
		n++;
	}
	if(n < DJ_ASSIST_MAX_TRANSITION_STEPS){
		plan.steps[n].action = DJ_ASSIST_ACTION_STOP_DECK;
		plan.steps[n].deck = fromDeck;
		n++;
	}
	if(plan.tempoLock && n < DJ_ASSIST_MAX_TRANSITION_STEPS){
		plan.steps[n].action = DJ_ASSIST_ACTION_RELEASE_SYNC;
		plan.steps[n].deck = toDeck;
		n++;
	}
	plan.stepCount = n;
}

bool DjAssistEngine::stepSubmitted(DjAssistTransitionAction action) const{
	for(uint8_t i = 0; i < plan_.stepCount; i++){
		if(plan_.steps[i].action == action) return plan_.steps[i].submitted;
	}
	return false;
}

bool DjAssistEngine::guardOk(const DjAssistGuardSnapshot& guard, DjAssistTransitionFailure& failure){
	if(!guard.mediaPresent){
		failure = DJ_ASSIST_FAIL_MEDIA_REMOVED;
		return false;
	}
	if(!guard.metadataValid[plan_.fromDeck] || !guard.metadataValid[plan_.toDeck]){
		failure = DJ_ASSIST_FAIL_METADATA_LOST;
		return false;
	}
	if(!guard.deckLoaded[plan_.toDeck]){
		failure = DJ_ASSIST_FAIL_TARGET_NOT_LOADED;
		return false;
	}
	if(!DjAssistScoring::identityMatches(guard.deckIdentity[plan_.toDeck], plan_.targetIdentity)){
		failure = DJ_ASSIST_FAIL_TARGET_CHANGED;
		return false;
	}
	if(guard.recording){
		failure = DJ_ASSIST_FAIL_CONFLICT;
		return false;
	}
	if(guard.loopActive[plan_.fromDeck] || guard.loopActive[plan_.toDeck]){
		failure = DJ_ASSIST_FAIL_CONFLICT;
		return false;
	}

	const bool stopSubmitted = stepSubmitted(DJ_ASSIST_ACTION_STOP_DECK);

	// Authoritative divergence detection: a monotonic non-system intent
	// generation (mix/play/sync - see DjAssistIntentGenerations,
	// DjSessionState.h) is latched the instant the corresponding user
	// command is ADMITTED, compared unconditionally, every tick, against
	// the baseline captured at arm() time. This catches a user command
	// that races the plan's own step in either order (queued/applied
	// before or after it), unlike the previous "has our own step
	// submitted yet" gating, which went blind the instant our own step
	// submitted regardless of true ordering - and, because system-origin
	// commands (including both LOCK_TEMPO and ENABLE_SYNC, which both
	// call setSync(true) - see buildSteps()) never bump these counters,
	// the plan's own idempotent steps can never trigger a false positive
	// either, which is what previously made LOCK_TEMPO's redundant sync
	// step read as a manual override.
	//
	// reconcileOwnershipOnDivergence() re-derives BOTH toDeck ownership
	// flags (start, sync) in one pass, independently, BEFORE either of
	// the two failure checks below runs - not as a chain of individual
	// early-return checks that each relinquish only their own property.
	// The previous shape returned on the FIRST divergence found, so a
	// user command that changed BOTH play and sync on the target deck in
	// one action would only ever have its play ownership cleared (the
	// play check ran first and returned before the sync check was even
	// reached), leaving toDeckSyncOwnedByPlan stale/true and rollback
	// would then release sync the user had just re-armed themselves.
	// Without this, a user re-asserting play/sync on the target deck
	// AFTER this plan's own step already applied would fail the
	// transition (correctly) but leave ownership true, so
	// tickRollback() would then stop/release the user's own newer
	// intent instead of leaving it alone - rollback must only ever undo
	// a mutation this plan is still the sole author of.
	reconcileOwnershipOnDivergence(
		guard.playIntentGeneration[plan_.toDeck], guard.syncIntentGeneration[plan_.toDeck]);
	if(guard.playIntentGeneration[plan_.toDeck] != plan_.armedPlayGeneration[plan_.toDeck]){
		failure = DJ_ASSIST_FAIL_MANUAL_OVERRIDE;
		return false;
	}
	if(guard.syncIntentGeneration[plan_.toDeck] != plan_.armedSyncGeneration[plan_.toDeck]){
		failure = DJ_ASSIST_FAIL_MANUAL_OVERRIDE;
		return false;
	}
	// Source-deck sync has no plan-owned rollback phase (the plan never
	// touches fromDeck sync), but a non-system sync command on the
	// outgoing deck is still a genuine manual override that must abort
	// the transition rather than silently continue past it.
	if(guard.syncIntentGeneration[plan_.fromDeck] != plan_.armedSyncGeneration[plan_.fromDeck]){
		failure = DJ_ASSIST_FAIL_MANUAL_OVERRIDE;
		return false;
	}
	if(guard.playIntentGeneration[plan_.fromDeck] != plan_.armedPlayGeneration[plan_.fromDeck]){
		failure = DJ_ASSIST_FAIL_MANUAL_OVERRIDE;
		return false;
	}
	if(guard.mixIntentGeneration != plan_.armedMixGeneration){
		failure = DJ_ASSIST_FAIL_MANUAL_OVERRIDE;
		return false;
	}
	// Rate has no discrete origin-tagged command to generation-track;
	// once the plan's own STOP_DECK step has submitted, the from-deck's
	// rate is no longer meaningfully "armed" (the deck is being released),
	// so this check stays gated as before.
	if(!stopSubmitted && guard.rateMilli[plan_.fromDeck] != plan_.armedFromRateMilli){
		failure = DJ_ASSIST_FAIL_MANUAL_OVERRIDE;
		return false;
	}

	return true;
}

void DjAssistEngine::reconcileOwnershipOnDivergence(uint32_t livePlayGeneration, uint32_t liveSyncGeneration){
	const DjAssistBridge::DjAssistOwnershipReconciliation reconciled = DjAssistBridge::reconcileOwnership(
		plan_.toDeckStartOwnedByPlan, plan_.toDeckSyncOwnedByPlan,
		livePlayGeneration, plan_.armedPlayGeneration[plan_.toDeck],
		liveSyncGeneration, plan_.armedSyncGeneration[plan_.toDeck]
	);
	plan_.toDeckStartOwnedByPlan = reconciled.toDeckStartOwnedByPlan;
	plan_.toDeckSyncOwnedByPlan = reconciled.toDeckSyncOwnedByPlan;
}

void DjAssistEngine::fail(DjAssistTransitionFailure reason){
	plan_.failure = reason;
	mode_ = DJ_ASSIST_MODE_TRANSITION_FAILED;
}

void DjAssistEngine::tick(DjAssistActuator& actuator, const DjAssistGuardSnapshot& guard, const DjAssistBoundaryHint& boundary){
	if(mode_ != DJ_ASSIST_MODE_TRANSITION_ARMED && mode_ != DJ_ASSIST_MODE_TRANSITION_RUNNING) return;

	DjAssistTransitionFailure failure = DJ_ASSIST_FAIL_NONE;
	if(!guardOk(guard, failure)){
		fail(failure);
		return;
	}

	if(mode_ == DJ_ASSIST_MODE_TRANSITION_ARMED) mode_ = DJ_ASSIST_MODE_TRANSITION_RUNNING;

	if(plan_.currentStep >= plan_.stepCount){
		mode_ = DJ_ASSIST_MODE_TRANSITION_COMPLETE;
		return;
	}

	DjAssistTransitionStep& step = plan_.steps[plan_.currentStep];

	if(step.action == DJ_ASSIST_ACTION_WAIT_BOUNDARY){
		// Only `reached` (the playhead having actually arrived at the one
		// captured target boundary) may advance this step - hasPhrase/
		// hasDownbeat describe a perpetually-recomputed *next* boundary
		// and are true almost every tick, which previously made this step
		// advance immediately instead of waiting.
		if(!boundary.reached) return;
		step.submitted = true;
		step.applied = true;
		plan_.currentStep++;
		return;
	}

	if(!step.submitted){
		uint32_t commandId = 0;
		if(!actuator.submit(step, commandId)){
			fail(DJ_ASSIST_FAIL_COMMAND_REJECTED);
			return;
		}
		step.commandId = commandId;
		step.submitted = true;
		return;
	}

	const DjCommandStatus status = actuator.poll(step.commandId);
	switch(DjAssistBridge::evaluateCommandOutcome(status)){
		case DjAssistBridge::DJ_ASSIST_COMMAND_DONE:
			step.applied = true;
			// Ownership is recorded from the exact applied mutation, never
			// inferred from a baseline+submitted heuristic - a manual
			// play/sync between arm and this step's submit is already
			// caught by guardOk()'s divergence checks above, so reaching
			// APPLIED here means this system command is what produced the
			// current playing/synced state.
			if(step.action == DJ_ASSIST_ACTION_START_DECK) plan_.toDeckStartOwnedByPlan = true;
			if(step.action == DJ_ASSIST_ACTION_LOCK_TEMPO || step.action == DJ_ASSIST_ACTION_ENABLE_SYNC){
				plan_.toDeckSyncOwnedByPlan = true;
			}
			plan_.currentStep++;
			if(plan_.currentStep >= plan_.stepCount) mode_ = DJ_ASSIST_MODE_TRANSITION_COMPLETE;
			return;
		case DjAssistBridge::DJ_ASSIST_COMMAND_FAILED:
		case DjAssistBridge::DJ_ASSIST_COMMAND_RESUBMIT:
			// SUPERSEDED (RESUBMIT) is only a bounded, same-plan retry
			// policy for the crossfade actuator's own final-mix command,
			// which never surfaces raw SUPERSEDED here (pollCrossfade()
			// maps it internally to PENDING). For every normal step, a
			// command that was superseded before it ever ran will never
			// apply - waiting on it forever would strand the transition.
			fail(DJ_ASSIST_FAIL_COMMAND_REJECTED);
			return;
		case DjAssistBridge::DJ_ASSIST_COMMAND_WAIT:
		default:
			return; // still in flight, keep waiting.
	}
}

void DjAssistEngine::cancelTransition(){
	if(mode_ == DJ_ASSIST_MODE_TRANSITION_ARMED || mode_ == DJ_ASSIST_MODE_TRANSITION_RUNNING){
		fail(DJ_ASSIST_FAIL_CANCELLED);
	}
}

const DjAssistTransitionPlan& DjAssistEngine::plan() const{
	return plan_;
}
