#include <InputLib/InputJayD.h>
#include "MixScreen.h"

MixScreen::MixScreen *MixScreen::MixScreen::instance = nullptr;

MixScreen::MixScreen::MixScreen(Display &display) : Context(display), screenLayout(&screen, HORIZONTAL),
													leftLayout(&screenLayout, VERTICAL),
													rightLayout(&screenLayout, VERTICAL), leftSeekBar(&leftLayout),
													rightSeekBar(&rightLayout), leftSongName(&leftLayout),
													rightSongName(&rightLayout){


	for(int i = 0; i < 3; i++){
		effectsLeft.push_back(new Effects(&leftLayout, false));
	}
	for(int i = 0; i < 3; i++){
		effectsRight.push_back(new Effects(&rightLayout, true));
	}

	instance = this;
	buildUI();

}

void MixScreen::MixScreen::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setBtnPressCallback(3, [](){
		if(!instance->isPlaying){
			instance->isPlaying=true;
		}else{
			instance->isPlaying=false;
		}
		instance->leftSeekBar.setPlaying(instance->isPlaying);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(2, [](){
		if(!instance->isPlaying){
			instance->isPlaying=true;
		}else{
			instance->isPlaying=false;
		}
		instance->rightSeekBar.setPlaying(instance->isPlaying);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		instance->leftSeekBar.setCurrentDuration(value);
		instance->draw();
		instance->screen.commit();
	});
}


void MixScreen::MixScreen::stop(){

}

void MixScreen::MixScreen::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void MixScreen::MixScreen::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(1);
	screenLayout.addChild(&leftLayout);
	screenLayout.addChild(&rightLayout);

	leftLayout.setWHType(FIXED, PARENT);
	leftLayout.setWidth(79);
	leftLayout.setGutter(10);
	leftLayout.setPadding(1);
	leftLayout.setBorder(1, TFT_RED);

	leftLayout.addChild(&leftSeekBar);
	leftLayout.addChild(&leftSongName);

	for(int i = 0; i < effectsLeft.size(); i++){
		leftLayout.addChild(effectsLeft[i]);
	}


	effectsLeft[0]->setEffect(LOWPASS);
	effectsLeft[1]->setEffect(HIGHPASS);
	effectsLeft[2]->setEffect(SPEED);

	rightLayout.setWHType(FIXED, PARENT);
	rightLayout.setWidth(79);
	rightLayout.setGutter(10);
	rightLayout.setPadding(1);
	rightLayout.setBorder(1, TFT_BLUE);

	rightLayout.addChild(&rightSeekBar);
	rightLayout.addChild(&rightSongName);

	for(int i = 0; i < effectsRight.size(); i++){
		rightLayout.addChild(effectsRight[i]);
	}
	effectsRight[0]->setEffect(LOWPASS);
	effectsRight[1]->setEffect(HIGHPASS);
	effectsRight[2]->setEffect(SPEED);

	screenLayout.reflow();
	leftLayout.reflow();
	rightLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();


}