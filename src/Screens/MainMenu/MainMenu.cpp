#include <Input/InputJayD.h>
#include <JayD.hpp>
#include "MainMenu.h"
#include "../Playback/Playback.h"
#include "../MixScreen/MixScreen.h"
#include "../Settings/SettingsScreen.h"
#include <Loop/LoopManager.h>
#include <FS/CompressedFile.h>
#include <SPIFFS.h>

MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(new LinearLayout(&screen, HORIZONTAL)){


	for(int i = 0; i < 3; i++){
		item.push_back(new MainMenuItem(screenLayout, static_cast<MenuItemType>(i)));
	}

	backgroundPicture = CompressedFile::open(SPIFFS.open("/mainMenuBackground.raw.hs"), 14, 10);
	jayDlogo = CompressedFile::open(SPIFFS.open("/jayD_logo.raw.hs"), 8, 7);

	instance = this;
	instance->item[1]->isSelected(true);
	buildUI();

	pack();
}

MainMenu::MainMenu::~MainMenu(){
	backgroundPicture.close();
	jayDlogo.close();
	instance = nullptr;
	free(backgroundBuffer);
	free(logoBuffer);

}

void MainMenu::MainMenu::start(){
	InputJayD::getInstance()->setHoldTime(0);

	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		if(instance == nullptr) return;
		if(value == 0) return;

		int8_t newSelected = instance->itemNum + value;
		newSelected = max(newSelected, (int8_t) 0);
		newSelected = min(newSelected, (int8_t) 2);

		if(instance->itemNum == newSelected) return;
		instance->itemNum = newSelected;

		for(MainMenuItem *i : instance->item){
			i->isSelected(false);
		}
		instance->item[instance->itemNum]->isSelected(true);

		instance->draw();
		instance->screen.commit();
	});

	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;

		Display &display = *instance->getScreen().getDisplay();
		int8_t selected = instance->itemNum;

		if(selected == 0){
			Playback::Playback* playback = new Playback::Playback(display);
			playback->push(instance);
		}else if(selected == 1){
			MixScreen::MixScreen* mix = new MixScreen::MixScreen(display);
			mix->push(instance);
		}else if(selected == 2){
			SettingsScreen::SettingsScreen* settings = new SettingsScreen::SettingsScreen(display);
			settings->push(instance);
		}
	});

	matrixManager.startRandom();

	jumpTime = 0;
	draw();
	screen.commit();

	LoopManager::addListener(this);
}

void MainMenu::MainMenu::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_MID);
	LoopManager::removeListener(this);
	matrixManager.stopRandom();
}

void MainMenu::MainMenu::draw(){
	screen.getSprite()->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);
	screen.getSprite()->drawIcon(logoBuffer,screen.getTotalX()+48 + sin((float) jumpTime / 500000.0f) * 20,screen.getTotalY()+8,64,24,1);
	screen.draw();

}

void MainMenu::MainMenu::buildUI(){

	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(31);

	for(int i = 0; i < item.size(); i++){
		screenLayout->addChild(item[i]);
	}

	screenLayout->reflow();

	screen.addChild(screenLayout);
	screen.repos();
}

void MainMenu::MainMenu::loop(uint micros){
	jumpTime += micros;

	draw();
	screen.commit();
}

void MainMenu::MainMenu::pack(){
	Context::pack();
	free(backgroundBuffer);
	backgroundBuffer = nullptr;
	free(logoBuffer);
	logoBuffer= nullptr;
}

void MainMenu::MainMenu::unpack(){
	Context::unpack();
	backgroundBuffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("MainMenu background picture unpack error");
		return;
	}

	backgroundPicture.seek(0);
	backgroundPicture.read(reinterpret_cast<uint8_t *>(backgroundBuffer), (160 * 128 * 2));

	logoBuffer = static_cast<Color *>(ps_malloc(45 * 42 * 2));
	if(logoBuffer == nullptr){
		Serial.println("MainMenu background picture unpack error");
		return;
	}

	jayDlogo.seek(0);
	jayDlogo.read(reinterpret_cast<uint8_t *>(logoBuffer), (45 * 42 * 2));
}