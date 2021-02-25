#ifndef JAYD_FIRMWARE_MAINMENUITEM_H
#define JAYD_FIRMWARE_MAINMENUITEM_H

#include <UI/CustomElement.h>

enum Items {
	PLAYBACK,DJ,SETTINGS
};
namespace MainMenu {
	class MainMenuItem : public CustomElement {
	public:
		MainMenuItem(ElementContainer *parent);

		void draw();

		void isSelected(bool selected);

		void setItems(Items items);

	private:
		Items items;
		bool selected = false;
	};
}
#endif //JAYD_FIRMWARE_MAINMENUITEM_H
