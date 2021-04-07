#ifndef JAYD_FIRMWARE_BOOLEANELEMENT_H
#define JAYD_FIRMWARE_BOOLEANELEMENT_H

#include "SettingsElement.h"

namespace SettingsScreen {
	class BooleanElement : public SettingsElement {
	public:
		BooleanElement(ElementContainer *partent, String name);

		void activate() override;

	private:

		bool booleanSwitch = false;

	protected:
		void drawControl() override;
	};
}

#endif //JAYD_FIRMWARE_BOOLEANELEMENT_H
