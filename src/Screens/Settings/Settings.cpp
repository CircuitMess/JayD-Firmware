#include "Settings.h"
#include <Input/InputJayD.h>

Settings::Settings *Settings::Settings::instance = nullptr;

Settings::Settings::Settings(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
												 firstElement(&screenLayout, "Song"),
												 secondElement(&screenLayout, "Color",
															   {"Blue", "Red", "Yellow", "Green", "Orange", "Purple"})/*,
												 thirdElement(&screenLayout, "Name"),
												 fourthElement(&screenLayout, "Brightness")*/{

	instance = this;
	buildUI();
	firstElement.setIsSelected(true);


}

void Settings::Settings::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		if(instance->disableMainSelector && instance->newValue == 1){
			if(value > 0){
				instance->secondElement.selectNext();
			}else if(value < 0){
				instance->secondElement.selectPrev();
			}
			instance->draw();
			instance->screen.commit();
			return;
		}
		instance->newValue = instance->newValue + value;
		if(instance->newValue < 0){
			instance->newValue = 3;
		}else if(instance->newValue > 3){
			instance->newValue = 0;
		}
		if(instance->newValue == 0){
			Serial.println(instance->newValue);
			instance->firstElement.setIsSelected(true);
		}else{
			instance->firstElement.setIsSelected(false);
		}

		if(instance->newValue == 1){
			instance->secondElement.setIsSelected(true);
		}else{
			instance->secondElement.setIsSelected(false);
		}

/*
		if(instance->newValue == 2){
			instance->thirdElement.setIsSelected(true);
		}else{
			instance->thirdElement.setIsSelected(false);
		}

		if(instance->newValue == 3){
			instance->fourthElement.setIsSelected(true);
		}else{
			instance->fourthElement.setIsSelected(false);

		}*/
		InputJayD::getInstance()->setBtnPressCallback(3, [](){
			if(instance->newValue == 0){
				instance->firstElement.activated();
				instance->draw();
				instance->screen.commit();
			}else if(instance->newValue == 1){
				instance->secondElement.activated();
				if(!instance->disableMainSelector){
					instance->disableMainSelector = true;
				}else if(instance->disableMainSelector){
					instance->disableMainSelector = false;
				}
				instance->draw();
				instance->screen.commit();
			}
			/*else if(instance->newValue == 1){
				instance->secondElement.activated();
			}
			else if(instance->newValue == 2){
				instance->thirdElement.activated();
			}
			else if(instance->newValue == 3){
				instance->fourthElement.activated();
			}*/
		});
		instance->draw();
		instance->screen.commit();
	});
}

void Settings::Settings::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(1);
	InputJayD::getInstance()->removeBtnPressCallback(3);
}

void Settings::Settings::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();

}

void Settings::Settings::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(5);
	screenLayout.addChild(&firstElement);
	screenLayout.addChild(&secondElement);
	/*screenLayout.addChild(&thirdElement);
	screenLayout.addChild(&fourthElement);*/

	screenLayout.reflow();
	screen.addChild(&screenLayout);
	screen.repos();
}