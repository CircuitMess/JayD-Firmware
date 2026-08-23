/*
 * Jay-D wireless control UI.
 * Vanilla JS, no framework, no external requests. Talks only to the on-device
 * v2 semantic API (see src/Wireless/WirelessApiCore.h / WirelessBringup.cpp).
 *
 * The pure helpers below have no DOM/network dependency and are exported for
 * tests/wireless_ui_self_test.mjs. The browser runtime section is guarded so
 * this file loads harmlessly under Node for that test.
 */
(function(){
'use strict';

/* ---------------------------------------------------------------------- *
 * Pure helpers (unit tested)
 * ---------------------------------------------------------------------- */

// Mirrors WirelessApi::validClientId / validClientCommandId (alnum, - and _,
// 1-32 chars). Command ids and client ids must satisfy this on the device.
function validId(value){
	return typeof value === 'string' && value.length > 0 && value.length <= 32 &&
		/^[A-Za-z0-9_-]+$/.test(value);
}

// Bounded, collision-resistant client command id. `nonce` should be a short
// per-page-load random string so ids from different tabs/reloads don't
// collide even if the clock/counter repeat.
function makeCommandIdFactory(nonce, nowFn){
	nonce = validId(nonce) ? nonce : 'x';
	let counter = 0;
	const now = nowFn || (() => Date.now());
	return function nextCommandId(){
		const stamp = Math.floor(now()).toString(36);
		const seq = (counter++ % 46656).toString(36); // bounded, wraps forever
		const raw = 'c' + nonce + stamp + seq;
		const id = raw.slice(0, 32);
		return validId(id) ? id : 'c' + seq.slice(0, 31);
	};
}

// Exponential backoff, bounded to maxMs. `failures` is clamped so the
// exponent can never overflow or grow unbounded.
function computeBackoff(failures, baseMs, maxMs){
	const capped = Math.max(0, Math.min(failures | 0, 10));
	return Math.min(maxMs, baseMs * Math.pow(2, capped));
}

// Decide the next poll delay, or null to mean "do not schedule, wait for an
// event (online/visibilitychange) to resume".
function choosePollDelay(opts){
	if(!opts.online) return null;
	if(opts.hidden) return opts.hiddenMs;
	const base = opts.isWriter ? opts.activeMs : opts.idleMs;
	if(opts.failures > 0) return computeBackoff(opts.failures, base, opts.hiddenMs * 4);
	return base;
}

// Tracks which client_command_ids have been sent so a reconnect / retry path
// can never silently replay one. `markSent` returns false on a duplicate.
function createCommandTracker(){
	const sent = new Set();
	return {
		markSent(id){
			if(sent.has(id)) return false;
			sent.add(id);
			return true;
		},
		has(id){
			return sent.has(id);
		},
		size(){
			return sent.size;
		}
	};
}

// Writer-lease state machine. Never assumes control survives a reconnect:
// RECONNECT always drops to read_only, requiring an explicit re-acquire.
const LEASE_STATES = ['read_only', 'acquiring', 'controlling', 'conflict'];
function leaseReducer(state, event){
	switch(event && event.type){
		case 'ACQUIRE_START': return 'acquiring';
		case 'ACQUIRE_OK': return 'controlling';
		case 'ACQUIRE_CONFLICT': return 'conflict';
		case 'RENEW_OK': return state === 'controlling' ? 'controlling' : state;
		case 'RENEW_EXPIRED': return 'read_only';
		case 'RENEW_NOT_OWNER': return 'read_only';
		case 'RELEASE_OK': return 'read_only';
		case 'RECONNECT': return 'read_only';
		default: return state;
	}
}

// Trailing-edge throttle per key, with an injectable clock/timer so it is
// deterministically testable. Only ever keeps the *latest* call per key -
// this is the "coalescing" for continuous controls (gain/mix/intensity).
function createThrottler(waitMs, deps){
	const now = (deps && deps.now) || (() => Date.now());
	const setTimer = (deps && deps.setTimer) || ((fn, ms) => setTimeout(fn, ms));
	const clearTimer = (deps && deps.clearTimer) || ((id) => clearTimeout(id));
	const lastRun = new Map();
	const pending = new Map();
	return {
		schedule(key, fn){
			const t = now();
			const last = lastRun.has(key) ? lastRun.get(key) : -Infinity;
			const elapsed = t - last;
			const existing = pending.get(key);
			if(existing !== undefined){
				clearTimer(existing);
				pending.delete(key);
			}
			if(elapsed >= waitMs){
				lastRun.set(key, t);
				fn();
				return;
			}
			const timer = setTimer(() => {
				pending.delete(key);
				lastRun.set(key, now());
				fn();
			}, waitMs - elapsed);
			pending.set(key, timer);
		}
	};
}

// Exact field shape per action, mirroring handleCommand()'s expectedFields
// switch in WirelessBringup.cpp. Keeping this table in one place avoids ever
// sending an action with the wrong field set (which the device rejects).
const COMMAND_SHAPE = {
	set_playing: (f) => ({ deck: f.deck, value: !!f.value }),
	seek: (f) => ({ deck: f.deck, value: f.value }),
	set_gain: (f) => ({ deck: f.deck, value: f.value }),
	set_mix: (f) => ({ value: f.value }),
	set_effect_type: (f) => ({ deck: f.deck, slot: f.slot, value: f.value }),
	set_effect_intensity: (f) => ({ deck: f.deck, slot: f.slot, value: f.value }),
	set_recording: (f) => ({ value: !!f.value }),
	assist_set_mode: (f) => ({ value: !!f.value }),
	assist_cancel_transition: () => ({}),
	assist_arm_transition: (f) => ({
		deck: f.deck,
		to_deck: f.toDeck,
		crossfade_beats: f.crossfadeBeats,
		start_at_boundary: !!f.startAtBoundary,
		tempo_lock: !!f.tempoLock
	})
};

// Pure snapshot-acceptance state machine, factored out of applySnapshot()
// so the two hardest invariants around a device reboot are directly unit
// tested: (1) a post-reboot snapshot is never mistaken for stale/duplicate
// data, and (2) nothing about accepting it ever replays a command.
//
// `prev` is `{ bootId, sessionId, lastSeq }` - the subset of `app` this
// decision depends on. Returns `{ accept: false }` when the snapshot must
// be ignored outright (stale/out-of-order within the *same* identity), or
// `{ accept: true, identityChanged, bootId, sessionId, lastSeq }` describing
// the state to adopt.
//
// Identity is compared strictly before sequence. The device's monotonic
// seq counter resets to 0 on every boot, which is numerically "stale"
// against whatever high lastSeq the browser saw before a restart; checking
// seq first would silently ignore every snapshot from the new boot
// forever, freezing the UI on pre-restart state and never running the
// lease/reconnect reset. So an identity change first resets the effective
// lastSeq to "none seen yet", then the sequence guard is applied on top of
// that reset baseline (still rejecting genuinely stale/duplicate snapshots
// within the same identity).
//
// boot_id is compared with strict string equality only. It travels the
// wire as an opaque decimal string (see buildCommandBody / WirelessBringup
// handleState) specifically so it can hold the full random 64-bit range
// without ever being Number-coerced - a bare JSON number would lose
// precision above 2^53 in the browser and could falsely appear unchanged
// (or falsely appear changed) after rounding.
function reconcileSnapshot(prev, snapshot){
	const bootChanged = prev.bootId !== null && snapshot.boot_id !== prev.bootId;
	const sessionChanged = prev.sessionId !== null && snapshot.session_id !== prev.sessionId;
	const identityChanged = bootChanged || sessionChanged;
	const effectiveLastSeq = identityChanged ? -1 : prev.lastSeq;
	const stale = typeof snapshot.seq === 'number' &&
		effectiveLastSeq !== -1 && snapshot.seq <= effectiveLastSeq;
	if(stale) return { accept: false, identityChanged: false };
	return {
		accept: true,
		identityChanged,
		bootId: snapshot.boot_id,
		sessionId: snapshot.session_id,
		lastSeq: snapshot.seq
	};
}

// The actual, DOM-independent half of applySnapshot(): decides via
// reconcileSnapshot() whether to accept the snapshot, and if so mutates the
// given app-like object's identity/tracker/lease bookkeeping exactly as the
// real runtime does. Split out purely so this mutation - not just the pure
// decision above it - has a direct unit test; the runtime's applySnapshot()
// calls this same function rather than re-implementing it, so deleting or
// reordering the identity-change resets fails a test even though they touch
// no DOM. Only the visible rendering (deck/mixer/status text, lease-state
// DOM update) stays in the DOM-only applySnapshot() below, since that part
// can't run without a document and isn't itself a correctness invariant here.
//
// `app` must have `bootId`, `sessionId`, `lastSeq`, `tracker`, `pollFailures`
// and `leaseState`; all but `tracker`/`leaseState` are read, and all are
// (re)written in place when the snapshot is accepted. Returns
// `{ accept, identityChanged }`.
function applySnapshotCore(app, snapshot){
	const decision = reconcileSnapshot(
		{ bootId: app.bootId, sessionId: app.sessionId, lastSeq: app.lastSeq },
		snapshot
	);
	if(!decision.accept) return { accept: false, identityChanged: false };
	app.lastSeq = decision.lastSeq;
	app.bootId = decision.bootId;
	app.sessionId = decision.sessionId;
	if(decision.identityChanged){
		// New boot/session identity: any in-flight or tracked command belongs
		// to a session the device no longer recognizes. Reset local
		// bookkeeping and drop to read-only rather than ever re-sending
		// anything against the new identity.
		app.tracker = createCommandTracker();
		app.pollFailures = 0;
		app.leaseState = leaseReducer(app.leaseState, { type: 'RECONNECT' });
	}
	return { accept: true, identityChanged: decision.identityChanged };
}

function buildCommandBody(identity, action, fields, commandId){
	const shape = COMMAND_SHAPE[action];
	if(!shape) throw new Error('unsupported_action: ' + action);
	if(!validId(commandId)) throw new Error('invalid_command_id');
	return Object.assign({
		boot_id: identity.bootId,
		session_id: identity.sessionId,
		client_command_id: commandId,
		action: action
	}, shape(fields));
}

const EFFECT_NAMES = ['None', 'Speed', 'Low-pass', 'High-pass', 'Reverb', 'Bitcrusher'];

function formatClock(seconds){
	if(typeof seconds !== 'number' || !isFinite(seconds) || seconds < 0) return '--:--';
	const total = Math.floor(seconds);
	const m = Math.floor(total / 60);
	const s = total % 60;
	return m + ':' + (s < 10 ? '0' : '') + s;
}

/* ---------------------------------------------------------------------- *
 * Assist (Coach / one-shot transition) text mapping - pure and testable.
 * These decode the short snake_case tokens the device emits in
 * `/api/v2/state`'s "assist" block (see WirelessBringup.cpp's
 * assist*Name()/appendAssistWarnings() for the authoritative token set)
 * into accessible, human-readable text. Unknown tokens fall back to the
 * raw token itself rather than throwing, since the device is always the
 * source of truth and a forward-compatible client shouldn't hard-fail on
 * an unrecognized-but-harmless new token.
 * ---------------------------------------------------------------------- */
const ASSIST_MODE_TEXT = {
	off: 'Off', coach: 'Coach', armed: 'Transition armed', running: 'Transition running',
	complete: 'Transition complete', failed: 'Transition failed'
};
const ASSIST_BOUNDARY_TEXT = { phrase: 'Next phrase', beat: 'Next downbeat', none: 'No grid available' };
const ASSIST_WARNING_TEXT = {
	out_of_range: 'Target tempo is outside the safe deck-rate range.',
	no_grid: 'No beat grid available for a safe boundary.',
	ending_soon: 'The current track is ending too soon for this transition.',
	recording_active: 'Recording is active; some actions are blocked.',
	loop_active: 'A loop is active on a deck involved in this transition.',
	no_metadata: 'Track metadata is missing or unreliable.'
};
const ASSIST_KEY_TEXT = {
	unknown: 'Unknown', incompatible: 'Incompatible', relative: 'Relative key',
	adjacent: 'Adjacent key', same: 'Same key'
};
const ASSIST_EXCLUDE_TEXT = { loaded: 'Already loaded', recent: 'Recently played', unsupported_metadata: 'Metadata unsupported' };
const ASSIST_ACTION_TEXT = {
	wait_boundary: 'Waiting for boundary', start_deck: 'Starting target deck', lock_tempo: 'Locking tempo',
	enable_sync: 'Enabling Sync', crossfade: 'Crossfading', stop_deck: 'Stopping old deck',
	release_sync: 'Releasing Sync', done: 'Done'
};
const ASSIST_FAILURE_TEXT = {
	metadata_lost: 'Metadata was lost mid-transition.', command_rejected: 'A command was rejected.',
	media_removed: 'Media was removed.', end_of_track: 'A deck reached end of track.',
	manual_override: 'Manual control overrode the transition.', conflict: 'A loop/recording conflict occurred.',
	target_not_loaded: 'The target track is no longer loaded.', target_changed: 'The target track changed.',
	cancelled: 'Cancelled.'
};
// Mirrors DjAssistReasonFlag's bit layout exactly (DjAssistTypes.h). Kept in
// sync manually since this is a small, stable, already-shipped bit layout.
const ASSIST_REASON_FLAGS = [
	[1 << 0, 'tempo narrow'], [1 << 1, 'tempo in range'], [1 << 2, 'tempo out of range'],
	[1 << 3, 'key same'], [1 << 4, 'key adjacent'], [1 << 5, 'key relative'], [1 << 6, 'key unknown'],
	[1 << 7, 'grid available'], [1 << 8, 'phrase available'], [1 << 9, 'rating known'],
	[1 << 10, 'duration short'], [1 << 11, 'low confidence']
];

function deckLabel(index){
	if(index === 0) return 'Deck A';
	if(index === 1) return 'Deck B';
	return 'Deck ' + index;
}

function describeCrossfadeDirection(dir){
	if(dir === -1) return 'Toward Deck A';
	if(dir === 1) return 'Toward Deck B';
	if(dir === 0) return 'Centered';
	return '\u2014';
}

// tempoDeltaMilli is candidateBpmMilli - deckBpmMilli (see DjAssistScoring.cpp),
// i.e. milli-BPM, not a rate ratio.
function formatBpmDelta(milli){
	if(typeof milli !== 'number' || !isFinite(milli)) return '\u2014';
	const bpm = milli / 1000;
	return (bpm > 0 ? '+' : '') + bpm.toFixed(1) + ' BPM';
}

// target_rate/armed rates are deck-rate multipliers where 1000 == 1.0x (unity).
function formatRatePercent(milli){
	if(typeof milli !== 'number' || !isFinite(milli)) return '\u2014';
	return (milli / 1000 * 100).toFixed(1) + '%';
}

function describeReasonFlags(flags){
	if(typeof flags !== 'number') return [];
	return ASSIST_REASON_FLAGS.filter(([bit]) => (flags & bit) !== 0).map(([, text]) => text);
}

const helpers = {
	validId,
	makeCommandIdFactory,
	computeBackoff,
	choosePollDelay,
	createCommandTracker,
	leaseReducer,
	LEASE_STATES,
	createThrottler,
	reconcileSnapshot,
	applySnapshotCore,
	buildCommandBody,
	COMMAND_SHAPE,
	EFFECT_NAMES,
	formatClock,
	ASSIST_MODE_TEXT,
	ASSIST_BOUNDARY_TEXT,
	ASSIST_WARNING_TEXT,
	ASSIST_KEY_TEXT,
	ASSIST_EXCLUDE_TEXT,
	ASSIST_ACTION_TEXT,
	ASSIST_FAILURE_TEXT,
	deckLabel,
	describeCrossfadeDirection,
	formatBpmDelta,
	formatRatePercent,
	describeReasonFlags
};

if(typeof module !== 'undefined' && module.exports){
	module.exports = helpers;
}

/* ---------------------------------------------------------------------- *
 * Browser runtime (no-op outside a DOM; not exercised by the Node test)
 * ---------------------------------------------------------------------- */
if(typeof document !== 'undefined'){
	(function runtime(){
		const STORAGE_CLIENT_ID = 'jayd_client_id';
		const STORAGE_TOKEN = 'jayd_token';
		const DECK_COUNT = 2;
		const EFFECT_SLOTS = 3;
		const CUE_COUNT = 8;

		let defaults = {
			activeMs: 1000,
			idleMs: 3000,
			hiddenMs: 5000,
			leaseMs: 15000
		};

		const $ = (id) => document.getElementById(id);

		const live = { region: null };
		function announce(text){
			if(!live.region) return;
			// Force SR re-announcement of repeated text via a toggled marker.
			live.region.textContent = '';
			window.setTimeout(() => { live.region.textContent = text; }, 30);
		}

		function randomNonce(){
			const bytes = new Uint8Array(6);
			(window.crypto || {}).getRandomValues ? window.crypto.getRandomValues(bytes) :
				bytes.forEach((_, i) => { bytes[i] = Math.floor(Math.random() * 256); });
			let out = '';
			for(const b of bytes) out += (b % 36).toString(36);
			return out;
		}

		function loadIdentity(){
			let clientId = null;
			let token = null;
			try{
				clientId = window.localStorage.getItem(STORAGE_CLIENT_ID);
				token = window.localStorage.getItem(STORAGE_TOKEN);
			}catch(e){ /* storage unavailable (private mode) - proceed unpaired */ }
			if(!validId(clientId)){
				clientId = 'web-' + randomNonce() + randomNonce();
				try{ window.localStorage.setItem(STORAGE_CLIENT_ID, clientId); }catch(e){}
			}
			return { clientId, token: (typeof token === 'string' && token.length > 0) ? token : null };
		}

		function saveToken(token){
			try{ window.localStorage.setItem(STORAGE_TOKEN, token); }catch(e){}
		}

		function forgetDevice(){
			try{
				window.localStorage.removeItem(STORAGE_TOKEN);
				window.localStorage.removeItem(STORAGE_CLIENT_ID);
			}catch(e){}
		}

		const app = {
			identity: loadIdentity(),
			bootId: null,
			sessionId: null,
			lastSeq: -1,
			leaseState: 'read_only',
			pollFailures: 0,
			pollTimer: null,
			pollController: null,
			pollInFlight: false,
			tracker: createCommandTracker(),
			throttle: createThrottler(200),
			nextCommandId: null,
			mode: 'boot'
		};
		app.nextCommandId = makeCommandIdFactory(app.identity.clientId.replace(/[^A-Za-z0-9]/g, '').slice(0, 8) || 'x');

		function authHeaders(extra){
			const headers = Object.assign({}, extra || {});
			if(app.identity.token) headers['Authorization'] = 'Bearer ' + app.identity.token;
			return headers;
		}

		async function api(path, options){
			const response = await fetch(path, Object.assign({}, options, {
				headers: authHeaders((options && options.headers) || {})
			}));
			let body = null;
			const text = await response.text();
			if(text){
				try{ body = JSON.parse(text); }
				catch(e){ throw Object.assign(new Error('malformed_response'), { response, malformed: true }); }
			}
			return { response, body };
		}

		function showSection(name){
			app.mode = name;
			['setup', 'pairing', 'app'].forEach((section) => {
				const el = $(section + '-section');
				if(el) el.hidden = section !== name;
			});
		}

		/* ---------------- Setup (WiFi provisioning) ---------------- */

		async function checkSetupMode(){
			try{
				const { response, body } = await api('/setup');
				if(response.status === 200 && body && body.mode === 'setup'){
					renderSetup(body);
					showSection('setup');
					return true;
				}
			}catch(e){ /* fall through to normal flow */ }
			return false;
		}

		function renderSetup(status){
			const ssidEl = $('setup-ssid');
			if(ssidEl) ssidEl.textContent = status.ssid || 'Jay-D';
			const form = $('setup-form');
			if(form && !form.dataset.bound){
				form.dataset.bound = '1';
				form.addEventListener('submit', onSetupSubmit);
			}
		}

		async function onSetupSubmit(event){
			event.preventDefault();
			const ssid = $('setup-ssid-input').value;
			const password = $('setup-password-input').value;
			const status = $('setup-status');
			status.textContent = 'Saving Wi-Fi details…';
			try{
				const { response, body } = await api('/setup/wifi', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify({ ssid, password })
				});
				if(response.status === 202){
					status.textContent = 'Saved. The device is restarting and will join your Wi-Fi network. ' +
						'Reconnect your browser to that network and reopen this page.';
				}else{
					status.textContent = describeError(body) || 'Could not save Wi-Fi details.';
				}
			}catch(e){
				status.textContent = 'Could not reach the device. Check you are still connected to its Wi-Fi network.';
			}
		}

		/* ---------------- Pairing ---------------- */

		function describeError(body){
			const code = body && body.error;
			switch(code){
				case 'pairing_window_closed': return 'Pairing window closed. Start pairing again on the device, then retry within 60 seconds.';
				case 'pairing_code_invalid': return 'That code was not accepted. Check the device screen and try again.';
				case 'client_id_invalid': return 'This browser\u2019s device id is invalid. Reloading may help.';
				case 'paired_client_limit': return 'This device already has the maximum number of paired browsers. Forget one on the device and retry.';
				case 'rate_limited': return 'Too many attempts. Wait about a minute and try again.';
				case 'origin_rejected': return 'This page must be opened directly from the device, not through a proxy or embed.';
				case 'unauthorized': return 'Pairing has expired. Please pair again.';
				case 'writer_lease_conflict': return 'Another browser currently has control.';
				case 'writer_lease_expired': return 'Control expired. Take control again to continue.';
				case 'writer_lease_not_owner': return 'This browser no longer has control.';
				case 'writer_lease_required': return 'Take control before sending commands.';
				case 'session_unavailable': return 'The DJ session is not running on the device right now.';
				case 'control_unavailable_in_setup_mode': return 'The device is in Wi-Fi setup mode; DJ control is unavailable until setup finishes.';
				case 'queue_full': return 'The device is busy. Try again in a moment.';
				case 'stale_identity': return 'Your control session is out of date \u2014 the device restarted or another browser took over. Take control again to continue.';
				case 'assist_rejected': return 'The device rejected that Assist request \u2014 check the current mode and that the target deck has a track loaded.';
				default: return code ? ('Device error: ' + code) : null;
			}
		}

		async function onPairingSubmit(event){
			event.preventDefault();
			const code = $('pairing-code-input').value.trim();
			const status = $('pairing-status');
			status.textContent = 'Pairing…';
			try{
				const { response, body } = await api('/api/v2/pair', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify({ code, client_id: app.identity.clientId })
				});
				if(response.status === 201 && body && body.token){
					app.identity.token = body.token;
					saveToken(body.token);
					status.textContent = '';
					await enterApp();
				}else{
					status.textContent = describeError(body) || 'Pairing failed. Try again.';
				}
			}catch(e){
				status.textContent = e && e.malformed ?
					'The device sent an unreadable response. Try again.' :
					'Could not reach the device. Check you are on its Wi-Fi network.';
			}
		}

		function renderPairing(){
			const form = $('pairing-form');
			if(form && !form.dataset.bound){
				form.dataset.bound = '1';
				form.addEventListener('submit', onPairingSubmit);
			}
			showSection('pairing');
		}

		/* ---------------- App shell (controlling / read-only) ---------------- */

		async function enterApp(){
			try{
				const { response, body } = await api('/api/v2/capabilities');
				if(response.status === 200 && body){
					defaults.activeMs = body.poll ? body.poll.active_ms : defaults.activeMs;
					defaults.idleMs = body.poll ? body.poll.idle_ms : defaults.idleMs;
					defaults.hiddenMs = body.poll ? body.poll.hidden_ms : defaults.hiddenMs;
					defaults.leaseMs = body.writer_lease_ms || defaults.leaseMs;
				}else if(response.status === 401){
					app.identity.token = null;
					forgetDevice();
					app.identity = loadIdentity();
					renderPairing();
					return;
				}
			}catch(e){ /* keep defaults, still try to enter app */ }
			showSection('app');
			buildShell();
			schedulePoll(0);
		}

		function buildShell(){
			const root = $('app-section');
			if(root.dataset.built) return;
			root.dataset.built = '1';
			for(let deck = 0; deck < DECK_COUNT; deck++){
				const el = $('deck-' + deck);
				if(!el) continue;
				el.querySelector('.deck-play').addEventListener('click', () => onTogglePlaying(deck));
				el.querySelector('.deck-seek').addEventListener('change', (e) => onSeek(deck, e.target.value));
				el.querySelector('.deck-gain').addEventListener('input', (e) => onGain(deck, e.target.value));
				for(let slot = 0; slot < EFFECT_SLOTS; slot++){
					const typeSel = el.querySelector('.effect-type[data-slot="' + slot + '"]');
					const intensity = el.querySelector('.effect-intensity[data-slot="' + slot + '"]');
					typeSel.addEventListener('change', (e) => onEffectType(deck, slot, e.target.value));
					intensity.addEventListener('input', (e) => onEffectIntensity(deck, slot, e.target.value));
				}
			}
			$('mix-fader').addEventListener('input', (e) => onMix(e.target.value));
			$('recording-toggle').addEventListener('click', onToggleRecording);
			$('assist-coach-toggle').addEventListener('click', onToggleCoach);
			$('assist-arm-form').addEventListener('submit', onArmTransition);
			$('assist-cancel').addEventListener('click', onCancelTransition);
			$('lease-take').addEventListener('click', onTakeControl);
			$('lease-release').addEventListener('click', onReleaseControl);
			$('forget-device').addEventListener('click', onForgetDevice);
			for(let i = 0; i < CUE_COUNT; i++){
				const btn = document.querySelector('.cue-button[data-cue="' + i + '"]');
				if(btn) btn.disabled = true;
			}
		}

		/* ---- lease control ---- */

		function setLeaseState(next, message){
			app.leaseState = next;
			$('lease-take').hidden = next === 'controlling';
			$('lease-release').hidden = next !== 'controlling';
			$('lease-status').textContent = {
				read_only: 'Read-only \u2014 you are viewing live status.',
				acquiring: 'Requesting control\u2026',
				controlling: 'You have control.',
				conflict: 'Another browser has control.'
			}[next] || next;
			setControlsEnabled(next === 'controlling');
			if(message) announce(message);
		}

		function setControlsEnabled(enabled){
			document.querySelectorAll('.requires-control').forEach((el) => {
				if(el.tagName === 'BUTTON' || el.tagName === 'INPUT' || el.tagName === 'SELECT'){
					el.disabled = !enabled;
				}
			});
		}

		async function onTakeControl(){
			app.leaseState = leaseReducer(app.leaseState, { type: 'ACQUIRE_START' });
			setLeaseState('acquiring');
			try{
				const { response, body } = await api('/api/v2/lease', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify({ operation: 'acquire' })
				});
				if(response.status === 200){
					setLeaseState(leaseReducer(app.leaseState, { type: 'ACQUIRE_OK' }), 'You have control.');
				}else{
					setLeaseState(leaseReducer(app.leaseState, { type: 'ACQUIRE_CONFLICT' }),
						describeError(body) || 'Could not take control.');
				}
			}catch(e){
				setLeaseState('read_only', 'Could not reach the device.');
			}
		}

		async function onReleaseControl(){
			try{
				await api('/api/v2/lease', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify({ operation: 'release' })
				});
			}catch(e){ /* best-effort */ }
			setLeaseState(leaseReducer(app.leaseState, { type: 'RELEASE_OK' }), 'Control released.');
		}

		async function renewLeaseIfNeeded(){
			if(app.leaseState !== 'controlling') return;
			try{
				const { response, body } = await api('/api/v2/lease', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify({ operation: 'renew' })
				});
				if(response.status !== 200){
					const reason = body && body.error === 'writer_lease_expired' ? 'RENEW_EXPIRED' : 'RENEW_NOT_OWNER';
					setLeaseState(leaseReducer(app.leaseState, { type: reason }), describeError(body) || 'Control was lost.');
				}
			}catch(e){ /* next poll tick will retry */ }
		}

		function onForgetDevice(){
			const button = $('forget-device');
			if(button.dataset.confirm !== '1'){
				button.dataset.confirm = '1';
				button.textContent = 'Confirm forget device?';
				window.setTimeout(() => {
					button.dataset.confirm = '';
					button.textContent = 'Forget device';
				}, 5000);
				return;
			}
			if(app.leaseState === 'controlling'){
				api('/api/v2/lease', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify({ operation: 'release' }),
					keepalive: true
				}).catch(() => {});
			}
			forgetDevice();
			window.location.reload();
		}

		/* ---- commands ---- */

		function identitySnapshot(){
			return { bootId: app.bootId, sessionId: app.sessionId };
		}

		async function sendCommand(action, fields){
			if(app.leaseState !== 'controlling' || app.bootId === null){
				announce('Take control before sending commands.');
				return;
			}
			let id = app.nextCommandId();
			while(!app.tracker.markSent(id)) id = app.nextCommandId();
			let body;
			try{
				body = buildCommandBody(identitySnapshot(), action, fields, id);
			}catch(e){
				announce('Internal error building command.');
				return;
			}
			try{
				const { response, body: result } = await api('/api/v2/command', {
					method: 'POST',
					headers: { 'Content-Type': 'application/json' },
					body: JSON.stringify(body)
				});
				if(response.status === 401){
					app.identity.token = null;
					forgetDevice();
					app.identity = loadIdentity();
					renderPairing();
					return;
				}
				if(response.status === 409 && result && (result.error === 'writer_lease_required')){
					setLeaseState('read_only', 'Control was lost. Take control again to continue.');
					return;
				}
				if(result && result.status === 'rejected'){
					announce(describeError({ error: result.error }) || 'Command was rejected.');
				}
				// Applied/accepted/superseded outcomes are reconciled from the
				// next state poll (server truth), never assumed locally.
			}catch(e){
				announce('Could not reach the device; the last change may not have applied.');
			}
		}

		function onTogglePlaying(deck){
			const playing = document.querySelector('#deck-' + deck + ' .deck-play').dataset.playing === '1';
			sendCommand('set_playing', { deck, value: !playing });
		}

		function onSeek(deck, value){
			sendCommand('seek', { deck, value: Number(value) });
		}

		function onGain(deck, value){
			app.throttle.schedule('gain:' + deck, () => sendCommand('set_gain', { deck, value: Number(value) }));
		}

		function onMix(value){
			app.throttle.schedule('mix', () => sendCommand('set_mix', { value: Number(value) }));
		}

		function onEffectType(deck, slot, value){
			sendCommand('set_effect_type', { deck, slot, value: Number(value) });
		}

		function onEffectIntensity(deck, slot, value){
			app.throttle.schedule('effect:' + deck + ':' + slot,
				() => sendCommand('set_effect_intensity', { deck, slot, value: Number(value) }));
		}

		function onToggleRecording(){
			const button = $('recording-toggle');
			const recording = button.dataset.recording === '1';
			sendCommand('set_recording', { value: !recording });
		}

		function onToggleCoach(){
			const coachOn = $('assist-coach-toggle').getAttribute('aria-pressed') === 'true';
			sendCommand('assist_set_mode', { value: !coachOn });
		}

		function onArmTransition(event){
			event.preventDefault();
			sendCommand('assist_arm_transition', {
				deck: Number($('assist-from-deck').value),
				toDeck: Number($('assist-to-deck').value),
				crossfadeBeats: Number($('assist-crossfade-beats').value),
				startAtBoundary: $('assist-start-at-boundary').checked,
				tempoLock: $('assist-tempo-lock').checked
			});
		}

		function onCancelTransition(){
			sendCommand('assist_cancel_transition', {});
		}

		/* ---- polling ---- */

		function schedulePoll(delayOverride){
			if(app.pollTimer) window.clearTimeout(app.pollTimer);
			const delay = typeof delayOverride === 'number' ? delayOverride : choosePollDelay({
				online: navigator.onLine,
				hidden: document.hidden,
				isWriter: app.leaseState === 'controlling',
				failures: app.pollFailures,
				activeMs: defaults.activeMs,
				idleMs: defaults.idleMs,
				hiddenMs: defaults.hiddenMs
			});
			if(delay === null) return; // paused; resumed by online/visibilitychange listeners
			app.pollTimer = window.setTimeout(pollOnce, delay);
		}

		async function pollOnce(){
			if(app.pollInFlight) return; // single in-flight request
			if(!navigator.onLine){ schedulePoll(); return; }
			app.pollInFlight = true;
			if(app.pollController) app.pollController.abort();
			const controller = new AbortController();
			app.pollController = controller;
			const requestedAt = ++app.lastRequestToken;
			try{
				const response = await fetch('/api/v2/state', {
					headers: authHeaders(),
					signal: controller.signal
				});
				if(requestedAt !== app.lastRequestToken) return; // stale, superseded
				const text = await response.text();
				if(requestedAt !== app.lastRequestToken) return;
				if(response.status === 401){
					app.identity.token = null;
					forgetDevice();
					app.identity = loadIdentity();
					renderPairing();
					return;
				}
				let snapshot;
				try{ snapshot = JSON.parse(text); }
				catch(e){
					app.pollFailures++;
					announce('The device sent unreadable state. Retrying.');
					return;
				}
				if(response.status !== 200){
					app.pollFailures++;
					announce(describeError(snapshot) || 'Could not read device state.');
					return;
				}
				app.pollFailures = 0;
				applySnapshot(snapshot);
				await renewLeaseIfNeeded();
			}catch(e){
				if(e && e.name === 'AbortError') return;
				app.pollFailures++;
			}finally{
				app.pollInFlight = false;
				if(requestedAt === app.lastRequestToken) schedulePoll();
			}
		}

		function applySnapshot(snapshot){
			// All identity/tracker/lease bookkeeping lives in the shared,
			// DOM-independent applySnapshotCore() so it has a direct unit
			// test (tests/wireless_ui_self_test.mjs) exercising this exact
			// code path; only DOM rendering stays here.
			const result = applySnapshotCore(app, snapshot);
			if(!result.accept) return; // stale/out-of-order within the same identity, ignore
			if(result.identityChanged){
				setLeaseState('read_only', 'The device restarted. Take control again if needed.');
			}
			$('session-status').textContent = snapshot.active ?
				(snapshot.mixer_running ? 'Mixer running' : 'Session active') : 'Session not running';
			for(let deck = 0; deck < DECK_COUNT; deck++){
				renderDeck(deck, snapshot.decks[deck]);
			}
			$('mix-fader').value = snapshot.mix;
			$('mix-value').textContent = Math.round((snapshot.mix / 255) * 100) + '% B';
			const recordButton = $('recording-toggle');
			recordButton.dataset.recording = snapshot.recording ? '1' : '0';
			recordButton.setAttribute('aria-pressed', snapshot.recording ? 'true' : 'false');
			recordButton.textContent = snapshot.recording ? 'Stop recording' : 'Start recording';
			$('recording-indicator').textContent = snapshot.recording ? 'Recording' : 'Not recording';
			renderAssist(snapshot.assist);
		}

		function renderDeck(deck, state){
			const root = $('deck-' + deck);
			if(!root || !state) return;
			const nameEl = root.querySelector('.deck-track');
			const path = state.path || '';
			nameEl.textContent = state.loaded ? (path.split('/').pop() || path) : 'No track loaded';
			nameEl.title = path;
			const playButton = root.querySelector('.deck-play');
			playButton.dataset.playing = state.playing ? '1' : '0';
			playButton.setAttribute('aria-pressed', state.playing ? 'true' : 'false');
			playButton.textContent = state.playing ? 'Pause' : 'Play';
			playButton.disabled = !state.loaded;
			const timing = root.querySelector('.deck-time');
			const hasTiming = state.timing === 'coarse';
			timing.textContent = hasTiming ? (formatClock(state.elapsed) + ' / ' + formatClock(state.duration)) : '--:-- / --:--';
			const seek = root.querySelector('.deck-seek');
			seek.max = state.duration || 0;
			if(document.activeElement !== seek) seek.value = state.elapsed || 0;
			seek.disabled = !hasTiming || !state.loaded;
			const gain = root.querySelector('.deck-gain');
			if(document.activeElement !== gain) gain.value = state.gain;
			(state.effects || []).forEach((effect, slot) => {
				const typeSel = root.querySelector('.effect-type[data-slot="' + slot + '"]');
				const intensity = root.querySelector('.effect-intensity[data-slot="' + slot + '"]');
				if(document.activeElement !== typeSel) typeSel.value = String(effect.type);
				if(document.activeElement !== intensity) intensity.value = effect.intensity;
				intensity.disabled = effect.type === 0;
			});
		}

		/* ---- Assist (Coach / one-shot transition) rendering ---- */

		function renderAssist(assist){
			const state = assist || { mode: 'off' };
			const mode = state.mode || 'off';
			$('assist-mode').textContent = 'Assist: ' + (ASSIST_MODE_TEXT[mode] || mode);
			const toggle = $('assist-coach-toggle');
			const coachOn = mode !== 'off';
			toggle.setAttribute('aria-pressed', coachOn ? 'true' : 'false');
			toggle.textContent = coachOn ? 'Disable Coach' : 'Enable Coach';

			const showAdvice = mode === 'coach';
			$('assist-advice').hidden = !showAdvice;
			if(showAdvice) renderAssistAdvice(state.advice || {});

			renderAssistSuggestions(mode === 'coach' ? (state.suggestions || []) : []);

			const inTransition = mode === 'armed' || mode === 'running' || mode === 'complete' || mode === 'failed';
			$('assist-transition').hidden = !inTransition;
			if(inTransition) renderAssistPlan(state.plan || {});

			$('assist-cancel').hidden = !(mode === 'armed' || mode === 'running');
			$('assist-arm-form').hidden = mode === 'armed' || mode === 'running';
		}

		function clearChildren(el){
			while(el.firstChild) el.removeChild(el.firstChild);
		}

		function renderAssistAdvice(advice){
			$('assist-advice-deck').textContent = typeof advice.suggested_deck === 'number' ?
				deckLabel(advice.suggested_deck) : '\u2014';
			$('assist-advice-boundary').textContent = ASSIST_BOUNDARY_TEXT[advice.boundary] || '\u2014';
			$('assist-advice-rate').textContent = formatRatePercent(advice.target_rate);
			$('assist-advice-direction').textContent = describeCrossfadeDirection(advice.crossfade_dir);
			const list = $('assist-advice-warnings');
			clearChildren(list);
			if(advice.valid === false){
				const li = document.createElement('li');
				li.textContent = 'No advice available yet \u2014 waiting on a playing deck with usable metadata.';
				list.appendChild(li);
				return;
			}
			(advice.warnings || []).forEach((code) => {
				const li = document.createElement('li');
				li.textContent = ASSIST_WARNING_TEXT[code] || code;
				list.appendChild(li);
			});
		}

		function renderAssistSuggestions(suggestions){
			const list = $('assist-suggestion-list');
			clearChildren(list);
			if(!suggestions.length){
				const li = document.createElement('li');
				li.textContent = 'No suggestions right now.';
				list.appendChild(li);
				return;
			}
			suggestions.forEach((suggestion) => {
				const li = document.createElement('li');
				const excluded = suggestion.exclude && suggestion.exclude !== 'none';
				if(excluded) li.classList.add('suggestion-excluded');
				const parts = [
					formatBpmDelta(suggestion.tempo_delta),
					ASSIST_KEY_TEXT[suggestion.key] || suggestion.key,
					suggestion.rating < 255 ? ('rating ' + suggestion.rating + '/5') : 'unrated',
					'confidence ' + Math.round((suggestion.confidence || 0) / 10) + '%'
				];
				if(excluded) parts.push('excluded: ' + (ASSIST_EXCLUDE_TEXT[suggestion.exclude] || suggestion.exclude));
				const summary = document.createElement('div');
				summary.textContent = parts.join(' \u2022 ');
				li.appendChild(summary);
				const reasons = describeReasonFlags(suggestion.reason_flags);
				if(reasons.length){
					const detail = document.createElement('div');
					detail.className = 'suggestion-reasons follow-up';
					detail.textContent = 'Reasons: ' + reasons.join(', ');
					li.appendChild(detail);
				}
				list.appendChild(li);
			});
		}

		function renderAssistPlan(plan){
			const failureText = plan.failure && plan.failure !== 'none' ? ASSIST_FAILURE_TEXT[plan.failure] || plan.failure : null;
			const stepText = 'From ' + deckLabel(plan.from_deck) + ' to ' + deckLabel(plan.to_deck) + ', ' +
				(plan.crossfade_beats || 0) + '-beat crossfade \u2014 step ' + ((plan.step || 0) + 1) + ' of ' +
				(plan.steps || 0) + ': ' + (ASSIST_ACTION_TEXT[plan.action] || plan.action);
			$('assist-transition-progress').textContent = failureText ? (stepText + '. Failed: ' + failureText) : stepText;
		}

		/* ---- lifecycle wiring ---- */

		document.addEventListener('visibilitychange', () => {
			if(!document.hidden) schedulePoll(0);
		});
		window.addEventListener('online', () => {
			app.pollFailures = 0;
			schedulePoll(0);
		});
		window.addEventListener('offline', () => {
			if(app.pollTimer) window.clearTimeout(app.pollTimer);
		});

		app.lastRequestToken = 0;

		async function boot(){
			live.region = $('live-region');
			const inSetup = await checkSetupMode();
			if(inSetup) return;
			if(app.identity.token){
				await enterApp();
			}else{
				renderPairing();
			}
		}

		if(document.readyState === 'loading'){
			document.addEventListener('DOMContentLoaded', boot);
		}else{
			boot();
		}
	})();
}

})();
