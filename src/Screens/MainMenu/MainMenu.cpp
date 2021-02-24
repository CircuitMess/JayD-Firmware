#include <InputLib/InputJayD.h>
#include "MainMenu.h"


MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
												 scrollLayout(&screenLayout), screenMode(&scrollLayout, HORIZONTAL){


	for(int i = 0; i < 3; i++){
		modeElement.push_back(new Mode(&screenMode));
	}

	instance = this;
	buildUI();

}

void MainMenu::MainMenu::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		int scrollVal = value * 20;
		if(value == 0) return;
		if((instance->scrollLayout.getScrollX() >= instance->scrollLayout.getMaxScrollX()) && (value > 0) ||
		   ((instance->scrollLayout.getScrollX() == 0) && value < 0)){
			scrollVal = 0;
		}
		instance->scrollLayout.setScroll(instance->scrollLayout.getScrollX() + scrollVal, 0);
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
	screenLayout.setGutter(2);
	screenLayout.addChild(&scrollLayout);

	scrollLayout.setWHType(FIXED, FIXED);
	scrollLayout.setWidth(40);
	scrollLayout.setHeight(40);
	scrollLayout.setBorder(1, TFT_RED);
	scrollLayout.addChild(&screenMode);

	screenMode.setWHType(PARENT, CHILDREN);
	screenMode.setPadding(5);
	screenMode.setGutter(25);
	//screenMode.setBorder(1, TFT_RED);
	for(int i=0; i < modeElement.size(); i++){
		screenMode.addChild(modeElement[i]);
	}

	screenLayout.reflow();
	scrollLayout.reflow();
	screenMode.reflow();

	screen.addChild(&screenLayout);
	screen.repos();
	pack();
}
