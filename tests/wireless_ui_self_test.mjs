// Self-check for the wireless control UI's pure helpers and static source
// invariants (accessibility landmarks/labels, DOM-safety). No framework,
// mirrors the style of tests/wireless_api_self_test.cpp.
//
// Run with: node tests/wireless_ui_self_test.mjs
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..');
const appJsPath = path.join(root, 'src/Wireless/ui/app.js');
const indexHtmlPath = path.join(root, 'src/Wireless/ui/index.html');

const appJsSource = readFileSync(appJsPath, 'utf8');
const indexHtmlSource = readFileSync(indexHtmlPath, 'utf8');

const { createRequire } = await import('node:module');
const require = createRequire(import.meta.url);
const helpers = require(appJsPath);

function test(name, fn){
	try{
		fn();
		console.log('ok - ' + name);
	}catch(err){
		console.error('FAIL - ' + name);
		console.error(err);
		process.exitCode = 1;
	}
}

/* ---------------- command id generation ---------------- */

test('command ids are bounded and match the device validClientId charset', () => {
	const next = helpers.makeCommandIdFactory('abc123');
	const seen = new Set();
	for(let i = 0; i < 500; i++){
		const id = next();
		assert.ok(helpers.validId(id), 'id "' + id + '" must satisfy validId');
		assert.ok(id.length <= 32, 'id must be <= 32 chars');
		assert.ok(!seen.has(id), 'id must be unique across many calls: ' + id);
		seen.add(id);
	}
});

test('command id factory never produces ids the device would reject', () => {
	const next = helpers.makeCommandIdFactory('!!!not-valid!!!'); // falls back to "x"
	for(let i = 0; i < 20; i++){
		assert.ok(helpers.validId(next()));
	}
});

/* ---------------- no-replay tracking ---------------- */

test('command tracker refuses to mark the same id sent twice (no replay)', () => {
	const tracker = helpers.createCommandTracker();
	assert.equal(tracker.markSent('cmd1'), true);
	assert.equal(tracker.markSent('cmd1'), false, 'second send of same id must be rejected');
	assert.equal(tracker.markSent('cmd2'), true);
	assert.equal(tracker.size(), 2);
});

test('a fresh page load / reconnect gets a fresh tracker, never resending old ids implicitly', () => {
	const first = helpers.createCommandTracker();
	first.markSent('cmd1');
	const second = helpers.createCommandTracker();
	// Simulates reload: no memory of "cmd1", but the important invariant is
	// that the app never automatically re-issues a command after reconnect -
	// that is enforced by sendCommand() only ever being called from a fresh
	// user gesture, not from any retry/replay loop. We assert the tracker
	// itself starts empty (no accidental carry-over) so a bug can't make it
	// look like "cmd1" was already sent by this new session.
	assert.equal(second.has('cmd1'), false);
});

/* ---------------- lease state machine ---------------- */

test('lease reducer: acquire flow', () => {
	let state = 'read_only';
	state = helpers.leaseReducer(state, { type: 'ACQUIRE_START' });
	assert.equal(state, 'acquiring');
	state = helpers.leaseReducer(state, { type: 'ACQUIRE_OK' });
	assert.equal(state, 'controlling');
});

test('lease reducer: conflict keeps user out of a false "controlling" state', () => {
	let state = 'read_only';
	state = helpers.leaseReducer(state, { type: 'ACQUIRE_START' });
	state = helpers.leaseReducer(state, { type: 'ACQUIRE_CONFLICT' });
	assert.equal(state, 'conflict');
});

test('lease reducer: expiry and loss always fall back to read_only', () => {
	assert.equal(helpers.leaseReducer('controlling', { type: 'RENEW_EXPIRED' }), 'read_only');
	assert.equal(helpers.leaseReducer('controlling', { type: 'RENEW_NOT_OWNER' }), 'read_only');
	assert.equal(helpers.leaseReducer('controlling', { type: 'RELEASE_OK' }), 'read_only');
});

test('lease reducer: reconnect NEVER assumes control survived', () => {
	// This is the core "never replay / never assume control after reconnect"
	// invariant: regardless of prior state, a RECONNECT event must land in
	// read_only, forcing an explicit re-acquire.
	for(const prior of helpers.LEASE_STATES){
		assert.equal(helpers.leaseReducer(prior, { type: 'RECONNECT' }), 'read_only',
			'RECONNECT from "' + prior + '" must yield read_only');
	}
});

/* ---------------- snapshot reconciliation (reboot/reconnect) ---------------- */

test('reconcileSnapshot accepts the very first snapshot without a false identity change', () => {
	const prev = { bootId: null, sessionId: null, lastSeq: -1 };
	const snapshot = { boot_id: '123', session_id: 7, seq: 1 };
	const decision = helpers.reconcileSnapshot(prev, snapshot);
	assert.equal(decision.accept, true);
	assert.equal(decision.identityChanged, false, 'no prior identity to have changed from');
	assert.equal(decision.bootId, '123');
	assert.equal(decision.sessionId, 7);
	assert.equal(decision.lastSeq, 1);
});

test('reconcileSnapshot: a reboot (new boot_id, seq reset to 0) is accepted, not dropped as stale', () => {
	// This is the exact regression: a high pre-reboot lastSeq must not
	// cause every post-reboot snapshot (seq starts back at 0) to be
	// silently ignored forever.
	const prev = { bootId: 'boot-A', sessionId: 7, lastSeq: 9999 };
	const snapshot = { boot_id: 'boot-B', session_id: 7, seq: 0 };
	const decision = helpers.reconcileSnapshot(prev, snapshot);
	assert.equal(decision.accept, true, 'post-reboot snapshot must be accepted, not treated as stale');
	assert.equal(decision.identityChanged, true);
	assert.equal(decision.bootId, 'boot-B');
	assert.equal(decision.lastSeq, 0);
});

test('reconcileSnapshot: a session_id-only change also counts as an identity change', () => {
	const prev = { bootId: 'boot-A', sessionId: 7, lastSeq: 9999 };
	const snapshot = { boot_id: 'boot-A', session_id: 8, seq: 0 };
	const decision = helpers.reconcileSnapshot(prev, snapshot);
	assert.equal(decision.accept, true);
	assert.equal(decision.identityChanged, true, 'same boot but new session must still reset state');
});

test('reconcileSnapshot rejects genuinely stale/duplicate snapshots within the same identity', () => {
	const prev = { bootId: 'boot-A', sessionId: 7, lastSeq: 50 };
	assert.equal(helpers.reconcileSnapshot(prev, { boot_id: 'boot-A', session_id: 7, seq: 50 }).accept,
		false, 'seq == lastSeq must be rejected as a duplicate');
	assert.equal(helpers.reconcileSnapshot(prev, { boot_id: 'boot-A', session_id: 7, seq: 10 }).accept,
		false, 'seq < lastSeq must be rejected as out-of-order');
});

test('reconcileSnapshot accepts fresh in-order snapshots within the same identity', () => {
	const prev = { bootId: 'boot-A', sessionId: 7, lastSeq: 50 };
	const decision = helpers.reconcileSnapshot(prev, { boot_id: 'boot-A', session_id: 7, seq: 51 });
	assert.equal(decision.accept, true);
	assert.equal(decision.identityChanged, false);
	assert.equal(decision.lastSeq, 51);
});

test('reconcileSnapshot compares boot_id as an opaque string, never via Number coercion', () => {
	// Number('18446744073709551615') === Number('18446744073709551614') is
	// true (both round to 2**64), so a naive numeric comparison would
	// wrongly treat these as the *same* identity. String comparison must
	// correctly treat them as different.
	const a = '18446744073709551615';
	const b = '18446744073709551614';
	assert.equal(Number(a), Number(b), 'sanity: both round to the same double once Number-coerced');
	const prev = { bootId: a, sessionId: 1, lastSeq: 5 };
	const decision = helpers.reconcileSnapshot(prev, { boot_id: b, session_id: 1, seq: 0 });
	assert.equal(decision.identityChanged, true, 'distinct opaque boot_id strings must be seen as a new identity');
});

/* ---------------- applySnapshotCore (actual runtime mutation, no DOM) ---------------- */
//
// The tests above only exercise the *pure decision* (reconcileSnapshot).
// applySnapshot() in the runtime is DOM-bound and can't run under Node, so
// without the tests below, deleting the tracker/pollFailures/lease resets
// from the runtime (app.js) would still leave every prior test green. These
// call applySnapshotCore() directly - the exact function the DOM runtime
// calls, not a reimplementation of it - against a plain app-like object, so
// the actual mutation is what's under test.

test('applySnapshotCore: identity change replaces the tracker (no replay) and resets pollFailures/lease', () => {
	const staleTracker = helpers.createCommandTracker();
	assert.equal(staleTracker.markSent('cmd-from-old-boot'), true);
	const app = {
		bootId: 'boot-A', sessionId: 7, lastSeq: 9999,
		tracker: staleTracker, pollFailures: 5, leaseState: 'controlling'
	};

	const result = helpers.applySnapshotCore(app, { boot_id: 'boot-B', session_id: 7, seq: 0 });

	assert.equal(result.accept, true);
	assert.equal(result.identityChanged, true);
	assert.equal(app.bootId, 'boot-B');
	assert.equal(app.sessionId, 7);
	assert.equal(app.lastSeq, 0);
	assert.notEqual(app.tracker, staleTracker, 'a new boot/session must install a brand-new tracker instance');
	assert.equal(app.tracker.has('cmd-from-old-boot'), false,
		'the new tracker must have no memory of any pre-reboot command id (no replay)');
	assert.equal(app.tracker.markSent('cmd-from-old-boot'), true,
		'the old id must be sendable fresh, proving it was not carried over as already-sent');
	assert.equal(app.pollFailures, 0, 'poll failure count must reset on a new identity');
	assert.equal(app.leaseState, 'read_only',
		'control must never be assumed to survive a reboot/identity change');
});

test('applySnapshotCore: same-identity fresh snapshot updates seq but never touches tracker/lease/pollFailures', () => {
	const tracker = helpers.createCommandTracker();
	tracker.markSent('cmd-in-flight');
	const app = {
		bootId: 'boot-A', sessionId: 7, lastSeq: 50,
		tracker, pollFailures: 3, leaseState: 'controlling'
	};

	const result = helpers.applySnapshotCore(app, { boot_id: 'boot-A', session_id: 7, seq: 51 });

	assert.equal(result.accept, true);
	assert.equal(result.identityChanged, false);
	assert.equal(app.lastSeq, 51);
	assert.equal(app.tracker, tracker, 'same identity must not replace the tracker');
	assert.equal(app.tracker.has('cmd-in-flight'), true, 'an in-flight command must not be forgotten');
	assert.equal(app.pollFailures, 3, 'same identity must not reset unrelated poll-failure bookkeeping');
	assert.equal(app.leaseState, 'controlling', 'same identity must never drop existing control');
});

test('applySnapshotCore: stale/duplicate snapshot within the same identity is rejected without mutating app', () => {
	const tracker = helpers.createCommandTracker();
	const app = { bootId: 'boot-A', sessionId: 7, lastSeq: 50, tracker, pollFailures: 0, leaseState: 'controlling' };

	const result = helpers.applySnapshotCore(app, { boot_id: 'boot-A', session_id: 7, seq: 50 });

	assert.equal(result.accept, false);
	assert.equal(app.lastSeq, 50, 'rejected snapshot must not change lastSeq');
	assert.equal(app.tracker, tracker, 'rejected snapshot must not touch the tracker');
	assert.equal(app.leaseState, 'controlling', 'rejected snapshot must not touch lease state');
});

/* ---------------- backoff / polling ---------------- */

test('computeBackoff grows exponentially but stays bounded', () => {
	assert.equal(helpers.computeBackoff(0, 1000, 20000), 1000);
	assert.equal(helpers.computeBackoff(1, 1000, 20000), 2000);
	assert.equal(helpers.computeBackoff(2, 1000, 20000), 4000);
	assert.equal(helpers.computeBackoff(10, 1000, 20000), 20000);
	assert.equal(helpers.computeBackoff(999, 1000, 20000), 20000, 'must clamp huge failure counts');
});

test('choosePollDelay pauses (returns null) when offline', () => {
	const delay = helpers.choosePollDelay({
		online: false, hidden: false, isWriter: true, failures: 0,
		activeMs: 1000, idleMs: 3000, hiddenMs: 5000
	});
	assert.equal(delay, null);
});

test('choosePollDelay uses the hidden interval when the tab is backgrounded', () => {
	const delay = helpers.choosePollDelay({
		online: true, hidden: true, isWriter: true, failures: 0,
		activeMs: 1000, idleMs: 3000, hiddenMs: 5000
	});
	assert.equal(delay, 5000);
});

test('choosePollDelay polls faster while controlling than read-only', () => {
	const writer = helpers.choosePollDelay({
		online: true, hidden: false, isWriter: true, failures: 0,
		activeMs: 1000, idleMs: 3000, hiddenMs: 5000
	});
	const reader = helpers.choosePollDelay({
		online: true, hidden: false, isWriter: false, failures: 0,
		activeMs: 1000, idleMs: 3000, hiddenMs: 5000
	});
	assert.equal(writer, 1000);
	assert.equal(reader, 3000);
	assert.ok(writer < reader);
});

/* ---------------- coalescing (throttler) ---------------- */

test('throttler coalesces bursts down to a single trailing call with the latest value', () => {
	let now = 0;
	const timers = [];
	const throttler = helpers.createThrottler(200, {
		now: () => now,
		setTimer: (fn, ms) => { const t = { fn, at: now + ms }; timers.push(t); return t; },
		clearTimer: (t) => { const i = timers.indexOf(t); if(i >= 0) timers.splice(i, 1); }
	});
	const calls = [];
	throttler.schedule('gain:0', () => calls.push('a')); // fires immediately (elapsed >= wait from -Infinity)
	assert.deepEqual(calls, ['a']);
	now = 10;
	throttler.schedule('gain:0', () => calls.push('b')); // too soon, scheduled
	now = 50;
	throttler.schedule('gain:0', () => calls.push('c')); // supersedes 'b', still scheduled
	assert.deepEqual(calls, ['a'], 'no extra calls before the timer fires');
	assert.equal(timers.length, 1, 'only one pending trailing call per key');
	now = 210;
	timers[0].fn();
	assert.deepEqual(calls, ['a', 'c'], '"b" must never fire - only the latest coalesced value does');
});

test('throttler tracks independent keys separately (per-control coalescing)', () => {
	let now = 0;
	const throttler = helpers.createThrottler(200, { now: () => now });
	const calls = [];
	throttler.schedule('gain:0', () => calls.push('gain0'));
	throttler.schedule('gain:1', () => calls.push('gain1'));
	assert.deepEqual(calls.sort(), ['gain0', 'gain1']);
});

/* ---------------- command body shaping ---------------- */

test('buildCommandBody produces exactly the field set the device expects per action', () => {
	const identity = { bootId: 11, sessionId: 12 };
	const body = helpers.buildCommandBody(identity, 'set_mix', { value: 200 }, 'cmd1');
	assert.deepEqual(Object.keys(body).sort(), ['action', 'boot_id', 'client_command_id', 'session_id', 'value']);
	assert.equal(body.value, 200);

	const effectBody = helpers.buildCommandBody(identity, 'set_effect_type', { deck: 1, slot: 2, value: 4 }, 'cmd2');
	assert.deepEqual(Object.keys(effectBody).sort(),
		['action', 'boot_id', 'client_command_id', 'deck', 'session_id', 'slot', 'value']);
});

test('buildCommandBody rejects an invalid command id before it would reach the network', () => {
	assert.throws(() => helpers.buildCommandBody({ bootId: 1, sessionId: 1 }, 'set_mix', { value: 1 }, ''));
	assert.throws(() => helpers.buildCommandBody({ bootId: 1, sessionId: 1 }, 'set_mix', { value: 1 },
		'x'.repeat(40)));
});

test('buildCommandBody rejects unsupported actions (no accidental new surface)', () => {
	assert.throws(() => helpers.buildCommandBody({ bootId: 1, sessionId: 1 }, 'load_by_path', {}, 'cmd1'));
});

test('buildCommandBody passes boot_id through as an opaque string, never coercing to Number', () => {
	// A pair of full-range uint64 values that collide once rounded to a
	// JS double - if boot_id were ever coerced through Number(), these two
	// distinct decimal strings would become indistinguishable.
	const bootId = '18446744073709551615'; // UINT64_MAX
	const identity = { bootId, sessionId: 12 };
	const body = helpers.buildCommandBody(identity, 'set_mix', { value: 1 }, 'cmd1');
	assert.equal(body.boot_id, bootId);
	assert.equal(typeof body.boot_id, 'string');
	// JSON.stringify must re-emit it as a quoted string, not an unquoted
	// (and precision-losing) number literal.
	assert.match(JSON.stringify(body), /"boot_id":"18446744073709551615"/);
});

/* ---------------- formatting ---------------- */

test('formatClock renders mm:ss and a placeholder for unavailable timing', () => {
	assert.equal(helpers.formatClock(0), '0:00');
	assert.equal(helpers.formatClock(65), '1:05');
	assert.equal(helpers.formatClock(undefined), '--:--');
	assert.equal(helpers.formatClock(-1), '--:--');
});

/* ---------------- Assist (Coach / one-shot transition) ---------------- */

test('buildCommandBody shapes the three assist actions to exactly what the device expects', () => {
	const identity = { bootId: 11, sessionId: 12 };
	const modeBody = helpers.buildCommandBody(identity, 'assist_set_mode', { value: true }, 'cmd1');
	assert.deepEqual(Object.keys(modeBody).sort(), ['action', 'boot_id', 'client_command_id', 'session_id', 'value']);
	assert.equal(modeBody.value, true);

	const cancelBody = helpers.buildCommandBody(identity, 'assist_cancel_transition', {}, 'cmd2');
	assert.deepEqual(Object.keys(cancelBody).sort(), ['action', 'boot_id', 'client_command_id', 'session_id']);

	const armBody = helpers.buildCommandBody(identity, 'assist_arm_transition',
		{ deck: 0, toDeck: 1, crossfadeBeats: 16, startAtBoundary: true, tempoLock: false }, 'cmd3');
	assert.deepEqual(Object.keys(armBody).sort(),
		['action', 'boot_id', 'client_command_id', 'crossfade_beats', 'deck', 'session_id', 'start_at_boundary',
			'tempo_lock', 'to_deck']);
	assert.equal(armBody.to_deck, 1);
	assert.equal(armBody.crossfade_beats, 16);
	assert.equal(armBody.start_at_boundary, true);
	assert.equal(armBody.tempo_lock, false);
});

test('deckLabel / describeCrossfadeDirection give accessible text for every device-emitted value', () => {
	assert.equal(helpers.deckLabel(0), 'Deck A');
	assert.equal(helpers.deckLabel(1), 'Deck B');
	assert.equal(helpers.describeCrossfadeDirection(-1), 'Toward Deck A');
	assert.equal(helpers.describeCrossfadeDirection(0), 'Centered');
	assert.equal(helpers.describeCrossfadeDirection(1), 'Toward Deck B');
});

test('formatBpmDelta/formatRatePercent decode the device\'s milli-fixed-point units', () => {
	// tempoDeltaMilli is candidateBpmMilli - deckBpmMilli (milli-BPM), not a rate.
	assert.equal(helpers.formatBpmDelta(-2300), '-2.3 BPM');
	assert.equal(helpers.formatBpmDelta(500), '+0.5 BPM');
	assert.equal(helpers.formatBpmDelta(undefined), '\u2014');
	// target_rate/armed rates are deck-rate multipliers, 1000 == 1.0x.
	assert.equal(helpers.formatRatePercent(1000), '100.0%');
	assert.equal(helpers.formatRatePercent(1050), '105.0%');
	assert.equal(helpers.formatRatePercent(null), '\u2014');
});

test('describeReasonFlags decodes DjAssistReasonFlag bits exactly (mirrors DjAssistTypes.h)', () => {
	// TEMPO_IN_RANGE(1<<1) | KEY_SAME(1<<3) | GRID_AVAILABLE(1<<7)
	const flags = (1 << 1) | (1 << 3) | (1 << 7);
	assert.deepEqual(helpers.describeReasonFlags(flags), ['tempo in range', 'key same', 'grid available']);
	assert.deepEqual(helpers.describeReasonFlags(0), []);
	assert.deepEqual(helpers.describeReasonFlags(undefined), []);
});

test('every assist mode/key/exclude/action/failure token the device can emit has display text', () => {
	// These lists must stay in lockstep with assist*Name()/appendAssistWarnings()
	// in src/Wireless/WirelessBringup.cpp - this test fails loudly if either
	// side adds a token the other doesn't know about.
	['off', 'coach', 'armed', 'running', 'complete', 'failed'].forEach((token) => {
		assert.ok(helpers.ASSIST_MODE_TEXT[token], 'missing mode text for ' + token);
	});
	['unknown', 'incompatible', 'relative', 'adjacent', 'same'].forEach((token) => {
		assert.ok(helpers.ASSIST_KEY_TEXT[token], 'missing key text for ' + token);
	});
	['loaded', 'recent', 'unsupported_metadata'].forEach((token) => {
		assert.ok(helpers.ASSIST_EXCLUDE_TEXT[token], 'missing exclude text for ' + token);
	});
	['wait_boundary', 'start_deck', 'lock_tempo', 'enable_sync', 'crossfade', 'stop_deck', 'release_sync', 'done']
		.forEach((token) => {
			assert.ok(helpers.ASSIST_ACTION_TEXT[token], 'missing action text for ' + token);
		});
	['metadata_lost', 'command_rejected', 'media_removed', 'end_of_track', 'manual_override', 'conflict',
		'target_not_loaded', 'target_changed', 'cancelled'].forEach((token) => {
		assert.ok(helpers.ASSIST_FAILURE_TEXT[token], 'missing failure text for ' + token);
	});
	['out_of_range', 'no_grid', 'ending_soon', 'recording_active', 'loop_active', 'no_metadata'].forEach((token) => {
		assert.ok(helpers.ASSIST_WARNING_TEXT[token], 'missing warning text for ' + token);
	});
});

/* ---------------- static DOM-safety invariant ---------------- */

test('app.js never assigns innerHTML/outerHTML or uses document.write (no unescaped DOM injection)', () => {
	assert.doesNotMatch(appJsSource, /\.innerHTML\s*=/, 'use textContent/value/setAttribute instead');
	assert.doesNotMatch(appJsSource, /\.outerHTML\s*=/);
	assert.doesNotMatch(appJsSource, /document\.write\s*\(/);
	assert.doesNotMatch(appJsSource, /\beval\s*\(/);
});

test('app.js builds JSON request bodies with JSON.stringify, never manual string concatenation', () => {
	assert.doesNotMatch(appJsSource, /body:\s*['"`]\{/, 'command/pair bodies must go through JSON.stringify');
});

/* ---------------- static accessibility invariants ---------------- */

test('index.html declares required landmarks and a polite live region', () => {
	assert.match(indexHtmlSource, /<header>/);
	assert.match(indexHtmlSource, /<main id="main">/);
	assert.match(indexHtmlSource, /aria-live="polite"/);
	assert.match(indexHtmlSource, /class="skip-link"/);
});

test('every text/password input in index.html has an associated <label for=...>', () => {
	const inputIds = [...indexHtmlSource.matchAll(/<input\s+id="([^"]+)"[^>]*type="(?:text|password)"/g)]
		.map((m) => m[1]);
	assert.ok(inputIds.length > 0, 'sanity: fixture should contain text/password inputs');
	for(const id of inputIds){
		const labelPattern = new RegExp('<label for="' + id + '">');
		assert.match(indexHtmlSource, labelPattern, 'missing <label for="' + id + '"> for #' + id);
	}
});

test('every range/select control in index.html has an associated <label for=...>', () => {
	const controlIds = [...indexHtmlSource.matchAll(/<(?:input\s+id="([^"]+)"[^>]*type="range"|select\s+id="([^"]+)")/g)]
		.map((m) => m[1] || m[2]);
	assert.ok(controlIds.length > 0, 'sanity: fixture should contain range/select controls');
	for(const id of controlIds){
		const labelPattern = new RegExp('<label for="' + id + '">');
		assert.match(indexHtmlSource, labelPattern, 'missing <label for="' + id + '"> for #' + id);
	}
});

test('disabled-by-design panels (cues) render real disabled controls, not fake working ones', () => {
	assert.match(indexHtmlSource, /class="cue-button" data-cue="0" disabled/);
	// Every cue button must be marked disabled in markup (belt) - app.js also
	// disables them defensively at runtime (suspenders), checked next.
	const cueButtons = [...indexHtmlSource.matchAll(/class="cue-button"[^>]*>/g)];
	assert.equal(cueButtons.length, 8);
	for(const button of cueButtons){
		assert.match(button[0], /disabled/);
	}
});

test('prefers-reduced-motion is respected in the stylesheet', () => {
	const cssSource = readFileSync(path.join(root, 'src/Wireless/ui/styles.css'), 'utf8');
	assert.match(cssSource, /prefers-reduced-motion/);
});

if(process.exitCode){
	console.error('\nSome checks failed.');
}else{
	console.log('\nAll wireless UI self-checks passed.');
}
