#include <Input/InputJayD.h>
#include <JayD.hpp>
#include "MainMenu.h"
#include "Bitmaps/mainMenu_background.hpp"
#include "../MixScreen/MixScreen.h"
#include <Loop/LoopManager.h>
#include <FS/CompressedFile.h>
#include <SPIFFS.h>

MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(new LinearLayout(&screen, HORIZONTAL)){


	for(int i = 0; i < 2; i++){
		item.push_back(new MainMenuItem(screenLayout, static_cast<MenuItemType>(i)));
	}

	fs::File file = SPIFFS.open("/mainMenuBackground.raw.hs");

	background = CompressedFile::open(file, 14, 10);

	instance = this;
	instance->item[0]->isSelected(true);
	buildUI();
	//pack();
}

MainMenu::MainMenu::~MainMenu(){

	instance = nullptr;
}

void MainMenu::MainMenu::start(){
	LoopManager::addListener(this);
	draw();
	screen.commit();

	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		if(instance == nullptr) return;
		if(value == 0) return;
		instance->itemNum = instance->itemNum + value;

		if(instance->itemNum < 0){
			instance->itemNum = 1;
		}else if(instance->itemNum > 1){
			instance->itemNum = 0;
		}

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
		Serial.println(selected);

		instance->stop();
		delete instance;

		if(selected == 0){
			//Playback::Playback* playback = new Playback::Playback(display);

			/*SongList::SongList* list = new SongList::SongList(display);
			list->unpack();
			list->start();*/
		}
	});
}

void MainMenu::MainMenu::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_MID);
}

void MainMenu::MainMenu::draw(){

	screen.getSprite()->drawIcon(buffer, 0, 0, 160, 128, 1);
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
	bool update = false;
	for(const auto &i : item){
		update |= i->needsUpdate();
	}
	if(update){
		draw();
		screen.commit();
	}
}

void MainMenu::MainMenu::pack(){
	Context::pack();
	free(buffer);
}

void MainMenu::MainMenu::unpack(){
	Context::unpack();
	buffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(buffer == nullptr){
		Serial.println("Settings background unpack error");
		return;
	}
	background.seek(0);
	background.read(reinterpret_cast<uint8_t *>(buffer), 160 * 128 * 2);

}


