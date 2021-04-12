#ifndef JAYD_FIRMWARE_MAINMENU_H
#define JAYD_FIRMWARE_MAINMENU_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/LinearLayout.h>
#include <UI/ScrollLayout.h>
#include "MainMenuItem.h"
#include <Display/AnimatedSprite.h>

struct MatrixPartitionGIFData {
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

		int8_t itemNum = 1;

		fs::File backgroundPicture;
		fs::File jayDlogo;

		Color *backgroundBuffer = nullptr;
		Color *logoBuffer = nullptr;

		void buildUI();

		uint32_t jumpTime;

		MatrixPartitionGIFData gifData[4];
		static const uint totalAnimations = 20;

		void startRandomAnimation(uint8_t index);

	};
}
#endif //JAYD_FIRMWARE_MAINMENU_H
