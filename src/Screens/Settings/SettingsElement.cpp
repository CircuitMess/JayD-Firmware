#include "SettingsElement.h"

SettingsScreen::SettingsElement::SettingsElement(ElementContainer *partent, String name)
		: CustomElement(partent, 160, 20), name(name){


}

void SettingsScreen::SettingsElement::setIsSelected(bool isSelected){
	SettingsElement::selected = isSelected;
}

void SettingsScreen::SettingsElement::draw(){
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);
	getSprite()->setCursor(getTotalX() + 5, getTotalY() + 10);
	getSprite()->println(name);
	if(selected){
		getSprite()->drawRect(getTotalX() + 3, getTotalY() + 6, 70, 15, TFT_WHITE);
	}
	drawControl();

}

bool SettingsScreen::SettingsElement::isSelected() const{
	return selected;
}
