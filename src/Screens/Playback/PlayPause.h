#ifndef JAYD_FIRMWARE_PLAYPAUSE_H
#define JAYD_FIRMWARE_PLAYPAUSE_H

#include <UI/CustomElement.h>
#include <Loop/LoopListener.h>

namespace Playback {
	class PlayPause : public CustomElement {
	public:
		PlayPause(ElementContainer *parent);

		void togglePlaying();

		virtual ~PlayPause();

		void draw();

		void setPlaying(bool playing);


	private:
		bool playing = false;

		Color *buffer[4];
	};
}

#endif //JAYD_FIRMWARE_PLAYPAUSE_H
