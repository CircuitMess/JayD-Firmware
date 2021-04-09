#include "SettingsScreen.h"
#include "../InputTest/InputTest.h"
#include <Input/InputJayD.h>
#include <SPIFFS.h>
#include <FS/CompressedFile.h>
#include <Settings.h>
#include <JayD.hpp>


SettingsScreen::SettingsScreen *SettingsScreen::SettingsScreen::instance = nullptr;

SettingsScreen::SettingsScreen::SettingsScreen(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
																   volumeSlider(&screenLayout, "Volume"), brightnessSlider(&screenLayout, "Brightness"),
																   inputTest(&screenLayout, "Input Test"),
																   saveSettings(&screenLayout, "Save"){

	instance = this;
	buildUI();
	volumeSlider.setIsSelected(true);
	selectedSetting = 0;

	fs::File file = SPIFFS.open("/settingsBackground.raw.hs");

	background = CompressedFile::open(file, 14, 13);

	volumeSlider.setSliderValue(Settings.get().volumeLevel);

	brightnessSlider.setSliderValue(Settings.get().brightnessLevel);

}

void SettingsScreen::SettingsScreen::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(ENC_MID, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->disableMainSelector && instance->selectedSetting == 0){
			instance->volumeSlider.moveSliderValue(value);
			Settings.get().volumeLevel = instance->volumeSlider.getSliderValue();
			instance->draw();
			instance->screen.commit();
			return;
		}
		if(instance->disableMainSelector && instance->selectedSetting == 1){
			instance->brightnessSlider.moveSliderValue(value);
			Settings.get().brightnessLevel = instance->brightnessSlider.getSliderValue();
			instance->draw();
			instance->screen.commit();
			return;
		}
		instance->selectedSetting = instance->selectedSetting + value;

		if(instance->selectedSetting < 0){
			instance->selectedSetting = 3;
		}else if(instance->selectedSetting > 3){
			instance->selectedSetting = 0;
		}
		if(instance->selectedSetting == 0){
			instance->volumeSlider.setIsSelected(true);

		}else{
			instance->volumeSlider.setIsSelected(false);
		}
		if(instance->selectedSetting == 1){
			instance->brightnessSlider.setIsSelected(true);

		}else{
			instance->brightnessSlider.setIsSelected(false);
		}
		if(instance->selectedSetting == 2){
			instance->inputTest.setIsSelected(true);

		}else{
			instance->inputTest.setIsSelected(false);
		}
		if(instance->selectedSetting == 3){
			instance->saveSettings.setIsSelected(true);

		}else{
			instance->saveSettings.setIsSelected(false);
		}
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;
		if(instance->selectedSetting == 0){
			instance->volumeSlider.activate();
			instance->disableMainSelector = !instance->disableMainSelector;
			instance->draw();
			instance->screen.commit();
		}else if(instance->selectedSetting == 1){
			instance->brightnessSlider.activate();
			instance->disableMainSelector = !instance->disableMainSelector;
			instance->draw();
			instance->screen.commit();
		}else if(instance->selectedSetting == 2){
			Display &display = *instance->getScreen().getDisplay();
			InputTest::InputTest *inputTest = new InputTest::InputTest(display);
			inputTest->push(instance);
		}else if(instance->selectedSetting == 3){
			instance->pop();
		}

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
	screenLayout.getSprite()->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);
	screenLayout.getSprite()->setTextColor(TFT_WHITE);
	screenLayout.getSprite()->setTextSize(1);
	screenLayout.getSprite()->setTextFont(1);
	screenLayout.getSprite()->setCursor(screenLayout.getTotalX() + 42, screenLayout.getTotalY() + 115);
	screenLayout.getSprite()->println("Version 1.0");
	for(int i = 0; i < 4; i++){
		if(!reinterpret_cast<SettingsElement *>(screenLayout.getChild(i))->isSelected()){
			screenLayout.getChild(i)->draw();
		}
	}
	for(int i = 0; i < 4; i++){
		if(reinterpret_cast<SettingsElement *>(screenLayout.getChild(i))->isSelected()){
			screenLayout.getChild(i)->draw();
		}
	}


}

void SettingsScreen::SettingsScreen::pack(){
	Context::pack();
	free(backgroundBuffer);
	backgroundBuffer = nullptr;
}

void SettingsScreen::SettingsScreen::unpack(){
	Context::unpack();
	backgroundBuffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("SettingsScreen background unpack error");
		return;
	}
	background.seek(0);
	background.read(reinterpret_cast<uint8_t *>(backgroundBuffer), 160 * 128 * 2);

}


void SettingsScreen::SettingsScreen::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(5);
	screenLayout.addChild(&volumeSlider);
	screenLayout.addChild(&brightnessSlider);
	screenLayout.addChild(&inputTest);
	screenLayout.addChild(&saveSettings);

	screenLayout.reflow();
	screen.addChild(&screenLayout);
	screen.repos();
}

SettingsScreen::SettingsScreen::~SettingsScreen(){
	instance = nullptr;
	background.close();
	free(backgroundBuffer);

}
