#ifndef JAYD_FIRMWARE_SONGSEEKBAR_H
#define JAYD_FIRMWARE_SONGSEEKBAR_H

#include <UI/CustomElement.h>

namespace MixScreen {
	class SongSeekBar : public CustomElement {
	public:
		SongSeekBar(ElementContainer *parent);

		void setPlaying(bool playing);

		void draw();

		void setTotalDuration(int totalDuration);

		void setCurrentDuration(int currentDuration);

	private:
		bool playing = false;
		int totalDuration = 0;
		int currentDuration = 0;
		float movingCursor=0;
	};

}
#endif //JAYD_FIRMWARE_SONGSEEKBAR_H
