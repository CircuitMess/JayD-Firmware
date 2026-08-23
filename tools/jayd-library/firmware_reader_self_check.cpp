#include "JaydMetadata.h"

#include <FS.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>

using JaydMetadata::Cue;
using JaydMetadata::Grid;
using JaydMetadata::Phrase;
using JaydMetadata::Reader;
using JaydMetadata::Status;
using JaydMetadata::Track;

static std::string fixture(const char* directory, const char* name){
	return std::string(directory) + "/" + name;
}

static Status open(Reader& reader, const char* directory, const char* name){
	const std::string path = fixture(directory, name);
	return reader.open(fs::File(path.c_str()));
}

int main(int argc, char** argv){
	assert(argc == 2);
	Reader reader;
	Track track;

	assert(open(reader, argv[1], "missing.jydm") == Status::Missing);
	assert(open(reader, argv[1], "valid.jydm") == Status::Ready);
	assert(reader.trackCount() == 1);
	assert(reader.trackByIndex(0, track) == Status::Ready);
	assert(track.sampleRate == 44100);
	assert(track.durationFrames == 110250);
	assert(track.bpmMilli == 128125);
	assert(track.cueCount == 2);
	assert(track.gridCount == 1);
	assert(track.phraseCount == 1);

	char text[32];
	assert(reader.readString(track.provenance, text, sizeof(text)));
	assert(strcmp(text, "firmware-reader-self-check") == 0);
	uint32_t provenanceHash = 0;
	assert(reader.readStringHash(track.provenance, provenanceHash));
	assert(provenanceHash != 0);
	assert(reader.trackByPath("safe/song.aac", track) == Status::Ready);
	assert(reader.trackByPath(
		"safe/song.aac",
		track,
		track.fingerprint,
		track.sourceId
	) == Status::Ready);
	uint8_t staleFingerprint[16];
	memcpy(staleFingerprint, track.fingerprint, sizeof(staleFingerprint));
	staleFingerprint[0] ^= 1;
	assert(reader.trackByPath("safe/song.aac", track, staleFingerprint) == Status::Stale);
	uint8_t staleSourceId[16];
	memcpy(staleSourceId, track.sourceId, sizeof(staleSourceId));
	staleSourceId[0] ^= 1;
	assert(reader.trackByPath("safe/song.aac", track, nullptr, staleSourceId) == Status::Stale);
	assert(reader.trackByFingerprint(track.fingerprint, track) == Status::Ready);
	assert(reader.trackBySourceId(track.sourceId, track) == Status::Ready);
	assert(reader.trackByPath("missing.aac", track) == Status::Missing);

	Cue cue;
	Grid grid;
	Phrase phrase;
	assert(reader.readCue(track, 0, cue));
	assert(cue.positionFrames == 22050);
	assert(reader.readCue(track, 1, cue));
	assert(cue.kind == 2 && cue.lengthFrames == 22050);
	assert(reader.readGrid(track, 0, grid));
	assert(grid.beatNumber == 1 && grid.confidence == 9000);
	assert(reader.readPhrase(track, 0, phrase));
	assert(phrase.confidence == 8000);

	assert(open(reader, argv[1], "fractional-valid.jydm") == Status::Ready);
	assert(reader.trackByIndex(0, track) == Status::Ready);
	assert(reader.readCue(track, 0, cue));
	assert(cue.lengthFrames == Reader::NoFrame && cue.lengthNumerator == 3 &&
		cue.lengthDenominator == 2);
	assert(reader.readCue(track, 1, cue));
	assert(cue.positionFrames == Reader::NoFrame && cue.positionNumerator == 5 &&
		cue.positionDenominator == 2);
	assert(open(reader, argv[1], "fractional-boundary.jydm") == Status::Ready);
	assert(open(reader, argv[1], "fractional-denominator-zero.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "fractional-cue-over.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "fractional-loop-over.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "fractional-grid-over.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "fractional-phrase-over.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "fractional-64-bit-product-overflow.jydm") == Status::Corrupt);

	assert(open(reader, argv[1], "unknown-optional.jydm") == Status::Ready);
	assert(open(reader, argv[1], "corrupt-crc.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "truncated.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "oversized-count.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "offset-overflow.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "overlapping-sections.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "unsupported-version.jydm") == Status::Unsupported);
	assert(open(reader, argv[1], "traversal-path.jydm") == Status::Corrupt);
	assert(open(reader, argv[1], "duplicate-path.jydm") == Status::Ready);
	assert(reader.trackByPath("safe/song.aac", track) == Status::Stale);

	puts("firmware metadata reader self-check passed");
	return 0;
}
