#ifndef JAYD_FIRMWARE_SLIDERSETTINGS_H
#define JAYD_FIRMWARE_SLIDERSETTINGS_H

#include "SettingsElement.h"


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

#endif
