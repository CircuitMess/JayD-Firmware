#include "Settings.h"
#include <Input/InputJayD.h>

Settings::Settings *Settings::Settings::instance = nullptr;

Settings::Settings::Settings(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
												 firstElement(&screenLayout, "Song"),
												 secondElement(&screenLayout, "Color",
															   {"Blue", "Red", "Yellow", "Green", "Orange", "Purple"}),
												 thirdElement(&screenLayout, "Brightness")
/*fourthElement(&screenLayout, "Brightness")*/{

	instance = this;
	buildUI();
	firstElement.setIsSelected(true);
	newValue=0;

}

void Settings::Settings::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->disableMainSelector && instance->newValue == 1){
			if(value > 0){
				instance->secondElement.selectNext();
			}else if(value < 0){
				instance->secondElement.selectPrev();
			}
			instance->draw();
			instance->screen.commit();
			return;
		}else if(instance->disableMainSelector && instance->newValue == 2){
			instance->thirdElement.setBightnessValue(value);
			instance->draw();
			instance->screen.commit();
			return;
		}
		instance->newValue = instance->newValue + value;
		if(instance->newValue < 0){
			instance->newValue = 2;
		}else if(instance->newValue > 2){
			instance->newValue = 0;
		}
		if(instance->newValue == 0){
			instance->firstElement.setIsSelected(true);
		}else{
			instance->firstElement.setIsSelected(false);
		}
		if(instance->newValue == 1){
			instance->secondElement.setIsSelected(true);
		}else{
			instance->secondElement.setIsSelected(false);
		}
		if(instance->newValue == 2){
			instance->thirdElement.setIsSelected(true);

		}else{
			instance->thirdElement.setIsSelected(false);
		}
		instance->draw();
		instance->screen.commit();
/*
		if(instance->newValue == 3){
			instance->fourthElement.setIsSelected(true);
		}else{
			instance->fourthElement.setIsSelected(false);

		}*/

	});
	InputJayD::getInstance()->setBtnPressCallback(2, [](){
		if(instance == nullptr) return;
		if(instance->newValue == 0){
			instance->firstElement.activate();
			instance->draw();
			instance->screen.commit();
		}else if(instance->newValue == 1){
			instance->secondElement.activate();
			instance->disableMainSelector = !instance->disableMainSelector;
			instance->draw();
			instance->screen.commit();
		}else if(instance->newValue == 2){
			instance->thirdElement.activate();
			instance->disableMainSelector = !instance->disableMainSelector;
			instance->draw();
			instance->screen.commit();
		}
		/*
		else if(instance->newValue == 3){
			instance->fourthElement.activate();
		}*/
	});
	instance->draw();
	instance->screen.commit();
}

void Settings::Settings::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(2);
}

void Settings::Settings::draw(){
	screen.getSprite()->clear(TFT_BLACK);

	for(int i=0;i<3;i++){
		if(!reinterpret_cast<SettingsElement*>(screenLayout.getChild(i))->isSelected()){
			screenLayout.getChild(i)->draw();
		}
	}
	for(int i = 0; i < 3; i++){
		if(reinterpret_cast<SettingsElement*>(screenLayout.getChild(i))->isSelected()){
			screenLayout.getChild(i)->draw();
		}
	}


}

void Settings::Settings::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(5);
	screenLayout.addChild(&firstElement);
	screenLayout.addChild(&secondElement);
	screenLayout.addChild(&thirdElement);
	//screenLayout.addChild(&fourthElement);

	screenLayout.reflow();
	screen.addChild(&screenLayout);
	screen.repos();
}

Settings::Settings::~Settings(){
	instance = nullptr;

}
