#include <SD.h>
#include "SongList.h"
#include "../MainMenu/MainMenu.h"
#include <JayD.h>
#include <SPIFFS.h>
#include <FS/CompressedFile.h>
#include "../../Fonts.h"
#include "../../DjSession/DjSession.h"
#include "../MixScreen/MixControlState.h"

// Out-of-class definitions (no initializer - it's already supplied in the
// class body) for the static const integral members that are odr-used by
// reference (e.g. min(capacity, maxPathPayload)/min(capacity, maxTrackCount)
// in reservePaths()/reserveSongs() below). Without these, whether the build
// links depends on the optimizer fully inlining/folding away every
// reference - a real link failure surfaced only when a full firmware
// compile pulled these functions in without doing so; this is the portable
// fix rather than relying on that.
const size_t SongList::SongList::maxTrackCount;
const size_t SongList::SongList::maxPathLength;
const size_t SongList::SongList::maxPathPayload;
const size_t SongList::SongList::maxIndexPayload;
const uint8_t SongList::SongList::maxDirectoryDepth;
const uint8_t SongList::SongList::visibleRows;
const uint8_t SongList::SongList::rowHeight;
const uint32_t SongList::SongList::holdTime;

namespace {
const char* indexPathA = "/.jayd-library.a";
const char* indexPathB = "/.jayd-library.b";

struct IndexProbe {
	bool valid = false;
	bool matchesCard = false;
	LibraryIndex::ValidationResult validation{};
};

bool readFileAt(void* context, uint32_t offset, uint8_t* data, size_t size){
	File* file = static_cast<File*>(context);
	return file->seek(offset) && file->read(data, size) == size;
}

IndexProbe probeIndex(
	const char* path,
	const LibraryIndex::Limits& limits,
	const LibraryIndex::CardIdentity& identity
){
	IndexProbe probe;
	File file = SD.open(path);
	if(!file) return probe;

	const size_t size = file.size();
	if(size <= UINT32_MAX){
		probe.validation = LibraryIndex::validate(
			readFileAt,
			&file,
			static_cast<uint32_t>(size),
			limits
		);
		probe.valid = probe.validation.error == LibraryIndex::ValidationError::None;
		probe.matchesCard = probe.valid &&
			LibraryIndex::matchesCard(probe.validation.header, identity);
	}
	file.close();
	return probe;
}

bool normalizePath(const char* path, char* normalized, size_t capacity){
	if(path == nullptr || capacity < 2) return false;
	const size_t length = strnlen(path, LibraryIndex::maxPathLength + 1);
	if(length == 0 || length > LibraryIndex::maxPathLength) return false;

	const bool needsSlash = path[0] != '/';
	if(length + (needsSlash ? 1 : 0) >= capacity) return false;
	size_t offset = 0;
	if(needsSlash) normalized[offset++] = '/';
	memcpy(normalized + offset, path, length + 1);
	return true;
}

bool isHiddenEntry(const char* path){
	const char* name = strrchr(path, '/');
	name = name == nullptr ? path : name + 1;
	return name[0] == '.';
}

uint64_t libraryKey(
	const LibraryIndex::CardIdentity& identity,
	uint32_t generation,
	uint32_t payloadCrc
){
	uint64_t key = 1469598103934665603ULL;
	const uint64_t values[] = {
		identity.cardType,
		identity.cardSize,
		identity.volumeSize,
		generation,
		payloadCrc
	};
	for(const uint64_t value : values){
		for(uint8_t byte = 0; byte < 8; ++byte){
			key = (key ^ uint8_t(value >> (byte * 8))) * 1099511628211ULL;
		}
	}
	return key;
}
}

SongList::SongList* SongList::SongList::instance = nullptr;

SongList::SongList::SongList(Display& display, DjSession* browseSession) :
		Context(display), browseSession(browseSession), browseMode(browseSession != nullptr){
	instance = this;
	SongList::pack();
}

SongList::SongList::~SongList(){
	instance = nullptr;
	clearSongs();
	free(backgroundBuffer);
}

void SongList::SongList::clearSongs(){
	free(pathBuffer);
	free(songOffsets);
	free(songMetadata);
	pathBuffer = nullptr;
	songOffsets = nullptr;
	songMetadata = nullptr;
	pathBytes = 0;
	pathCapacity = 0;
	songCount = 0;
	songCapacity = 0;
}

bool SongList::SongList::reservePaths(size_t required){
	if(required <= pathCapacity) return true;

	size_t capacity = pathCapacity == 0 ? 4096 : pathCapacity;
	while(capacity < required && capacity < maxPathPayload){
		capacity = min(capacity * 2, maxPathPayload);
	}
	if(capacity < required) return false;

	char* resized = static_cast<char*>(ps_realloc(pathBuffer, capacity));
	if(resized == nullptr) return false;
	pathBuffer = resized;
	pathCapacity = capacity;
	return true;
}

bool SongList::SongList::reserveSongs(size_t required){
	if(required <= songCapacity) return true;

	size_t capacity = songCapacity == 0 ? 64 : songCapacity;
	while(capacity < required && capacity < maxTrackCount){
		capacity = min(capacity * 2, maxTrackCount);
	}
	if(capacity < required) return false;

	uint32_t* resized = static_cast<uint32_t*>(
		ps_realloc(songOffsets, capacity * sizeof(uint32_t))
	);
	if(resized == nullptr) return false;
	songOffsets = resized;

	LibraryIndex::FileEvidence* resizedMetadata = static_cast<LibraryIndex::FileEvidence*>(
		ps_realloc(songMetadata, capacity * sizeof(LibraryIndex::FileEvidence))
	);
	if(resizedMetadata == nullptr) return false;
	songMetadata = resizedMetadata;
	songCapacity = capacity;
	return true;
}

bool SongList::SongList::fingerprintFile(
	File& file,
	LibraryIndex::FileEvidence& evidence
){
	memset(&evidence, 0, sizeof(evidence));
	evidence.fileSize = file.size();
	const uint16_t sampleSize = static_cast<uint16_t>(
		evidence.fileSize < LibraryIndex::fingerprintBlockSize ?
			evidence.fileSize : LibraryIndex::fingerprintBlockSize
	);
	evidence.firstBytes = sampleSize;
	evidence.lastBytes = sampleSize;

	const time_t modified = file.getLastWrite();
	if(modified > 0 && static_cast<uint64_t>(modified) <= UINT32_MAX){
		evidence.flags |= LibraryIndex::RecordHasFatMtime;
		evidence.fatMtime = static_cast<uint32_t>(modified);
	}

	uint8_t sample[LibraryIndex::fingerprintBlockSize];
	if(sampleSize == 0){
		evidence.firstCrc32 = LibraryIndex::crc32Finish(LibraryIndex::crc32Start());
		evidence.lastCrc32 = evidence.firstCrc32;
		return true;
	}
	if(!file.seek(0) || file.read(sample, sampleSize) != sampleSize) return false;
	evidence.firstCrc32 = LibraryIndex::crc32Finish(
		LibraryIndex::crc32Update(LibraryIndex::crc32Start(), sample, sampleSize)
	);
	const uint64_t lastOffset = evidence.fileSize - sampleSize;
	if(lastOffset > UINT32_MAX || !file.seek(static_cast<uint32_t>(lastOffset)) ||
	   file.read(sample, sampleSize) != sampleSize){
		return false;
	}
	evidence.lastCrc32 = LibraryIndex::crc32Finish(
		LibraryIndex::crc32Update(LibraryIndex::crc32Start(), sample, sampleSize)
	);
	return true;
}

bool SongList::SongList::addSong(const char* path, File& file){
	const size_t length = strnlen(path, maxPathLength + 1);
	if(length == 0 || length > maxPathLength ||
	   !LibraryIndex::isSupportedPath(path, length)){
		scanLimited = true;
		return true;
	}

	const size_t storedLength = length + 1;
	if(songCount >= maxTrackCount || storedLength > maxPathPayload - pathBytes){
		scanLimited = true;
		scanStoppedAtLimit = true;
		return false;
	}

	if(!reservePaths(pathBytes + storedLength) || !reserveSongs(songCount + 1)){
		allocationFailed = true;
		return false;
	}

	LibraryIndex::FileEvidence evidence{};
	if(!fingerprintFile(file, evidence)) return false;

	songOffsets[songCount++] = pathBytes;
	songMetadata[songCount - 1] = evidence;
	memcpy(pathBuffer + pathBytes, path, storedLength);
	pathBytes += storedLength;
	indexProgress = songCount;
	return true;
}

const char* SongList::SongList::songPath(size_t index) const{
	if(index >= songCount || pathBuffer == nullptr || songOffsets == nullptr) return nullptr;
	return pathBuffer + songOffsets[index];
}

LibraryIndex::CardIdentity SongList::SongList::currentCardIdentity() const{
	LibraryIndex::CardIdentity identity{};
	identity.strength = LibraryIndex::IdentityStrength::Weak;
	identity.cardType = static_cast<uint32_t>(SD.cardType());
	identity.cardSize = SD.cardSize();
	identity.volumeSize = SD.totalBytes();
	return identity;
}

bool SongList::SongList::loadIndex(const char* path){
	const LibraryIndex::Limits limits{
		maxTrackCount,
		maxIndexPayload,
		maxPathPayload
	};
	File file = SD.open(path);
	if(!file || file.size() > UINT32_MAX){
		file.close();
		return false;
	}
	const LibraryIndex::ValidationResult validation = LibraryIndex::validate(
		readFileAt,
		&file,
		static_cast<uint32_t>(file.size()),
		limits
	);
	if(validation.error != LibraryIndex::ValidationError::None ||
	   !LibraryIndex::matchesCard(validation.header, currentCardIdentity())){
		file.close();
		return false;
	}

	clearSongs();
	if(validation.pathBytes > 0){
		pathBuffer = static_cast<char*>(ps_malloc(validation.pathBytes));
		if(pathBuffer == nullptr){
			allocationFailed = true;
			file.close();
			return false;
		}
	}
	if(validation.header.recordCount > 0){
		songOffsets = static_cast<uint32_t*>(
			ps_malloc(validation.header.recordCount * sizeof(uint32_t))
		);
		songMetadata = static_cast<LibraryIndex::FileEvidence*>(
			ps_malloc(validation.header.recordCount * sizeof(LibraryIndex::FileEvidence))
		);
		if(songOffsets == nullptr || songMetadata == nullptr){
			allocationFailed = true;
			file.close();
			clearSongs();
			return false;
		}
	}

	uint32_t offset = sizeof(LibraryIndex::Header);
	uint32_t remaining = validation.header.payloadLength;
	uint32_t payloadCrc = LibraryIndex::crc32Start();
	bool loaded = true;
	for(uint32_t i = 0; i < validation.header.recordCount; i++){
		if(remaining < sizeof(LibraryIndex::Record)){
			loaded = false;
			break;
		}
		LibraryIndex::Record record{};
		if(!file.seek(offset) ||
		   file.read(reinterpret_cast<uint8_t*>(&record), sizeof(record)) != sizeof(record)){
			loaded = false;
			break;
		}
		const uint32_t storedLength = static_cast<uint32_t>(record.pathLength) + 1;
		const uint16_t expectedBytes = static_cast<uint16_t>(
			record.fileSize < LibraryIndex::fingerprintBlockSize ?
				record.fileSize : LibraryIndex::fingerprintBlockSize
		);
		if(record.pathLength == 0 || record.pathLength > maxPathLength ||
		   record.recordSize != sizeof(record) + storedLength ||
		   record.recordSize > remaining ||
		   pathBytes > validation.pathBytes ||
		   storedLength > validation.pathBytes - pathBytes ||
		   (record.flags & ~LibraryIndex::RecordHasFatMtime) != 0 ||
		   record.reserved != 0 ||
		   ((record.flags & LibraryIndex::RecordHasFatMtime) == 0 && record.fatMtime != 0) ||
		   record.firstBytes != expectedBytes || record.lastBytes != expectedBytes){
			loaded = false;
			break;
		}

		char pathValue[maxPathLength + 1];
		if(file.read(reinterpret_cast<uint8_t*>(pathValue), storedLength) != storedLength ||
		   pathValue[record.pathLength] != '\0' ||
		   memchr(pathValue, '\0', record.pathLength) != nullptr ||
		   !LibraryIndex::isSupportedPath(pathValue, record.pathLength)){
			loaded = false;
			break;
		}
		payloadCrc = LibraryIndex::crc32Update(
			payloadCrc,
			reinterpret_cast<const uint8_t*>(&record),
			sizeof(record)
		);
		payloadCrc = LibraryIndex::crc32Update(
			payloadCrc,
			reinterpret_cast<const uint8_t*>(pathValue),
			storedLength
		);

		songOffsets[i] = pathBytes;
		memcpy(pathBuffer + pathBytes, pathValue, storedLength);
		LibraryIndex::FileEvidence& metadata = songMetadata[i];
		metadata.fileSize = record.fileSize;
		metadata.fatMtime = record.fatMtime;
		metadata.flags = record.flags;
		metadata.firstBytes = record.firstBytes;
		metadata.lastBytes = record.lastBytes;
		metadata.firstCrc32 = record.firstCrc32;
		metadata.lastCrc32 = record.lastCrc32;
		pathBytes += storedLength;
		offset += record.recordSize;
		remaining -= record.recordSize;
	}
	file.close();
	if(!loaded || remaining != 0 || pathBytes != validation.pathBytes ||
	   LibraryIndex::crc32Finish(payloadCrc) != validation.header.payloadCrc32){
		clearSongs();
		return false;
	}

	pathCapacity = pathBytes;
	songCount = validation.header.recordCount;
	songCapacity = songCount;
	indexGeneration = validation.header.generation;
	indexPayloadCrc = validation.header.payloadCrc32;
	scanLimited = (validation.header.flags & LibraryIndex::HeaderScanLimited) != 0;
	indexState = IndexState::Verifying;
	indexProgress = 0;
	indexProgressTotal = songCount;
	return true;
}

bool SongList::SongList::loadBestIndex(){
	const LibraryIndex::Limits limits{
		maxTrackCount,
		maxIndexPayload,
		maxPathPayload
	};
	const LibraryIndex::CardIdentity identity = currentCardIdentity();
	const IndexProbe a = probeIndex(indexPathA, limits, identity);
	const IndexProbe b = probeIndex(indexPathB, limits, identity);

	if(SD.exists(indexPathA) && !a.valid){
		Serial.printf(
			"SongList: index A rejected: %s\n",
			LibraryIndex::validationErrorName(a.validation.error)
		);
	}
	if(SD.exists(indexPathB) && !b.valid){
		Serial.printf(
			"SongList: index B rejected: %s\n",
			LibraryIndex::validationErrorName(b.validation.error)
		);
	}

	const char* chosen = nullptr;
	if(a.matchesCard && b.matchesCard){
		chosen = a.validation.header.generation >= b.validation.header.generation ?
			indexPathA : indexPathB;
	}else if(a.matchesCard){
		chosen = indexPathA;
	}else if(b.matchesCard){
		chosen = indexPathB;
	}else if(a.valid || b.valid){
		indexState = IndexState::Stale;
	}
	return chosen != nullptr && loadIndex(chosen);
}

bool SongList::SongList::writeGeneration(const char* path, uint32_t generation){
	LibraryIndex::Header header{};
	LibraryIndex::initializeHeader(header);
	header.generation = generation;
	header.buildState = static_cast<uint8_t>(LibraryIndex::BuildState::Building);
	const LibraryIndex::CardIdentity identity = currentCardIdentity();
	header.identityStrength = static_cast<uint8_t>(identity.strength);
	header.cardType = identity.cardType;
	header.cardSize = identity.cardSize;
	header.volumeSize = identity.volumeSize;
	header.recordCount = songCount;
	header.payloadLength = songCount * sizeof(LibraryIndex::Record) + pathBytes;
	if(scanLimited) header.flags |= LibraryIndex::HeaderScanLimited;
	header.headerCrc32 = LibraryIndex::headerCrc(header);

	File index = SD.open(path, "w");
	if(!index ||
	   index.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) != sizeof(header)){
		index.close();
		return false;
	}

	uint32_t payloadCrc = LibraryIndex::crc32Start();
	for(size_t i = 0; i < songCount; i++){
		const char* pathValue = songPath(i);
		const size_t pathLength = strlen(pathValue);
		const LibraryIndex::FileEvidence& evidence = songMetadata[i];
		LibraryIndex::Record record{};
		record.recordSize = sizeof(record) + pathLength + 1;
		record.pathLength = pathLength;
		record.flags = evidence.flags;
		record.firstBytes = evidence.firstBytes;
		record.lastBytes = evidence.lastBytes;
		record.fileSize = evidence.fileSize;
		record.fatMtime = evidence.fatMtime;
		record.firstCrc32 = evidence.firstCrc32;
		record.lastCrc32 = evidence.lastCrc32;
		if(index.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record)) != sizeof(record) ||
		   index.write(reinterpret_cast<const uint8_t*>(pathValue), pathLength + 1) != pathLength + 1){
			index.close();
			return false;
		}
		payloadCrc = LibraryIndex::crc32Update(
			payloadCrc,
			reinterpret_cast<const uint8_t*>(&record),
			sizeof(record)
		);
		payloadCrc = LibraryIndex::crc32Update(
			payloadCrc,
			reinterpret_cast<const uint8_t*>(pathValue),
			pathLength + 1
		);
	}

	header.payloadCrc32 = LibraryIndex::crc32Finish(payloadCrc);
	header.buildState = static_cast<uint8_t>(LibraryIndex::BuildState::Complete);
	header.headerCrc32 = LibraryIndex::headerCrc(header);
	if(!index.seek(0) ||
	   index.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) != sizeof(header)){
		index.close();
		return false;
	}
	index.flush();
	index.close();

	const LibraryIndex::Limits limits{
		maxTrackCount,
		maxIndexPayload,
		maxPathPayload
	};
	const IndexProbe written = probeIndex(path, limits, identity);
	const bool committed = written.matchesCard &&
		written.validation.header.generation == generation;
	if(committed) indexPayloadCrc = header.payloadCrc32;
	return committed;
}

bool SongList::SongList::buildIndex(){
	indexState = IndexState::Building;
	indexProgress = 0;
	indexProgressTotal = 0;
	waiting = false;
	draw();
	screen.commit();

	clearSongs();
	File root = SD.open("/");
	if(!root) return false;
	const bool scanned = searchDirectories(root, 0);
	root.close();
	if((!scanned && !scanStoppedAtLimit) || allocationFailed) return false;

	const LibraryIndex::Limits limits{
		maxTrackCount,
		maxIndexPayload,
		maxPathPayload
	};
	const LibraryIndex::CardIdentity identity = currentCardIdentity();
	const IndexProbe a = probeIndex(indexPathA, limits, identity);
	const IndexProbe b = probeIndex(indexPathB, limits, identity);
	uint32_t latestGeneration = 0;
	if(a.matchesCard) latestGeneration = a.validation.header.generation;
	if(b.matchesCard) latestGeneration = max(latestGeneration, b.validation.header.generation);
	if(latestGeneration == UINT32_MAX) return false;

	const char* inactive = indexPathA;
	if(a.matchesCard && b.matchesCard){
		inactive = a.validation.header.generation <= b.validation.header.generation ?
			indexPathA : indexPathB;
	}else if(a.matchesCard){
		inactive = indexPathB;
	}else if(b.matchesCard){
		inactive = indexPathA;
	}else if(a.valid && !b.valid){
		inactive = indexPathB;
	}else if(b.valid && !a.valid){
		inactive = indexPathA;
	}else if(a.valid && b.valid){
		inactive = a.validation.header.generation <= b.validation.header.generation ?
			indexPathA : indexPathB;
	}

	indexState = IndexState::Verifying;
	draw();
	screen.commit();
	if(!writeGeneration(inactive, latestGeneration + 1)) return false;

	indexGeneration = latestGeneration + 1;
	indexState = IndexState::Ready;
	indexProgress = songCount;
	indexProgressTotal = songCount;
	return true;
}

void SongList::SongList::checkSD(bool forceRebuild){
	clearSongs();
	selectedElement = 0;
	firstVisible = 0;
	empty = true;
	scanLimited = false;
	scanStoppedAtLimit = false;
	allocationFailed = false;
	indexState = IndexState::Absent;
	indexProgress = 0;
	indexProgressTotal = 0;
	identityStrength = LibraryIndex::IdentityStrength::Unknown;
	waiting = true;

	if(!insertedSD){
		insertedSD = SD.begin(22, SPI);
	}

	if(!insertedSD){
		verifiedIndexIdentity = {};
		indexGeneration = 0;
		indexPayloadCrc = 0;
		if(DjSession::get()) DjSession::get()->invalidateLibraryMetadata();
		waiting = false;
		draw();
		screen.commit();
		return;
	}

	identityStrength = LibraryIndex::IdentityStrength::Weak;
	draw();
	screen.commit();

	File root = SD.open("/");
	insertedSD = root;
	if(!insertedSD){
		root.close();
		verifiedIndexIdentity = {};
		indexGeneration = 0;
		indexPayloadCrc = 0;
		if(DjSession::get()) DjSession::get()->invalidateLibraryMetadata();
		waiting = false;
		draw();
		screen.commit();
		return;
	}
	root.close();

	const bool indexed = loadBestIndex();
	bool identityVerified = indexed;
	DjSession* session = DjSession::get();
	const bool rebuildRequested = forceRebuild || !indexed;
	const bool rebuildAllowed = !session || session->libraryWorkAllowed();
	if(rebuildRequested && rebuildAllowed){
		if(buildIndex()){
			identityVerified = true;
		}else{
			const bool memoryFailure = allocationFailed;
			clearSongs();
			allocationFailed = false;
			const bool recovered = loadBestIndex();
			identityVerified = identityVerified || recovered;
			if(!recovered && memoryFailure) allocationFailed = true;
			indexState = LibraryIndex::stateAfterRecovery(indexState, recovered);
		}
	}else if(rebuildRequested && !rebuildAllowed){
		indexState = IndexState::Verifying;
	}
	if(allocationFailed){
		Serial.printf(
			"SongList: allocation failed after %u tracks (%u bytes)\n",
			static_cast<unsigned int>(songCount),
			static_cast<unsigned int>(pathBytes)
		);
		clearSongs();
	}

	waiting = false;
	empty = songCount == 0;
	const LibraryIndex::CardIdentity identity = currentCardIdentity();
	const LibraryIndex::IdentityAction identityAction =
		LibraryIndex::updateVerifiedIdentity(
			verifiedIndexIdentity,
			identity,
			identityVerified,
			indexGeneration,
			indexPayloadCrc
		);
	indexGeneration = verifiedIndexIdentity.generation;
	indexPayloadCrc = verifiedIndexIdentity.payloadCrc;
	if(session && identityAction == LibraryIndex::IdentityAction::Refresh){
		session->refreshLibraryMetadata(
			verifiedIndexIdentity.generation,
			libraryKey(
				verifiedIndexIdentity.card,
				verifiedIndexIdentity.generation,
				verifiedIndexIdentity.payloadCrc
			)
		);
	}else if(session && identityAction == LibraryIndex::IdentityAction::Invalidate){
		session->invalidateLibraryMetadata();
	}
	Serial.printf(
		"SongList: indexed %u tracks (%u path bytes)%s\n",
		static_cast<unsigned int>(songCount),
		static_cast<unsigned int>(pathBytes),
		scanLimited ? ", limit reached" : ""
	);
	draw();
	screen.commit();
}

bool SongList::SongList::searchDirectories(File dir, uint8_t depth){
	if(!dir) return true;

	File f;
	while(f = dir.openNextFile()){
		const char* rawPath = f.name();
		if(rawPath == nullptr || isHiddenEntry(rawPath)){
			f.close();
			continue;
		}
		if(f.isDirectory()){
			if(depth >= maxDirectoryDepth){
				scanLimited = true;
				f.close();
				continue;
			}
			const bool keepScanning = searchDirectories(f, depth + 1);
			f.close();
			if(!keepScanning) return false;
			continue;
		}

		char path[maxPathLength + 1];
		if(!normalizePath(rawPath, path, sizeof(path)) ||
		   !LibraryIndex::isSupportedPath(path, strlen(path))){
			f.close();
			continue;
		}

		const bool keepScanning = addSong(path, f);
		f.close();
		if(!keepScanning) return false;
	}
	return true;
}

bool SongList::SongList::trackMatches(size_t index, File& file){
	if(index >= songCount || songMetadata == nullptr) return false;
	LibraryIndex::FileEvidence evidence{};
	if(!fingerprintFile(file, evidence)) return false;

	LibraryIndex::Record record{};
	const LibraryIndex::FileEvidence& expected = songMetadata[index];
	record.flags = expected.flags;
	record.firstBytes = expected.firstBytes;
	record.lastBytes = expected.lastBytes;
	record.fileSize = expected.fileSize;
	record.fatMtime = expected.fatMtime;
	record.firstCrc32 = expected.firstCrc32;
	record.lastCrc32 = expected.lastCrc32;
	return LibraryIndex::matchesFile(record, evidence);
}

SongList::SongList::IndexInfo SongList::SongList::getIndexInfo() const{
	return {
		indexState,
		indexGeneration,
		songCount,
		identityStrength,
		indexProgress,
		indexProgressTotal
	};
}

void SongList::SongList::loop(uint t){
	(void) t;
	if(browseMode) updateBrowseResult();
}

void SongList::SongList::start(){

	InputJayD::getInstance()->setEncoderMovedCallback(ENC_MID, [](int8_t value){
		if(instance == nullptr) return;

		if(instance->empty || !instance->insertedSD) return;

		instance->selectedElement += value;
		if(instance->selectedElement < 0){
			instance->selectedElement = 0;
		}else if(instance->selectedElement >= instance->songCount){
			instance->selectedElement = instance->songCount - 1;
		}

		if(instance->selectedElement < instance->firstVisible){
			instance->firstVisible = instance->selectedElement;
		}else if(instance->selectedElement >= instance->firstVisible + visibleRows){
			instance->firstVisible = instance->selectedElement - visibleRows + 1;
		}
		instance->draw();
		instance->screen.commit();


	});

	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;

		if(instance->browseMode){
			instance->browseStatus = "RESCANNING SD...";
			instance->checkSD();
			instance->browseStatus = instance->insertedSD ? "SD RESCANNED" : "SD NOT AVAILABLE";
			instance->draw();
			instance->screen.commit();
			return;
		}

		if(!instance->insertedSD){
			instance->checkSD();
			return;
		}

		if(instance->empty || !instance->insertedSD || instance->songCount <= instance->selectedElement) return;

		const char* selectedPath = instance->songPath(instance->selectedElement);
		if(selectedPath == nullptr) return;
		String path(selectedPath);
		fs::File file = SD.open(path);
		if(!file){
			file.close();
			File root = SD.open("/");
			if(!root){
				root.close();
				SD.end();
				instance->insertedSD = false;
				instance->checkSD();
			}else{
				root.close();
				instance->indexState = IndexState::Stale;
				instance->draw();
				instance->screen.commit();
			}
			return;
		}
		if(!instance->trackMatches(instance->selectedElement, file)){
			file.close();
			instance->indexState = IndexState::Stale;
			instance->draw();
			instance->screen.commit();
			return;
		}
		file.close();

		instance->pop(new String(path));
	});

	if(browseMode){
		// Browse mode is a modal picker: holding the center button cancels
		// without loading anything (mirrors the normal picker's pop-to-cancel,
		// since a single press here rescans rather than pops).
		InputJayD::getInstance()->setBtnHeldCallback(BTN_MID, holdTime, [](){
			if(instance == nullptr) return;
			instance->pop();
		});
	}

	Input.addListener(this);
	waiting = false;
	checkSD();

	draw();
	screen.commit();
}

void SongList::SongList::stop(){
	InputJayD::getInstance()->removeBtnHeldCallback(BTN_MID);
	Input.removeListener(this);
}

void SongList::SongList::draw(){

	Sprite* canvas = screen.getSprite();

	canvas->setFont(&u8g2_font_DigitalDisco_tf);
	canvas->setTextColor(TFT_WHITE);

	if(backgroundBuffer == nullptr){
		canvas->clear(TFT_BLACK);
	}else{
		canvas->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);
	}

	if(backgroundBuffer != nullptr){
		canvas->drawIcon(backgroundBuffer, 0, 0, 160, 19, 1);
	}

	canvas->setTextDatum(BC_DATUM);
	canvas->drawString(browseMode ? "BROWSE  CENTER: RESCAN" : stateLabel(), screen.getWidth()/2, 15);

	if(waiting){
		canvas->drawString("Loading...", screen.getWidth()/2, 65);
		canvas->setTextDatum(TL_DATUM);
		return;
	}

	if(!insertedSD){
		canvas->drawString("Not inserted!", screen.getWidth()/2, 65);
		canvas->setTextDatum(TL_DATUM);
		return;
	}else if(empty){
		if(indexState == IndexState::Building){
			canvas->drawString("Building...", screen.getWidth()/2, 65);
		}else if(indexState == IndexState::Error){
			canvas->drawString(allocationFailed ? "Memory error!" : "Index error!", screen.getWidth()/2, 65);
		}else{
			canvas->drawString("Empty!", screen.getWidth()/2, 65);
		}
		canvas->setTextDatum(TL_DATUM);
		return;
	}
	canvas->setFont(&u8g2_font_profont12_tf);
	canvas->setTextDatum(CL_DATUM);
	for(uint8_t row = 0; row < visibleRows; row++){
		const size_t index = firstVisible + row;
		const char* path = songPath(index);
		if(path == nullptr) break;

		const int y = 23 + row * rowHeight;
		if(index == selectedElement){
			canvas->drawRect(3, y, 154, rowHeight - 2, TFT_LIGHTGREY);
		}

		const char* filename = strrchr(path, '/');
		filename = filename == nullptr ? path : filename + 1;
		size_t length = strlen(filename);
		if(length >= 4) length -= 4;

		char label[maxPathLength + 4];
		memcpy(label, filename, length);
		label[length] = '\0';
		if(canvas->textWidth(label) > 142){
			size_t low = 0;
			size_t high = length;
			while(low < high){
				const size_t middle = (low + high + 1) / 2;
				memcpy(label, filename, middle);
				memcpy(label + middle, "...", 4);
				if(canvas->textWidth(label) <= 142){
					low = middle;
				}else{
					high = middle - 1;
				}
			}
			memcpy(label, filename, low);
			memcpy(label + low, "...", 4);
		}
		canvas->drawString(label, 8, y + rowHeight / 2);
	}
	canvas->setTextDatum(TL_DATUM);

	if(browseMode){
		canvas->fillRect(0, 111, 160, 17, TFT_BLACK);
		canvas->setTextColor(TFT_WHITE);
		canvas->setTextDatum(BC_DATUM);
		const String footer = browseStatus.length() ? browseStatus : "A: LOAD A       B: LOAD B";
		canvas->drawString(footer.substring(0, 26), 80, 124);
		canvas->setTextDatum(TL_DATUM);
	}
}


void SongList::SongList::pack(){
	Context::pack();
	free(backgroundBuffer);
	backgroundBuffer = nullptr;
}

void SongList::SongList::unpack(){
	Context::unpack();

	waiting = true;

	backgroundBuffer = static_cast<Color*>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("SongList bg buffer error");
		return;
	}

	fs::File bgFile = CompressedFile::open(SPIFFS.open("/SongListBackground.raw.hs"), 10, 9);
	bgFile.read(reinterpret_cast<uint8_t*>(backgroundBuffer), 160 * 128 * 2);
	bgFile.close();
}

void SongList::SongList::encTwoTop(){
	delete parent;
	stop();
	delete this;
	MainMenu::MainMenu::getInstance()->unpack();
	MainMenu::MainMenu::getInstance()->start();
}

void SongList::SongList::encTwoBot(){
	checkSD(true);
}

const char* SongList::SongList::stateLabel() const{
	switch(indexState){
		case IndexState::Absent: return "SD loading";
		case IndexState::Building: return "SD building";
		case IndexState::Verifying: return "SD verify";
		case IndexState::Ready: return scanLimited ? "SD LIMIT" : "SD ready";
		case IndexState::Stale: return "SD stale";
		case IndexState::Error: return "SD error";
	}
	return "SD card";
}

void SongList::SongList::btn(uint8_t i){
	const int8_t deck = MixControlState::browseDeckForButton(i);
	if(!browseMode || deck < 0) return;
	loadSelected(deck);
}

bool SongList::SongList::allowsEncoderChords() const{
	return !browseMode;
}

bool SongList::SongList::selectedPath(String& path){
	if(empty || !insertedSD || songCount <= size_t(selectedElement)) return false;
	const char* selected = songPath(selectedElement);
	if(selected == nullptr) return false;
	path = selected;
	fs::File file = SD.open(path);
	if(file){
		file.close();
		return true;
	}
	file.close();
	SD.end();
	insertedSD = false;
	checkSD();
	return false;
}

void SongList::SongList::loadSelected(uint8_t deck){
	if(!browseSession || deck >= DJ_DECK_COUNT) return;
	String path;
	if(!selectedPath(path)) return;
	const DjSubmitResult result = browseSession->loadDeck(deck, path.c_str(), DJ_ORIGIN_PHYSICAL);
	if(!result.accepted()){
		browseStatus = "LOAD REJECTED";
		draw();
		screen.commit();
		return;
	}
	pendingLoad[deck] = result.id;
	browseStatus = deck == 0 ? "LOADING DECK A..." : "LOADING DECK B...";
	draw();
	screen.commit();
}

void SongList::SongList::updateBrowseResult(){
	if(!browseMode || (!pendingLoad[0] && !pendingLoad[1])) return;
	DjSnapshot snapshot;
	if(!browseSession->copySnapshot(snapshot)) return;
	for(const auto& result : snapshot.recentResults){
		for(uint8_t deck = 0; deck < DJ_DECK_COUNT; deck++){
			if(!pendingLoad[deck] || result.id != pendingLoad[deck] ||
			   result.status == DJ_COMMAND_ACCEPTED) continue;
			pendingLoad[deck] = 0;
			if(result.status == DJ_COMMAND_APPLIED){
				browseStatus = deck == 0 ? "LOADED DECK A" : "LOADED DECK B";
			}else{
				browseStatus = "LOAD FAILED";
			}
			draw();
			screen.commit();
		}
	}
}
