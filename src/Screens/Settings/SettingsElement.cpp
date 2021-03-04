#include "SettingsElement.h"

Settings::SettingsElement::SettingsElement(ElementContainer *partent, String name)
		: CustomElement(partent, 160, 10), name(name){


}

void Settings::SettingsElement::setIsSelected(bool isSelected){
	SettingsElement::selected = isSelected;
}

void Settings::SettingsElement::draw(){
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);
	getSprite()->setCursor(getTotalX() + 5, getTotalY() + 5);
	getSprite()->println(name);
	if(selected){
		getSprite()->drawRect(getTotalX() + 3, getTotalY() + 1, 70, 15, TFT_WHITE);
	}
	drawControl();

}
