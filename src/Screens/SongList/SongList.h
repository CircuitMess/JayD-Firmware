#ifndef JAYD_FIRMWARE_SONGLIST_H
#define JAYD_FIRMWARE_SONGLIST_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/Image.h>
#include <Input/InputJayD.h>
#include "../../InputKeys.h"
#include "LibraryIndex.h"

class DjSession;

namespace SongList {
	class SongList : public Context, public LoopListener, public InputListener {
	public:
		using IndexState = LibraryIndex::State;

		struct IndexInfo {
			IndexState state;
			uint32_t generation;
			size_t count;
			LibraryIndex::IdentityStrength identityStrength;
			uint32_t progress;
			uint32_t progressTotal;
		};

		explicit SongList(Display &display, DjSession* browseSession = nullptr);

		virtual ~SongList() override;

		void start() override;

		void stop() override;

		void draw() override;

		void loop(uint t) override;

		void pack() override;

		void unpack() override;

		IndexInfo getIndexInfo() const;
	private:
		static SongList *instance;

		int selectedElement = 0;
		int firstVisible = 0;

		Color *backgroundBuffer = nullptr;
		char* pathBuffer = nullptr;
		uint32_t* songOffsets = nullptr;
		LibraryIndex::FileEvidence* songMetadata = nullptr;
		size_t pathBytes = 0;
		size_t pathCapacity = 0;
		size_t songCount = 0;
		size_t songCapacity = 0;

		void clearSongs();
		void checkSD(bool forceRebuild = false);
		bool loadBestIndex();
		bool loadIndex(const char* path);
		bool buildIndex();
		bool writeGeneration(const char* path, uint32_t generation);
		bool searchDirectories(File dir, uint8_t depth);
		bool addSong(const char* path, File& file);
		bool fingerprintFile(File& file, LibraryIndex::FileEvidence& evidence);
		bool trackMatches(size_t index, File& file);
		bool reservePaths(size_t required);
		bool reserveSongs(size_t required);
		const char* songPath(size_t index) const;
		LibraryIndex::CardIdentity currentCardIdentity() const;
		const char* stateLabel() const;

		void encTwoTop() override;
		void encTwoBot() override;
		void btn(uint8_t i) override;
		bool allowsEncoderChords() const override;
		bool selectedPath(String& path);
		void loadSelected(uint8_t deck);
		void updateBrowseResult();

		DjSession* browseSession = nullptr;
		uint32_t pendingLoad[2] = {};
		String browseStatus;
		bool browseMode = false;
		bool waiting = false;
		bool insertedSD = true;
		bool empty = true;
		bool scanLimited = false;
		bool scanStoppedAtLimit = false;
		bool allocationFailed = false;
		IndexState indexState = IndexState::Absent;
		uint32_t indexGeneration = 0;
		uint32_t indexPayloadCrc = 0;
		uint32_t indexProgress = 0;
		uint32_t indexProgressTotal = 0;
		LibraryIndex::IdentityStrength identityStrength =
			LibraryIndex::IdentityStrength::Unknown;
		LibraryIndex::VerifiedIdentity verifiedIndexIdentity = {};

		static const size_t maxTrackCount = 4096;
		static const size_t maxPathLength = 255;
		static const size_t maxPathPayload = 128 * 1024;
		static const size_t maxIndexPayload =
			maxTrackCount * sizeof(LibraryIndex::Record) + maxPathPayload;
		static const uint8_t maxDirectoryDepth = 12;
		static const uint8_t visibleRows = 5;
		static const uint8_t rowHeight = 20;
		static const uint32_t holdTime = 500;
	};
}
#endif //JAYD_FIRMWARE_SONGLIST_H
