#ifndef JAYD_FIRMWARE_SONGLIST_H
#define JAYD_FIRMWARE_SONGLIST_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/Image.h>
#include <UI/LinearLayout.h>
#include <UI/ScrollLayout.h>
#include "ListItem.h"

namespace Songlist {
	class SongList : public Context {
	public:

		SongList(Display &display);

		void start();

		void stop();

		void draw();

	private:

		static SongList *instance;

		LinearLayout screenLayout;
		ScrollLayout scrollLayout;
		LinearLayout list;

		std::vector<ListItem *> song;

		void buildUI();
	};
}
#endif //JAYD_FIRMWARE_SONGLIST_H
