#ifndef JAYD_FIRMWARE_SETTINGSSCREEN_H
#define JAYD_FIRMWARE_SETTINGSSCREEN_H

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include "SettingsElement.h"
#include "SliderElement.h"
#include <FS.h>


namespace SettingsScreen {
	class SettingsScreen : public Context {
	public:

		SettingsScreen(Display &display);

		void start();

		void stop();

		void draw();

		void pack() override;

		void unpack() override;

		virtual ~SettingsScreen();

	private:
		static SettingsScreen *instance;

		LinearLayout screenLayout;

		SliderElement volumeSlider;
		SliderElement brightnessSlider;

		int selectedSetting = 0;

		bool disableMainSelector = false;

		void buildUI();


		fs::File background;

		Color* backgroundBuffer= nullptr;

	};
}


#endif //JAYD_FIRMWARE_SETTINGSSCREEN_H
