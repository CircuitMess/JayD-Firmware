#include <InputLib/InputJayD.h>
#include "MainMenu.h"


MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(&screen, HORIZONTAL)
												{


	for(int i = 0; i < 3; i++){
		modeElement.push_back(new Mode(&screenLayout));
	}

	instance = this;
	buildUI();

}

void MainMenu::MainMenu::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		if(!instance->isSelected[0]){
			instance->isSelected[0] = true;
		}else{
			instance->isSelected[0] = false;
		}
		if(!instance->isSelected[1]){
			instance->isSelected[1] = true;
		}else{
			instance->isSelected[1] = false;
		}
		if(!instance->isSelected[2]){
			instance->isSelected[2] = true;
		}else{
			instance->isSelected[2] = false;
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
	screenLayout.setPadding(2);
	screenLayout.setGutter(20);
	for(int i=0;i<modeElement.size();i++){
		screenLayout.addChild(modeElement[i]);
	}

	screenLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();
	pack();
}
