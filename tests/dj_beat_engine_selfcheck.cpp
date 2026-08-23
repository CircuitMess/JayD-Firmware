#include <assert.h>
#include <stdint.h>
#include "../src/DjSession/DjBeatEngine.h"

// ---------------------------------------------------------------------------
// Checked fixed-point / rational math, including overflow and boundaries.
// ---------------------------------------------------------------------------
static void testCheckedMath(){
	int64_t sum = 0;
	assert(djCheckedAdd(5, 10, sum) && sum == 15);
	assert(!djCheckedAdd(INT64_MAX, 1, sum));
	assert(!djCheckedAdd(INT64_MIN, -1, sum));
	assert(djCheckedAdd(INT64_MAX, 0, sum) && sum == INT64_MAX);

	uint64_t usum = 0;
	assert(djCheckedAddU(5, 10, usum) && usum == 15);
	assert(!djCheckedAddU(UINT64_MAX, 1, usum));
	assert(djCheckedAddU(UINT64_MAX, 0, usum) && usum == UINT64_MAX);

	uint64_t result = 0;
	// 100 frames * 15000 / 12000 = 125
	assert(djCheckedMulDiv(100, 15000, 12000, result) && result == 125);
	assert(!djCheckedMulDiv(1, 1, 0, result)); // divide by zero rejected
	assert(!djCheckedMulDiv(UINT64_MAX, 2, 1, result)); // overflow rejected
	assert(djCheckedMulDiv(0, 15000, 12000, result) && result == 0);

	int64_t signedResult = 0;
	assert(djCheckedMulDivSigned(-100, 15000, 12000, signedResult) && signedResult == -125);
	assert(djCheckedMulDivSigned(100, 15000, 12000, signedResult) && signedResult == 125);
	assert(djCheckedMulDivSigned(0, 15000, 12000, signedResult) && signedResult == 0);
	assert(!djCheckedMulDivSigned(1, 1, 0, signedResult));
	assert(!djCheckedMulDivSigned(INT64_MIN, 2, 1, signedResult)); // overflow rejected
}

// ---------------------------------------------------------------------------
// Sparse anchors / downbeats: grid build validation, capability disable.
// ---------------------------------------------------------------------------
static void testGridBuildValidAndCapacity(){
	// 44100 Hz, 120.000 BPM -> framesPerBeat = 44100*60000/120000 = 22050,
	// framesPerQuarterBeat = 22050/4 = 5512.5 (exact via rational, not integer).
	DjGridAnchor anchors[4] = {};
	anchors[0].frame = 0;      anchors[0].quarterBeat = 0;
	anchors[1].frame = 22050;  anchors[1].quarterBeat = 4;  // +1 beat
	anchors[2].frame = 44100;  anchors[2].quarterBeat = 8;  // +1 beat
	anchors[3].frame = 88200;  anchors[3].quarterBeat = 16; // +2 beats (downbeat-style sparse gap)

	DjBeatGrid grid;
	assert(grid.build(44100, 120000, 1000000, anchors, 4));
	assert(grid.valid());
	assert(grid.sampleRate() == 44100);
	assert(grid.bpmMilli() == 120000);

	uint64_t boundaryFrame = 0;
	int64_t quarterBeat = 0;
	assert(grid.quarterBeatAtFrame(0, quarterBeat, boundaryFrame));
	assert(quarterBeat == 0 && boundaryFrame == 0);

	// Exactly on an anchor.
	assert(grid.quarterBeatAtFrame(44100, quarterBeat, boundaryFrame));
	assert(quarterBeat == 8 && boundaryFrame == 44100);

	// Mid-way between anchors 2 and 3 (sparse gap spans 2 beats = 8 quarter-beats).
	assert(grid.quarterBeatAtFrame(66150, quarterBeat, boundaryFrame)); // +22050 frames = +1 beat = +4 qb
	assert(quarterBeat == 12);

	// Exact frame round-trip for an arbitrary absolute quarter-beat.
	uint64_t frame = 0;
	assert(grid.frameAtQuarterBeat(16, frame) && frame == 88200);
	assert(grid.frameAtQuarterBeat(12, frame) && frame == 66150);

	// Capacity truncation: more anchors than DJ_GRID_ANCHOR_CAPACITY is
	// accepted but subsampled to the bounded capacity, never overruns storage.
	DjGridAnchor many[DJ_GRID_ANCHOR_CAPACITY + 20];
	for(uint16_t i = 0; i < DJ_GRID_ANCHOR_CAPACITY + 20; ++i){
		many[i].frame = uint64_t(i) * 1000;
		many[i].quarterBeat = int64_t(i) * 4;
	}
	DjBeatGrid bigGrid;
	assert(bigGrid.build(44100, 120000, uint64_t(DJ_GRID_ANCHOR_CAPACITY + 20) * 1000, many, DJ_GRID_ANCHOR_CAPACITY + 20));
	assert(bigGrid.valid());
}

static void testGridBuildDisablesOnInvalidInput(){
	DjGridAnchor anchors[2] = {};
	anchors[0].frame = 0; anchors[0].quarterBeat = 0;
	anchors[1].frame = 22050; anchors[1].quarterBeat = 4;

	DjBeatGrid grid;
	// Sample rate out of range.
	assert(!grid.build(0, 120000, 1000000, anchors, 2));
	assert(!grid.valid());
	assert(!grid.build(DJ_GRID_MAX_SAMPLE_RATE + 1, 120000, 1000000, anchors, 2));
	assert(!grid.valid());

	// BPM out of supported band.
	assert(!grid.build(44100, DJ_GRID_MIN_BPM_MILLI - 1, 1000000, anchors, 2));
	assert(!grid.build(44100, DJ_GRID_MAX_BPM_MILLI + 1, 1000000, anchors, 2));

	// No anchors at all.
	assert(!grid.build(44100, 120000, 1000000, nullptr, 0));

	// Non-monotonic frame (ambiguous / inconsistent grid) disables the capability.
	DjGridAnchor badFrame[2] = {};
	badFrame[0].frame = 100; badFrame[0].quarterBeat = 0;
	badFrame[1].frame = 100; badFrame[1].quarterBeat = 4; // not strictly increasing
	assert(!grid.build(44100, 120000, 1000000, badFrame, 2));
	assert(!grid.valid());

	// Non-monotonic quarter-beat.
	DjGridAnchor badBeat[2] = {};
	badBeat[0].frame = 0; badBeat[0].quarterBeat = 4;
	badBeat[1].frame = 22050; badBeat[1].quarterBeat = 4; // not strictly increasing
	assert(!grid.build(44100, 120000, 1000000, badBeat, 2));

	// Anchor beyond the track's own duration.
	DjGridAnchor beyond[2] = {};
	beyond[0].frame = 0; beyond[0].quarterBeat = 0;
	beyond[1].frame = 2000000; beyond[1].quarterBeat = 4;
	assert(!grid.build(44100, 120000, 1000000, beyond, 2));

	// Once invalid, queries fail closed rather than guessing.
	uint64_t frame = 0;
	int64_t qb = 0;
	assert(!grid.quarterBeatAtFrame(0, qb, frame));
	assert(!grid.frameAtQuarterBeat(0, frame));
	assert(!grid.nextBoundary(0, 4, frame));
}

// ---------------------------------------------------------------------------
// Quantize late policy: within one audio block applies now, else scheduled.
// ---------------------------------------------------------------------------
static void testQuantizeLatePolicy(){
	assert(djQuantizeDecide(1000, 1000) == DJ_QUANTIZE_APPLY_NOW); // already there
	assert(djQuantizeDecide(1000, 1000 + DJ_QUANTIZE_TOLERANCE_FRAMES) == DJ_QUANTIZE_APPLY_NOW); // exactly at tolerance
	assert(djQuantizeDecide(1000, 1000 + DJ_QUANTIZE_TOLERANCE_FRAMES + 1) == DJ_QUANTIZE_SCHEDULE);
	assert(djQuantizeDecide(1000, 500) == DJ_QUANTIZE_APPLY_NOW); // target already passed: treat as now
	assert(djQuantizeQuarterBeats(DJ_QUANTIZE_BEAT_1) == 4);
	assert(djQuantizeQuarterBeats(DJ_QUANTIZE_BEAT_1_2) == 2);
	assert(djQuantizeQuarterBeats(DJ_QUANTIZE_BEAT_1_4) == 1);
	assert(djQuantizeQuarterBeats(DJ_QUANTIZE_OFF) == 0);
}

static void testQuantizeTargetSnapsBackWithinTolerance(){
	DjGridAnchor anchors[3] = {};
	anchors[0].frame = 0;     anchors[0].quarterBeat = 0;
	anchors[1].frame = 22050; anchors[1].quarterBeat = 4;
	anchors[2].frame = 44100; anchors[2].quarterBeat = 8;
	DjBeatGrid grid;
	assert(grid.build(44100, 120000, 200000, anchors, 3));

	uint64_t target = 0;
	// Just a few frames past a boundary: snap back to it rather than waiting
	// almost a full beat for the next one.
	assert(djQuantizeTarget(grid, 22050 + 10, 4, target));
	assert(target == 22050);

	// Well clear of tolerance past a boundary: wait for the next one instead.
	assert(djQuantizeTarget(grid, 22050 + DJ_QUANTIZE_TOLERANCE_FRAMES + 1, 4, target));
	assert(target == 44100);

	// Exactly on a boundary: that boundary itself, no drift.
	assert(djQuantizeTarget(grid, 22050, 4, target));
	assert(target == 22050);
}

static void testBeatPhase(){
	DjGridAnchor anchors[3] = {};
	anchors[0].frame = 0;     anchors[0].quarterBeat = 0;
	anchors[1].frame = 22050; anchors[1].quarterBeat = 4;
	anchors[2].frame = 44100; anchors[2].quarterBeat = 8;
	DjBeatGrid grid;
	assert(grid.build(44100, 120000, 200000, anchors, 3));

	int64_t phase = -1;
	uint64_t framesPerBeat = 0;
	assert(djBeatPhase(grid, 0, phase, framesPerBeat));
	assert(phase == 0);
	assert(framesPerBeat == 22050);

	assert(djBeatPhase(grid, 11025, phase, framesPerBeat));
	assert(phase == 11025);
	assert(framesPerBeat == 22050);

	// Just before the next beat boundary: phase approaches framesPerBeat, never reaches/exceeds it.
	assert(djBeatPhase(grid, 22049, phase, framesPerBeat));
	assert(phase == 22049);
	assert(uint64_t(phase) < framesPerBeat);
}

// ---------------------------------------------------------------------------
// Loop bounds and wrap scheduling (nextBoundary across sparse anchors).
// ---------------------------------------------------------------------------
static void testLoopLengthsAndBoundaryWrap(){
	assert(djLoopQuarterBeats(DJ_LOOP_BEAT_1_2) == 2);
	assert(djLoopQuarterBeats(DJ_LOOP_BEAT_1) == 4);
	assert(djLoopQuarterBeats(DJ_LOOP_BEAT_2) == 8);
	assert(djLoopQuarterBeats(DJ_LOOP_BEAT_4) == 16);
	assert(djLoopQuarterBeats(DJ_LOOP_BEAT_8) == 32);

	DjGridAnchor anchors[3] = {};
	anchors[0].frame = 0;     anchors[0].quarterBeat = 0;
	anchors[1].frame = 22050; anchors[1].quarterBeat = 4;
	anchors[2].frame = 44100; anchors[2].quarterBeat = 8;
	DjBeatGrid grid;
	assert(grid.build(44100, 120000, 200000, anchors, 3));

	uint64_t boundary = 0;
	// From frame 0, the next whole-beat boundary is frame 0 itself.
	assert(grid.nextBoundary(0, 4, boundary) && boundary == 0);
	// From just after frame 0, the next whole-beat boundary is one beat later.
	assert(grid.nextBoundary(1, 4, boundary) && boundary == 22050);
	// From the middle of a beat, still rounds up to the next boundary.
	assert(grid.nextBoundary(11025, 4, boundary) && boundary == 22050);
	// Half-beat resolution finds the intermediate boundary.
	assert(grid.nextBoundary(11025, 2, boundary) && boundary == 11025);
	// Wraps correctly past the last cached anchor via extrapolation.
	assert(grid.nextBoundary(44101, 4, boundary) && boundary == 66150);
}

// ---------------------------------------------------------------------------
// DjLoopEngine: bounds/impossible-length rejection, valid-length mask,
// pending->active wrap scheduling, busy-guard (one seek at a time), bounded
// retry -> disengage, and reloop.
// ---------------------------------------------------------------------------
static DjBeatGrid makeSimpleGrid(uint64_t durationFrames){
	DjGridAnchor anchors[3] = {};
	anchors[0].frame = 0;     anchors[0].quarterBeat = 0;
	anchors[1].frame = 22050; anchors[1].quarterBeat = 4;
	anchors[2].frame = 44100; anchors[2].quarterBeat = 8;
	DjBeatGrid grid;
	assert(grid.build(44100, 120000, durationFrames, anchors, 3));
	return grid;
}

static void testLoopEngineBoundsAndMask(){
	DjBeatGrid grid = makeSimpleGrid(100000);

	DjLoopEngine engine;
	// A 1-beat loop from frame 0 is well within bounds.
	assert(engine.engage(grid, 0, 100000, DJ_LOOP_BEAT_1, 1));
	assert(engine.state() == DJ_LOOP_PENDING);
	assert(engine.startFrame() == 0);
	assert(engine.endFrame() == 22050);

	// An 8-beat loop this close to a short track's end is impossible (past EOF).
	DjLoopEngine tooLong;
	assert(!tooLong.engage(grid, 88200, 100000, DJ_LOOP_BEAT_8, 2));
	assert(tooLong.state() == DJ_LOOP_INACTIVE);

	// Valid-length mask agrees with engage()'s own bounds check.
	const uint8_t mask = djValidLoopLengthMask(grid, 88200, 100000);
	assert((mask & (1u << DJ_LOOP_BEAT_1_2)) != 0); // short loop still fits
	assert((mask & (1u << DJ_LOOP_BEAT_8)) == 0);   // long loop does not
}

static void testLoopEngineWrapAndBusyGuard(){
	DjBeatGrid grid = makeSimpleGrid(1000000);
	DjLoopEngine engine;
	assert(engine.engage(grid, 0, 1000000, DJ_LOOP_BEAT_1, 42));

	uint64_t target = 0;
	// Not yet reached the pending start boundary (already at 0, so it fires now).
	assert(engine.needsSeek(0, target) && target == 0);
	engine.beginSeek();
	// While a seek is in flight, no further seek is requested even if asked repeatedly.
	assert(!engine.needsSeek(0, target));
	assert(!engine.needsSeek(0, target));
	engine.completeSeek(true);
	assert(engine.state() == DJ_LOOP_ACTIVE);

	// No seek needed mid-loop.
	assert(!engine.needsSeek(10000, target));
	// Exactly at the end boundary, a single wrap-seek is requested.
	assert(engine.needsSeek(22050, target) && target == 0);
	engine.beginSeek();
	assert(!engine.needsSeek(22050, target)); // busy-guarded, no storm
	engine.completeSeek(true);
	assert(engine.state() == DJ_LOOP_ACTIVE);
}

static void testLoopEngineRetryLimitDisengages(){
	DjBeatGrid grid = makeSimpleGrid(1000000);
	DjLoopEngine engine;
	assert(engine.engage(grid, 0, 1000000, DJ_LOOP_BEAT_1, 7));
	uint64_t target = 0;
	assert(engine.needsSeek(0, target));
	engine.beginSeek();

	for(uint8_t attempt = 0; attempt + 1 < DJ_LOOP_SEEK_RETRY_LIMIT; ++attempt){
		engine.completeSeek(false);
		assert(engine.state() == DJ_LOOP_PENDING); // still retrying, not yet given up
		assert(engine.needsSeek(0, target));
		engine.beginSeek();
	}
	engine.completeSeek(false); // final failure hits the retry limit
	assert(engine.state() == DJ_LOOP_INACTIVE);
}

static void testLoopEngineReloop(){
	DjBeatGrid grid = makeSimpleGrid(1000000);
	DjLoopEngine engine;
	assert(!engine.reloop(9)); // cannot reloop before ever engaging
	assert(engine.engage(grid, 0, 1000000, DJ_LOOP_BEAT_1, 1));
	uint64_t target = 0;
	assert(engine.needsSeek(0, target));
	engine.beginSeek();
	engine.completeSeek(true);
	assert(engine.state() == DJ_LOOP_ACTIVE);

	// Mid-loop, force an immediate jump back to the loop start.
	assert(engine.reloop(55));
	assert(engine.state() == DJ_LOOP_PENDING);
	assert(engine.needsSeek(11000, target) && target == 0); // already past start, fires immediately
	assert(engine.consumePendingCommandId() == 55);
	assert(engine.consumePendingCommandId() == 0); // consumed exactly once
}

// ---------------------------------------------------------------------------
// Sync controller: off/armed/locked/out-of-range/error, convergence, no
// feedback oscillation (nudge cooldown), and no playback started by sync.
// ---------------------------------------------------------------------------
static void testSyncOffAndError(){
	DjSyncController sync;
	DjSyncInputs in;
	DjSyncOutputs out = sync.tick(in); // not armed
	assert(out.state == DJ_SYNC_OFF);
	assert(!out.applyRate && !out.applyNudge && !out.hardAlign);

	in.armed = true;
	in.masterValid = false;
	out = sync.tick(in); // missing master validity -> error, not a guess
	assert(out.state == DJ_SYNC_ERROR);

	in.masterValid = true;
	in.followerValid = true;
	in.followerBpmMilli = 0; // divide-by-zero guarded
	out = sync.tick(in);
	assert(out.state == DJ_SYNC_ERROR);
}

static void testSyncOutOfRange(){
	DjSyncController sync;
	DjSyncInputs in;
	in.armed = true;
	in.masterValid = true;
	in.followerValid = true;
	in.masterBpmMilli = 200000;  // 200 BPM
	in.followerBpmMilli = 100000; // 100 BPM -> ratio 2.0x, outside 0.5-1.5x
	DjSyncOutputs out = sync.tick(in);
	assert(out.state == DJ_SYNC_OUT_OF_RANGE);
	assert(!out.applyRate); // must not command an out-of-limit rate
}

static void testSyncArmedHoldsUntilBothPlaying(){
	DjSyncController sync;
	DjSyncInputs in;
	in.armed = true;
	in.masterValid = true;
	in.followerValid = true;
	in.masterBpmMilli = 120000;
	in.followerBpmMilli = 120000;
	in.masterPlaying = false; // sync never starts playback itself
	in.followerPlaying = false;
	DjSyncOutputs out = sync.tick(in);
	assert(out.state == DJ_SYNC_ARMED);
	assert(out.applyRate && out.targetRate == DJ_RATE_NEUTRAL);
	assert(!out.hardAlign && !out.applyNudge);
}

static void testSyncLocksWithinTolerance(){
	DjSyncController sync;
	DjSyncInputs in;
	in.armed = true;
	in.masterValid = true;
	in.followerValid = true;
	in.masterPlaying = true;
	in.followerPlaying = true;
	in.masterBpmMilli = 120000;
	in.followerBpmMilli = 120000;
	in.masterFramesPerBeat = 22050;
	in.followerFramesPerBeat = 22050;
	in.masterPhaseFrames = 100;
	in.followerPhaseFrames = 100 + int64_t(DJ_SYNC_LOCK_TOLERANCE_FRAMES); // exactly at tolerance
	DjSyncOutputs out = sync.tick(in);
	assert(out.state == DJ_SYNC_LOCKED);
	assert(out.applyRate && !out.applyNudge && !out.hardAlign);
}

static void testSyncNudgesWithCooldownNoOscillation(){
	DjSyncController sync;
	DjSyncInputs in;
	in.armed = true;
	in.masterValid = true;
	in.followerValid = true;
	in.masterPlaying = true;
	in.followerPlaying = true;
	in.masterBpmMilli = 120000;
	in.followerBpmMilli = 120000;
	in.masterFramesPerBeat = 22050;
	in.followerFramesPerBeat = 22050;
	in.masterPhaseFrames = 0;
	in.followerPhaseFrames = 2000; // moderate error: within hard-align threshold, beyond lock tolerance

	DjSyncOutputs first = sync.tick(in);
	assert(first.state == DJ_SYNC_ARMED);
	assert(first.applyNudge);
	assert(first.nudgeAmount != 0);
	assert(!first.hardAlign);

	// Immediately re-ticking with the same (uncorrected) error must not nudge
	// again until the cooldown elapses -- this is what prevents feedback
	// oscillation / rate-command spam. Keep ticking (bounded) until the next
	// nudge fires and confirm the cooldown was actually honored.
	uint32_t ticksUntilNextNudge = 0;
	bool nudgedAgain = false;
	for(; ticksUntilNextNudge < DJ_SYNC_NUDGE_COOLDOWN_TICKS + 2; ++ticksUntilNextNudge){
		DjSyncOutputs out = sync.tick(in);
		if(out.applyNudge){
			nudgedAgain = true;
			break;
		}
	}
	assert(nudgedAgain);
	assert(ticksUntilNextNudge + 1 >= DJ_SYNC_NUDGE_COOLDOWN_TICKS); // cooldown honored, no spam
}

static void testSyncHardAlignBeyondThreshold(){
	DjSyncController sync;
	DjSyncInputs in;
	in.armed = true;
	in.masterValid = true;
	in.followerValid = true;
	in.masterPlaying = true;
	in.followerPlaying = true;
	in.masterBpmMilli = 60000;
	in.followerBpmMilli = 60000;
	in.followerSampleRate = 44100;
	in.masterFramesPerBeat = 44100;
	in.followerFramesPerBeat = 44100;
	in.masterPhaseFrames = 0;
	// Error far larger than the hard-align threshold (250ms @ 44100 = 11025 frames),
	// but still less than half a beat (22050) so the wrap logic doesn't fold it away.
	in.followerPhaseFrames = 15000;
	DjSyncOutputs out = sync.tick(in);
	assert(out.hardAlign);
	assert(out.hardAlignPhaseErrorFrames == 15000);
	assert(!out.applyNudge); // one corrective seek, not a continued nudge
}

static void testSyncLossOfMetadataGoesToError(){
	DjSyncController sync;
	DjSyncInputs in;
	in.armed = true;
	in.masterValid = true;
	in.followerValid = true;
	in.masterPlaying = true;
	in.followerPlaying = true;
	in.masterBpmMilli = 120000;
	in.followerBpmMilli = 120000;
	in.masterFramesPerBeat = 22050;
	in.followerFramesPerBeat = 22050;
	sync.tick(in);
	assert(sync.state() == DJ_SYNC_ARMED || sync.state() == DJ_SYNC_LOCKED);

	// Master metadata lost mid-session (e.g. card pulled / re-index) is
	// observable as an explicit error transition, not a silent hold.
	in.masterValid = false;
	DjSyncOutputs out = sync.tick(in);
	assert(out.state == DJ_SYNC_ERROR);

	// User stops the master deck: sync should not force playback, it holds
	// rate at armed without chasing phase.
	in.masterValid = true;
	in.masterPlaying = false;
	out = sync.tick(in);
	assert(out.state == DJ_SYNC_ARMED);
	assert(!out.hardAlign && !out.applyNudge);
}

// Faithful model of JayD-Library's SpeedModifier, mirroring
// AudioLib/SpeedModifier.cpp's real semantics exactly:
//   - setRate() clamps to [MinRate,MaxRate] and unconditionally overwrites
//     requestedRate; nudgeRate() adds a signed delta on top of the *current*
//     requestedRate and applies it via setRate(). This is the exact
//     interaction that made the sync controller's per-tick unconditional
//     setRate() call erase an in-flight nudge before the applyRate cache fix.
//   - nudgeRate()/setRate() only ever change requestedRate. Actual playback
//     speed (currentRate) ramps toward it by at most RateScale/BUFFER_SAMPLES
//     per generated output sample (SpeedModifier::advanceRate()), and the
//     source position advances by currentRate (Q16.16 input-samples per
//     output-sample) each sample (SpeedModifier::generate()'s
//     sourcePosition += currentRate accumulator). Phase movement is
//     therefore a function of *consumed source frames over elapsed output
//     samples*, not a direct function of the commanded rate delta -- an
//     earlier version of this test incorrectly subtracted the Q16.16
//     nudgeAmount straight from a frame count, which does not correspond to
//     any real unit conversion. This model reproduces the real per-sample
//     ramp + fixed-point accumulator instead.
struct FaithfulSpeedModifierModel {
	// Mirrors AudioSetup.hpp's BUFFER_SAMPLES, which sets SpeedModifier's
	// per-sample ramp step (RateScale / BUFFER_SAMPLES) and is used below as
	// the number of output samples simulated per controller tick (i.e. one
	// tick is modeled as one generate() call / one audio block).
	static constexpr uint32_t kBufferSamples = 256;
	static constexpr DjRate kRampStep = DJ_RATE_SCALE / kBufferSamples;

	DjRate requestedRate = DJ_RATE_NEUTRAL;
	DjRate currentRate = DJ_RATE_NEUTRAL;
	uint64_t sourcePositionFrac = 0; // Q16.16 accumulator, mirrors sourcePosition

	void setRate(DjRate rate){
		if(rate < DJ_RATE_MIN) rate = DJ_RATE_MIN;
		if(rate > DJ_RATE_MAX) rate = DJ_RATE_MAX;
		requestedRate = rate;
	}

	void nudgeRate(int32_t amount){
		int64_t nudged = int64_t(requestedRate) + amount;
		if(nudged <= int64_t(DJ_RATE_MIN)) setRate(DJ_RATE_MIN);
		else if(nudged >= int64_t(DJ_RATE_MAX)) setRate(DJ_RATE_MAX);
		else setRate(DjRate(nudged));
	}

	void advanceRate(){
		if(currentRate < requestedRate){
			DjRate remaining = requestedRate - currentRate;
			currentRate += remaining < kRampStep ? remaining : kRampStep;
		}else if(currentRate > requestedRate){
			DjRate remaining = currentRate - requestedRate;
			currentRate -= remaining < kRampStep ? remaining : kRampStep;
		}
	}

	// Simulates one generate() call's worth of output samples: ramps
	// currentRate toward requestedRate and accumulates source position one
	// sample at a time, exactly as the real generate()/advanceRate() do.
	// Returns the number of source frames actually consumed.
	int64_t consumeSourceFrames(uint32_t outputSamples){
		for(uint32_t i = 0; i < outputSamples; ++i){
			advanceRate();
			sourcePositionFrac += currentRate;
		}
		const int64_t consumed = int64_t(sourcePositionFrac >> 16);
		sourcePositionFrac &= 0xffff;
		return consumed;
	}
};

// Regression for the applyRate/nudge persistence bug: DjSyncController used
// to set applyRate unconditionally on every armed+valid+in-range tick. Since
// the real SpeedModifier::setRate() unconditionally overwrites requestedRate,
// that meant a nudge applied on tick N was silently erased by tick N+1's
// baseline re-command, defeating phase convergence entirely. Drives the
// controller through a faithful SpeedModifier model (real ramp + fixed-point
// accumulator, not a direct rate-to-frame subtraction) across multiple
// cooldown windows and proves the correction persists/accumulates and
// converges without oscillation or rate-command spam.
//
// Also covers a second, related regression found by post-lock review: the
// nudges that drive convergence are *permanent* additions to requestedRate
// (SpeedModifier::nudgeRate() never removes them), so once locked, the
// residual offset from the correction would otherwise stay baked in forever
// -- running the follower away from its true BPM-derived tempo and
// eventually forcing reverse nudges once it drifts back out past master.
// The controller now forces a one-shot baseline restoration exactly on the
// lock transition (and on hard align) instead of leaving that residual
// active, without reintroducing the original bug (i.e. it must NOT re-issue
// applyRate every tick while already locked/settled). This test drives well
// past the first lock and proves: phase stays within tolerance, requestedRate
// settles to and stays at the exact BPM-derived baseline (no leftover
// offset), no further nudges fire (no reverse corrections/oscillation), and
// no further applyRate calls occur (no setRate spam) during settling.
static void testSyncNudgePersistsAndConverges(){
	DjSyncController sync;
	FaithfulSpeedModifierModel model;
	DjSyncInputs in;
	in.armed = true;
	in.masterValid = true;
	in.followerValid = true;
	in.masterPlaying = true;
	in.followerPlaying = true;
	in.masterBpmMilli = 120000;
	in.followerBpmMilli = 120000;
	in.masterFramesPerBeat = 22050;
	in.followerFramesPerBeat = 22050;
	in.masterPhaseFrames = 0;
	in.followerPhaseFrames = 2000;

	const DjRate baselineTarget = DJ_RATE_NEUTRAL; // equal BPM => 1.0x baseline, never changes
	int64_t cumulativeNudge = 0;
	uint32_t nudgeCount = 0;
	int64_t lastNudgeMagnitude = INT64_MAX;
	// True once the one-shot baseline restoration has cancelled the nudge
	// residue (at lock or hard align); while true, requestedRate must be
	// exactly baselineTarget with no accumulated offset left over.
	bool baselineRestored = false;

	uint32_t lockTick = 0;
	bool locked = false;
	const uint32_t maxTicks = uint32_t(DJ_SYNC_NUDGE_COOLDOWN_TICKS) * 32;
	for(uint32_t tick = 0; tick < maxTicks; ++tick){
		DjSyncOutputs out = sync.tick(in);
		if(out.applyRate){
			assert(out.targetRate == baselineTarget);
			model.setRate(out.targetRate);
			if(nudgeCount > 0 && !baselineRestored){
				// One-shot cancellation of the nudge residue (lock or hard
				// align transition), not a per-tick re-command.
				cumulativeNudge = 0;
				baselineRestored = true;
			}
		}
		if(out.applyNudge){
			const int64_t magnitude = out.nudgeAmount < 0 ? -int64_t(out.nudgeAmount) : int64_t(out.nudgeAmount);
			assert(magnitude <= lastNudgeMagnitude); // strictly non-increasing: no oscillation/spam
			lastNudgeMagnitude = magnitude;
			model.nudgeRate(out.nudgeAmount);
			cumulativeNudge += out.nudgeAmount;
			++nudgeCount;
			baselineRestored = false; // a fresh correction is active again
		}

		// The critical regression check: once a baseline has been commanded,
		// every persisted nudge must remain visible in requestedRate until
		// explicitly (and only explicitly) cancelled. Before the first fix,
		// the very next tick's unconditional setRate(baselineTarget) erased
		// it here; before the second fix, it was never cancelled at all.
		if(nudgeCount > 0){
			const int64_t expected = baselineRestored
				? int64_t(baselineTarget) : int64_t(baselineTarget) + cumulativeNudge;
			assert(int64_t(model.requestedRate) == expected);
		}

		// Advance the follower's actual playback via the real ramp +
		// fixed-point accumulator (one tick == one BUFFER_SAMPLES-sized audio
		// block). Only the resulting extra/short source frames actually
		// consumed relative to nominal (neutral-rate) playback are physically
		// meaningful phase movement.
		const int64_t consumed = model.consumeSourceFrames(FaithfulSpeedModifierModel::kBufferSamples);
		in.followerPhaseFrames -= (consumed - int64_t(FaithfulSpeedModifierModel::kBufferSamples));

		if(!locked && out.state == DJ_SYNC_LOCKED){
			locked = true;
			lockTick = tick;
			break;
		}
	}

	assert(locked);
	assert(nudgeCount >= 2); // correction continued across multiple cooldown windows
	assert(in.followerPhaseFrames >= 0 &&
		uint64_t(in.followerPhaseFrames) <= DJ_SYNC_LOCK_TOLERANCE_FRAMES);
	assert(sync.state() == DJ_SYNC_LOCKED); // converged, not oscillating/spamming forever
	assert(baselineRestored); // the residual nudge offset was explicitly cancelled at lock
	assert(model.requestedRate == baselineTarget); // no leftover offset baked in

	// Exact result for this starting error/geometry, derived from the
	// faithful ramp+accumulator model above (not a naive rate-to-frame
	// analytic guess): 17 nudges of strictly non-increasing magnitude
	// (proving no oscillation) before the first lock/baseline-restore.
	assert(nudgeCount == 17);
	(void)lockTick;

	// Extend well past the first lock for a bounded settling interval and
	// prove the fix holds up over time, not just at the instant of lock:
	// phase stays within tolerance, requestedRate/currentRate stay pinned to
	// the exact baseline (no drift back off it), no further nudges fire
	// (no reverse corrections/oscillation), and no further applyRate calls
	// occur (no setRate spam) as long as nothing external changes.
	const int64_t phaseAtLock = in.followerPhaseFrames;
	const DjRate requestedAtLock = model.requestedRate;
	const DjRate currentAtLock = model.currentRate;
	for(uint32_t settleTick = 0; settleTick < uint32_t(DJ_SYNC_NUDGE_COOLDOWN_TICKS) * 4; ++settleTick){
		DjSyncOutputs out = sync.tick(in);
		assert(out.state == DJ_SYNC_LOCKED); // no oscillation back to ARMED/OUT_OF_RANGE
		assert(!out.applyNudge);             // no reverse corrective nudges
		assert(!out.applyRate);               // no setRate spam once settled
		const int64_t consumed = model.consumeSourceFrames(FaithfulSpeedModifierModel::kBufferSamples);
		in.followerPhaseFrames -= (consumed - int64_t(FaithfulSpeedModifierModel::kBufferSamples));
		assert(in.followerPhaseFrames >= 0 &&
			uint64_t(in.followerPhaseFrames) <= DJ_SYNC_LOCK_TOLERANCE_FRAMES);
		assert(model.requestedRate == baselineTarget);
	}
	assert(model.requestedRate == requestedAtLock);
	assert(model.currentRate == currentAtLock);
	assert(in.followerPhaseFrames == phaseAtLock); // fully settled, no residual drift
}

int main(){
	testCheckedMath();
	testGridBuildValidAndCapacity();
	testGridBuildDisablesOnInvalidInput();
	testQuantizeLatePolicy();
	testQuantizeTargetSnapsBackWithinTolerance();
	testBeatPhase();
	testLoopLengthsAndBoundaryWrap();
	testLoopEngineBoundsAndMask();
	testLoopEngineWrapAndBusyGuard();
	testLoopEngineRetryLimitDisengages();
	testLoopEngineReloop();
	testSyncOffAndError();
	testSyncOutOfRange();
	testSyncArmedHoldsUntilBothPlaying();
	testSyncLocksWithinTolerance();
	testSyncNudgesWithCooldownNoOscillation();
	testSyncHardAlignBeyondThreshold();
	testSyncLossOfMetadataGoesToError();
	testSyncNudgePersistsAndConverges();
	return 0;
}
