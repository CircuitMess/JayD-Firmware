#include "MixScreen.hpp"

MixScreen *MixScreen::instance = nullptr;

MixScreen::MixScreen(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
										 leftLayout(&screenLayout, VERTICAL), rightLayout(&screenLayout, VERTICAL){


	instance = this;
	buildUI();

}

void MixScreen::start(){
	draw();
	screen.commit();

}

void MixScreen::stop(){

}

void MixScreen::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void MixScreen::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(5);
	screenLayout.addChild(&leftLayout);
	screenLayout.addChild(&rightLayout);

	leftLayout.setWHType(FIXED,PARENT);
	leftLayout.setWidth(77);
	leftLayout.setGutter(5);
	leftLayout.setPadding(2);

	rightLayout.setWHType(FIXED,PARENT);
	rightLayout.setWidth(77);
	rightLayout.setGutter(5);
	rightLayout.setPadding(2);

	screenLayout.reflow();
	leftLayout.reflow();
	rightLayout.reflow();

	screen.addChild(&screenLayout);
	screen.commit();


}