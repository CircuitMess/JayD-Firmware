#include "SettingsScreen.h"
#include <Input/InputJayD.h>
#include <SPIFFS.h>
#include <FS/CompressedFile.h>
#include <Settings.h>

SettingsScreen::SettingsScreen *SettingsScreen::SettingsScreen::instance = nullptr;

SettingsScreen::SettingsScreen::SettingsScreen(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
																   volumeSlider(&screenLayout, "Volume"), brightnessSlider(&screenLayout, "Brightness"){

	instance = this;
	buildUI();
	volumeSlider.setIsSelected(true);
	newValue = 0;

	fs::File file = SPIFFS.open("/settingsBackground.raw.hs");

	background = CompressedFile::open(file, 14, 13);

	volumeSlider.setSliderValue(Settings.get().volumeLevel);

	brightnessSlider.setSliderValue(Settings.get().brightnessLevel);

}

void SettingsScreen::SettingsScreen::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->disableMainSelector && instance->newValue == 0){
			instance->volumeSlider.moveSliderValue(value);
			Settings.get().volumeLevel=instance->volumeSlider.getSliderValue();
			instance->draw();
			instance->screen.commit();
			return;
		}
		if(instance->disableMainSelector && instance->newValue == 1){
			instance->brightnessSlider.moveSliderValue(value);
			Settings.get().brightnessLevel=instance->brightnessSlider.getSliderValue();
			instance->draw();
			instance->screen.commit();
			return;
		}
		instance->newValue = instance->newValue + value;

		if(instance->newValue < 0){
			instance->newValue = 1;
		}else if(instance->newValue > 1){
			instance->newValue = 0;
		}
		if(instance->newValue == 0){
			instance->volumeSlider.setIsSelected(true);

		}else{
			instance->volumeSlider.setIsSelected(false);
		}
		if(instance->newValue == 1){
			instance->brightnessSlider.setIsSelected(true);

		}else{
			instance->brightnessSlider.setIsSelected(false);
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
			instance->volumeSlider.activate();
			instance->disableMainSelector = !instance->disableMainSelector;
			instance->draw();
			instance->screen.commit();
		}else if(instance->newValue == 1){
			instance->brightnessSlider.activate();
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

void SettingsScreen::SettingsScreen::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(2);
	Settings.store();

}

void SettingsScreen::SettingsScreen::draw(){
	screen.getSprite()->drawIcon(buffer, 0, 0, 160, 128, 1);

	for(int i = 0; i < 2; i++){
		if(!reinterpret_cast<SettingsElement *>(screenLayout.getChild(i))->isSelected()){
			screenLayout.getChild(i)->draw();
		}
	}
	for(int i = 0; i < 2; i++){
		if(reinterpret_cast<SettingsElement *>(screenLayout.getChild(i))->isSelected()){
			screenLayout.getChild(i)->draw();
		}
	}


}

void SettingsScreen::SettingsScreen::pack(){
	Context::pack();
	free(buffer);
}

void SettingsScreen::SettingsScreen::unpack(){
	Context::unpack();
	buffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(buffer == nullptr){
		Serial.println("SettingsScreen background unpack error");
		return;
	}
	background.seek(0);
	background.read(reinterpret_cast<uint8_t *>(buffer), 160 * 128 * 2);

}


void SettingsScreen::SettingsScreen::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(5);
	screenLayout.addChild(&volumeSlider);
	screenLayout.addChild(&brightnessSlider);
	//screenLayout.addChild(&fourthElement);

	screenLayout.reflow();
	screen.addChild(&screenLayout);
	screen.repos();
}

SettingsScreen::SettingsScreen::~SettingsScreen(){
	instance = nullptr;
	background.close();

}
