#include "JaydMetadata.h"

#include <string.h>

namespace JaydMetadata {
namespace {

const uint8_t Magic[8] = {'J', 'A', 'Y', 'D', 'M', 'E', 'T', 'A'};
const uint16_t FormatVersion = 1;
const uint16_t SectionVersion = 1;
const uint16_t EndianTag = 0x4c45;
const uint16_t HeaderSize = 64;
const uint16_t DirectoryEntrySize = 24;
const uint32_t MaxSections = 64;
const uint32_t MaxPlaylists = 256;
const uint32_t MaxPlaylistEntries = 65535;
const uint32_t MaxBpmMilli = 999999;
const size_t IoBufferSize = 512;

const char KnownSections[8][4] = {
	{'M', 'E', 'T', 'A'},
	{'T', 'R', 'A', 'K'},
	{'C', 'U', 'E', 'S'},
	{'G', 'R', 'I', 'D'},
	{'P', 'H', 'R', 'A'},
	{'P', 'L', 'S', 'T'},
	{'P', 'L', 'E', 'N'},
	{'S', 'T', 'R', 'S'},
};

const uint16_t KnownEntrySizes[8] = {8, 128, 64, 40, 32, 16, 8, 1};

uint16_t u16(const uint8_t* data){
	return uint16_t(data[0]) | (uint16_t(data[1]) << 8);
}

uint32_t u32(const uint8_t* data){
	return uint32_t(data[0]) | (uint32_t(data[1]) << 8) |
		(uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}

int32_t i32(const uint8_t* data){
	return static_cast<int32_t>(u32(data));
}

uint64_t u64(const uint8_t* data){
	return uint64_t(u32(data)) | (uint64_t(u32(data + 4)) << 32);
}

bool allZero(const uint8_t* data, size_t size){
	for(size_t i = 0; i < size; ++i){
		if(data[i] != 0) return false;
	}
	return true;
}

bool addFits(uint64_t left, uint64_t right, uint64_t& result){
	if(right > UINT64_MAX - left) return false;
	result = left + right;
	return true;
}

bool multiplyFits(uint64_t left, uint64_t right, uint64_t& result){
	if(left && right > UINT64_MAX / left) return false;
	result = left * right;
	return true;
}

bool spanFits(uint32_t first, uint32_t count, uint32_t total){
	return first <= total && count <= total - first;
}

bool validKey(uint16_t key){
	return key == 0 || ((key & 0xff) >= 1 && (key & 0xff) <= 12 && (key & ~0x1ff) == 0);
}

struct Wide {
	uint32_t words[4];
	bool overflow;
};

Wide wide(uint64_t value){
	Wide result = {{uint32_t(value), uint32_t(value >> 32), 0, 0}, false};
	return result;
}

Wide multiply(Wide value, uint32_t factor){
	uint64_t carry = 0;
	for(uint8_t i = 0; i < 4; ++i){
		const uint64_t product = uint64_t(value.words[i]) * factor + carry;
		value.words[i] = uint32_t(product);
		carry = product >> 32;
	}
	value.overflow = value.overflow || carry != 0;
	return value;
}

Wide add(Wide left, const Wide& right){
	uint64_t carry = 0;
	for(uint8_t i = 0; i < 4; ++i){
		const uint64_t sum = uint64_t(left.words[i]) + right.words[i] + carry;
		left.words[i] = uint32_t(sum);
		carry = sum >> 32;
	}
	left.overflow = left.overflow || right.overflow || carry != 0;
	return left;
}

bool lessOrEqual(const Wide& left, const Wide& right){
	if(left.overflow || right.overflow) return false;
	for(int8_t i = 3; i >= 0; --i){
		if(left.words[i] != right.words[i]) return left.words[i] < right.words[i];
	}
	return true;
}

bool rationalWithin(uint64_t numerator, uint32_t denominator, uint32_t sampleRate, uint64_t duration){
	return lessOrEqual(multiply(wide(numerator), sampleRate), multiply(wide(duration), denominator));
}

bool validPosition(uint64_t frame, uint64_t numerator, uint32_t denominator,
	uint32_t sampleRate, uint64_t duration){
	if(denominator == 0 && numerator != 0) return false;
	if(frame != Reader::NoFrame){
		return frame <= Reader::MaxFrame && (duration == 0 || frame <= duration);
	}
	if(denominator == 0) return false;
	return duration == 0 || rationalWithin(numerator, denominator, sampleRate, duration);
}

bool validOptionalLength(uint64_t frame, uint64_t numerator, uint32_t denominator,
	uint64_t position, uint64_t positionNumerator, uint32_t positionDenominator,
	uint32_t sampleRate, uint64_t duration){
	if(denominator == 0 && numerator != 0) return false;
	if(frame == Reader::NoFrame && denominator == 0) return true;
	if(frame != Reader::NoFrame && frame > Reader::MaxFrame) return false;
	if(duration == 0) return true;

	if(position != Reader::NoFrame){
		if(position > duration) return false;
		if(frame != Reader::NoFrame) return frame <= duration - position;
		return rationalWithin(numerator, denominator, sampleRate, duration - position);
	}

	if(frame != Reader::NoFrame){
		const Wide total = add(
			multiply(wide(positionNumerator), sampleRate),
			multiply(wide(frame), positionDenominator)
		);
		return lessOrEqual(total, multiply(wide(duration), positionDenominator));
	}
	const Wide total = multiply(add(
		multiply(wide(positionNumerator), denominator),
		multiply(wide(numerator), positionDenominator)
	), sampleRate);
	const Wide available = multiply(multiply(wide(duration), positionDenominator), denominator);
	return lessOrEqual(total, available);
}

int knownSection(const uint8_t kind[4]){
	for(uint8_t i = 0; i < 8; ++i){
		if(memcmp(kind, KnownSections[i], 4) == 0) return i;
	}
	return -1;
}

} // namespace

Reader::Reader() : status_(Status::Missing), fileSize_(0), trackCount_(0){
	memset(sections_, 0, sizeof(sections_));
}

Status Reader::open(const fs::File& file){
	close();
	if(!file) return status_;

	file_ = file;
	const uint64_t size = file_.size();
	if(size < HeaderSize || size > MaxFileSize) return fail(Status::Corrupt);
	fileSize_ = static_cast<uint32_t>(size);

	uint8_t header[HeaderSize];
	if(!readAt(0, header, sizeof(header))) return fail(Status::Corrupt);
	if(memcmp(header, Magic, sizeof(Magic)) != 0) return fail(Status::Corrupt);
	if(u16(header + 8) != FormatVersion || u16(header + 10) != EndianTag) return fail(Status::Unsupported);
	if(u16(header + 12) < HeaderSize || u16(header + 14) < DirectoryEntrySize) return fail(Status::Unsupported);
	if(u32(header + 24) != 0 || !allZero(header + 48, 16)) return fail(Status::Unsupported);
	if(u64(header + 28) != fileSize_) return fail(Status::Corrupt);
	if(u32(header + 16) > MaxSections || u32(header + 20) > MaxTracks) return fail(Status::Corrupt);
	if(!validateCrc(u32(header + 44))) return fail(Status::Corrupt);

	Status directoryStatus = readDirectory(header);
	if(directoryStatus != Status::Ready) return fail(directoryStatus);
	if(!validateStrings() || !validateRecords()) return fail(Status::Corrupt);

	trackCount_ = u32(header + 20);
	status_ = Status::Ready;
	return status_;
}

void Reader::close(){
	if(file_) file_.close();
	file_ = fs::File();
	status_ = Status::Missing;
	fileSize_ = 0;
	trackCount_ = 0;
	memset(sections_, 0, sizeof(sections_));
}

Status Reader::status() const{
	return status_;
}

uint32_t Reader::trackCount() const{
	return status_ == Status::Ready ? trackCount_ : 0;
}

Status Reader::fail(Status status){
	status_ = status;
	fileSize_ = 0;
	trackCount_ = 0;
	memset(sections_, 0, sizeof(sections_));
	if(file_) file_.close();
	return status_;
}

bool Reader::readAt(uint64_t offset, void* output, size_t size){
	if(!file_ || offset > fileSize_ || size > fileSize_ - offset || offset > UINT32_MAX) return false;
	if(!file_.seek(static_cast<uint32_t>(offset))) return false;
	return file_.read(static_cast<uint8_t*>(output), size) == size;
}

bool Reader::validateCrc(uint32_t expected){
	static const uint32_t table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
	};
	uint8_t buffer[IoBufferSize];
	uint32_t crc = 0xffffffff;
	uint32_t position = 0;
	if(!file_.seek(0)) return false;
	while(position < fileSize_){
		size_t requested = fileSize_ - position;
		if(requested > sizeof(buffer)) requested = sizeof(buffer);
		if(file_.read(buffer, requested) != requested) return false;
		for(size_t i = 0; i < requested; ++i){
			const uint32_t absolute = position + i;
			uint8_t value = absolute >= 44 && absolute < 48 ? 0 : buffer[i];
			crc = table[(crc ^ value) & 0x0f] ^ (crc >> 4);
			crc = table[(crc ^ (value >> 4)) & 0x0f] ^ (crc >> 4);
		}
		position += requested;
	}
	return (crc ^ 0xffffffff) == expected;
}

bool Reader::readSectionEntry(uint64_t directoryOffset, uint16_t directoryEntrySize, uint32_t index, uint8_t output[24]){
	uint64_t relative;
	uint64_t offset;
	if(!multiplyFits(index, directoryEntrySize, relative) || !addFits(directoryOffset, relative, offset)) return false;
	return readAt(offset, output, DirectoryEntrySize);
}

Status Reader::readDirectory(const uint8_t header[64]){
	const uint16_t directoryEntrySize = u16(header + 14);
	const uint32_t sectionCount = u32(header + 16);
	const uint64_t directoryOffset = u64(header + 36);
	const uint16_t declaredHeaderSize = u16(header + 12);
	uint64_t directoryBytes;
	uint64_t directoryEnd;
	if(!multiplyFits(sectionCount, directoryEntrySize, directoryBytes) ||
		!addFits(directoryOffset, directoryBytes, directoryEnd) ||
		directoryOffset < declaredHeaderSize || directoryEnd > fileSize_) return Status::Corrupt;

	for(uint32_t i = 0; i < sectionCount; ++i){
		uint8_t entry[DirectoryEntrySize];
		if(!readSectionEntry(directoryOffset, directoryEntrySize, i, entry)) return Status::Corrupt;
		const uint16_t version = u16(entry + 4);
		const uint16_t entrySize = u16(entry + 6);
		const uint32_t count = u32(entry + 8);
		const uint32_t byteSize = u32(entry + 12);
		const uint64_t offset = u64(entry + 16);
		uint64_t payloadBytes;
		uint64_t end;
		if(!multiplyFits(count, entrySize, payloadBytes) || payloadBytes > byteSize ||
			!addFits(offset, byteSize, end) || offset < directoryEnd || end > fileSize_ ||
			(offset & 7) != 0) return Status::Corrupt;

		for(uint32_t previous = 0; previous < i; ++previous){
			uint8_t other[DirectoryEntrySize];
			if(!readSectionEntry(directoryOffset, directoryEntrySize, previous, other)) return Status::Corrupt;
			const uint64_t otherOffset = u64(other + 16);
			uint64_t otherEnd;
			if(memcmp(entry, other, 4) == 0 ||
				!addFits(otherOffset, u32(other + 12), otherEnd) ||
				(offset < otherEnd && end > otherOffset)) return Status::Corrupt;
		}

		const int known = knownSection(entry);
		if(known < 0) continue;
		if(version != SectionVersion || entrySize < KnownEntrySizes[known]) return Status::Unsupported;
		if(known == StringSection && (entrySize != 1 || count != byteSize)) return Status::Corrupt;
		Section& section = sections_[known];
		section.entrySize = entrySize;
		section.count = count;
		section.byteSize = byteSize;
		section.offset = offset;
		section.present = true;
	}

	for(uint8_t i = 0; i < SectionCount; ++i){
		if(!sections_[i].present) return Status::Unsupported;
	}
	const uint32_t declaredTracks = u32(header + 20);
	if(sections_[TrackSection].count != declaredTracks ||
		sections_[TrackSection].count > MaxTracks ||
		sections_[MetaSection].count > MaxMetadataEntries ||
		sections_[CueSection].count > declaredTracks * MaxCuesPerTrack ||
		sections_[GridSection].count > declaredTracks * MaxGridPerTrack ||
		sections_[PhraseSection].count > declaredTracks * MaxPhrasesPerTrack ||
		sections_[PlaylistSection].count > MaxPlaylists ||
		sections_[PlaylistEntrySection].count > MaxPlaylistEntries) return Status::Corrupt;
	return Status::Ready;
}

bool Reader::validateStrings(){
	const Section& strings = sections_[StringSection];
	if(strings.byteSize == 0) return false;
	uint8_t buffer[IoBufferSize];
	uint32_t position = 0;
	uint32_t stringLength = 0;
	uint8_t continuation = 0;
	uint32_t codepoint = 0;
	uint32_t minimum = 0;
	while(position < strings.byteSize){
		size_t requested = strings.byteSize - position;
		if(requested > sizeof(buffer)) requested = sizeof(buffer);
		if(!readAt(strings.offset + position, buffer, requested)) return false;
		for(size_t i = 0; i < requested; ++i){
			const uint8_t value = buffer[i];
			if(position == 0 && i == 0 && value != 0) return false;
			if(value == 0){
				if(continuation != 0 || stringLength > MaxStringBytes) return false;
				stringLength = 0;
				continue;
			}
			if(++stringLength > MaxStringBytes) return false;
			if(continuation){
				if((value & 0xc0) != 0x80) return false;
				codepoint = (codepoint << 6) | (value & 0x3f);
				if(--continuation == 0 &&
					(codepoint < minimum || codepoint > 0x10ffff ||
					 (codepoint >= 0xd800 && codepoint <= 0xdfff))) return false;
			}else if(value < 0x80){
				continue;
			}else if((value & 0xe0) == 0xc0){
				continuation = 1;
				codepoint = value & 0x1f;
				minimum = 0x80;
			}else if((value & 0xf0) == 0xe0){
				continuation = 2;
				codepoint = value & 0x0f;
				minimum = 0x800;
			}else if((value & 0xf8) == 0xf0){
				continuation = 3;
				codepoint = value & 0x07;
				minimum = 0x10000;
			}else{
				return false;
			}
		}
		position += requested;
	}
	uint8_t last;
	return continuation == 0 && readAt(strings.offset + strings.byteSize - 1, &last, 1) && last == 0;
}

bool Reader::readString(uint32_t offset, char* output, size_t capacity){
	if(status_ != Status::Ready || !output || capacity == 0) return false;
	const Section& strings = sections_[StringSection];
	if(offset >= strings.byteSize) return false;
	if(offset){
		uint8_t previous;
		if(!readAt(strings.offset + offset - 1, &previous, 1) || previous != 0) return false;
	}
	size_t written = 0;
	uint8_t buffer[128];
	uint32_t position = offset;
	while(position < strings.byteSize){
		size_t requested = strings.byteSize - position;
		if(requested > sizeof(buffer)) requested = sizeof(buffer);
		if(!readAt(strings.offset + position, buffer, requested)) return false;
		for(size_t i = 0; i < requested; ++i){
			if(buffer[i] == 0){
				if(written >= capacity) return false;
				output[written] = '\0';
				return true;
			}
			if(written + 1 >= capacity) return false;
			output[written++] = static_cast<char>(buffer[i]);
		}
		position += requested;
	}
	return false;
}

bool Reader::readStringHash(uint32_t offset, uint32_t& hash){
	if(status_ != Status::Ready) return false;
	const Section& strings = sections_[StringSection];
	if(offset >= strings.byteSize) return false;
	if(offset){
		uint8_t previous;
		if(!readAt(strings.offset + offset - 1, &previous, 1) || previous != 0) return false;
	}

	hash = 2166136261U;
	uint8_t buffer[128];
	uint32_t position = offset;
	while(position < strings.byteSize){
		size_t requested = strings.byteSize - position;
		if(requested > sizeof(buffer)) requested = sizeof(buffer);
		if(!readAt(strings.offset + position, buffer, requested)) return false;
		for(size_t i = 0; i < requested; ++i){
			if(buffer[i] == 0) return true;
			hash = (hash ^ buffer[i]) * 16777619U;
		}
		position += requested;
	}
	return false;
}

bool Reader::validateStringOffset(uint32_t offset){
	const Section& strings = sections_[StringSection];
	if(offset >= strings.byteSize) return false;
	if(offset){
		uint8_t previous;
		if(!readAt(strings.offset + offset - 1, &previous, 1) || previous != 0) return false;
	}
	uint8_t buffer[128];
	uint32_t length = 0;
	uint32_t position = offset;
	while(position < strings.byteSize){
		size_t requested = strings.byteSize - position;
		if(requested > sizeof(buffer)) requested = sizeof(buffer);
		if(!readAt(strings.offset + position, buffer, requested)) return false;
		for(size_t i = 0; i < requested; ++i){
			if(buffer[i] == 0) return true;
			if(++length > MaxStringBytes) return false;
		}
		position += requested;
	}
	return false;
}

bool Reader::validatePath(uint32_t offset){
	char path[MaxPathBytes + 1];
	if(!readString(offset, path, sizeof(path)) || path[0] == '\0' || path[0] == '/') return false;
	const char* segment = path;
	for(const char* cursor = path; ; ++cursor){
		const char value = *cursor;
		if(value == '\\') return false;
		if(value == '/' || value == '\0'){
			const size_t length = cursor - segment;
			if(length == 0 || (length == 1 && segment[0] == '.') ||
				(length == 2 && segment[0] == '.' && segment[1] == '.')) return false;
			if(value == '\0') break;
			segment = cursor + 1;
		}
	}
	return !(path[1] == ':' &&
		((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')));
}

bool Reader::validateTrack(uint32_t index, Track* output){
	const Section& section = sections_[TrackSection];
	if(index >= section.count) return false;
	uint8_t record[128];
	if(!readAt(section.offset + uint64_t(index) * section.entrySize, record, sizeof(record))) return false;
	const uint32_t path = u32(record + 32);
	const uint32_t title = u32(record + 36);
	const uint32_t artist = u32(record + 40);
	const uint32_t album = u32(record + 44);
	const uint32_t provenance = u32(record + 48);
	const uint32_t firstCue = u32(record + 52);
	const uint32_t cueCount = u32(record + 56);
	const uint32_t firstGrid = u32(record + 60);
	const uint32_t gridCount = u32(record + 64);
	const uint32_t firstPhrase = u32(record + 68);
	const uint32_t phraseCount = u32(record + 72);
	const uint32_t sampleRate = u32(record + 76);
	const uint64_t durationFrames = u64(record + 80);
	const uint32_t bpmMilli = u32(record + 88);
	const uint16_t key = u16(record + 92);
	const uint8_t rating = record[94];
	if(record[95] != 0 || !allZero(record + 104, 24) ||
		sampleRate > MaxSampleRate || durationFrames > MaxFrame ||
		(durationFrames != 0 && sampleRate == 0) || bpmMilli > MaxBpmMilli ||
		(rating > 5 && rating != 0xff) || !validKey(key) ||
		cueCount > MaxCuesPerTrack || !spanFits(firstCue, cueCount, sections_[CueSection].count) ||
		gridCount > MaxGridPerTrack || !spanFits(firstGrid, gridCount, sections_[GridSection].count) ||
		phraseCount > MaxPhrasesPerTrack || !spanFits(firstPhrase, phraseCount, sections_[PhraseSection].count) ||
		!validatePath(path) || !validateStringOffset(title) || !validateStringOffset(artist) ||
		!validateStringOffset(album) || !validateStringOffset(provenance)) return false;

	if(output){
		output->index = index;
		memcpy(output->fingerprint, record, 16);
		memcpy(output->sourceId, record + 16, 16);
		output->path = path;
		output->title = title;
		output->artist = artist;
		output->album = album;
		output->provenance = provenance;
		output->firstCue = firstCue;
		output->cueCount = cueCount;
		output->firstGrid = firstGrid;
		output->gridCount = gridCount;
		output->firstPhrase = firstPhrase;
		output->phraseCount = phraseCount;
		output->sampleRate = sampleRate;
		output->durationFrames = durationFrames;
		output->bpmMilli = bpmMilli;
		output->key = key;
		output->rating = rating;
		output->playCount = u32(record + 96);
		output->color = u32(record + 100);
	}
	return true;
}

bool Reader::validateCue(uint32_t index, uint32_t owner, uint32_t sampleRate,
	uint64_t durationFrames, Cue* output){
	const Section& section = sections_[CueSection];
	if(index >= section.count) return false;
	uint8_t record[64];
	if(!readAt(section.offset + uint64_t(index) * section.entrySize, record, sizeof(record))) return false;
	const uint8_t kind = record[4];
	const uint64_t position = u64(record + 8);
	const uint64_t length = u64(record + 16);
	const uint64_t positionNumerator = u64(record + 24);
	const uint32_t positionDenominator = u32(record + 32);
	const uint64_t lengthNumerator = u64(record + 36);
	const uint32_t lengthDenominator = u32(record + 44);
	const uint32_t label = u32(record + 48);
	if(u32(record) != owner || (kind != 1 && kind != 2) || !allZero(record + 56, 8) ||
		!validPosition(position, positionNumerator, positionDenominator, sampleRate, durationFrames) ||
		!validOptionalLength(length, lengthNumerator, lengthDenominator,
			position, positionNumerator, positionDenominator, sampleRate, durationFrames) ||
		!validateStringOffset(label)) return false;
	if(output){
		output->kind = kind;
		output->flags = record[5];
		output->slot = u16(record + 6);
		output->positionFrames = position;
		output->lengthFrames = length;
		output->positionNumerator = positionNumerator;
		output->positionDenominator = positionDenominator;
		output->lengthNumerator = lengthNumerator;
		output->lengthDenominator = lengthDenominator;
		output->label = label;
		output->color = u32(record + 52);
	}
	return true;
}

bool Reader::validateGrid(uint32_t index, uint32_t owner, uint32_t sampleRate,
	uint64_t durationFrames, Grid* output){
	const Section& section = sections_[GridSection];
	if(index >= section.count) return false;
	uint8_t record[40];
	if(!readAt(section.offset + uint64_t(index) * section.entrySize, record, sizeof(record))) return false;
	const uint64_t position = u64(record + 4);
	const uint64_t numerator = u64(record + 12);
	const uint32_t denominator = u32(record + 20);
	const uint32_t bpm = u32(record + 24);
	const uint16_t confidence = u16(record + 32);
	if(u32(record) != owner || !allZero(record + 36, 4) || bpm > MaxBpmMilli || confidence > 10000 ||
		!validPosition(position, numerator, denominator, sampleRate, durationFrames)) return false;
	if(output){
		output->positionFrames = position;
		output->positionNumerator = numerator;
		output->positionDenominator = denominator;
		output->bpmMilli = bpm;
		output->beatNumber = i32(record + 28);
		output->confidence = confidence;
		output->flags = u16(record + 34);
	}
	return true;
}

bool Reader::validatePhrase(uint32_t index, uint32_t owner, uint32_t sampleRate,
	uint64_t durationFrames, Phrase* output){
	const Section& section = sections_[PhraseSection];
	if(index >= section.count) return false;
	uint8_t record[32];
	if(!readAt(section.offset + uint64_t(index) * section.entrySize, record, sizeof(record))) return false;
	const uint64_t position = u64(record + 4);
	const uint64_t numerator = u64(record + 12);
	const uint32_t denominator = u32(record + 20);
	const uint32_t kind = u32(record + 24);
	const uint16_t confidence = u16(record + 28);
	if(u32(record) != owner || confidence > 10000 ||
		!validPosition(position, numerator, denominator, sampleRate, durationFrames) ||
		!validateStringOffset(kind)) return false;
	if(output){
		output->positionFrames = position;
		output->positionNumerator = numerator;
		output->positionDenominator = denominator;
		output->kind = kind;
		output->confidence = confidence;
		output->flags = u16(record + 30);
	}
	return true;
}

bool Reader::validateRecords(){
	status_ = Status::Ready;
	for(uint32_t index = 0; index < sections_[MetaSection].count; ++index){
		uint8_t record[8];
		if(!readAt(sections_[MetaSection].offset + uint64_t(index) * sections_[MetaSection].entrySize,
			record, sizeof(record)) ||
			!validateStringOffset(u32(record)) || !validateStringOffset(u32(record + 4))) return false;
	}
	for(uint32_t index = 0; index < sections_[TrackSection].count; ++index){
		Track track;
		if(!validateTrack(index, &track)) return false;
		for(uint32_t child = 0; child < track.cueCount; ++child){
			if(!validateCue(track.firstCue + child, index, track.sampleRate, track.durationFrames, nullptr)) return false;
		}
		for(uint32_t child = 0; child < track.gridCount; ++child){
			if(!validateGrid(track.firstGrid + child, index, track.sampleRate, track.durationFrames, nullptr)) return false;
		}
		for(uint32_t child = 0; child < track.phraseCount; ++child){
			if(!validatePhrase(track.firstPhrase + child, index, track.sampleRate, track.durationFrames, nullptr)) return false;
		}
	}
	for(uint32_t index = 0; index < sections_[PlaylistSection].count; ++index){
		uint8_t record[16];
		if(!readAt(sections_[PlaylistSection].offset + uint64_t(index) * sections_[PlaylistSection].entrySize,
			record, sizeof(record)) ||
			!validateStringOffset(u32(record)) ||
			!spanFits(u32(record + 4), u32(record + 8), sections_[PlaylistEntrySection].count) ||
			u32(record + 12) != 0) return false;
	}
	for(uint32_t index = 0; index < sections_[PlaylistEntrySection].count; ++index){
		uint8_t record[8];
		if(!readAt(sections_[PlaylistEntrySection].offset + uint64_t(index) * sections_[PlaylistEntrySection].entrySize,
			record, sizeof(record)) || u32(record) >= sections_[TrackSection].count) return false;
	}
	return true;
}

Status Reader::trackByIndex(uint32_t index, Track& track){
	if(status_ != Status::Ready) return status_;
	return validateTrack(index, &track) ? Status::Ready : (index >= trackCount_ ? Status::Missing : Status::Corrupt);
}

Status Reader::trackByPath(
	const char* normalizedPath,
	Track& track,
	const uint8_t* expectedFingerprint,
	const uint8_t* expectedSourceId
){
	if(status_ != Status::Ready) return status_;
	if(!normalizedPath) return Status::Missing;
	const size_t length = strlen(normalizedPath);
	if(length == 0 || length > MaxPathBytes) return Status::Missing;
	bool sawPath = false;
	bool matched = false;
	for(uint32_t index = 0; index < trackCount_; ++index){
		Track candidate;
		char path[MaxPathBytes + 1];
		if(!validateTrack(index, &candidate) || !readString(candidate.path, path, sizeof(path))) return Status::Corrupt;
		if(strcmp(path, normalizedPath) != 0) continue;
		sawPath = true;
		if(expectedFingerprint && memcmp(expectedFingerprint, candidate.fingerprint, 16) != 0) continue;
		if(expectedSourceId && memcmp(expectedSourceId, candidate.sourceId, 16) != 0) continue;
		if(matched) return Status::Stale;
		track = candidate;
		matched = true;
	}
	if(matched) return Status::Ready;
	return sawPath ? Status::Stale : Status::Missing;
}

Status Reader::trackByFingerprint(const uint8_t fingerprint[16], Track& track){
	if(status_ != Status::Ready) return status_;
	if(!fingerprint) return Status::Missing;
	bool matched = false;
	for(uint32_t index = 0; index < trackCount_; ++index){
		Track candidate;
		if(!validateTrack(index, &candidate)) return Status::Corrupt;
		if(memcmp(fingerprint, candidate.fingerprint, 16) != 0) continue;
		if(matched) return Status::Stale;
		track = candidate;
		matched = true;
	}
	return matched ? Status::Ready : Status::Missing;
}

Status Reader::trackBySourceId(const uint8_t sourceId[16], Track& track){
	if(status_ != Status::Ready) return status_;
	if(!sourceId) return Status::Missing;
	bool matched = false;
	for(uint32_t index = 0; index < trackCount_; ++index){
		Track candidate;
		if(!validateTrack(index, &candidate)) return Status::Corrupt;
		if(memcmp(sourceId, candidate.sourceId, 16) != 0) continue;
		if(matched) return Status::Stale;
		track = candidate;
		matched = true;
	}
	return matched ? Status::Ready : Status::Missing;
}

bool Reader::readCue(const Track& track, uint32_t relativeIndex, Cue& cue){
	return status_ == Status::Ready && relativeIndex < track.cueCount &&
		validateCue(track.firstCue + relativeIndex, track.index, track.sampleRate, track.durationFrames, &cue);
}

bool Reader::readGrid(const Track& track, uint32_t relativeIndex, Grid& grid){
	return status_ == Status::Ready && relativeIndex < track.gridCount &&
		validateGrid(track.firstGrid + relativeIndex, track.index, track.sampleRate, track.durationFrames, &grid);
}

bool Reader::readPhrase(const Track& track, uint32_t relativeIndex, Phrase& phrase){
	return status_ == Status::Ready && relativeIndex < track.phraseCount &&
		validatePhrase(track.firstPhrase + relativeIndex, track.index, track.sampleRate, track.durationFrames, &phrase);
}

} // namespace JaydMetadata
