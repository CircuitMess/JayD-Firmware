#ifndef JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP
#define JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP

#include "SettingsElement.h"
#include "DropDownSetting.h"

namespace Settings {
	class BrightnessSetting : public SettingsElement {
	public:
		BrightnessSetting(ElementContainer *partent, String name);

		void activate() override;

		void setBightnessValue(uint8_t value);

	private:
		DropDownSetting *dropDownSetting;

		bool showBrightnessValueBar = false;

		int8_t value;

		int8_t brightnessValue = 0;

	protected:
		void drawControl() override;
	};
}

#endif //JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP
