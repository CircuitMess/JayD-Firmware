#include <Input/InputJayD.h>
#include <JayD.hpp>
#include "MainMenu.h"
#include "Bitmaps/mainMenu_background.h"
#include "../MixScreen/MixScreen.h"
#include "../SongList/SongList.h"
#include "../Playback/Playback.h"

MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(new LinearLayout(&screen, HORIZONTAL)){


	for(int i = 0; i < 3; i++){
		item.push_back(new MainMenuItem(screenLayout, static_cast<MenuItemType>(i)));
	}

	instance = this;
	instance->item[0]->isSelected(true);
	buildUI();
	pack();
}

MainMenu::MainMenu::~MainMenu(){
	instance = nullptr;
}

void MainMenu::MainMenu::start(){
	draw();
	screen.commit();

	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		if(instance == nullptr) return;
		if(value == 0) return;

		instance->itemNum = instance->itemNum + value;

		if(instance->itemNum < 0){
			instance->itemNum = 2;
		}else if (instance->itemNum>2){
			instance->itemNum = 0;
		}

		for(MainMenuItem* i : instance->item){
			i->isSelected(false);
		}
		instance->item[instance->itemNum]->isSelected(true);

		instance->draw();
		instance->screen.commit();
	});

	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;

		Display& display = *instance->getScreen().getDisplay();
		int8_t selected = instance->itemNum;
		Serial.println(selected);

		instance->stop();
		delete instance;

		if(selected == 0){
			//Playback::Playback* playback = new Playback::Playback(display);

			SongList::SongList* list = new SongList::SongList(display);
			list->unpack();
			list->start();
		}
	});
}

void MainMenu::MainMenu::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_MID);
}

void MainMenu::MainMenu::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void MainMenu::MainMenu::buildUI(){

	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(30);

	for(int i = 0; i < item.size(); i++){
		screenLayout->addChild(item[i]);
	}

	screenLayout->reflow();

	screen.addChild(screenLayout);
	screen.repos();
}

void MainMenu::MainMenu::loop(uint micros){
	for(int i = 0; i < item.size(); i++){
		if(item[i]->needsUpdate()){
			draw();
			screen.commit();
		}
	}
}

