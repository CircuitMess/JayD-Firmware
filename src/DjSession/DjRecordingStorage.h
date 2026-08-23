#ifndef JAYD_FIRMWARE_DJRECORDINGSTORAGE_H
#define JAYD_FIRMWARE_DJRECORDINGSTORAGE_H

#include <stddef.h>
#include <stdint.h>
#include "DjRecordingLogic.h"

// Bounded, collision-safe SD storage for finalized Jay-D recordings, and
// conservative recovery of a WAV file left behind by an interrupted session.
//
// Recordings live under a single flat directory with a fixed-width numeric
// name. The naming space is intentionally capped (see DjRecordingLogic.h) so
// allocation is a bounded SD.exists() scan, never an unbounded directory
// walk or heap allocation. Once the capacity is exhausted, new recordings
// fail explicitly until the user frees space by deleting old recordings.
//
// The naming/exhaustion search and WAV header validation/repair arithmetic
// are pure and filesystem-free -- see DjRecordingLogic.h, which is also what
// the host self-check exercises directly. This module only adds the real SD
// I/O around that logic.
namespace DjRecordingStorage {
	// Finds the next free, collision-safe path under the recordings
	// directory and writes it into outPath (capacity outCapacity). Never
	// overwrites an existing file. Returns false when the bounded naming
	// space is exhausted or the directory could not be created.
	bool allocatePath(char* outPath, size_t outCapacity);

	// Distinguishes *why* finalizeRecording() failed; see DjRecordingLogic.h
	// for the (host-testable, filesystem-free) decision logic.
	using FinalizeOutcome = DjRecordingLogic::FinalizeOutcome;

	// Moves the just-completed temp recording at tempPath into a freshly
	// allocated permanent path, writing the final path into outPath. Leaves
	// the temp file in place on any failure.
	FinalizeOutcome finalizeRecording(const char* tempPath, char* outPath, size_t outCapacity);

	struct RecoveryResult {
		uint32_t repaired = 0;
		uint32_t failed = 0;
	};

	// Conservative boot-time recovery for a Jay-D WAV recording left behind
	// at the fixed temp path after an interrupted session (e.g. power loss
	// after the initial header was written but before finalization). Only
	// repairs files matching Jay-D's own RIFF/WAVE PCM header shape and a
	// sample-aligned size; anything else -- including a nonexistent file --
	// is left untouched. On success the repaired file is moved into
	// permanent storage via finalizeRecording().
	RecoveryResult recoverOrphan(const char* tempPath);
}

#endif //JAYD_FIRMWARE_DJRECORDINGSTORAGE_H
