#ifndef JAYD_FIRMWARE_MAINMENU_H
#define JAYD_FIRMWARE_MAINMENU_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/LinearLayout.h>
#include <UI/ScrollLayout.h>
#include "MainMenuItem.h"
#include <Display/AnimatedSprite.h>

namespace MainMenu {
	class MainMenu : public Context, public LoopListener {
	public:

		MainMenu(Display &display);

		virtual ~MainMenu();

		void start();

		void stop();

		void draw();

		void loop(uint micros) override;

		void pack() override;

		void unpack() override;

		static MainMenu* getInstance();
	private:

		static MainMenu *instance;

		LinearLayout *screenLayout;

		std::vector<MainMenuItem *> items;

		int8_t itemNum = 1;

		Color *backgroundBuffer = nullptr;
		Color *logoBuffer = nullptr;

		void buildUI();

		uint32_t jumpTime;

	};
}
#endif //JAYD_FIRMWARE_MAINMENU_H
