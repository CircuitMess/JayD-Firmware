#ifndef JAYD_FIRMWARE_SONGLIST_H
#define JAYD_FIRMWARE_SONGLIST_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/Image.h>
#include <UI/LinearLayout.h>
#include <UI/ScrollLayout.h>
#include "ListItem.h"
#include <Input/InputJayD.h>
#include "../../InputKeys.h"

namespace SongList {
	class SongList : public Context, public LoopListener, public InputListener {
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

		Color *backgroundBuffer = nullptr;

		std::vector<ListItem *> songs;

		void buildUI();

		void checkSD();

		void searchDirectories(File dir);

		void encFour() override;
		bool waiting = false;
		bool insertedSD = true;
		bool empty = true;

		uint32_t prevSDCheck = 0;

		static const uint16_t checkInterval = 500;
	};
}
#endif //JAYD_FIRMWARE_SONGLIST_H
