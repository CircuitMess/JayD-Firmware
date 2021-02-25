#ifndef JAYD_FIRMWARE_SONGNAME_H
#define JAYD_FIRMWARE_SONGNAME_H

#include <UI/CustomElement.h>

namespace Playback {
	class SongName : public CustomElement {
	public:
		SongName(ElementContainer *parent);

		void draw();

		void setSongName(const String &songName);

	private:
		String songName;
	};
}
#endif //JAYD_FIRMWARE_SONGNAME_H
