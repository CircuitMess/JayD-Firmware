#ifndef JAYD_FIRMWARE_SLIDERSETTINGS_H
#define JAYD_FIRMWARE_SLIDERSETTINGS_H

#include "SettingsElement.h"


namespace SettingsScreen {
	class SliderElement : public SettingsElement {
	public:
		SliderElement(ElementContainer *partent, String name);

		void toggle() override;

		void moveSliderValue(int8_t value);

		void setSliderValue(uint8_t sliderValue);

		uint8_t getSliderValue() const;

	private:

		bool sliderIsSelected = false;

		uint8_t sliderValue=0 ;

	protected:
		void drawControl() override;
	};
}

#endif
