#include <Input/InputJayD.h>
#include <JayD.hpp>
#include "MainMenu.h"
#include <Loop/LoopManager.h>
#include <FS/CompressedFile.h>
#include <SPIFFS.h>

MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(new LinearLayout(&screen, HORIZONTAL)){


	for(int i = 0; i < 3; i++){
		item.push_back(new MainMenuItem(screenLayout, static_cast<MenuItemType>(i)));
	}

	backgroundPicture = SPIFFS.open("/mainMenuBackground.raw.hs");
	jayDlogo = SPIFFS.open("/jayD_logo.raw.hs");

	picture[0] = CompressedFile::open(backgroundPicture, 14, 10);
	picture[1]= CompressedFile::open(jayDlogo, 8, 7);

	instance = this;
	instance->item[0]->isSelected(true);
	buildUI();
	//pack();
}

MainMenu::MainMenu::~MainMenu(){
	backgroundPicture.close();
	jayDlogo.close();
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
			instance->itemNum = instance->item.size() - 1;
		}else if(instance->itemNum >= instance->item.size()){
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
	screen.getSprite()->drawIcon(buffer[0], 0, 0, 160, 128, 1);
	screen.getSprite()->drawIcon(buffer[1],screen.getTotalX()+48,screen.getTotalY()+10,64,24,1);
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
	for(int i=0;i<2;i++){
		free(buffer[i]);
	}
}

void MainMenu::MainMenu::unpack(){
	Context::unpack();
	for(int i=0;i<2;i++){
		buffer[i]= nullptr;
		buffer[i] = static_cast<Color *>(ps_malloc(i==0 ? (160*128*2) : (45 * 42 * 2)));
		if(buffer[i] == nullptr){
			Serial.println("MainMenu pictures unpack error");
			return;
		}
		picture[i].seek(0);
		picture[i].read(reinterpret_cast<uint8_t *>(buffer[i]), i==0 ? (160*128*2) : (45 * 42 * 2));
	}

}


