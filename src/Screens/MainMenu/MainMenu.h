#ifndef JAYD_FIRMWARE_MAINMENU_H
#define JAYD_FIRMWARE_MAINMENU_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/LinearLayout.h>
#include <UI/ScrollLayout.h>
#include "MainMenuItem.h"
#include <Display/AnimatedSprite.h>

struct MatrixPartitionGIFData
{
	std::vector<uint> usedIdleAnimations;
	std::vector<uint> unusedIdleAnimations;
	uint8_t animationLoopCounter = 0;
	bool animationLoopDone = 0;
	int8_t requiredAnimationLoops = 0;
};


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

	private:

		static MainMenu *instance;

		LinearLayout *screenLayout;

		std::vector<MainMenuItem *> item;

		int8_t itemNum = 0;

		fs::File background;

		Color* buffer= nullptr;

		void buildUI();

		MatrixPartitionGIFData gifData[4];
		static const uint totalAnimations = 10;
		void startRandomAnimation(uint8_t index);
		
	};
}
#endif //JAYD_FIRMWARE_MAINMENU_H
