#include "BrightnessSettings.h"

Settings::BrightnessSetting::BrightnessSetting(ElementContainer *partent, String name): SettingsElement(partent, name){

}

void Settings::BrightnessSetting::setBightnessValue(uint8_t value){
	brightnessValue=brightnessValue+value;
	if(brightnessValue>100){
		brightnessValue=100;
	}else if(brightnessValue<0){
		brightnessValue=0;
	}
}

void Settings::BrightnessSetting::activated(){
	showBrightnessValueBar = !showBrightnessValueBar;
}

void Settings::BrightnessSetting::drawControl(){
	float movingCursor;
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setCursor(getTotalX() + 122, getTotalY() + 8);
	getSprite()->println(brightnessValue);
	if(showBrightnessValueBar){
		if(brightnessValue==0){
			movingCursor=1;
		}else{
			movingCursor=((float)brightnessValue/100)*54.0f;
		}
		getSprite()->drawRect(getTotalX()+100,getTotalY()+20,2,5,TFT_WHITE);
		getSprite()->drawRect(getTotalX()+155,getTotalY()+20,2,5,TFT_WHITE);
		getSprite()->drawRect(getTotalX()+100,getTotalY()+22,55,1,TFT_WHITE);
		getSprite()->drawRect(getTotalX()+100+movingCursor,getTotalY()+20,2,5,TFT_WHITE);
	}
}


