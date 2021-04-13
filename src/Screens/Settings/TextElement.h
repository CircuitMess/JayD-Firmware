#ifndef JAYD_FIRMWARE_TEXTELEMENT_HPP
#define JAYD_FIRMWARE_TEXTELEMENT_HPP

#include "SettingsElement.h"

namespace SettingsScreen {
	class TextElement : public SettingsElement {
	public:
		TextElement(ElementContainer *partent, String name);

		void toggle() override;

	private:


	protected:
		void drawControl() override;
	};
}

#endif //JAYD_FIRMWARE_TEXTELEMENT_HPP
