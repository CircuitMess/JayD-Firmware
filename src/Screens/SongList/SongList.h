#ifndef JAYD_FIRMWARE_SONGLIST_H
#define JAYD_FIRMWARE_SONGLIST_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/Image.h>
#include <UI/LinearLayout.h>
#include <UI/ScrollLayout.h>
#include "ListItem.h"
#include <Input/InputJayD.h>

namespace SongList {
	class SongList : public Context, public LoopListener {
	public:

		explicit SongList(Display &display);

		virtual ~SongList() override;

		void start() override;

		void stop() override;

		void draw() override;

		void loop(uint t) override;

		void pack() override;

		void unpack() override;

	private:
		static SongList *instance;
		ScrollLayout* scrollLayout;
		LinearLayout* list;

		int selectedElement = 0;

		fs::File background;
		Color *backgroundBuffer = nullptr;

		std::vector<ListItem *> songs;
		std::vector<Element*> listTemp;

		void buildUI();

		void populateList();
		bool insertedSD = false;

		uint32_t prevSDCheck = 0;

		static const uint16_t sdCardInterval = 500;
	};
}
#endif //JAYD_FIRMWARE_SONGLIST_H
