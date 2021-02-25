#ifndef JAYD_FIRMWARE_SONGNAME_HPP
#define JAYD_FIRMWARE_SONGNAME_HPP

#include <UI/CustomElement.h>

namespace MixScreen {
	class SongName : public CustomElement {
	public:
		SongName(ElementContainer *parent, String songName);

		void draw();

	private:
		String songName;
	};

}


#endif //JAYD_FIRMWARE_SONGNAME_HPP
