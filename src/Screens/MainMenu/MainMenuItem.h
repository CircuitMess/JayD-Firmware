#ifndef JAYD_FIRMWARE_MAINMENUITEM_H
#define JAYD_FIRMWARE_MAINMENUITEM_H

#include <UI/CustomElement.h>
#include <Display/AnimatedSprite.h>

enum MenuItemType {
	DJ, PLAYBACK, SETTINGS
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

		AnimatedSprite *gif = nullptr;

		Color *buffer = nullptr;

		uint32_t startMicros = 0;

		static const char* const gifIcons[3];
		static const char* const icons[3];
		static const char* const texts[3];

	};
}
#endif //JAYD_FIRMWARE_MAINMENUITEM_H
