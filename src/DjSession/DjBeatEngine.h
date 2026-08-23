#ifndef JAYD_FIRMWARE_DJBEATENGINE_H
#define JAYD_FIRMWARE_DJBEATENGINE_H

#include <stddef.h>
#include <stdint.h>

// Firmware-owned beat-grid, quantize, loop, and sync engine.
//
// Everything in this header is pure logic: no SD/Arduino/library dependency,
// so it can be exercised by host self-checks exactly as it runs on-device.
// All frame<->beat conversion uses checked integer/rational math in
// quarter-beat units (a beat is always subdivided into 4 units, which exactly
// covers the supported quantize/loop resolutions of 1, 1/2 and 1/4 beat, and
// 1/2, 1, 2, 4 and 8 beats respectively) so there is no floating-point drift
// and no synthesized authority from estimated positions: a grid is only ever
// built from sparse anchors that were themselves already validated by the
// metadata reader.

static constexpr uint8_t DJ_GRID_ANCHOR_CAPACITY = 48;
static constexpr uint32_t DJ_GRID_MIN_BPM_MILLI = 40000;   // 40.000 BPM
static constexpr uint32_t DJ_GRID_MAX_BPM_MILLI = 220000;  // 220.000 BPM
static constexpr uint32_t DJ_GRID_MAX_SAMPLE_RATE = 768000;
static constexpr uint16_t DJ_GRID_MIN_CONFIDENCE = 2000;   // basis points (0-10000), i.e. >=20%
static constexpr int32_t DJ_QUARTER_BEATS_PER_BEAT = 4;

// Mirrors JayD-Library's SpeedModifier::Rate Q16.16 range/scale
// (0.5x-1.5x, neutral at 1.0x). Kept as plain constants here so this header
// has no dependency on the audio library and can be host-tested in isolation;
// DjSession.cpp asserts these match SpeedModifier's constants at compile time.
typedef uint32_t DjRate;
static constexpr DjRate DJ_RATE_SCALE = 1UL << 16;
static constexpr DjRate DJ_RATE_MIN = DJ_RATE_SCALE / 2;
static constexpr DjRate DJ_RATE_NEUTRAL = DJ_RATE_SCALE;
static constexpr DjRate DJ_RATE_MAX = DJ_RATE_SCALE + DJ_RATE_SCALE / 2;

// ---------------------------------------------------------------------------
// Checked integer helpers
// ---------------------------------------------------------------------------

inline bool djCheckedAdd(int64_t a, int64_t b, int64_t& result){
	if(b > 0 && a > INT64_MAX - b) return false;
	if(b < 0 && a < INT64_MIN - b) return false;
	result = a + b;
	return true;
}

inline bool djCheckedAddU(uint64_t a, uint64_t b, uint64_t& result){
	if(a > UINT64_MAX - b) return false;
	result = a + b;
	return true;
}

// result = floor(value * numerator / denominator), with an explicit overflow
// check on the intermediate product instead of relying on a wider integer
// type (this must behave identically on the 32-bit firmware target and on a
// 64-bit host running the self-checks).
inline bool djCheckedMulDiv(uint64_t value, uint64_t numerator, uint64_t denominator, uint64_t& result){
	if(denominator == 0) return false;
	if(numerator != 0 && value > UINT64_MAX / numerator) return false;
	result = (value * numerator) / denominator;
	return true;
}

inline bool djCheckedMulDivSigned(int64_t value, uint64_t numerator, uint64_t denominator, int64_t& result){
	if(denominator == 0) return false;
	const bool negative = value < 0;
	const uint64_t magnitude = negative ? uint64_t(-(value + 1)) + 1 : uint64_t(value);
	uint64_t unsignedResult;
	if(!djCheckedMulDiv(magnitude, numerator, denominator, unsignedResult)) return false;
	if(unsignedResult > uint64_t(INT64_MAX)) return false;
	result = negative ? -int64_t(unsignedResult) : int64_t(unsignedResult);
	return true;
}

// ---------------------------------------------------------------------------
// Beat grid
// ---------------------------------------------------------------------------

enum DjQuantizeResolution : uint8_t {
	DJ_QUANTIZE_OFF,
	DJ_QUANTIZE_BEAT_1,
	DJ_QUANTIZE_BEAT_1_2,
	DJ_QUANTIZE_BEAT_1_4,
	DJ_QUANTIZE_RESOLUTION_COUNT
};

// Quarter-beats per quantize boundary; 0 means "not a valid grid resolution".
inline int32_t djQuantizeQuarterBeats(DjQuantizeResolution resolution){
	switch(resolution){
		case DJ_QUANTIZE_BEAT_1: return 4;
		case DJ_QUANTIZE_BEAT_1_2: return 2;
		case DJ_QUANTIZE_BEAT_1_4: return 1;
		default: return 0;
	}
}

enum DjLoopLength : uint8_t {
	DJ_LOOP_BEAT_1_2,
	DJ_LOOP_BEAT_1,
	DJ_LOOP_BEAT_2,
	DJ_LOOP_BEAT_4,
	DJ_LOOP_BEAT_8,
	DJ_LOOP_LENGTH_COUNT
};

inline int32_t djLoopQuarterBeats(DjLoopLength length){
	switch(length){
		case DJ_LOOP_BEAT_1_2: return 2;
		case DJ_LOOP_BEAT_1: return 4;
		case DJ_LOOP_BEAT_2: return 8;
		case DJ_LOOP_BEAT_4: return 16;
		case DJ_LOOP_BEAT_8: return 32;
		default: return 0;
	}
}

struct DjGridAnchor {
	uint64_t frame = 0;
	int64_t quarterBeat = 0;
};

// Sparse, checked, monotonic frame<->beat model for a single loaded track.
// Anchors are cached at load time from the metadata reader's own ordered grid
// section (never synthesized), subsampled to a bounded capacity. Tempo is
// treated as the track's single global BPM between cached anchors: local
// per-anchor tempo variation between two subsampled anchors is not modeled in
// this scope (documented simplification; see PR description).
class DjBeatGrid {
public:
	void reset(){
		anchorCount_ = 0;
		sampleRate_ = 0;
		bpmMilli_ = 0;
		durationFrames_ = 0;
		valid_ = false;
	}

	// Returns false (and leaves the grid invalid/disabled) whenever the
	// inputs are missing, out of the supported BPM/rate range, or the anchors
	// are not strictly increasing in both frame and quarter-beat -- i.e. any
	// ambiguity or inconsistency disables the capability rather than risking
	// a wrong quantize/loop/sync decision.
	bool build(uint32_t sampleRate, uint32_t bpmMilli, uint64_t durationFrames,
			   const DjGridAnchor* anchors, uint16_t anchorCount){
		reset();
		if(sampleRate == 0 || sampleRate > DJ_GRID_MAX_SAMPLE_RATE) return false;
		if(bpmMilli < DJ_GRID_MIN_BPM_MILLI || bpmMilli > DJ_GRID_MAX_BPM_MILLI) return false;
		if(anchorCount == 0 || anchors == nullptr) return false;

		const uint16_t count = anchorCount > DJ_GRID_ANCHOR_CAPACITY ? DJ_GRID_ANCHOR_CAPACITY : anchorCount;
		for(uint16_t i = 0; i < count; ++i){
			if(durationFrames != 0 && anchors[i].frame > durationFrames) return false;
			if(i > 0){
				if(anchors[i].frame <= anchors[i - 1].frame) return false;
				if(anchors[i].quarterBeat <= anchors[i - 1].quarterBeat) return false;
			}
			anchors_[i] = anchors[i];
		}

		sampleRate_ = sampleRate;
		bpmMilli_ = bpmMilli;
		durationFrames_ = durationFrames;
		anchorCount_ = count;
		valid_ = true;
		return true;
	}

	bool valid() const{ return valid_; }
	uint32_t sampleRate() const{ return sampleRate_; }
	uint32_t bpmMilli() const{ return bpmMilli_; }

	// Exact frames-per-quarter-beat as a rational: framesPerQuarterBeatNumerator / bpmMilli.
	uint64_t framesPerQuarterBeatNumerator() const{
		return uint64_t(sampleRate_) * 15000ULL;
	}

	// Enclosing quarter-beat and its exact boundary frame for a given source frame.
	bool quarterBeatAtFrame(uint64_t frame, int64_t& quarterBeat, uint64_t& boundaryFrame) const{
		if(!valid_) return false;
		const uint16_t anchorIndex = findAnchorAtOrBefore(frame);
		const DjGridAnchor& anchor = anchors_[anchorIndex];
		if(frame < anchor.frame) return false;

		uint64_t offset;
		if(!djCheckedMulDiv(frame - anchor.frame, bpmMilli_, framesPerQuarterBeatNumerator(), offset)) return false;
		if(offset > uint64_t(INT64_MAX)) return false;
		if(!djCheckedAdd(anchor.quarterBeat, int64_t(offset), quarterBeat)) return false;
		return frameAtQuarterBeat(quarterBeat, boundaryFrame);
	}

	// Exact boundary frame for an absolute quarter-beat index.
	bool frameAtQuarterBeat(int64_t quarterBeat, uint64_t& frame) const{
		if(!valid_) return false;
		const uint16_t anchorIndex = findAnchorAtOrBeforeQuarterBeat(quarterBeat);
		const DjGridAnchor& anchor = anchors_[anchorIndex];
		const int64_t quarterBeatOffset = quarterBeat - anchor.quarterBeat;
		if(quarterBeatOffset < 0) return false; // before the first anchor: unsupported

		uint64_t offsetFrames;
		if(!djCheckedMulDiv(uint64_t(quarterBeatOffset), framesPerQuarterBeatNumerator(), bpmMilli_, offsetFrames)){
			return false;
		}
		if(!djCheckedAddU(anchor.frame, offsetFrames, frame)) return false;
		if(durationFrames_ != 0 && frame > durationFrames_) return false;
		return true;
	}

	// Next boundary at or after `frame`, on a grid of `quarterBeatStep`
	// quarter-beats (e.g. 4 for a whole-beat quantize/loop resolution).
	bool nextBoundary(uint64_t frame, int32_t quarterBeatStep, uint64_t& boundaryFrame) const{
		if(!valid_ || quarterBeatStep <= 0) return false;
		int64_t currentQuarterBeat;
		uint64_t currentBoundaryFrame;
		if(!quarterBeatAtFrame(frame, currentQuarterBeat, currentBoundaryFrame)) return false;

		// Round currentQuarterBeat up to the next multiple of quarterBeatStep
		// measured from the anchor grid's own origin (quarter-beat 0).
		int64_t steppedQuarterBeat = (currentQuarterBeat / quarterBeatStep) * quarterBeatStep;
		if(steppedQuarterBeat < 0 && currentQuarterBeat % quarterBeatStep != 0) steppedQuarterBeat -= quarterBeatStep;
		uint64_t candidateFrame;
		if(!frameAtQuarterBeat(steppedQuarterBeat, candidateFrame)) return false;
		if(candidateFrame < frame){
			if(!djCheckedAdd(steppedQuarterBeat, quarterBeatStep, steppedQuarterBeat)) return false;
			if(!frameAtQuarterBeat(steppedQuarterBeat, candidateFrame)) return false;
		}
		boundaryFrame = candidateFrame;
		return true;
	}

	// Boundary at or before `frame` (the mirror of nextBoundary), used by the
	// late-quantize policy to detect "we just passed a boundary".
	bool previousBoundary(uint64_t frame, int32_t quarterBeatStep, uint64_t& boundaryFrame) const{
		if(!valid_ || quarterBeatStep <= 0) return false;
		int64_t currentQuarterBeat;
		uint64_t currentBoundaryFrame;
		if(!quarterBeatAtFrame(frame, currentQuarterBeat, currentBoundaryFrame)) return false;

		int64_t steppedQuarterBeat = (currentQuarterBeat / quarterBeatStep) * quarterBeatStep;
		if(steppedQuarterBeat > currentQuarterBeat) steppedQuarterBeat -= quarterBeatStep; // floor for negatives
		uint64_t candidateFrame;
		if(!frameAtQuarterBeat(steppedQuarterBeat, candidateFrame)) return false;
		if(candidateFrame > frame){
			if(!djCheckedAdd(steppedQuarterBeat, -int64_t(quarterBeatStep), steppedQuarterBeat)) return false;
			if(!frameAtQuarterBeat(steppedQuarterBeat, candidateFrame)) return false;
		}
		boundaryFrame = candidateFrame;
		return true;
	}

private:
	uint16_t findAnchorAtOrBefore(uint64_t frame) const{
		uint16_t low = 0, high = anchorCount_;
		while(low + 1 < high){
			const uint16_t mid = low + (high - low) / 2;
			if(anchors_[mid].frame <= frame) low = mid; else high = mid;
		}
		return low;
	}

	uint16_t findAnchorAtOrBeforeQuarterBeat(int64_t quarterBeat) const{
		uint16_t low = 0, high = anchorCount_;
		while(low + 1 < high){
			const uint16_t mid = low + (high - low) / 2;
			if(anchors_[mid].quarterBeat <= quarterBeat) low = mid; else high = mid;
		}
		return low;
	}

	DjGridAnchor anchors_[DJ_GRID_ANCHOR_CAPACITY] = {};
	uint16_t anchorCount_ = 0;
	uint32_t sampleRate_ = 0;
	uint32_t bpmMilli_ = 0;
	uint64_t durationFrames_ = 0;
	bool valid_ = false;
};

// ---------------------------------------------------------------------------
// Quantize scheduling decision (pure logic, no timers/hardware)
// ---------------------------------------------------------------------------

static constexpr uint32_t DJ_QUANTIZE_TOLERANCE_FRAMES = 256; // one audio output block

// One whole-beat step, expressed in quarter-beats (this module's atomic unit).
static constexpr int32_t DJ_BEAT_QUARTER_BEATS = 4;

// Current position within the enclosing beat (0 <= phaseFrames < framesPerBeat)
// plus the exact length of that beat in frames. Shared by both the quantize
// late-policy helper below and DjSession's sync-phase wiring so beat-phase is
// computed identically everywhere.
inline bool djBeatPhase(const DjBeatGrid& grid, uint64_t frame, int64_t& phaseFrames, uint64_t& framesPerBeat){
	int64_t quarterBeat;
	uint64_t boundaryFrame;
	if(!grid.quarterBeatAtFrame(frame, quarterBeat, boundaryFrame)) return false;

	int64_t beatQuarterBeat = (quarterBeat / DJ_BEAT_QUARTER_BEATS) * DJ_BEAT_QUARTER_BEATS;
	if(beatQuarterBeat > quarterBeat) beatQuarterBeat -= DJ_BEAT_QUARTER_BEATS; // floor for negatives
	int64_t nextBeatQuarterBeat;
	if(!djCheckedAdd(beatQuarterBeat, DJ_BEAT_QUARTER_BEATS, nextBeatQuarterBeat)) return false;

	uint64_t beatFrame, nextBeatFrame;
	if(!grid.frameAtQuarterBeat(beatQuarterBeat, beatFrame)) return false;
	if(!grid.frameAtQuarterBeat(nextBeatQuarterBeat, nextBeatFrame)) return false;
	if(nextBeatFrame <= beatFrame) return false;

	framesPerBeat = nextBeatFrame - beatFrame;
	phaseFrames = int64_t(frame - beatFrame);
	return true;
}

enum DjQuantizeDecision : uint8_t {
	DJ_QUANTIZE_APPLY_NOW,
	DJ_QUANTIZE_SCHEDULE
};


// Decides whether a quantized action should apply immediately (already at or
// within tolerance of a boundary) or be scheduled for a future boundary.
inline DjQuantizeDecision djQuantizeDecide(uint64_t currentFrame, uint64_t targetFrame){
	const uint64_t distance = targetFrame > currentFrame ? targetFrame - currentFrame : 0;
	return distance <= DJ_QUANTIZE_TOLERANCE_FRAMES ? DJ_QUANTIZE_APPLY_NOW : DJ_QUANTIZE_SCHEDULE;
}

// Full late policy: chooses the boundary just behind `frame` when it is
// within one audio block's tolerance (we effectively just crossed it, so
// snap back rather than waiting almost a whole step for the next one),
// otherwise the next boundary ahead. Used for both play-start (paused deck,
// applied synchronously) and loop engage/reloop (playing deck, applied once
// the target frame is reached).
inline bool djQuantizeTarget(const DjBeatGrid& grid, uint64_t frame, int32_t quarterBeatStep, uint64_t& target){
	uint64_t next = 0;
	if(!grid.nextBoundary(frame, quarterBeatStep, next)) return false;
	uint64_t prev = 0;
	if(grid.previousBoundary(frame, quarterBeatStep, prev) &&
	   frame - prev <= DJ_QUANTIZE_TOLERANCE_FRAMES){
		target = prev;
		return true;
	}
	target = next;
	return true;
}

// ---------------------------------------------------------------------------
// Loop state
// ---------------------------------------------------------------------------

enum DjLoopState : uint8_t {
	DJ_LOOP_INACTIVE,
	DJ_LOOP_PENDING,
	DJ_LOOP_ACTIVE
};

static constexpr uint8_t DJ_LOOP_SEEK_RETRY_LIMIT = 3;

struct DjLoopSnapshot {
	DjLoopState state = DJ_LOOP_INACTIVE;
	DjLoopLength length = DJ_LOOP_BEAT_1;
	uint64_t startFrame = 0;
	uint64_t endFrame = 0;
	uint8_t validLengthMask = 0;
};

// Shared bounds computation used both to validate an actual engage() and to
// report which lengths are currently possible (EOF/track-bounds check) for
// the snapshot's valid-length mask, so the two can never disagree.
inline bool djComputeLoopBounds(const DjBeatGrid& grid, uint64_t currentFrame, int32_t quarterBeats,
								  uint64_t durationFrames, uint64_t& startFrame, uint64_t& endFrame){
	if(!grid.valid() || quarterBeats <= 0) return false;
	if(!djQuantizeTarget(grid, currentFrame, quarterBeats, startFrame)) return false;
	int64_t startQuarterBeat;
	uint64_t recomputedStart;
	if(!grid.quarterBeatAtFrame(startFrame, startQuarterBeat, recomputedStart)) return false;
	if(!djCheckedAdd(startQuarterBeat, quarterBeats, startQuarterBeat)) return false;
	if(!grid.frameAtQuarterBeat(startQuarterBeat, endFrame)) return false;
	if(durationFrames != 0 && endFrame > durationFrames) return false; // impossible length: past EOF
	return endFrame > startFrame;
}

inline uint8_t djValidLoopLengthMask(const DjBeatGrid& grid, uint64_t currentFrame, uint64_t durationFrames){
	uint8_t mask = 0;
	for(uint8_t i = 0; i < DJ_LOOP_LENGTH_COUNT; ++i){
		uint64_t start, end;
		if(djComputeLoopBounds(grid, currentFrame, djLoopQuarterBeats(static_cast<DjLoopLength>(i)),
								durationFrames, start, end)){
			mask |= uint8_t(1u << i);
		}
	}
	return mask;
}

// Deterministic per-deck loop state machine: bounds-checked engage, a single
// pending seek in flight at a time (never a busy-loop of repeated seek
// attempts), and a bounded retry count before giving up and disengaging.
class DjLoopEngine {
public:
	void reset(){
		state_ = DJ_LOOP_INACTIVE;
		length_ = DJ_LOOP_BEAT_1;
		startFrame_ = 0;
		endFrame_ = 0;
		seekInFlight_ = false;
		seekRetries_ = 0;
		pendingCommandId_ = 0;
	}

	DjLoopState state() const{ return state_; }
	uint64_t startFrame() const{ return startFrame_; }
	uint64_t endFrame() const{ return endFrame_; }

	// Validates the requested length against the grid/current position and
	// EOF/track bounds. On success the loop becomes PENDING at the next
	// length-aligned boundary; the caller (which knows whether the deck is
	// playing/paused) decides via djQuantizeDecide whether that boundary
	// counts as "now" or a genuine deferral, but either way the seek is only
	// actually issued once via needsSeek()/beginSeek() below.
	bool engage(const DjBeatGrid& grid, uint64_t currentFrame, uint64_t durationFrames,
				DjLoopLength length, uint32_t commandId){
		if(length >= DJ_LOOP_LENGTH_COUNT) return false;
		uint64_t start, end;
		if(!djComputeLoopBounds(grid, currentFrame, djLoopQuarterBeats(length), durationFrames, start, end)){
			return false;
		}
		state_ = DJ_LOOP_PENDING;
		length_ = length;
		startFrame_ = start;
		endFrame_ = end;
		seekInFlight_ = false;
		seekRetries_ = 0;
		pendingCommandId_ = commandId;
		return true;
	}

	// Forces an immediate jump back to the loop's own start; only valid while
	// a loop is already active.
	bool reloop(uint32_t commandId){
		if(state_ != DJ_LOOP_ACTIVE) return false;
		state_ = DJ_LOOP_PENDING;
		seekInFlight_ = false;
		seekRetries_ = 0;
		pendingCommandId_ = commandId;
		return true;
	}

	void disengage(){
		state_ = DJ_LOOP_INACTIVE;
		seekInFlight_ = false;
		seekRetries_ = 0;
		pendingCommandId_ = 0;
	}

	// Returns true at most once per boundary crossing (initial pending start,
	// or end-of-loop wrap) and only when no seek is already outstanding.
	bool needsSeek(uint64_t currentFrame, uint64_t& targetFrame) const{
		if(seekInFlight_) return false;
		if(state_ == DJ_LOOP_PENDING && currentFrame >= startFrame_){
			targetFrame = startFrame_;
			return true;
		}
		if(state_ == DJ_LOOP_ACTIVE && currentFrame >= endFrame_){
			targetFrame = startFrame_;
			return true;
		}
		return false;
	}

	void beginSeek(){
		seekInFlight_ = true;
	}

	// Reports the outcome of the seek requested via needsSeek(); on repeated
	// failure beyond the retry limit, disengages rather than retrying forever.
	void completeSeek(bool success){
		seekInFlight_ = false;
		if(success){
			seekRetries_ = 0;
			state_ = DJ_LOOP_ACTIVE;
			return;
		}
		if(++seekRetries_ >= DJ_LOOP_SEEK_RETRY_LIMIT) disengage();
	}

	bool hasPendingCommand() const{ return pendingCommandId_ != 0; }

	// Peek without consuming (used by the tick loop, which must decide
	// whether to report a result only once the seek's outcome is known).
	uint32_t pendingCommandId() const{ return pendingCommandId_; }

	void clearPendingCommandId(){ pendingCommandId_ = 0; }

	uint32_t consumePendingCommandId(){
		const uint32_t id = pendingCommandId_;
		pendingCommandId_ = 0;
		return id;
	}

	DjLoopSnapshot snapshot(uint8_t validLengthMask) const{
		DjLoopSnapshot out;
		out.state = state_;
		out.length = length_;
		out.startFrame = startFrame_;
		out.endFrame = endFrame_;
		out.validLengthMask = validLengthMask;
		return out;
	}

private:
	DjLoopState state_ = DJ_LOOP_INACTIVE;
	DjLoopLength length_ = DJ_LOOP_BEAT_1;
	uint64_t startFrame_ = 0;
	uint64_t endFrame_ = 0;
	bool seekInFlight_ = false;
	uint8_t seekRetries_ = 0;
	uint32_t pendingCommandId_ = 0;
};

// ---------------------------------------------------------------------------
// Sync engine (pure logic)
// ---------------------------------------------------------------------------

enum DjSyncState : uint8_t {
	DJ_SYNC_OFF,
	DJ_SYNC_ARMED,
	DJ_SYNC_LOCKED,
	DJ_SYNC_OUT_OF_RANGE,
	DJ_SYNC_ERROR
};

static constexpr uint64_t DJ_SYNC_LOCK_TOLERANCE_FRAMES = 64;
static constexpr uint32_t DJ_SYNC_HARD_ALIGN_THRESHOLD_MS = 250;
static constexpr int32_t DJ_SYNC_MAX_NUDGE = DJ_RATE_SCALE / 100; // +-1% rate
static constexpr uint8_t DJ_SYNC_NUDGE_COOLDOWN_TICKS = 8;

struct DjSyncInputs {
	bool armed = false;
	bool masterValid = false;   // master deck loaded, grid valid, BPM present
	bool masterPlaying = false;
	bool followerValid = false; // follower deck loaded, grid valid, BPM present
	bool followerPlaying = false;
	uint32_t masterBpmMilli = 0;
	uint32_t followerBpmMilli = 0;
	uint32_t followerSampleRate = 0;
	int64_t masterPhaseFrames = 0;   // frame offset since the master's last beat boundary
	uint64_t masterFramesPerBeat = 0;
	int64_t followerPhaseFrames = 0; // frame offset since the follower's last beat boundary
	uint64_t followerFramesPerBeat = 0;
};

struct DjSyncOutputs {
	DjSyncState state = DJ_SYNC_OFF;
	bool applyRate = false;
	DjRate targetRate = DJ_RATE_NEUTRAL;
	bool applyNudge = false;
	int32_t nudgeAmount = 0;
	bool hardAlign = false;
	int64_t hardAlignPhaseErrorFrames = 0;
};

// Deterministic, bounded, checked-integer sync controller. Called at most
// once per firmware loop tick per follower deck; never blocks and never
// starts playback (that remains a semantic command's job).
class DjSyncController {
public:
	void reset(){
		state_ = DJ_SYNC_OFF;
		ticksSinceNudge_ = DJ_SYNC_NUDGE_COOLDOWN_TICKS;
		hasCommandedRate_ = false;
		correctionActive_ = false;
	}

	DjSyncState state() const{ return state_; }

	DjSyncOutputs tick(const DjSyncInputs& in){
		DjSyncOutputs out;
		++ticksSinceNudge_;

		if(!in.armed){
			state_ = DJ_SYNC_OFF;
			out.state = state_;
			hasCommandedRate_ = false;
			correctionActive_ = false;
			return out;
		}

		if(!in.masterValid || !in.followerValid || in.followerBpmMilli == 0){
			state_ = DJ_SYNC_ERROR;
			out.state = state_;
			hasCommandedRate_ = false;
			correctionActive_ = false;
			return out;
		}

		uint64_t targetRateWide;
		if(!djCheckedMulDiv(in.masterBpmMilli, DJ_RATE_SCALE, in.followerBpmMilli, targetRateWide) ||
			targetRateWide > UINT32_MAX){
			state_ = DJ_SYNC_ERROR;
			out.state = state_;
			hasCommandedRate_ = false;
			correctionActive_ = false;
			return out;
		}
		const DjRate targetRate = DjRate(targetRateWide);

		if(targetRate < DJ_RATE_MIN || targetRate > DJ_RATE_MAX){
			state_ = DJ_SYNC_OUT_OF_RANGE;
			out.state = state_;
			hasCommandedRate_ = false;
			correctionActive_ = false;
			return out;
		}

		// The real SpeedModifier::setRate() unconditionally overwrites
		// requestedRate, which would silently erase an in-flight nudge on the
		// very next tick if we re-commanded the same baseline every tick. Only
		// emit applyRate when the commanded baseline genuinely changes (or has
		// never been established since the last OFF/ERROR/OUT_OF_RANGE reset),
		// so a bounded nudge below can persist and accumulate across ticks.
		if(!hasCommandedRate_ || commandedRate_ != targetRate){
			out.applyRate = true;
			out.targetRate = targetRate;
			hasCommandedRate_ = true;
			commandedRate_ = targetRate;
			// A brand-new baseline overwrites requestedRate outright, so any
			// nudge residue from a prior correction is moot -- nothing left
			// to explicitly cancel.
			correctionActive_ = false;
		}

		if(!in.masterPlaying || !in.followerPlaying){
			// Not both transports running yet: hold the rate, do not chase phase.
			state_ = DJ_SYNC_ARMED;
			out.state = state_;
			return out;
		}

		int64_t phaseErrorFollowerFrames = 0;
		const bool phaseKnown = computePhaseError(in, phaseErrorFollowerFrames);

		if(!phaseKnown){
			state_ = DJ_SYNC_ARMED;
			out.state = state_;
			return out;
		}

		const uint64_t magnitude = phaseErrorFollowerFrames < 0
			? uint64_t(-phaseErrorFollowerFrames) : uint64_t(phaseErrorFollowerFrames);

		if(magnitude <= DJ_SYNC_LOCK_TOLERANCE_FRAMES){
			state_ = DJ_SYNC_LOCKED;
			out.state = state_;
			// Phase has converged. Any nudges that got us here are baked
			// permanently into the real requestedRate (SpeedModifier::
			// nudgeRate() only ever adds to it, never removes); left alone,
			// that residual offset would run the follower away from its true
			// BPM-derived tempo forever and eventually force reverse nudges
			// once it drifts back out past master. Restore the exact
			// baseline exactly once on this lock transition -- not every
			// tick, so a future genuine drift can still nudge again.
			if(correctionActive_){
				out.applyRate = true;
				out.targetRate = targetRate;
				hasCommandedRate_ = true;
				commandedRate_ = targetRate;
				correctionActive_ = false;
			}
			return out;
		}

		const uint64_t hardAlignThresholdFrames =
			uint64_t(in.followerSampleRate) * DJ_SYNC_HARD_ALIGN_THRESHOLD_MS / 1000;
		if(hardAlignThresholdFrames != 0 && magnitude > hardAlignThresholdFrames){
			out.hardAlign = true;
			out.hardAlignPhaseErrorFrames = phaseErrorFollowerFrames;
			state_ = DJ_SYNC_ARMED;
			out.state = state_;
			ticksSinceNudge_ = 0;
			// The upcoming seek corrects position directly; a leftover rate
			// nudge from before the hard align would immediately start
			// drifting the (now-aligned) phase again in the wrong direction.
			// Cancel it the same way as at lock.
			if(correctionActive_){
				out.applyRate = true;
				out.targetRate = targetRate;
				hasCommandedRate_ = true;
				commandedRate_ = targetRate;
				correctionActive_ = false;
			}
			return out;
		}

		state_ = DJ_SYNC_ARMED;
		out.state = state_;
		if(ticksSinceNudge_ >= DJ_SYNC_NUDGE_COOLDOWN_TICKS){
			int64_t nudge = phaseErrorFollowerFrames / 4; // gentle proportional correction
			if(nudge > DJ_SYNC_MAX_NUDGE) nudge = DJ_SYNC_MAX_NUDGE;
			if(nudge < -DJ_SYNC_MAX_NUDGE) nudge = -DJ_SYNC_MAX_NUDGE;
			if(nudge != 0){
				out.applyNudge = true;
				out.nudgeAmount = int32_t(nudge);
				ticksSinceNudge_ = 0;
				correctionActive_ = true;
			}
		}
		return out;
	}

private:
	// Maps the master's current beat phase onto the follower's timeline and
	// returns the signed, wrapped (to +-half a follower beat) frame error.
	static bool computePhaseError(const DjSyncInputs& in, int64_t& errorFrames){
		if(in.masterFramesPerBeat == 0 || in.followerFramesPerBeat == 0) return false;

		int64_t masterPhaseOnFollower;
		if(!djCheckedMulDivSigned(in.masterPhaseFrames, in.followerFramesPerBeat,
								   in.masterFramesPerBeat, masterPhaseOnFollower)){
			return false;
		}

		int64_t error;
		if(!djCheckedAdd(in.followerPhaseFrames, -masterPhaseOnFollower, error)) return false;

		const int64_t framesPerBeat = int64_t(in.followerFramesPerBeat);
		const int64_t half = framesPerBeat / 2;
		error %= framesPerBeat;
		if(error > half) error -= framesPerBeat;
		if(error < -half) error += framesPerBeat;
		errorFrames = error;
		return true;
	}

	DjSyncState state_ = DJ_SYNC_OFF;
	uint8_t ticksSinceNudge_ = DJ_SYNC_NUDGE_COOLDOWN_TICKS;
	bool hasCommandedRate_ = false;
	DjRate commandedRate_ = DJ_RATE_NEUTRAL;
	// True once a nudge has biased the real requestedRate away from the
	// pure BPM-derived baseline; cleared exactly once (at the next lock,
	// hard align, or baseline/target change) when that bias is explicitly
	// cancelled -- never per-tick, so it doesn't fight a still-converging
	// correction.
	bool correctionActive_ = false;
};

#endif
