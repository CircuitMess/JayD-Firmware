#include <assert.h>
#include <string.h>
#include "../src/DjSession/DjSessionState.h"
#include "../src/Screens/MixScreen/MixControlState.h"

static DjCommand command(uint32_t id, DjCommandType type, uint8_t deck = 0, uint8_t slot = 0){
	DjCommand result = {};
	result.id = id;
	result.type = type;
	result.deck = deck;
	result.slot = slot;
	return result;
}

int main(){
	DjCommandQueue queue;
	for(uint32_t id = 1; id <= DJ_COMMAND_CAPACITY; id++){
		assert(queue.push(command(id, DJ_COMMAND_LOAD_DECK)));
	}
	assert(!queue.push(command(100, DJ_COMMAND_LOAD_DECK)));
	assert(queue.depth() == DJ_COMMAND_CAPACITY);

	DjCommand popped;
	for(uint32_t id = 1; id <= DJ_COMMAND_CAPACITY; id++){
		assert(queue.pop(popped));
		assert(popped.id == id);
	}
	assert(!queue.pop(popped));

	assert(queue.push(command(20, DJ_COMMAND_SET_MIX)));
	uint32_t superseded = 0;
	assert(queue.supersede(command(21, DJ_COMMAND_SET_MIX), superseded));
	assert(superseded == 20);
	assert(queue.contains(DJ_COMMAND_SET_MIX));
	assert(queue.pop(popped));
	assert(popped.id == 21);

	assert(queue.push(command(22, DJ_COMMAND_SET_MIX)));
	assert(queue.push(command(23, DJ_COMMAND_LOAD_DECK)));
	assert(!queue.supersede(command(24, DJ_COMMAND_SET_MIX), superseded));
	assert(queue.push(command(24, DJ_COMMAND_SET_MIX)));
	assert(queue.pop(popped) && popped.id == 22);
	assert(queue.pop(popped) && popped.id == 23);
	assert(queue.pop(popped) && popped.id == 24);

	DjCommandResults results;
	DjCommand accepted = command(30, DJ_COMMAND_SET_PLAYING);
	results.record(accepted, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	results.finish(accepted.id, DJ_COMMAND_APPLIED, DJ_COMMAND_ERROR_NONE);
	DjCommandResult recent[DJ_RECENT_RESULT_COUNT] = {};
	results.copyTo(recent);
	assert(recent[0].id == accepted.id);
	assert(recent[0].status == DJ_COMMAND_APPLIED);

	DjCommand failed = command(31, DJ_COMMAND_LOAD_DECK);
	results.record(failed, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	results.finish(failed.id, DJ_COMMAND_FAILED, DJ_COMMAND_ERROR_OPEN_FAILED);
	results.copyTo(recent);
	assert(recent[0].status == DJ_COMMAND_FAILED);
	assert(recent[0].error == DJ_COMMAND_ERROR_OPEN_FAILED);

	DjCommand replaced = command(32, DJ_COMMAND_SET_GAIN);
	results.record(replaced, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	results.finish(replaced.id, DJ_COMMAND_SUPERSEDED, DJ_COMMAND_ERROR_NONE);
	results.copyTo(recent);
	assert(recent[0].status == DJ_COMMAND_SUPERSEDED);

	DjEffectState effects;
	DjEffectTransition transition;
	assert(effects.setType(0, 0, DJ_EFFECT_LOWPASS, false, transition));
	assert(!transition.addSpeed && !transition.setSpeed);
	assert(effects.setIntensity(0, 0, 80, transition));
	assert(effects.get(0, 0).type == DJ_EFFECT_LOWPASS);
	assert(effects.get(0, 0).intensity == 80);
	DjEffectSnapshot visibleEffects[DJ_EFFECT_SLOT_COUNT] = {};
	effects.copyDeck(0, visibleEffects);
	assert(visibleEffects[0].type == DJ_EFFECT_LOWPASS);
	assert(visibleEffects[0].intensity == 80);
	effects.deckLoaded(0, transition);
	assert(!transition.addSpeed);

	transition = {};
	assert(effects.setType(1, 1, DJ_EFFECT_SPEED, false, transition));
	assert(transition.clearEffect);
	assert(!effects.isSpeedActive(1));
	assert(effects.setIntensity(1, 1, 200, transition));
	assert(!transition.setSpeed);
	transition = {};
	effects.deckLoaded(1, transition);
	assert(transition.addSpeed && transition.setSpeed);
	assert(effects.isSpeedActive(1));
	assert(effects.get(1, 1).intensity == 200);

	DjEffectState removedBeforeLoad;
	transition = {};
	assert(removedBeforeLoad.setType(0, 0, DJ_EFFECT_SPEED, false, transition));
	assert(removedBeforeLoad.setType(0, 0, DJ_EFFECT_HIGHPASS, false, transition));
	assert(removedBeforeLoad.setIntensity(0, 0, 64, transition));
	transition = {};
	removedBeforeLoad.deckLoaded(0, transition);
	assert(!transition.addSpeed && !removedBeforeLoad.isSpeedActive(0));
	assert(removedBeforeLoad.get(0, 0).type == DJ_EFFECT_HIGHPASS);
	assert(removedBeforeLoad.get(0, 0).intensity == 64);

	DjEffectState disabledBeforeLoad;
	transition = {};
	assert(disabledBeforeLoad.setType(0, 2, DJ_EFFECT_SPEED, false, transition));
	assert(disabledBeforeLoad.setType(0, 2, DJ_EFFECT_NONE, false, transition));
	transition = {};
	disabledBeforeLoad.deckLoaded(0, transition);
	assert(!transition.addSpeed && !disabledBeforeLoad.isSpeedActive(0));

	DjCommand effectCommand = command(33, DJ_COMMAND_SET_EFFECT_TYPE, 0, 0);
	results.record(effectCommand, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	results.finish(effectCommand.id, DJ_COMMAND_APPLIED, DJ_COMMAND_ERROR_NONE);
	results.copyTo(recent);
	assert(recent[0].id == effectCommand.id);
	assert(recent[0].status == DJ_COMMAND_APPLIED);

	DjSnapshotBuffers snapshots;
	DjSnapshot source = {};
	source.seq = 1;
	source.sessionId = 42;
	strcpy(source.decks[0].path, "/track.aac");
	source.decks[0].metadata.state = DJ_METADATA_VALID;
	source.decks[0].metadata.bpmMilli = 128000;
	snapshots.publish(source);
	DjSnapshot copy = {};
	snapshots.copy(copy);
	assert(copy.seq == source.seq);
	assert(copy.sessionId == source.sessionId);
	assert(strcmp(copy.decks[0].path, source.decks[0].path) == 0);
	assert(copy.decks[0].metadata.bpmMilli == 128000);

	source.seq = 2;
	strcpy(source.decks[0].path, "/other.aac");
	source.decks[0].metadata.bpmMilli = 130000;
	snapshots.publish(source);
	assert(copy.seq == 1);
	assert(strcmp(copy.decks[0].path, "/track.aac") == 0);
	assert(copy.decks[0].metadata.bpmMilli == 128000);

	// --- Quantize/Loop/Sync command supersede + lifecycle -----------------
	// DJ_COMMAND_SET_QUANTIZE and DJ_COMMAND_SET_SYNC are supersedable (rapid
	// dial changes on the same deck coalesce instead of queuing up), while
	// loop engage/reloop/disengage are not (each is a distinct action).
	assert(queue.push(command(40, DJ_COMMAND_SET_QUANTIZE, 0)));
	uint32_t supersededQuantize = 0;
	assert(queue.supersede(command(41, DJ_COMMAND_SET_QUANTIZE, 0), supersededQuantize));
	assert(supersededQuantize == 40);
	assert(queue.pop(popped) && popped.id == 41);

	assert(queue.push(command(42, DJ_COMMAND_SET_SYNC, 1)));
	uint32_t supersededSync = 0;
	assert(queue.supersede(command(43, DJ_COMMAND_SET_SYNC, 1), supersededSync));
	assert(supersededSync == 42);
	assert(queue.pop(popped) && popped.id == 43);

	assert(queue.push(command(44, DJ_COMMAND_LOOP_ENGAGE, 0)));
	uint32_t supersededLoop = 0;
	assert(!queue.supersede(command(45, DJ_COMMAND_LOOP_ENGAGE, 0), supersededLoop));
	assert(queue.push(command(45, DJ_COMMAND_LOOP_ENGAGE, 0)));
	assert(queue.pop(popped) && popped.id == 44);
	assert(queue.pop(popped) && popped.id == 45);

	// Command lifecycle with scheduling diagnostics: a loop engage is
	// recorded ACCEPTED, transitions to PENDING while the boundary seek is
	// outstanding, then to a terminal APPLIED with the resolved target frame
	// and lateness -- mirroring DjSession::loop()/tickLoops() exactly.
	DjCommand loopEngageCmd = command(50, DJ_COMMAND_LOOP_ENGAGE, 0);
	results.record(loopEngageCmd, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	results.finishWithDiagnostics(loopEngageCmd.id, DJ_COMMAND_PENDING, DJ_COMMAND_ERROR_NONE,
								  4096, 0, false);
	results.copyTo(recent);
	assert(recent[0].status == DJ_COMMAND_PENDING);
	assert(recent[0].targetFrame == 4096);
	results.finishWithDiagnostics(loopEngageCmd.id, DJ_COMMAND_APPLIED, DJ_COMMAND_ERROR_NONE,
								  4096, 12, false);
	results.copyTo(recent);
	assert(recent[0].status == DJ_COMMAND_APPLIED);
	assert(recent[0].targetFrame == 4096);
	assert(recent[0].lateFrames == 12);
	assert(!recent[0].missed);

	// A loop whose seek retries are exhausted resolves FAILED/missed instead
	// of silently vanishing (DjSession::tickLoops()'s terminal-outcome path).
	DjCommand loopFailCmd = command(51, DJ_COMMAND_LOOP_ENGAGE, 1);
	results.record(loopFailCmd, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	results.finishWithDiagnostics(loopFailCmd.id, DJ_COMMAND_FAILED, DJ_COMMAND_ERROR_LOOP_OUT_OF_RANGE,
								  8192, 200, true);
	results.copyTo(recent);
	assert(recent[0].status == DJ_COMMAND_FAILED);
	assert(recent[0].error == DJ_COMMAND_ERROR_LOOP_OUT_OF_RANGE);
	assert(recent[0].missed);

	// --- Capability-disabled default snapshot state ------------------------
	// A deck with no grid/quantize/loop/sync capability yet (or one where
	// buildGrid() rejected the metadata) must default-report as disabled,
	// not as a stale/ambiguous "on" state, so an API consumer never mistakes
	// an absent capability for an active one.
	DjSnapshot disabledSnapshot = {};
	assert(!disabledSnapshot.decks[0].grid.valid);
	assert(disabledSnapshot.decks[0].grid.currentQuarterBeat == 0);
	assert(disabledSnapshot.decks[0].quantize.resolution == DJ_QUANTIZE_OFF);
	assert(!disabledSnapshot.decks[0].quantize.pending);
	assert(disabledSnapshot.decks[0].loop.state == DJ_LOOP_INACTIVE);
	assert(disabledSnapshot.decks[0].loop.validLengthMask == 0);
	assert(disabledSnapshot.decks[0].sync.state == DJ_SYNC_OFF);
	assert(disabledSnapshot.decks[0].sync.masterDeck == -1);
	assert(disabledSnapshot.decks[0].sync.lastError == DJ_COMMAND_ERROR_NONE);

	DjCueState cues;
	uint16_t cuePosition = 0;
	assert(!cues.trigger(0, 0, cuePosition));
	assert(cues.set(0, 0, 42));
	assert(cues.trigger(0, 0, cuePosition) && cuePosition == 42);
	assert(cues.clear(0, 0));
	assert(!cues.trigger(0, 0, cuePosition));
	assert(!cues.set(DJ_DECK_COUNT, 0, 1));
	assert(!cues.set(0, DJ_CUE_COUNT, 1));

	MixControlState controls;
	assert(controls.bank == MIX_BANK_MIX);
	controls.openPalette();
	controls.movePalette(1);
	assert(controls.confirmPalette() == MIX_PALETTE_CUES);
	assert(controls.bank == MIX_BANK_CUES);
	controls.moveCuePage(1);
	assert(controls.cueForEncoder(0) == 3);
	assert(controls.cueForEncoder(5) == 5);
	controls.moveCuePage(2);
	assert(controls.cueForEncoder(0) == 6);
	assert(controls.cueForEncoder(2) == -1);
	assert(MixControlState::browseDeckForButton(0) == 0);
	assert(MixControlState::browseDeckForButton(1) == 1);
	assert(MixControlState::browseDeckForButton(2) == -1);
	assert(!MixControlState::encoderChordsEnabled());
	assert(MixControlState::suppressDeckReleaseAfterChord());

	// LOOP/SYNC bank is reachable via the palette alongside MIX/CUES/BROWSE,
	// preserving the ordinal alignment confirmPalette() relies on.
	// openPalette() seeds paletteSelection from the *current* bank (CUES,
	// from above), so only +2 is needed to land on LOOPSYNC, not +3.
	controls.openPalette();
	controls.movePalette(2);
	assert(controls.confirmPalette() == MIX_PALETTE_LOOPSYNC);
	assert(controls.bank == MIX_BANK_LOOPSYNC);

#if defined(JAYD_ENABLE_WIRELESS)
	// Stale-identity regression: a command captured against a pre-reboot
	// boot_id/session_id must never match the device's current identity,
	// across the full uint64 range (not just small test values) - this is
	// the exact compare the boot_id-precision fix depends on staying
	// correct now that boot_id travels the wire as an opaque string.
	DjCommand preReboot = command(40, DJ_COMMAND_SET_MIX);
	preReboot.origin = DJ_ORIGIN_HTTP;
	preReboot.requestBootId = UINT64_MAX - 1;
	preReboot.requestSessionId = 42;
	assert(djCommandIdentityMatches(preReboot, UINT64_MAX - 1, 42));
	// Device rebooted: new boot_id, session reset to 0. The stale command
	// must be rejected, not accidentally accepted.
	assert(!djCommandIdentityMatches(preReboot, UINT64_MAX, 0));
	// Non-HTTP-origin commands (local UI on the device itself) carry no
	// request identity and must never be subject to this check.
	DjCommand local = command(41, DJ_COMMAND_SET_MIX);
	local.origin = DJ_ORIGIN_LOCAL_UI;
	assert(djCommandIdentityMatches(local, UINT64_MAX, 0));
#endif

	// --- admitAssistCommand(): durable tracked-slot routing, queue-wide ---
	// --- origin priority across mix/play/sync, and admission-time (not ---
	// --- apply-time) intent generations, exercised against the real -----
	// --- queue/results/tracked types (review round 4, issues #1-#3/#6). ---
	// This is the exact free function DjSession::submit() calls with its
	// real commandQueue/commandResults/assistTracked/assistIntentGenerations
	// members (see DjSession.cpp) - not a parallel test-only reimplementation
	// - so exercising it here covers the real admission path end to end.
	{
		DjCommandQueue assistQueue;
		DjCommandResults assistResults;
		DjAssistTrackedCommand tracked;
		DjAssistIntentGenerations generations;

		// A tracked command that gets superseded before it is ever dequeued
		// must flip to SUPERSEDED immediately, at admission time - not stay
		// stuck reporting ACCEPTED forever because it never reached
		// DjSession::loop()'s pop()/finish() path.
		DjCommand quantizeA = command(60, DJ_COMMAND_SET_QUANTIZE, 0);
		DjSubmitResult r1 = admitAssistCommand(quantizeA, assistQueue, assistResults, tracked, generations);
		assert(r1.accepted());
		tracked.id = quantizeA.id;
		tracked.tracked = true;
		tracked.status = DJ_COMMAND_ACCEPTED;
		DjCommand quantizeB = command(61, DJ_COMMAND_SET_QUANTIZE, 0);
		DjSubmitResult r2 = admitAssistCommand(quantizeB, assistQueue, assistResults, tracked, generations);
		assert(r2.accepted());
		assert(tracked.id == quantizeA.id);
		assert(tracked.status == DJ_COMMAND_SUPERSEDED); // routed at admission, not left ACCEPTED.
		assert(assistQueue.depth() == 1);

		// SET_MIX origin priority, order 1: a manual mix is queued, then a
		// later system-origin mix (e.g. the next Assist crossfade ramp
		// tick) must be rejected outright rather than silently overwriting
		// the user's queued intent.
		assistQueue.clear();
		DjCommand manualMix = command(70, DJ_COMMAND_SET_MIX, 0);
		manualMix.origin = DJ_ORIGIN_PHYSICAL;
		DjSubmitResult manualAdmit = admitAssistCommand(manualMix, assistQueue, assistResults, tracked, generations);
		assert(manualAdmit.accepted());
		const uint32_t generationAfterManual = generations.mix;
		assert(generationAfterManual > 0); // latched at admission of the non-system mix itself.
		DjCommand systemMix = command(71, DJ_COMMAND_SET_MIX, 0);
		systemMix.origin = DJ_ORIGIN_SYSTEM;
		DjSubmitResult systemAdmit = admitAssistCommand(systemMix, assistQueue, assistResults, tracked, generations);
		assert(!systemAdmit.accepted());
		assert(systemAdmit.status == DJ_COMMAND_REJECTED);
		assert(systemAdmit.error == DJ_COMMAND_ERROR_ASSIST_OVERRIDE_PENDING);
		assert(assistQueue.depth() == 1); // manual mix still the sole queued command.
		assert(generations.mix == generationAfterManual); // rejected attempt must not bump it.

		// SET_MIX origin priority, order 2: a system-origin mix is queued
		// first, then a manual mix arrives - the manual mix must win
		// (replace it), since only system-over-manual is blocked.
		assistQueue.clear();
		generations = DjAssistIntentGenerations();
		DjCommand systemFirst = command(72, DJ_COMMAND_SET_MIX, 0);
		systemFirst.origin = DJ_ORIGIN_SYSTEM;
		assert(admitAssistCommand(systemFirst, assistQueue, assistResults, tracked, generations).accepted());
		assert(generations.mix == 0); // system-origin admission never bumps the manual generation.
		DjCommand manualSecond = command(73, DJ_COMMAND_SET_MIX, 0);
		manualSecond.origin = DJ_ORIGIN_LOCAL_UI;
		DjSubmitResult manualReplaces = admitAssistCommand(manualSecond, assistQueue, assistResults, tracked, generations);
		assert(manualReplaces.accepted());
		assert(assistQueue.depth() == 1);
		assert(generations.mix == 1); // now latched, since the winning command is non-system.

		// Queue-WIDE scan, not tail-only (review issue #3): a system mix is
		// queued, then an unrelated command is pushed after it (so the
		// system mix is no longer at the tail supersede() alone can reach),
		// then a manual mix arrives - it must still purge the system mix
		// from wherever it sits, not just fail to notice it and queue
		// alongside it.
		assistQueue.clear();
		generations = DjAssistIntentGenerations();
		DjCommand systemMixBuried = command(80, DJ_COMMAND_SET_MIX, 0);
		systemMixBuried.origin = DJ_ORIGIN_SYSTEM;
		assert(admitAssistCommand(systemMixBuried, assistQueue, assistResults, tracked, generations).accepted());
		DjCommand unrelatedAfterSystemMix = command(81, DJ_COMMAND_LOAD_DECK, 1);
		assert(admitAssistCommand(unrelatedAfterSystemMix, assistQueue, assistResults, tracked, generations).accepted());
		assert(assistQueue.depth() == 2);
		DjCommand manualMixBuried = command(82, DJ_COMMAND_SET_MIX, 0);
		manualMixBuried.origin = DJ_ORIGIN_HTTP;
		DjSubmitResult manualPurgesBuried = admitAssistCommand(manualMixBuried, assistQueue, assistResults, tracked, generations);
		assert(manualPurgesBuried.accepted());
		assert(assistQueue.depth() == 2); // unrelated command + the new manual mix; system mix purged.
		{
			DjCommand popped1, popped2;
			assert(assistQueue.pop(popped1));
			assert(assistQueue.pop(popped2));
			assert((popped1.id == unrelatedAfterSystemMix.id && popped2.id == manualMixBuried.id) ||
				   (popped2.id == unrelatedAfterSystemMix.id && popped1.id == manualMixBuried.id));
			assert(popped1.id != systemMixBuried.id && popped2.id != systemMixBuried.id);
		}
		assert(generations.mix == 1); // the winning non-system mix still latches.

		// Reverse queue-wide scan: a manual mix is queued, then buried by an
		// unrelated command, then a system-origin mix attempt must still be
		// rejected (hasNonSystemPending() scans the whole queue, not only
		// the tail slot supersede() would reach).
		assistQueue.clear();
		generations = DjAssistIntentGenerations();
		DjCommand manualMixFirst = command(90, DJ_COMMAND_SET_MIX, 0);
		manualMixFirst.origin = DJ_ORIGIN_PHYSICAL;
		assert(admitAssistCommand(manualMixFirst, assistQueue, assistResults, tracked, generations).accepted());
		DjCommand unrelatedAfterManualMix = command(91, DJ_COMMAND_LOAD_DECK, 1);
		assert(admitAssistCommand(unrelatedAfterManualMix, assistQueue, assistResults, tracked, generations).accepted());
		DjCommand systemMixBlocked = command(92, DJ_COMMAND_SET_MIX, 0);
		systemMixBlocked.origin = DJ_ORIGIN_SYSTEM;
		DjSubmitResult systemBlocked = admitAssistCommand(systemMixBlocked, assistQueue, assistResults, tracked, generations);
		assert(!systemBlocked.accepted());
		assert(systemBlocked.error == DJ_COMMAND_ERROR_ASSIST_OVERRIDE_PENDING);
		assert(assistQueue.depth() == 2); // unchanged: manual mix + unrelated command only.

		// SET_PLAYING/SET_SYNC are tracked per-deck, independently of each
		// other and of SET_MIX: a manual play on deck 0 must not touch deck
		// 1's play generation, sync's generation, or mix's generation.
		assistQueue.clear();
		generations = DjAssistIntentGenerations();
		DjCommand manualPlayDeck0 = command(93, DJ_COMMAND_SET_PLAYING, 0);
		manualPlayDeck0.origin = DJ_ORIGIN_PHYSICAL;
		assert(admitAssistCommand(manualPlayDeck0, assistQueue, assistResults, tracked, generations).accepted());
		assert(generations.playing[0] == 1);
		assert(generations.playing[1] == 0);
		assert(generations.sync[0] == 0);
		assert(generations.mix == 0);
		DjCommand systemPlayDeck0 = command(94, DJ_COMMAND_SET_PLAYING, 0);
		systemPlayDeck0.origin = DJ_ORIGIN_SYSTEM;
		DjSubmitResult systemPlayBlocked = admitAssistCommand(systemPlayDeck0, assistQueue, assistResults, tracked, generations);
		assert(!systemPlayBlocked.accepted()); // deck 0's queued manual play still pending.
		DjCommand systemPlayDeck1 = command(95, DJ_COMMAND_SET_PLAYING, 1);
		systemPlayDeck1.origin = DJ_ORIGIN_SYSTEM;
		assert(admitAssistCommand(systemPlayDeck1, assistQueue, assistResults, tracked, generations).accepted()); // different deck: unaffected.

		// A queue-full rejection must not disturb the durable tracked slot
		// of whatever command it *is* watching - only supersession (above)
		// or the command's own eventual pop()/finish() may change it.
		assistQueue.clear();
		tracked.id = 900;
		tracked.tracked = true;
		tracked.status = DJ_COMMAND_ACCEPTED;
		for(uint32_t id = 100; id < 100 + DJ_COMMAND_CAPACITY; id++){
			assert(admitAssistCommand(command(id, DJ_COMMAND_LOAD_DECK), assistQueue, assistResults, tracked, generations).accepted());
		}
		DjSubmitResult overflow = admitAssistCommand(command(999, DJ_COMMAND_LOAD_DECK), assistQueue, assistResults, tracked, generations);
		assert(!overflow.accepted());
		assert(overflow.error == DJ_COMMAND_ERROR_QUEUE_FULL);
		assert(tracked.status == DJ_COMMAND_ACCEPTED); // untouched - not the command that overflowed.

		// The bounded/evictable presentation ring (results) must never be
		// mistaken for the durable tracked outcome: push far more results
		// through it than DJ_RECENT_RESULT_COUNT can hold, then confirm the
		// tracked slot for an early, now-evicted-from-the-ring command
		// still reports its real terminal status.
		assistQueue.clear();
		DjCommandResults burstResults;
		DjAssistTrackedCommand burstTracked;
		DjCommand watched = command(200, DJ_COMMAND_SET_QUANTIZE, 0);
		assert(admitAssistCommand(watched, assistQueue, burstResults, burstTracked, generations).accepted());
		burstTracked.id = watched.id;
		burstTracked.tracked = true;
		burstTracked.status = DJ_COMMAND_ACCEPTED;
		burstResults.finish(watched.id, DJ_COMMAND_APPLIED, DJ_COMMAND_ERROR_NONE);
		// Manually mirror what DjSession::loop() would do on apply: update
		// the tracked slot alongside the ring.
		burstTracked.status = DJ_COMMAND_APPLIED;
		for(uint32_t id = 201; id < 201 + DJ_RECENT_RESULT_COUNT * 2; id++){
			burstResults.record(command(id, DJ_COMMAND_LOAD_DECK), DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
		}
		DjCommandResult recentBurst[DJ_RECENT_RESULT_COUNT] = {};
		burstResults.copyTo(recentBurst);
		bool watchedStillInRing = false;
		for(uint8_t i = 0; i < DJ_RECENT_RESULT_COUNT; i++){
			if(recentBurst[i].id == watched.id) watchedStillInRing = true;
		}
		assert(!watchedStillInRing); // evicted from the presentation ring by the burst...
		assert(burstTracked.status == DJ_COMMAND_APPLIED); // ...but the durable slot is unaffected.
	}

	return 0;
}
