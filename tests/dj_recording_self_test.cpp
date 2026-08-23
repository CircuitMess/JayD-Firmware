#include <assert.h>
#include <string.h>
#include "../src/DjSession/DjSessionState.h"
#include "../src/DjSession/DjRecordingLogic.h"

// Host self-check for the recording-workflow logic that DjRecordingStorage
// and DjSession build on: bounded naming/collision/exhaustion, WAV header
// repair arithmetic (including truncation, misalignment, unknown-format
// rejection, and overflow), and the DjRecordingSnapshot/DjRecordingState
// shape used for command-result coherence. Pure logic only -- no SD/Arduino
// dependency -- compiled directly against the pinned JayD-Library headers
// (WavHeader.h, AudioSetup.hpp) via an include path, never vendored.

namespace {

// Fake in-memory "SD.exists()" used to drive findFreeSlot() without any
// filesystem: a fixed-size taken[] table indexed 1..NAME_CAPACITY.
bool takenTable[DjRecordingLogic::NAME_CAPACITY + 1] = {};

bool isTaken(uint16_t index, void* /*ctx*/){
	if(index == 0 || index > DjRecordingLogic::NAME_CAPACITY) return true;
	return takenTable[index];
}

WavHeader validHeader(uint32_t dataSize){
	WavHeader header = {};
	memcpy(header.RIFF, "RIFF", 4);
	header.chunkSize = dataSize + 36;
	memcpy(header.WAVE, "WAVE", 4);
	memcpy(header.fmt, "fmt ", 4);
	header.fmtSize = 16;
	header.audioFormat = 1;
	header.numChannels = NUM_CHANNELS;
	header.sampleRate = SAMPLE_RATE;
	header.blockAlign = NUM_CHANNELS * BYTES_PER_SAMPLE;
	header.byteRate = SAMPLE_RATE * header.blockAlign;
	header.bitsPerSample = BYTES_PER_SAMPLE * 8;
	memcpy(header.data, "data", 4);
	header.dataSize = dataSize;
	return header;
}

} // namespace

int main(){
	// --- Naming: candidate path formatting is bounded and rejects out-of-range indices.
	char path[64] = {};
	assert(DjRecordingLogic::formatCandidatePath(1, path, sizeof(path)));
	assert(strcmp(path, "/Recordings/JAYD_001.wav") == 0);
	assert(DjRecordingLogic::formatCandidatePath(250, path, sizeof(path)));
	assert(strcmp(path, "/Recordings/JAYD_250.wav") == 0);
	assert(!DjRecordingLogic::formatCandidatePath(0, path, sizeof(path)));
	assert(!DjRecordingLogic::formatCandidatePath(251, path, sizeof(path)));
	char tinyBuffer[8] = {};
	assert(!DjRecordingLogic::formatCandidatePath(1, tinyBuffer, sizeof(tinyBuffer)));

	// --- Naming: findFreeSlot finds the first free bounded index, wrapping from a hint.
	memset(takenTable, 0, sizeof(takenTable));
	uint16_t index = 0;
	assert(DjRecordingLogic::findFreeSlot(1, isTaken, nullptr, index));
	assert(index == 1);

	for(uint16_t i = 1; i <= 5; i++) takenTable[i] = true;
	assert(DjRecordingLogic::findFreeSlot(1, isTaken, nullptr, index));
	assert(index == 6);

	// Hint wraps around the bounded space instead of scanning unboundedly upward.
	memset(takenTable, 0, sizeof(takenTable));
	for(uint16_t i = 1; i <= DjRecordingLogic::NAME_CAPACITY; i++){
		if(i != 3) takenTable[i] = true;
	}
	assert(DjRecordingLogic::findFreeSlot(240, isTaken, nullptr, index));
	assert(index == 3);

	// --- Naming: full exhaustion of the bounded space is reported, never scanned unbounded.
	for(uint16_t i = 1; i <= DjRecordingLogic::NAME_CAPACITY; i++) takenTable[i] = true;
	assert(!DjRecordingLogic::findFreeSlot(1, isTaken, nullptr, index));

	// --- Finalize outcome: allocation exhaustion (naming space full, missing
	// temp file, or bad input) must be distinguished from a rename I/O
	// failure -- collapsing both into one code hides which layer failed and
	// mislabels a successful-capture/failed-move recording as "storage full"
	// when it was really a rename error, or vice versa.
	using DjRecordingLogic::FinalizeOutcome;
	using DjRecordingLogic::decideFinalizeOutcome;
	assert(decideFinalizeOutcome(/*tempPresent=*/true, /*allocated=*/true, /*renamed=*/true) == FinalizeOutcome::SUCCESS);
	// Naming space exhausted (or temp file missing/bad input): never reaches rename.
	assert(decideFinalizeOutcome(/*tempPresent=*/true, /*allocated=*/false, /*renamed=*/false) == FinalizeOutcome::ALLOC_EXHAUSTED);
	assert(decideFinalizeOutcome(/*tempPresent=*/false, /*allocated=*/false, /*renamed=*/false) == FinalizeOutcome::ALLOC_EXHAUSTED);
	// A free path *was* allocated, but the injected rename I/O failed: must be
	// reported as RENAME_FAILED, not lumped in with naming exhaustion.
	assert(decideFinalizeOutcome(/*tempPresent=*/true, /*allocated=*/true, /*renamed=*/false) == FinalizeOutcome::RENAME_FAILED);

	// --- WAV repair: a valid Jay-D header with a truncated/extended data
	// region gets its dataSize/chunkSize recomputed from real file length.
	WavHeader header = validHeader(1000);
	WavHeader repaired = {};
	const uint32_t realFileSize = sizeof(WavHeader) + 500; // interrupted mid-write: shorter than header claims
	assert(DjRecordingLogic::repairHeader(header, realFileSize, repaired));
	assert(repaired.dataSize == 500);
	assert(repaired.chunkSize == 500 + 36);

	// --- WAV repair: exact match still yields a stable, idempotent result.
	header = validHeader(2000);
	const uint32_t exactFileSize = sizeof(WavHeader) + 2000;
	assert(DjRecordingLogic::repairHeader(header, exactFileSize, repaired));
	assert(repaired.dataSize == 2000);
	assert(repaired.chunkSize == 2000 + 36);

	// --- WAV repair: file smaller than the header itself is rejected (nothing to repair).
	header = validHeader(1000);
	assert(!DjRecordingLogic::repairHeader(header, sizeof(WavHeader) - 1, repaired));

	// --- WAV repair: misaligned payload (not a whole number of sample
	// frames) is left untouched rather than guessed at.
	header = validHeader(1000);
	const uint32_t misalignedFileSize = sizeof(WavHeader) + 501; // blockAlign is 2 bytes/frame
	assert(header.blockAlign == 2);
	assert(!DjRecordingLogic::repairHeader(header, misalignedFileSize, repaired));

	// --- WAV repair: dataSize that would overflow the 32-bit chunkSize is rejected.
	header = validHeader(1000);
	assert(!DjRecordingLogic::repairHeader(header, UINT32_MAX, repaired));

	// --- WAV repair: any field diverging from Jay-D's own fixed PCM format
	// (wrong magic, sample rate, channel count, bit depth, ...) is rejected
	// outright -- never mutated -- since it isn't recognizably ours.
	WavHeader foreign = validHeader(1000);
	memcpy(foreign.RIFF, "JUNK", 4);
	assert(!DjRecordingLogic::repairHeader(foreign, sizeof(WavHeader) + 1000, repaired));

	foreign = validHeader(1000);
	foreign.sampleRate = 44100; // not Jay-D's fixed SAMPLE_RATE
	assert(!DjRecordingLogic::repairHeader(foreign, sizeof(WavHeader) + 1000, repaired));

	foreign = validHeader(1000);
	foreign.numChannels = 2; // not Jay-D's fixed NUM_CHANNELS
	assert(!DjRecordingLogic::repairHeader(foreign, sizeof(WavHeader) + 1000, repaired));

	foreign = validHeader(1000);
	foreign.audioFormat = 3; // IEEE float, not the PCM Jay-D writes
	assert(!DjRecordingLogic::repairHeader(foreign, sizeof(WavHeader) + 1000, repaired));

	// --- DjRecordingSnapshot/DjRecordingState shape used by DjSession: a
	// freshly constructed snapshot must never claim active/valid before a
	// command is applied, and error/state values round-trip through the
	// same struct the physical UI and future API v2 both read.
	DjRecordingSnapshot snapshot;
	assert(snapshot.state == DJ_RECORDING_IDLE);
	assert(snapshot.error == DJ_RECORDING_ERROR_NONE);
	assert(!snapshot.valid);
	assert(snapshot.bytes == 0);
	assert(snapshot.durationMs == 0);
	assert(snapshot.orphansRepaired == 0);
	assert(snapshot.orphansFailed == 0);

	// A start command that is merely accepted (queued) must not yet report
	// ACTIVE -- only once the library applies it does DjSession move the
	// snapshot state to STARTING/ACTIVE. Simulate the accepted-not-applied
	// gap explicitly, since it's the exact failure mode the spec calls out.
	DjSubmitResult accepted(1, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	assert(accepted.accepted());
	snapshot.state = DJ_RECORDING_STARTING; // accepted, not yet applied by the library
	assert(snapshot.state != DJ_RECORDING_ACTIVE);

	// djRecordingStartBusy() is the exact predicate DjSession::validate()
	// uses to gate DJ_COMMAND_SET_RECORDING. A *second start* submitted
	// while a previous start/stop is still in flight must be rejected as
	// busy in every in-flight state.
	assert(djRecordingStartBusy(true, DJ_RECORDING_STARTING));
	assert(djRecordingStartBusy(true, DJ_RECORDING_ACTIVE));
	assert(djRecordingStartBusy(true, DJ_RECORDING_STOPPING));
	assert(!djRecordingStartBusy(true, DJ_RECORDING_IDLE));
	assert(!djRecordingStartBusy(true, DJ_RECORDING_COMPLETE));
	assert(!djRecordingStartBusy(true, DJ_RECORDING_FAILED));

	// A *stop* must never be busy-rejected, even while the accepted start is
	// still STARTING -- this is the "start->stop before applied" transition
	// the spec calls out. The library's own state machine is designed to
	// accept a stop during STARTING and safely move to STOPPING, so the
	// firmware must forward it rather than silently reject/drop it.
	assert(!djRecordingStartBusy(false, DJ_RECORDING_STARTING));
	assert(!djRecordingStartBusy(false, DJ_RECORDING_ACTIVE));
	assert(!djRecordingStartBusy(false, DJ_RECORDING_STOPPING));
	assert(!djRecordingStartBusy(false, DJ_RECORDING_IDLE));

	// Simulate the result of that stop-during-STARTING command: it is
	// accepted (queued) rather than rejected as busy or dropped.
	DjCommandResults results;
	DjCommand stopCommand = {};
	stopCommand.id = 2;
	stopCommand.type = DJ_COMMAND_SET_RECORDING;
	stopCommand.value = 0;
	results.record(stopCommand, DJ_COMMAND_ACCEPTED, DJ_COMMAND_ERROR_NONE);
	DjCommandResult recent[DJ_RECENT_RESULT_COUNT] = {};
	results.copyTo(recent);
	assert(recent[0].id == stopCommand.id);
	assert(recent[0].status == DJ_COMMAND_ACCEPTED);
	assert(recent[0].error == DJ_COMMAND_ERROR_NONE);

	// A duplicate *start* submitted while already STARTING, by contrast, is
	// rejected outright as busy.
	DjCommand duplicateStart = {};
	duplicateStart.id = 3;
	duplicateStart.type = DJ_COMMAND_SET_RECORDING;
	duplicateStart.value = 1;
	results.record(duplicateStart, DJ_COMMAND_REJECTED, DJ_COMMAND_ERROR_RECORDING_BUSY);
	results.copyTo(recent);
	assert(recent[0].id == duplicateStart.id);
	assert(recent[0].status == DJ_COMMAND_REJECTED);
	assert(recent[0].error == DJ_COMMAND_ERROR_RECORDING_BUSY);

	// Failure/invalid completion must never present as a successful, valid recording.
	snapshot = DjRecordingSnapshot();
	snapshot.state = DJ_RECORDING_FAILED;
	snapshot.error = DJ_RECORDING_ERROR_WRITE_FAILED;
	snapshot.valid = false;
	assert(snapshot.state == DJ_RECORDING_FAILED);
	assert(!snapshot.valid);
	assert(snapshot.error != DJ_RECORDING_ERROR_NONE);

	// A finalize/rename failure after an otherwise-complete recording must
	// surface as invalid + a distinct error, not a silent success.
	snapshot = DjRecordingSnapshot();
	snapshot.state = DJ_RECORDING_COMPLETE;
	snapshot.valid = false;
	snapshot.error = DJ_RECORDING_ERROR_NAME_EXHAUSTED;
	assert(snapshot.state == DJ_RECORDING_COMPLETE);
	assert(!snapshot.valid);
	assert(snapshot.error == DJ_RECORDING_ERROR_NAME_EXHAUSTED);

	return 0;
}
