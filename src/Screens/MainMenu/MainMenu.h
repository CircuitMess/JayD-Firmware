#ifndef JAYD_FIRMWARE_MAINMENU_H
#define JAYD_FIRMWARE_MAINMENU_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/LinearLayout.h>
#include <UI/ScrollLayout.h>
#include "MainMenuItem.h"


namespace MainMenu {
	class MainMenu : public Context {
	public:

		MainMenu(Display &display);

		void start();

		void stop();

		void draw();

	private:

		static MainMenu *instance;

		LinearLayout screenLayout;

		std::vector<MainMenuItem *> item;

		int8_t itemNum = 0;


		void buildUI();
	};
}
#endif //JAYD_FIRMWARE_MAINMENU_H
