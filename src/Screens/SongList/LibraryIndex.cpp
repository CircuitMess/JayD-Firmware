#include "LibraryIndex.h"

#include <limits.h>
#include <string.h>

namespace LibraryIndex {
namespace {

const uint8_t indexMagic[8] = { 'J', 'A', 'Y', 'D', 'I', 'D', 'X', '2' };
const uint16_t knownHeaderFlags =
	HeaderHasFatMtime | HeaderHasFingerprints | HeaderScanLimited;
const uint16_t knownRecordFlags = RecordHasFatMtime;

ValidationResult fail(ValidationError error, const Header& header, uint32_t pathBytes = 0){
	ValidationResult result{};
	result.error = error;
	result.header = header;
	result.pathBytes = pathBytes;
	return result;
}

bool addWouldOverflow(uint32_t left, uint32_t right){
	return right > UINT32_MAX - left;
}

bool isAAC(const char* path, size_t length){
	if(length < 5 || path[length - 4] != '.') return false;
	return (path[length - 3] == 'a' || path[length - 3] == 'A') &&
		   (path[length - 2] == 'a' || path[length - 2] == 'A') &&
		   (path[length - 1] == 'c' || path[length - 1] == 'C');
}

}

uint32_t crc32Start(){
	return UINT32_MAX;
}

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t size){
	static const uint32_t table[16] = {
		0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
		0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
		0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
		0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL
	};
	while(size--){
		const uint8_t value = *data++;
		crc = (crc >> 4) ^ table[(crc ^ value) & 0x0F];
		crc = (crc >> 4) ^ table[(crc ^ (value >> 4)) & 0x0F];
	}
	return crc;
}

uint32_t crc32Finish(uint32_t crc){
	return crc ^ UINT32_MAX;
}

uint32_t headerCrc(const Header& header){
	Header copy = header;
	copy.headerCrc32 = 0;
	return crc32Finish(crc32Update(
		crc32Start(),
		reinterpret_cast<const uint8_t*>(&copy),
		sizeof(copy)
	));
}

void initializeHeader(Header& header){
	memset(&header, 0, sizeof(header));
	memcpy(header.magic, indexMagic, sizeof(indexMagic));
	header.schema = schemaVersion;
	header.headerSize = sizeof(Header);
	header.identityStrength = static_cast<uint8_t>(IdentityStrength::Weak);
	header.flags = HeaderHasFatMtime | HeaderHasFingerprints;
}

bool matchesCard(const Header& header, const CardIdentity& identity){
	return header.identityStrength == static_cast<uint8_t>(identity.strength) &&
		   header.cardType == identity.cardType &&
		   header.cardSize == identity.cardSize &&
		   header.volumeSize == identity.volumeSize;
}

IdentityAction updateVerifiedIdentity(
	VerifiedIdentity& verified,
	const CardIdentity& currentCard,
	bool attemptVerified,
	uint32_t generation,
	uint32_t payloadCrc
){
	if(attemptVerified){
		verified.card = currentCard;
		verified.generation = generation;
		verified.payloadCrc = payloadCrc;
		verified.valid = true;
		return IdentityAction::Refresh;
	}
	if(!verified.valid) return IdentityAction::Preserve;
	if(verified.card.strength == currentCard.strength &&
	   verified.card.cardType == currentCard.cardType &&
	   verified.card.cardSize == currentCard.cardSize &&
	   verified.card.volumeSize == currentCard.volumeSize){
		return IdentityAction::Preserve;
	}
	verified = VerifiedIdentity{};
	return IdentityAction::Invalidate;
}

bool isSupportedPath(const char* path, size_t length){
	if(path == nullptr || length < 5 || length > maxPathLength || path[0] != '/' ||
	   path[length - 1] == '/' || !isAAC(path, length)){
		return false;
	}

	size_t componentStart = 1;
	for(size_t i = 1; i <= length; i++){
		const bool componentEnd = i == length || path[i] == '/';
		if(!componentEnd){
			const uint8_t value = static_cast<uint8_t>(path[i]);
			if(value < 0x20 || path[i] == '\\') return false;
			continue;
		}

		const size_t componentLength = i - componentStart;
		if(componentLength == 0 || path[componentStart] == '.'){
			return false;
		}
		componentStart = i + 1;
	}
	return true;
}

bool matchesFile(const Record& record, const FileEvidence& evidence){
	if(record.fileSize != evidence.fileSize ||
	   record.firstBytes != evidence.firstBytes ||
	   record.lastBytes != evidence.lastBytes ||
	   record.firstCrc32 != evidence.firstCrc32 ||
	   record.lastCrc32 != evidence.lastCrc32){
		return false;
	}
	if((record.flags & RecordHasFatMtime) != 0){
		return (evidence.flags & RecordHasFatMtime) != 0 &&
			   record.fatMtime == evidence.fatMtime;
	}
	return true;
}

State stateAfterRecovery(State recoveredState, bool recovered){
	return recovered ? recoveredState : State::Error;
}

ValidationResult validate(ReadAt readAt, void* context, uint32_t fileSize, const Limits& limits){
	Header header{};
	if(readAt == nullptr || fileSize < sizeof(Header) ||
	   !readAt(context, 0, reinterpret_cast<uint8_t*>(&header), sizeof(header))){
		return fail(ValidationError::Read, header);
	}
	if(memcmp(header.magic, indexMagic, sizeof(indexMagic)) != 0) return fail(ValidationError::Magic, header);
	if(header.schema != schemaVersion) return fail(ValidationError::Schema, header);
	if(header.headerSize != sizeof(Header)) return fail(ValidationError::HeaderSize, header);
	if(header.headerCrc32 != headerCrc(header)) return fail(ValidationError::HeaderCrc, header);
	if(header.generation == 0) return fail(ValidationError::Generation, header);
	if(header.buildState != static_cast<uint8_t>(BuildState::Complete)){
		return fail(ValidationError::BuildState, header);
	}
	if(header.identityStrength != static_cast<uint8_t>(IdentityStrength::Weak)){
		return fail(ValidationError::Identity, header);
	}
	if((header.flags & ~knownHeaderFlags) != 0 ||
	   (header.flags & HeaderHasFingerprints) == 0){
		return fail(ValidationError::Flags, header);
	}
	if(header.reserved[0] != 0 || header.reserved[1] != 0){
		return fail(ValidationError::Reserved, header);
	}
	if(header.recordCount > limits.maxRecords) return fail(ValidationError::Count, header);
	if(header.payloadLength > limits.maxPayloadLength){
		return fail(ValidationError::PayloadLength, header);
	}
	if(addWouldOverflow(sizeof(Header), header.payloadLength)){
		return fail(ValidationError::Overflow, header);
	}
	if(fileSize != sizeof(Header) + header.payloadLength){
		return fail(ValidationError::FileSize, header);
	}

	uint32_t offset = sizeof(Header);
	uint32_t remaining = header.payloadLength;
	uint32_t pathBytes = 0;
	uint32_t payloadCrc = crc32Start();
	for(uint32_t i = 0; i < header.recordCount; i++){
		if(remaining < sizeof(Record)) return fail(ValidationError::RecordSize, header, pathBytes);

		Record record{};
		if(!readAt(context, offset, reinterpret_cast<uint8_t*>(&record), sizeof(record))){
			return fail(ValidationError::Read, header, pathBytes);
		}
		payloadCrc = crc32Update(
			payloadCrc,
			reinterpret_cast<const uint8_t*>(&record),
			sizeof(record)
		);
		if((record.flags & ~knownRecordFlags) != 0 || record.reserved != 0 ||
		   ((record.flags & RecordHasFatMtime) == 0 && record.fatMtime != 0)){
			return fail(ValidationError::RecordFlags, header, pathBytes);
		}

		const uint16_t expectedBytes = static_cast<uint16_t>(
			record.fileSize < fingerprintBlockSize ? record.fileSize : fingerprintBlockSize
		);
		if(record.firstBytes != expectedBytes || record.lastBytes != expectedBytes){
			return fail(ValidationError::Fingerprint, header, pathBytes);
		}
		if(record.pathLength == 0 || record.pathLength > maxPathLength){
			return fail(ValidationError::PathLength, header, pathBytes);
		}
		const uint32_t pathStored = static_cast<uint32_t>(record.pathLength) + 1;
		if(pathStored > UINT16_MAX - sizeof(Record) ||
		   record.recordSize != sizeof(Record) + pathStored ||
		   record.recordSize > remaining){
			return fail(ValidationError::RecordSize, header, pathBytes);
		}
		if(addWouldOverflow(pathBytes, pathStored) ||
		   pathBytes + pathStored > limits.maxPathBytes){
			return fail(ValidationError::Overflow, header, pathBytes);
		}

		char path[maxPathLength + 1];
		if(!readAt(
			context,
			offset + sizeof(Record),
			reinterpret_cast<uint8_t*>(path),
			pathStored
		)){
			return fail(ValidationError::Read, header, pathBytes);
		}
		payloadCrc = crc32Update(
			payloadCrc,
			reinterpret_cast<const uint8_t*>(path),
			pathStored
		);
		if(path[record.pathLength] != '\0' ||
		   memchr(path, '\0', record.pathLength) != nullptr){
			return fail(ValidationError::PathNul, header, pathBytes);
		}
		if(!isSupportedPath(path, record.pathLength)){
			return fail(ValidationError::Path, header, pathBytes);
		}

		offset += record.recordSize;
		remaining -= record.recordSize;
		pathBytes += pathStored;
	}
	if(remaining != 0 || offset != fileSize){
		return fail(ValidationError::Count, header, pathBytes);
	}
	if(crc32Finish(payloadCrc) != header.payloadCrc32){
		return fail(ValidationError::PayloadCrc, header, pathBytes);
	}

	ValidationResult result{};
	result.error = ValidationError::None;
	result.header = header;
	result.pathBytes = pathBytes;
	return result;
}

const char* validationErrorName(ValidationError error){
	switch(error){
		case ValidationError::None: return "none";
		case ValidationError::Read: return "read";
		case ValidationError::Magic: return "magic";
		case ValidationError::Schema: return "schema";
		case ValidationError::HeaderSize: return "header-size";
		case ValidationError::HeaderCrc: return "header-crc";
		case ValidationError::Generation: return "generation";
		case ValidationError::BuildState: return "build-state";
		case ValidationError::Identity: return "identity";
		case ValidationError::Flags: return "flags";
		case ValidationError::Reserved: return "reserved";
		case ValidationError::Count: return "count";
		case ValidationError::PayloadLength: return "payload-length";
		case ValidationError::FileSize: return "file-size";
		case ValidationError::PayloadCrc: return "payload-crc";
		case ValidationError::RecordSize: return "record-size";
		case ValidationError::RecordFlags: return "record-flags";
		case ValidationError::Fingerprint: return "fingerprint";
		case ValidationError::PathLength: return "path-length";
		case ValidationError::PathNul: return "path-nul";
		case ValidationError::Path: return "path";
		case ValidationError::Overflow: return "overflow";
	}
	return "unknown";
}

}
