#ifndef JAYD_FIRMWARE_LISTITEM_H
#define JAYD_FIRMWARE_LISTITEM_H

#include <UI/CustomElement.h>

namespace SongList {
	class ListItem : public CustomElement {
	public:
		ListItem(ElementContainer *parent,String songName);

		void draw();

		void setSelected(bool selected);

		const String& getName() const;

		bool checkScrollUpdate();

	private:
		bool selected = false;
		String songName;

		bool scrolling = false;
		const int32_t scrollSpeed = 100; //in milliseconds
		uint32_t currentTime = 0;
		int32_t nameLength;
		int32_t scrollCursor = 0;
		uint8_t scrollOffset = 30;
	};
}
#endif //JAYD_FIRMWARE_LISTITEM_H
