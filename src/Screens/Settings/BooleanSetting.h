#ifndef JAYD_FIRMWARE_BOOLEANSETTING_H
#define JAYD_FIRMWARE_BOOLEANSETTING_H

#include "SettingsElement.h"

namespace Settings {
	class BooleanSetting : public SettingsElement {
	public:
		BooleanSetting(ElementContainer *partent, String name);

		void activated() override;

	private:

		bool booleanSwitch=false;

	protected:
		void drawControl() override;
	};
}

#endif //JAYD_FIRMWARE_BOOLEANSETTING_H
