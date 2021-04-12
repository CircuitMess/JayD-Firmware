#ifndef JAYD_FIRMWARE_PLAYBACK_SONGNAME_H
#define JAYD_FIRMWARE_PLAYBACK_SONGNAME_H

#include <UI/CustomElement.h>

namespace Playback {
	class SongName : public CustomElement {
	public:
		SongName(ElementContainer *parent);

		void draw();
		bool checkScrollUpdate();

		void setSongName(const String& songName);

	private:
		String songName = "-";
		bool scrolling = false;
		const int32_t scrollSpeed = 100; //in milliseconds
		uint32_t currentTime = 0;
		int32_t nameLength;
		int32_t scrollCursor = 0;
		uint8_t scrollOffset = 30;
	};
}
#endif //JAYD_FIRMWARE_PLAYBACK_SONGNAME_H
