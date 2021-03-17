#ifndef JAYD_FIRMWARE_MIXSCREEN_H
#define JAYD_FIRMWARE_MIXSCREEN_H

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include "SongSeekBar.h"
#include "SongName.hpp"
#include "EffectElement.hpp"

namespace MixScreen {
	class MixScreen : public Context, public LoopListener{
	public:

		MixScreen(Display &display);

		void start();

		void stop();

		void draw();

		void loop(uint micros) override;

		virtual ~MixScreen();

	private:
		static MixScreen *instance;

		LinearLayout screenLayout;
		LinearLayout leftLayout;
		LinearLayout rightLayout;

		SongSeekBar leftSeekBar;
		SongSeekBar rightSeekBar;

		SongName leftSongName;
		SongName rightSongName;

		std::vector<EffectElement *> effectsLeft;
		std::vector<EffectElement *> effectsRight;

		bool isPlaying = false;

		bool isSelected[6] ;

		int8_t effects[6];

		int intensity[6];

		void buildUI();


	};
}


#endif //JAYD_FIRMWARE_MIXSCREEN_H
