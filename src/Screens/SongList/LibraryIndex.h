#ifndef JAYD_FIRMWARE_LIBRARY_INDEX_H
#define JAYD_FIRMWARE_LIBRARY_INDEX_H

#include <stddef.h>
#include <stdint.h>

namespace LibraryIndex {

static const uint16_t schemaVersion = 2;
static const uint16_t maxPathLength = 255;
static const uint16_t fingerprintBlockSize = 64;

enum class BuildState : uint8_t {
	Building = 1,
	Complete = 2
};

enum class IdentityStrength : uint8_t {
	Unknown = 0,
	Weak = 1,
	Strong = 2
};

enum class State : uint8_t {
	Absent,
	Building,
	Verifying,
	Ready,
	Stale,
	Error
};

enum HeaderFlags : uint16_t {
	HeaderHasFatMtime = 1 << 0,
	HeaderHasFingerprints = 1 << 1,
	HeaderScanLimited = 1 << 2
};

enum RecordFlags : uint16_t {
	RecordHasFatMtime = 1 << 0
};

#pragma pack(push, 1)
struct Header {
	uint8_t magic[8];
	uint16_t schema;
	uint16_t headerSize;
	uint32_t generation;
	uint8_t buildState;
	uint8_t identityStrength;
	uint16_t flags;
	uint32_t cardType;
	uint64_t cardSize;
	uint64_t volumeSize;
	uint32_t recordCount;
	uint32_t payloadLength;
	uint32_t payloadCrc32;
	uint32_t headerCrc32;
	uint32_t reserved[2];
};

struct Record {
	uint16_t recordSize;
	uint16_t pathLength;
	uint16_t flags;
	uint16_t firstBytes;
	uint16_t lastBytes;
	uint16_t reserved;
	uint64_t fileSize;
	uint32_t fatMtime;
	uint32_t firstCrc32;
	uint32_t lastCrc32;
};
#pragma pack(pop)

static_assert(sizeof(Header) == 64, "Library index header contract changed");
static_assert(sizeof(Record) == 32, "Library index record contract changed");

struct CardIdentity {
	IdentityStrength strength;
	uint32_t cardType;
	uint64_t cardSize;
	uint64_t volumeSize;
};

struct VerifiedIdentity {
	CardIdentity card;
	uint32_t generation;
	uint32_t payloadCrc;
	bool valid;
};

enum class IdentityAction : uint8_t {
	Preserve,
	Refresh,
	Invalidate
};

struct Limits {
	uint32_t maxRecords;
	uint32_t maxPayloadLength;
	uint32_t maxPathBytes;
};

enum class ValidationError : uint8_t {
	None,
	Read,
	Magic,
	Schema,
	HeaderSize,
	HeaderCrc,
	Generation,
	BuildState,
	Identity,
	Flags,
	Reserved,
	Count,
	PayloadLength,
	FileSize,
	PayloadCrc,
	RecordSize,
	RecordFlags,
	Fingerprint,
	PathLength,
	PathNul,
	Path,
	Overflow
};

typedef bool (*ReadAt)(void* context, uint32_t offset, uint8_t* data, size_t size);

struct ValidationResult {
	ValidationError error;
	Header header;
	uint32_t pathBytes;
};

struct FileEvidence {
	uint64_t fileSize;
	uint32_t fatMtime;
	uint16_t flags;
	uint16_t firstBytes;
	uint16_t lastBytes;
	uint32_t firstCrc32;
	uint32_t lastCrc32;
};

uint32_t crc32Start();
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t size);
uint32_t crc32Finish(uint32_t crc);
uint32_t headerCrc(const Header& header);
void initializeHeader(Header& header);
bool matchesCard(const Header& header, const CardIdentity& identity);
IdentityAction updateVerifiedIdentity(
	VerifiedIdentity& verified,
	const CardIdentity& currentCard,
	bool attemptVerified,
	uint32_t generation,
	uint32_t payloadCrc
);
bool isSupportedPath(const char* path, size_t length);
bool matchesFile(const Record& record, const FileEvidence& evidence);
State stateAfterRecovery(State recoveredState, bool recovered);
ValidationResult validate(ReadAt readAt, void* context, uint32_t fileSize, const Limits& limits);
const char* validationErrorName(ValidationError error);

}

#endif
