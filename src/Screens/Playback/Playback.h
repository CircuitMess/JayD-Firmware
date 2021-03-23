#ifndef JAYD_FIRMWARE_PLAYBACK_H
#define JAYD_FIRMWARE_PLAYBACK_H

#include <Support/Context.h>
#include <UI/Image.h>
#include <UI/LinearLayout.h>
#include <UI/TextElement.h>
#include "SongName.h"
#include "PlayPause.h"
#include "TrackCounter.hpp"
#include <FS.h>
#include <AudioLib/OutputI2S.h>
#include <AudioLib/SourceWAV.h>

namespace Playback {
	class Playback : public Context, public LoopListener {
	public:

		Playback(Display &display);

		virtual ~Playback();

		void start();

		void stop();

		void draw();

		void returned(void* data) override;
		void loop(uint micros) override;

	private:
		static Playback *instance;

		LinearLayout* screenLayout;
		LinearLayout* songNameLayout;
		LinearLayout* timeElapsedLayout;
		LinearLayout* buttonLayout;
		SongName* songName;
		PlayPause* playOrPause;
		TrackCounter* trackCount;

		void buildUI();

		fs::File file;

		bool playing = false;
		OutputI2S* i2s = nullptr;
		SourceWAV* wav = nullptr;

	};
}

#endif //JAYD_FIRMWARE_PLAYBACK_H
