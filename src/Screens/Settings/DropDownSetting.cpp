#include "DropDownSetting.h"

Settings::DropDownSetting::DropDownSetting(ElementContainer *partent, String name, std::vector<String> dropDownContent)
		: SettingsElement(partent, name), dropDownContent(dropDownContent){


}

void Settings::DropDownSetting::activated(){
	showDropDown=!showDropDown;
}

void Settings::DropDownSetting::selectNext(){
	index = index + 1;
	if(index > dropDownContent.size() - 1){
		index = dropDownContent.size() - 1;
	}
}

void Settings::DropDownSetting::selectPrev(){
	index = index - 1;
	if(index < 0){
		index = 0;
	}
}

void Settings::DropDownSetting::drawControl(){
	getSprite()->setTextColor(TFT_BLACK);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);
	getSprite()->setCursor(getTotalX() + 115, getTotalY() + 8);
	if(!showDropDown){
		getSprite()->fillRoundRect(getTotalX() + 100, getTotalY() + 6, 55, 11, 2, TFT_DARKGREY);
		getSprite()->fillTriangle(getTotalX() + 141, getTotalY() + 13, getTotalX() + 145, getTotalY() + 9,
								  getTotalX() + 149, getTotalY() + 13, TFT_WHITE);
		getSprite()->drawTriangle(getTotalX() + 141, getTotalY() + 13, getTotalX() + 145, getTotalY() + 9,
								  getTotalX() + 149, getTotalY() + 13, TFT_BLACK);

	}else if(showDropDown){
		getSprite()->fillRoundRect(getTotalX() + 100, getTotalY() + 6, 55, 11, 2, TFT_DARKGREY);
		getSprite()->fillTriangle(getTotalX() + 141, getTotalY() + 9, getTotalX() + 145, getTotalY() + 13,
								  getTotalX() + 149, getTotalY() + 9, TFT_WHITE);
		getSprite()->drawTriangle(getTotalX() + 141, getTotalY() + 9, getTotalX() + 145, getTotalY() + 13,
								  getTotalX() + 149, getTotalY() + 9, TFT_BLACK);
		for(int i = 0; i < dropDownContent.size(); i++){
			getSprite()->drawRect(getTotalX() + 100, getTotalY() + 17 + (10 * i), 55, 11, TFT_WHITE);
			getSprite()->setTextColor(TFT_WHITE);
			getSprite()->setTextSize(1);
			getSprite()->setTextFont(1);
			getSprite()->setCursor(getTotalX() + 105, getTotalY() + 18 + (10 * i));
			getSprite()->printf("%s", dropDownContent[i].c_str());

		}
		getSprite()->fillTriangle(getTotalX() + 93, getTotalY() + 25 + (10 * index), getTotalX() + 97,
								  getTotalY() + 20 + (10 * index),
								  getTotalX() + 93, getTotalY() + 16 + (10 * index), TFT_WHITE);
		getSprite()->drawTriangle(getTotalX() + 93, getTotalY() + 25 + (10 * index), getTotalX() + 97,
								  getTotalY() + 20 + (10 * index),
								  getTotalX() + 93, getTotalY() + 16 + (10 * index), TFT_BLACK);
	}
	getSprite()->setTextColor(TFT_BLACK);
	getSprite()->setCursor(getTotalX() + 105, getTotalY() + 8);
	getSprite()->println(dropDownContent[index]);
	getSprite()->drawRoundRect(getTotalX() + 100, getTotalY() + 6, 55, 11, 2, TFT_BLACK);
}