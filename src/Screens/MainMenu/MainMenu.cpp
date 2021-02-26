#include <Input/InputJayD.h>
#include "MainMenu.h"


MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(&screen, HORIZONTAL){

	for(int i = 0; i < 3; i++){
		item.push_back(new MainMenuItem(&screenLayout));
	}

	instance = this;
	buildUI();
	instance->item[0]->isSelected(true);
}

void MainMenu::MainMenu::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
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
	InputJayD::getInstance()->removeEncoderMovedCallback(1);
}

void MainMenu::MainMenu::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void MainMenu::MainMenu::buildUI(){

	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(30);

	for(int i = 0; i < item.size(); i++){
		screenLayout.addChild(item[i]);
	}

	item[0]->setItems(static_cast<Items>(0));
	item[1]->setItems(static_cast<Items>(1));
	item[2]->setItems(static_cast<Items>(2));

	screenLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();
	pack();
}
