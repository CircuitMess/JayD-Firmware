#ifndef JAYD_FIRMWARE_DJRECORDINGLOGIC_H
#define JAYD_FIRMWARE_DJRECORDINGLOGIC_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <AudioLib/WavHeader.h>
#include <AudioSetup.hpp>

// Pure, dependency-free recording-storage logic shared by DjRecordingStorage
// (the on-device SD-backed implementation) and the host self-check. Nothing
// in this header touches a filesystem, so it can be exercised directly on a
// desktop build against the pinned JayD-Library headers, without stubbing
// SD/Arduino.
namespace DjRecordingLogic {
	static constexpr const char* DIR = "/Recordings";
	static constexpr uint16_t NAME_CAPACITY = 250;

	// Formats the bounded candidate path for a 1-based name index.
	inline bool formatCandidatePath(uint16_t index, char* outPath, size_t outCapacity){
		if(!outPath || outCapacity == 0 || index == 0 || index > NAME_CAPACITY) return false;
		char candidate[48];
		const int written = snprintf(candidate, sizeof(candidate), "%s/JAYD_%03u.wav", DIR, index);
		if(written <= 0 || static_cast<size_t>(written) >= sizeof(candidate)) return false;
		const size_t length = strlen(candidate);
		if(length >= outCapacity) return false;
		memcpy(outPath, candidate, length + 1);
		return true;
	}

	// Bounded free-slot search. Starting at `hint` (1-based), tries at most
	// NAME_CAPACITY indices, calling `taken(index, ctx)` for each; returns
	// the first index reported free, or false once the whole bounded space
	// has been tried (naming exhausted).
	inline bool findFreeSlot(uint16_t hint, bool (*taken)(uint16_t index, void* ctx), void* ctx, uint16_t& outIndex){
		if(!taken || hint == 0 || hint > NAME_CAPACITY) return false;
		for(uint16_t tries = 0; tries < NAME_CAPACITY; tries++){
			const uint16_t index = static_cast<uint16_t>((hint - 1 + tries) % NAME_CAPACITY) + 1;
			if(!taken(index, ctx)){
				outIndex = index;
				return true;
			}
		}
		return false; // bounded naming space exhausted
	}

	// Validates that `header` matches Jay-D's own RIFF/WAVE PCM format and
	// that `fileSize` is consistent with it (sample-aligned, no overflow of
	// the 32-bit chunk/data sizes), then computes the corrected dataSize and
	// chunkSize from the actual file length. Returns false -- leaving
	// outRepaired untouched -- for any unknown/nonconforming/misaligned
	// input; never mutates its arguments in that case.
	inline bool repairHeader(const WavHeader& header, uint32_t fileSize, WavHeader& outRepaired){
		const bool looksLikeJayDWav =
				memcmp(header.RIFF, "RIFF", 4) == 0 &&
				memcmp(header.WAVE, "WAVE", 4) == 0 &&
				memcmp(header.fmt, "fmt ", 4) == 0 &&
				memcmp(header.data, "data", 4) == 0 &&
				header.fmtSize == 16 &&
				header.audioFormat == 1 &&
				header.numChannels == NUM_CHANNELS &&
				header.sampleRate == SAMPLE_RATE &&
				header.bitsPerSample == BYTES_PER_SAMPLE * 8 &&
				header.blockAlign == NUM_CHANNELS * BYTES_PER_SAMPLE &&
				header.blockAlign != 0;
		if(!looksLikeJayDWav) return false; // unknown/nonconforming: leave untouched

		if(fileSize < sizeof(WavHeader)) return false;
		const uint32_t dataSize = fileSize - static_cast<uint32_t>(sizeof(WavHeader));
		if(dataSize % header.blockAlign != 0) return false; // misaligned payload: leave untouched
		if(dataSize > UINT32_MAX - 36) return false; // would overflow chunkSize: leave untouched

		outRepaired = header;
		outRepaired.dataSize = dataSize;
		outRepaired.chunkSize = dataSize + 36; // dataSize + (sizeof(WavHeader) - 8)
		return true;
	}

	// Distinguishes *why* a finalize attempt failed so callers can report an
	// accurate error instead of collapsing every failure into one code.
	enum class FinalizeOutcome : uint8_t {
		SUCCESS,
		ALLOC_EXHAUSTED, // bounded naming space full, directory couldn't be created, or bad input
		RENAME_FAILED    // a free path was allocated but the rename I/O failed
	};

	// Pure decision for finalizeRecording(): given whether the temp file was
	// present, a free permanent path was allocated, and the rename I/O
	// succeeded, decides the outcome. Kept separate from the SD-backed I/O
	// in DjRecordingStorage.cpp so both failure paths are host-testable
	// without stubbing the filesystem.
	inline FinalizeOutcome decideFinalizeOutcome(bool tempPresent, bool allocated, bool renamed){
		if(!tempPresent || !allocated) return FinalizeOutcome::ALLOC_EXHAUSTED;
		if(!renamed) return FinalizeOutcome::RENAME_FAILED;
		return FinalizeOutcome::SUCCESS;
	}
}

#endif //JAYD_FIRMWARE_DJRECORDINGLOGIC_H
