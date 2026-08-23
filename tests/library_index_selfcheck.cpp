#include "../src/Screens/SongList/LibraryIndex.h"

#include <assert.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

using LibraryIndex::BuildState;
using LibraryIndex::CardIdentity;
using LibraryIndex::FileEvidence;
using LibraryIndex::Header;
using LibraryIndex::IdentityStrength;
using LibraryIndex::Limits;
using LibraryIndex::Record;
using LibraryIndex::ValidationError;
using LibraryIndex::ValidationResult;

struct Entry {
	std::string path;
	FileEvidence evidence;
};

const Limits limits{4096, 4096 * sizeof(Record) + 128 * 1024, 128 * 1024};
const CardIdentity card{IdentityStrength::Weak, 3, 32ULL * 1024 * 1024 * 1024, 31914983424ULL};

bool readVector(void* context, uint32_t offset, uint8_t* data, size_t size){
	const std::vector<uint8_t>& bytes = *static_cast<std::vector<uint8_t>*>(context);
	if(offset > bytes.size() || size > bytes.size() - offset) return false;
	memcpy(data, bytes.data() + offset, size);
	return true;
}

template<typename T>
void append(std::vector<uint8_t>& bytes, const T& value){
	const uint8_t* begin = reinterpret_cast<const uint8_t*>(&value);
	bytes.insert(bytes.end(), begin, begin + sizeof(value));
}

Entry entry(const char* path, uint64_t size, uint32_t mtime, uint32_t first, uint32_t last){
	Entry value;
	value.path = path;
	value.evidence.fileSize = size;
	value.evidence.fatMtime = mtime;
	value.evidence.flags = LibraryIndex::RecordHasFatMtime;
	value.evidence.firstBytes = std::min<uint64_t>(size, LibraryIndex::fingerprintBlockSize);
	value.evidence.lastBytes = value.evidence.firstBytes;
	value.evidence.firstCrc32 = first;
	value.evidence.lastCrc32 = last;
	return value;
}

std::vector<uint8_t> makeIndex(
	uint32_t generation,
	BuildState state,
	const CardIdentity& identity,
	const std::vector<Entry>& entries
){
	std::vector<uint8_t> payload;
	for(const Entry& item : entries){
		Record record{};
		record.recordSize = sizeof(record) + item.path.size() + 1;
		record.pathLength = item.path.size();
		record.flags = item.evidence.flags;
		record.firstBytes = item.evidence.firstBytes;
		record.lastBytes = item.evidence.lastBytes;
		record.fileSize = item.evidence.fileSize;
		record.fatMtime = item.evidence.fatMtime;
		record.firstCrc32 = item.evidence.firstCrc32;
		record.lastCrc32 = item.evidence.lastCrc32;
		append(payload, record);
		payload.insert(payload.end(), item.path.begin(), item.path.end());
		payload.push_back('\0');
	}

	Header header{};
	LibraryIndex::initializeHeader(header);
	header.generation = generation;
	header.buildState = static_cast<uint8_t>(state);
	header.identityStrength = static_cast<uint8_t>(identity.strength);
	header.cardType = identity.cardType;
	header.cardSize = identity.cardSize;
	header.volumeSize = identity.volumeSize;
	header.recordCount = entries.size();
	header.payloadLength = payload.size();
	header.payloadCrc32 = LibraryIndex::crc32Finish(LibraryIndex::crc32Update(
		LibraryIndex::crc32Start(),
		payload.data(),
		payload.size()
	));
	header.headerCrc32 = LibraryIndex::headerCrc(header);

	std::vector<uint8_t> bytes;
	append(bytes, header);
	bytes.insert(bytes.end(), payload.begin(), payload.end());
	return bytes;
}

Header& header(std::vector<uint8_t>& bytes){
	return *reinterpret_cast<Header*>(bytes.data());
}

Record& firstRecord(std::vector<uint8_t>& bytes){
	return *reinterpret_cast<Record*>(bytes.data() + sizeof(Header));
}

void refreshChecksums(std::vector<uint8_t>& bytes){
	Header& value = header(bytes);
	value.payloadCrc32 = LibraryIndex::crc32Finish(LibraryIndex::crc32Update(
		LibraryIndex::crc32Start(),
		bytes.data() + sizeof(Header),
		bytes.size() - sizeof(Header)
	));
	value.headerCrc32 = LibraryIndex::headerCrc(value);
}

ValidationResult validate(std::vector<uint8_t>& bytes, const Limits& customLimits = limits){
	return LibraryIndex::validate(
		readVector,
		&bytes,
		static_cast<uint32_t>(bytes.size()),
		customLimits
	);
}

bool pathsMatch(const std::vector<Entry>& indexed, const std::vector<Entry>& scanned){
	if(indexed.size() != scanned.size()) return false;
	for(size_t i = 0; i < indexed.size(); i++){
		if(indexed[i].path != scanned[i].path) return false;
		Record record{};
		record.flags = indexed[i].evidence.flags;
		record.fileSize = indexed[i].evidence.fileSize;
		record.fatMtime = indexed[i].evidence.fatMtime;
		record.firstBytes = indexed[i].evidence.firstBytes;
		record.lastBytes = indexed[i].evidence.lastBytes;
		record.firstCrc32 = indexed[i].evidence.firstCrc32;
		record.lastCrc32 = indexed[i].evidence.lastCrc32;
		if(!LibraryIndex::matchesFile(record, scanned[i].evidence)) return false;
	}
	return true;
}

int newestValid(
	std::vector<uint8_t>& a,
	std::vector<uint8_t>& b,
	const CardIdentity& identity
){
	const ValidationResult left = validate(a);
	const ValidationResult right = validate(b);
	const bool leftValid = left.error == ValidationError::None &&
		LibraryIndex::matchesCard(left.header, identity);
	const bool rightValid = right.error == ValidationError::None &&
		LibraryIndex::matchesCard(right.header, identity);
	if(leftValid && rightValid) return left.header.generation >= right.header.generation ? 0 : 1;
	if(leftValid) return 0;
	if(rightValid) return 1;
	return -1;
}

}

int main(){
	const std::vector<Entry> original{
		entry("/music/alpha.aac", 4096, 100, 0x11111111, 0x22222222),
		entry("/music/beta.aac", 8192, 200, 0x33333333, 0x44444444)
	};

	std::vector<uint8_t> generationA = makeIndex(7, BuildState::Complete, card, original);
	std::vector<uint8_t> generationB = makeIndex(8, BuildState::Complete, card, original);
	assert(validate(generationA).error == ValidationError::None);
	assert(newestValid(generationA, generationB, card) == 1);
	assert(LibraryIndex::stateAfterRecovery(
		LibraryIndex::State::Verifying,
		true
	) == LibraryIndex::State::Verifying);
	assert(LibraryIndex::stateAfterRecovery(
		LibraryIndex::State::Building,
		false
	) == LibraryIndex::State::Error);

	std::vector<uint8_t> interrupted = generationB;
	header(interrupted).buildState = static_cast<uint8_t>(BuildState::Building);
	header(interrupted).headerCrc32 = LibraryIndex::headerCrc(header(interrupted));
	assert(validate(interrupted).error == ValidationError::BuildState);
	assert(newestValid(generationA, interrupted, card) == 0);

	std::vector<uint8_t> corruptNewest = generationB;
	firstRecord(corruptNewest).firstCrc32 ^= 1;
	assert(validate(corruptNewest).error == ValidationError::PayloadCrc);
	assert(newestValid(generationA, corruptNewest, card) == 0);

	std::vector<uint8_t> malformed = generationA;
	header(malformed).headerSize++;
	header(malformed).headerCrc32 = LibraryIndex::headerCrc(header(malformed));
	assert(validate(malformed).error == ValidationError::HeaderSize);

	malformed = generationA;
	header(malformed).recordCount = 4097;
	header(malformed).headerCrc32 = LibraryIndex::headerCrc(header(malformed));
	assert(validate(malformed).error == ValidationError::Count);

	malformed = generationA;
	header(malformed).recordCount = 1;
	header(malformed).headerCrc32 = LibraryIndex::headerCrc(header(malformed));
	assert(validate(malformed).error == ValidationError::Count);

	malformed = generationA;
	header(malformed).payloadLength--;
	header(malformed).headerCrc32 = LibraryIndex::headerCrc(header(malformed));
	assert(validate(malformed).error == ValidationError::FileSize);

	malformed = generationA;
	header(malformed).payloadLength = UINT32_MAX;
	header(malformed).headerCrc32 = LibraryIndex::headerCrc(header(malformed));
	const Limits overflowLimits{4096, UINT32_MAX, 128 * 1024};
	assert(validate(malformed, overflowLimits).error == ValidationError::Overflow);

	malformed = generationA;
	firstRecord(malformed).recordSize = UINT16_MAX;
	refreshChecksums(malformed);
	assert(validate(malformed).error == ValidationError::RecordSize);

	malformed = makeIndex(
		1,
		BuildState::Complete,
		card,
		{entry("/music/../escape.aac", 64, 1, 1, 1)}
	);
	assert(validate(malformed).error == ValidationError::Path);

	std::string embeddedNul("/music/a\0b.aac", 14);
	malformed = makeIndex(
		1,
		BuildState::Complete,
		card,
		{Entry{embeddedNul, entry("/x.aac", 64, 1, 1, 1).evidence}}
	);
	assert(validate(malformed).error == ValidationError::PathNul);

	malformed = generationA;
	header(malformed).headerCrc32 ^= 1;
	assert(validate(malformed).error == ValidationError::HeaderCrc);

	malformed = generationA;
	header(malformed).magic[0] ^= 1;
	header(malformed).headerCrc32 = LibraryIndex::headerCrc(header(malformed));
	assert(validate(malformed).error == ValidationError::Magic);

	malformed = makeIndex(
		1,
		BuildState::Complete,
		card,
		{entry("/music/.hidden.aac", 64, 1, 1, 1)}
	);
	assert(validate(malformed).error == ValidationError::Path);

	malformed = makeIndex(
		1,
		BuildState::Complete,
		card,
		{entry("/music/not-supported.mp3", 64, 1, 1, 1)}
	);
	assert(validate(malformed).error == ValidationError::Path);

	const CardIdentity sameCapacityOtherType{
		IdentityStrength::Weak,
		card.cardType + 1,
		card.cardSize,
		card.volumeSize
	};
	const CardIdentity sameCapacityOtherVolume{
		IdentityStrength::Weak,
		card.cardType,
		card.cardSize,
		card.volumeSize - 4096
	};
	const ValidationResult valid = validate(generationA);
	assert(!LibraryIndex::matchesCard(valid.header, sameCapacityOtherType));
	assert(!LibraryIndex::matchesCard(valid.header, sameCapacityOtherVolume));
	std::vector<uint8_t> otherCardGeneration = makeIndex(
		8,
		BuildState::Complete,
		sameCapacityOtherVolume,
		original
	);
	assert(newestValid(generationA, otherCardGeneration, card) == 0);

	std::vector<Entry> removed{original[0]};
	std::vector<Entry> added = original;
	added.push_back(entry("/music/gamma.aac", 16384, 300, 5, 6));
	std::vector<Entry> renamed = original;
	renamed[1].path = "/music/renamed.aac";
	assert(pathsMatch(original, original));
	assert(!pathsMatch(original, removed));
	assert(!pathsMatch(original, added));
	assert(!pathsMatch(original, renamed));
	renamed[1].evidence.firstCrc32 ^= 1;
	assert(!pathsMatch(original, renamed));

	std::vector<uint8_t> rebuilt = makeIndex(9, BuildState::Complete, card, added);
	assert(validate(rebuilt).error == ValidationError::None);
	assert(newestValid(generationA, rebuilt, card) == 1);
	assert(validate(generationA).error == ValidationError::None);

	std::cout << "library index self-checks passed\n";
	return 0;
}
