#ifndef JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP
#define JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP

#include "SettingsElement.h"
#include "DropDownSetting.h"

namespace Settings {
	class SliderSetting : public SettingsElement {
	public:
		SliderSetting(ElementContainer *partent, String name);

		void activate() override;

		void moveSliderValue(int8_t value);

	private:

		bool sliderIsSelected = false;

		uint8_t sliderValue = 0;

	protected:
		void drawControl() override;
	};
}

#endif //JAYD_FIRMWARE_BRIGHTNESSSETTINGS_HPP
