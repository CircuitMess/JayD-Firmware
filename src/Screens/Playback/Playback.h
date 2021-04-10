#ifndef JAYD_FIRMWARE_PLAYBACK_H
#define JAYD_FIRMWARE_PLAYBACK_H

#include <Support/Context.h>
#include <UI/Image.h>
#include <UI/LinearLayout.h>
#include <UI/TextElement.h>
#include "SongName.h"
#include "PlayPause.h"
#include "TrackCounter.h"
#include "../SongList/SongList.h"
#include <FS.h>
#include <AudioLib/OutputI2S.h>
#include <AudioLib/SourceWAV.h>
#include <AudioLib/Systems/PlaybackSystem.h>

namespace Playback {
	class Playback : public Context, public LoopListener {
	public:

		Playback(Display &display);

		virtual ~Playback();

		void start();

		void stop();

		void draw();

		void pack() override;

		void unpack() override;

		void returned(void *data) override;

		void loop(uint micros) override;

	private:
		static Playback *instance;
		SongList::SongList* selector = nullptr;

		LinearLayout *screenLayout;
		LinearLayout *songNameLayout;
		LinearLayout *timeElapsedLayout;
		LinearLayout *buttonLayout;
		SongName *songName;
		PlayPause *playOrPause;
		TrackCounter *trackCount;

		void buildUI();

		fs::File file;
		PlaybackSystem* system = nullptr;
		bool playing = false;

		fs::File background;
		Color *backgroundBuffer = nullptr;

		uint32_t lastDraw = 0;
		bool drawQueued = false;
	};
}

#endif //JAYD_FIRMWARE_PLAYBACK_H
