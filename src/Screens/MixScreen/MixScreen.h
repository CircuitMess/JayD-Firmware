#ifndef JAYD_FIRMWARE_MIXSCREEN_H
#define JAYD_FIRMWARE_MIXSCREEN_H

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include "SongSeekBar.h"
#include "SongName.hpp"
#include "Effects.hpp"

namespace MixScreen {
	class MixScreen : public Context {
	public:

		MixScreen(Display &display);

		void start();

		void stop();

		void draw();

	private:
		static MixScreen *instance;

		LinearLayout screenLayout;
		LinearLayout leftLayout;
		LinearLayout rightLayout;

		SongSeekBar leftSeekBar;
		SongSeekBar rightSeekBar;

		SongName leftSongName;
		SongName rightSongName;

		std::vector<Effects *> effectsLeft;
		std::vector<Effects *> effectsRight;

		bool isPlaying=false;

		void buildUI();
	};
}


#endif //JAYD_FIRMWARE_MIXSCREEN_H
