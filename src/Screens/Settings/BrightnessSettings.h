#ifndef JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP
#define JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP

#include "SettingsElement.h"
#include "DropDownSetting.h"

namespace Settings {
	class BrightnessSetting : public SettingsElement {
	public:
		BrightnessSetting(ElementContainer *partent, String name);

		void activate() override;

		void moveBightnessValue(int8_t value);

	private:

		bool sliderIsSelected = false;

		uint8_t sliderValue = 0;

	protected:
		void drawControl() override;
	};
}

#endif //JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP
