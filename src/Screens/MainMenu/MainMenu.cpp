#include <Input/InputJayD.h>
#include "MainMenu.h"
#include "Bitmaps/mainMenu_background.h"

MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(&screen, HORIZONTAL){


	for(int i = 0; i < 3; i++){
		item.push_back(new MainMenuItem(&screenLayout, static_cast<MenuItemType>(i)));
	}

	instance = this;
	instance->item[0]->isSelected(true);
	buildUI();

}

void MainMenu::MainMenu::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		if(value == 0) return;
		instance->itemNum = instance->itemNum + value;

		if(instance->itemNum < 0){
			instance->itemNum = 2;
		}else if (instance->itemNum>2){
			instance->itemNum = 0;
		}

		if(instance->itemNum == 0){
			instance->item[0]->isSelected(true);
		}else{
			instance->item[0]->isSelected(false);
		}
		if(instance->itemNum == 1){
			instance->item[1]->isSelected(true);
		}else{
			instance->item[1]->isSelected(false);
		}
		if(instance->itemNum == 2){
			instance->item[2]->isSelected(true);
		}else{
			instance->item[2]->isSelected(false);
		}
		instance->draw();
		instance->screen.commit();
	});

}

void MainMenu::MainMenu::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
}

void MainMenu::MainMenu::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	Serial.println("Prije:");
	Serial.println(ESP.getFreeHeap());
//	screen.getSprite()->drawIcon(mainMenuBackground,0,0,160,128,1);
	Serial.println("Poslije:");
	Serial.println(ESP.getFreeHeap());

	screen.draw();
}

void MainMenu::MainMenu::buildUI(){

	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(30);

	for(int i = 0; i < item.size(); i++){
		screenLayout.addChild(item[i]);
	}

	screenLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();
	pack();
}

void MainMenu::MainMenu::loop(uint micros){
	for(int i = 0; i < item.size(); i++){
		if(item[i]->needsUpdate()){
			draw();
			screen.commit();
		}
	}

}

