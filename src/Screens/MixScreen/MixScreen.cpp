#include "MixScreen.hpp"

MixScreen *MixScreen::instance = nullptr;

MixScreen::MixScreen(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
										 leftLayout(&screenLayout, VERTICAL), rightLayout(&screenLayout, VERTICAL){


	leftSeekBar.push_back(new LeftSeekBar(&leftLayout));
	rightSeekBar.push_back(new RightSeekBar(&rightLayout));

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

	leftLayout.setWHType(FIXED, PARENT);
	leftLayout.setWidth(77);
	leftLayout.setGutter(5);
	leftLayout.setPadding(2);

	leftLayout.addChild(leftSeekBar[0]);

	rightLayout.setWHType(FIXED, PARENT);
	rightLayout.setWidth(77);
	rightLayout.setGutter(5);
	rightLayout.setPadding(2);

	rightLayout.addChild(rightSeekBar[0]);

	screenLayout.reflow();
	leftLayout.reflow();
	rightLayout.reflow();

	screen.addChild(&screenLayout);
	screen.commit();


}