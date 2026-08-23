#include "DjRecordingStorage.h"
#include "DjRecordingLogic.h"
#include <SD.h>

namespace DjRecordingStorage {

namespace {

bool ensureDirectory(){
	if(SD.exists(DjRecordingLogic::DIR)) return true;
	return SD.mkdir(DjRecordingLogic::DIR);
}

bool sdExists(uint16_t index, void* /*ctx*/){
	char candidate[48];
	if(!DjRecordingLogic::formatCandidatePath(index, candidate, sizeof(candidate))) return true; // unusable index: treat as taken
	return SD.exists(candidate);
}

} // namespace

bool allocatePath(char* outPath, size_t outCapacity){
	if(!outPath || outCapacity == 0) return false;
	if(!ensureDirectory()) return false;

	// Hint avoids rescanning from index 1 every time once recordings pile
	// up; findFreeSlot() is still fully bounded (at most NAME_CAPACITY
	// SD.exists() calls) regardless of where it starts.
	static uint16_t nextHint = 1;
	uint16_t index = 0;
	if(!DjRecordingLogic::findFreeSlot(nextHint, sdExists, nullptr, index)) return false;
	if(!DjRecordingLogic::formatCandidatePath(index, outPath, outCapacity)) return false;
	nextHint = static_cast<uint16_t>(index % DjRecordingLogic::NAME_CAPACITY) + 1;
	return true;
}

FinalizeOutcome finalizeRecording(const char* tempPath, char* outPath, size_t outCapacity){
	const bool tempPresent = tempPath && outPath && outCapacity > 0 && SD.exists(tempPath);
	const bool allocated = tempPresent && allocatePath(outPath, outCapacity);
	if(!allocated){
		return DjRecordingLogic::decideFinalizeOutcome(tempPresent, false, false);
	}
	const bool renamed = SD.rename(tempPath, outPath);
	if(!renamed) outPath[0] = '\0';
	return DjRecordingLogic::decideFinalizeOutcome(tempPresent, allocated, renamed);
}

RecoveryResult recoverOrphan(const char* tempPath){
	RecoveryResult result;
	if(!tempPath || !SD.exists(tempPath)) return result;

	// Opened "r+" (not "w"): on this FS stack FILE_WRITE maps straight to
	// fopen's "w" mode, which truncates on an existing file. We must never
	// touch the recorded sample data, only the fixed 44-byte header.
	fs::File file = SD.open(tempPath, "r+");
	if(!file){
		result.failed++;
		return result;
	}

	const uint32_t fileSize = file.size();
	WavHeader header = {};
	if(fileSize < sizeof(WavHeader) ||
	   file.read(reinterpret_cast<uint8_t*>(&header), sizeof(WavHeader)) != sizeof(WavHeader)){
		file.close();
		result.failed++;
		return result;
	}

	WavHeader repaired = {};
	if(!DjRecordingLogic::repairHeader(header, fileSize, repaired)){
		file.close();
		result.failed++;
		return result; // unknown/nonconforming/misaligned file: leave it untouched
	}

	if(repaired.dataSize != header.dataSize || repaired.chunkSize != header.chunkSize){
		if(!file.seek(0) ||
		   file.write(reinterpret_cast<const uint8_t*>(&repaired), sizeof(WavHeader)) != sizeof(WavHeader)){
			file.close();
			result.failed++;
			return result;
		}
	}
	file.close();

	char finalPath[64];
	if(finalizeRecording(tempPath, finalPath, sizeof(finalPath)) != FinalizeOutcome::SUCCESS){
		result.failed++;
		return result;
	}
	result.repaired++;
	return result;
}

} // namespace DjRecordingStorage
