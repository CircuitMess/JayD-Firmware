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

	private:
		bool selected = false;
		String songName;
	};
}
#endif //JAYD_FIRMWARE_LISTITEM_H
