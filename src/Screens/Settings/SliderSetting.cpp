#include "SliderSetting.h"

Settings::SliderSetting::SliderSetting(ElementContainer *partent, String name) : SettingsElement(partent, name){

}

void Settings::SliderSetting::moveSliderValue(int8_t value){
	sliderValue=constrain(sliderValue + value,0,255);
}

void Settings::SliderSetting::activate(){
	sliderIsSelected = !sliderIsSelected;
}

void Settings::SliderSetting::drawControl(){
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



