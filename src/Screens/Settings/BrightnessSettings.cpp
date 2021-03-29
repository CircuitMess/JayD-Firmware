#include "BrightnessSettings.h"

Settings::BrightnessSetting::BrightnessSetting(ElementContainer *partent, String name) : SettingsElement(partent, name){

}

void Settings::BrightnessSetting::moveBightnessValue(int8_t value){
	sliderValue=constrain(sliderValue + value,0,255);
}

void Settings::BrightnessSetting::activate(){
	sliderIsSelected = !sliderIsSelected;
}

void Settings::BrightnessSetting::drawControl(){
		float movingCursor;
		if(sliderValue == 0){
			movingCursor = 0;
		}else{
			movingCursor = ((float) sliderValue / 255) * 51.0f;
		}
		getSprite()->drawRect(getTotalX() + 100, getTotalY() + 10, 2, 5, TFT_WHITE);
		getSprite()->drawRect(getTotalX() + 153, getTotalY() + 10, 2, 5, TFT_WHITE);
		getSprite()->drawRect(getTotalX() + 100, getTotalY() + 12, 55, 1, TFT_WHITE);
		if(sliderIsSelected){
			getSprite()->drawRect(getTotalX() + 101 + movingCursor, getTotalY() + 9, 2, 6, TFT_BLUE);
		}else{
			getSprite()->drawRect(getTotalX() + 101 + movingCursor, getTotalY() + 10, 2, 5, TFT_WHITE);

		}

}



