#ifndef JAYD_FIRMWARE_SETTINGS_H
#define JAYD_FIRMWARE_SETTINGS_H

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include "SettingsElement.h"
#include "BooleanSetting.h"
#include "DropDownSetting.h"
#include "SliderSetting.h"
#include <FS.h>


namespace Settings {
	class Settings : public Context {
	public:

		Settings(Display &display);

		void start();

		void stop();

		void draw();

		void pack() override;

		void unpack() override;

		virtual ~Settings();

	private:
		static Settings *instance;

		LinearLayout screenLayout;

		BooleanSetting firstElement;
		DropDownSetting secondElement;
		SliderSetting thirdElement;
		//	SettingsElement fourthElement;

		int newValue = 0;

		bool disableMainSelector = false;

		void buildUI();

		bool disableBrightnessBar = false;

		fs::File background;

		Color* buffer;

	};
}


#endif //JAYD_FIRMWARE_SETTINGS_H
