#ifndef JAYD_FIRMWARE_MAINMENUITEM_H
#define JAYD_FIRMWARE_MAINMENUITEM_H

#include <UI/CustomElement.h>
#include <Display/AnimatedSprite.h>

enum MenuItemType {
	/* PLAYBACK,*/ DJ, SETTINGS
};

namespace MainMenu {
	class MainMenuItem : public CustomElement {
	public:

		MainMenuItem(ElementContainer *parent, MenuItemType type);

		virtual ~MainMenuItem();

		void draw();

		void isSelected(bool selected);

		bool needsUpdate();


	private:

		MenuItemType type;

		bool selected = false;

		AnimatedSprite* gif= nullptr;

	//	String gifIcons[MenuItemType::PLAYBACK];
	};
}
#endif //JAYD_FIRMWARE_MAINMENUITEM_H
