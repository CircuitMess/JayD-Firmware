#ifndef JAYD_FIRMWARE_SONGLIST_H
#define JAYD_FIRMWARE_SONGLIST_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/Image.h>
#include <UI/LinearLayout.h>
#include <UI/ScrollLayout.h>
#include "ListItem.h"

namespace SongList {
	class SongList : public Context {
	public:

		SongList(Display &display);

		virtual ~SongList();

		void start();

		void stop();

		void draw();

	private:

		static SongList *instance;
		ScrollLayout scrollLayout;
		LinearLayout list;

		int selectedElement = 0;

		std::vector<ListItem *> songs;

		void buildUI();
	};
}
#endif //JAYD_FIRMWARE_SONGLIST_H
