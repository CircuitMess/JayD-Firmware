#include "SettingsScreen.h"
#include "../InputTest/InputTest.h"
#include <Input/InputJayD.h>
#include <SPIFFS.h>
#include <FS/CompressedFile.h>
#include <Settings.h>
#include <JayD.h>
#include <AudioLib/Systems/PlaybackSystem.h>

SettingsScreen::SettingsScreen* SettingsScreen::SettingsScreen::instance = nullptr;

SettingsScreen::SettingsScreen::SettingsScreen(Display &display) : Context(display), screenLayout(new LinearLayout(&screen, VERTICAL)),
																   volumeSlider(new SliderElement(screenLayout, "Volume")), brightnessSlider(new SliderElement(screenLayout, "Brightness")),
																   inputTest(new TextElement(screenLayout, "Input Test")),
																   saveSettings(new TextElement(screenLayout, "Save")){

	instance = this;
	buildUI();
	volumeSlider->setIsSelected(true);
	selectedSetting = 0;

	volumeSlider->setSliderValue(Settings.get().volumeLevel);

	brightnessSlider->setSliderValue(Settings.get().brightnessLevel);

	SettingsScreen::pack();
}

void SettingsScreen::SettingsScreen::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(ENC_MID, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->disableMainSelector && instance->selectedSetting == 0){
			instance->volumeSlider->moveSliderValue(value);
			Settings.get().volumeLevel = instance->volumeSlider->getSliderValue();
			instance->playback->updateGain();
			instance->draw();
			instance->screen.commit();
			return;
		}
		if(instance->disableMainSelector && instance->selectedSetting == 1){
			instance->brightnessSlider->moveSliderValue(value);
			Settings.get().brightnessLevel = instance->brightnessSlider->getSliderValue();
			LEDmatrix.setBrightness(80.0f * (float) instance->brightnessSlider->getSliderValue() / 255.0f);
			matrixManager.push();
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
			instance->volumeSlider->setIsSelected(true);

		}else{
			instance->volumeSlider->setIsSelected(false);
		}
		if(instance->selectedSetting == 1){
			instance->brightnessSlider->setIsSelected(true);

		}else{
			instance->brightnessSlider->setIsSelected(false);
		}
		if(instance->selectedSetting == 2){
			instance->inputTest->setIsSelected(true);

		}else{
			instance->inputTest->setIsSelected(false);
		}
		if(instance->selectedSetting == 3){
			instance->saveSettings->setIsSelected(true);

		}else{
			instance->saveSettings->setIsSelected(false);
		}
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;
		if(instance->selectedSetting == 0){

			instance->volumeSlider->toggle();
			instance->disableMainSelector = !instance->disableMainSelector;
			instance->draw();
			instance->screen.commit();
			if(instance->disableMainSelector) {
				Settings.get().volumeLevel = instance->volumeSlider->getSliderValue();
				instance->playback->updateGain();
				instance->playback->start();
			}else{
				instance->playback->stop();
			}
		}else if(instance->selectedSetting == 1){
			instance->brightnessSlider->toggle();
			instance->disableMainSelector = !instance->disableMainSelector;
			if(instance->disableMainSelector) {
				LEDmatrix.setBrightness(80.0f * (float) instance->brightnessSlider->getSliderValue() / 255.0f);
				matrixManager.startRandom();
			}else{
				matrixManager.stopRandom();
			}
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
	introSong = SPIFFS.open("/intro.aac");
	playback = new PlaybackSystem(introSong);
	Settings.get().volumeLevel = instance->volumeSlider->getSliderValue();
	instance->playback->updateGain();
	playback->setRepeat(true);
}

void SettingsScreen::SettingsScreen::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(2);
	matrixManager.stopRandom();
	Settings.store();
	playback->stop();
	introSong.close();
	delete playback;
}

void SettingsScreen::SettingsScreen::draw(){
	screen.getSprite()->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);
	screen.getSprite()->setTextColor(TFT_WHITE);
	screen.getSprite()->setTextSize(1);
	screen.getSprite()->setTextFont(1);
	screen.getSprite()->setCursor(screenLayout->getTotalX() + 42, screenLayout->getTotalY() + 115);
	screen.getSprite()->println("Version 1.3");

	for(int i = 0; i < 4; i++){
		if(!reinterpret_cast<SettingsElement *>(screenLayout->getChild(i))->isSelected()){
			screenLayout->getChild(i)->draw();
		}
	}
	for(int i = 0; i < 4; i++){
		if(reinterpret_cast<SettingsElement *>(screenLayout->getChild(i))->isSelected()){
			screenLayout->getChild(i)->draw();
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

	backgroundBuffer = static_cast<Color*>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("SettingsScreen background unpack error");
		return;
	}

	fs::File bgFile = CompressedFile::open(SPIFFS.open("/settingsBackground.raw.hs"), 14, 13);
	bgFile.read(reinterpret_cast<uint8_t*>(backgroundBuffer), 160 * 128 * 2);
	bgFile.close();
}


void SettingsScreen::SettingsScreen::buildUI(){
	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(5);
	screenLayout->addChild(volumeSlider);
	screenLayout->addChild(brightnessSlider);
	screenLayout->addChild(inputTest);
	screenLayout->addChild(saveSettings);

	screenLayout->reflow();
	screen.addChild(screenLayout);
	screen.repos();
}

SettingsScreen::SettingsScreen::~SettingsScreen(){
	instance = nullptr;
	free(backgroundBuffer);
}
