#ifndef JAYD_FIRMWARE_LISTITEM_H
#define JAYD_FIRMWARE_LISTITEM_H

#include <UI/CustomElement.h>

class ListItem : public CustomElement {
public:
	ListItem(ElementContainer *parent);

	void draw();

	void setSelected(bool selected);

private:
	bool selected=false;
};

#endif //JAYD_FIRMWARE_LISTITEM_H