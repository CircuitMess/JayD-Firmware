#ifndef JAYD_FIRMWARE_SONGSEEKBAR_H
#define JAYD_FIRMWARE_SONGSEEKBAR_H

#include <UI/CustomElement.h>

namespace MixScreen {
	class SongSeekBar : public CustomElement {
	public:
		SongSeekBar(ElementContainer *parent);

		virtual ~SongSeekBar();

		void setPlaying(bool playing);
		bool isPlaying() const;

		void draw();

		void setTotalDuration(int totalDuration);
		void setCurrentDuration(int currentDuration);

		int getCurrentDuration() const;
		int getTotalDuration() const;

	private:
		bool playing = false;
		int totalDuration = 0;
		int currentDuration = 0;
		float movingCursor=0;

		Color *buffer[2] = { nullptr };
	};

}
#endif //JAYD_FIRMWARE_SONGSEEKBAR_H
