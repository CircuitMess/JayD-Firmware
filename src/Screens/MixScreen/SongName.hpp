#ifndef JAYD_FIRMWARE_SONGNAME_HPP
#define JAYD_FIRMWARE_SONGNAME_HPP

#include <UI/CustomElement.h>

namespace MixScreen {
	class SongName : public CustomElement {
	public:
		SongName(ElementContainer *parent, String songName);

		void draw();
		bool checkScrollUpdate();

	private:
		String songName;
		bool scrolling = false;
		const int32_t scrollSpeed = 100; //in milliseconds
		uint32_t currentTime = 0;
		int32_t nameLength;
		int32_t scrollCursor = 0;
	};

}


#endif //JAYD_FIRMWARE_SONGNAME_HPP