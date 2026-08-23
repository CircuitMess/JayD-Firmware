#pragma once

#include <FS.h>
#include <stddef.h>
#include <stdint.h>

namespace JaydMetadata {

enum class Status : uint8_t {
	Ready,
	Missing,
	Stale,
	Corrupt,
	Unsupported,
};

struct Track {
	uint32_t index;
	uint8_t fingerprint[16];
	uint8_t sourceId[16];
	uint32_t path;
	uint32_t title;
	uint32_t artist;
	uint32_t album;
	uint32_t provenance;
	uint32_t firstCue;
	uint32_t cueCount;
	uint32_t firstGrid;
	uint32_t gridCount;
	uint32_t firstPhrase;
	uint32_t phraseCount;
	uint32_t sampleRate;
	uint64_t durationFrames;
	uint32_t bpmMilli;
	uint16_t key;
	uint8_t rating;
	uint32_t playCount;
	uint32_t color;
};

struct Cue {
	uint8_t kind;
	uint8_t flags;
	uint16_t slot;
	uint64_t positionFrames;
	uint64_t lengthFrames;
	uint64_t positionNumerator;
	uint32_t positionDenominator;
	uint64_t lengthNumerator;
	uint32_t lengthDenominator;
	uint32_t label;
	uint32_t color;
};

struct Grid {
	uint64_t positionFrames;
	uint64_t positionNumerator;
	uint32_t positionDenominator;
	uint32_t bpmMilli;
	int32_t beatNumber;
	uint16_t confidence;
	uint16_t flags;
};

struct Phrase {
	uint64_t positionFrames;
	uint64_t positionNumerator;
	uint32_t positionDenominator;
	uint32_t kind;
	uint16_t confidence;
	uint16_t flags;
};

class Reader {
public:
	static const uint32_t MaxFileSize = 32U * 1024U * 1024U;
	static const uint32_t MaxTracks = 4096;
	static const uint32_t MaxCuesPerTrack = 64;
	static const uint32_t MaxGridPerTrack = 256;
	static const uint32_t MaxPhrasesPerTrack = 128;
	static const uint32_t MaxMetadataEntries = 8192;
	static const uint32_t MaxPathBytes = 1024;
	static const uint32_t MaxStringBytes = 4096;
	static const uint32_t MaxSampleRate = 768000;
	static const uint64_t MaxFrame = 0x7fffffffffffffffULL;
	static const uint64_t NoFrame = 0xffffffffffffffffULL;

	Reader();

	Status open(const fs::File& file);
	void close();
	Status status() const;
	uint32_t trackCount() const;

	Status trackByIndex(uint32_t index, Track& track);
	Status trackByPath(
		const char* normalizedPath,
		Track& track,
		const uint8_t* expectedFingerprint = nullptr,
		const uint8_t* expectedSourceId = nullptr
	);
	Status trackByFingerprint(const uint8_t fingerprint[16], Track& track);
	Status trackBySourceId(const uint8_t sourceId[16], Track& track);

	bool readString(uint32_t offset, char* output, size_t capacity);
	bool readStringHash(uint32_t offset, uint32_t& hash);
	bool readCue(const Track& track, uint32_t relativeIndex, Cue& cue);
	bool readGrid(const Track& track, uint32_t relativeIndex, Grid& grid);
	bool readPhrase(const Track& track, uint32_t relativeIndex, Phrase& phrase);

private:
	struct Section {
		uint16_t entrySize;
		uint32_t count;
		uint32_t byteSize;
		uint64_t offset;
		bool present;
	};

	enum SectionIndex : uint8_t {
		MetaSection,
		TrackSection,
		CueSection,
		GridSection,
		PhraseSection,
		PlaylistSection,
		PlaylistEntrySection,
		StringSection,
		SectionCount,
	};

	fs::File file_;
	Status status_;
	uint32_t fileSize_;
	uint32_t trackCount_;
	Section sections_[SectionCount];

	Status fail(Status status);
	bool readAt(uint64_t offset, void* output, size_t size);
	bool validateCrc(uint32_t expected);
	Status readDirectory(const uint8_t header[64]);
	bool validateStrings();
	bool validateRecords();
	bool validateTrack(uint32_t index, Track* output);
	bool validateCue(uint32_t index, uint32_t owner, uint32_t sampleRate, uint64_t durationFrames, Cue* output);
	bool validateGrid(uint32_t index, uint32_t owner, uint32_t sampleRate, uint64_t durationFrames, Grid* output);
	bool validatePhrase(uint32_t index, uint32_t owner, uint32_t sampleRate, uint64_t durationFrames, Phrase* output);
	bool validateStringOffset(uint32_t offset);
	bool validatePath(uint32_t offset);
	bool readSectionEntry(uint64_t directoryOffset, uint16_t directoryEntrySize, uint32_t index, uint8_t output[24]);
};

} // namespace JaydMetadata
