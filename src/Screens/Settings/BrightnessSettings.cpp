#include "BrightnessSettings.h"

Settings::BrightnessSetting::BrightnessSetting(ElementContainer *partent, String name) : SettingsElement(partent, name){

}

void Settings::BrightnessSetting::setBightnessValue(uint8_t value){
	brightnessValue = brightnessValue + value;
	if(brightnessValue > 100){
		brightnessValue = 100;
	}else if(brightnessValue < 0){
		brightnessValue = 0;
	}
}

void Settings::BrightnessSetting::activate(){
	showBrightnessValueBar = !showBrightnessValueBar;
}

void Settings::BrightnessSetting::drawControl(){
		float movingCursor;
		if(brightnessValue == 0){
			movingCursor = 0;
		}else{
			movingCursor = ((float) brightnessValue / 100) * 51.0f;
		}
		getSprite()->drawRect(getTotalX() + 100, getTotalY() + 10, 2, 5, TFT_WHITE);
		getSprite()->drawRect(getTotalX() + 153, getTotalY() + 10, 2, 5, TFT_WHITE);
		getSprite()->drawRect(getTotalX() + 100, getTotalY() + 12, 55, 1, TFT_WHITE);
		if(showBrightnessValueBar){
			getSprite()->drawRect(getTotalX() + 101 + movingCursor, getTotalY() + 9, 2, 6, TFT_BLUE);
		}else{
			getSprite()->drawRect(getTotalX() + 101 + movingCursor, getTotalY() + 10, 2, 5, TFT_WHITE);

		}

}



