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
	InputJayD::getInstance()->setBtnPressCallback(0, [](){
		if(!instance->isPlaying){
			instance->isPlaying = true;
		}else{
			instance->isPlaying = false;
		}
		instance->leftSeekBar.setPlaying(instance->isPlaying);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(1, [](){
		if(!instance->isPlaying){
			instance->isPlaying = true;
		}else{
			instance->isPlaying = false;
		}
		instance->rightSeekBar.setPlaying(instance->isPlaying);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(3,[](){
		if(!instance->isSelected){
			instance->isSelected = true;
		}else{
			instance->isSelected= false;
		}
		instance->effectsLeft[0]->setSelected(instance->isSelected);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(8,[](){
		if(!instance->isSelected){
			instance->isSelected = true;
		}else{
			instance->isSelected= false;
		}
		instance->effectsLeft[1]->setSelected(instance->isSelected);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(7,[](){
		if(!instance->isSelected){
			instance->isSelected = true;
		}else{
			instance->isSelected= false;
		}
		instance->effectsLeft[2]->setSelected(instance->isSelected);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(6,[](){
		if(!instance->isSelected){
			instance->isSelected = true;
		}else{
			instance->isSelected= false;
		}
		instance->effectsRight[0]->setSelected(instance->isSelected);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(5,[](){
		if(!instance->isSelected){
			instance->isSelected = true;
		}else{
			instance->isSelected= false;
		}
		instance->effectsRight[1]->setSelected(instance->isSelected);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(4,[](){
		if(!instance->isSelected){
			instance->isSelected = true;
		}else{
			instance->isSelected= false;
		}
		instance->effectsRight[2]->setSelected(instance->isSelected);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		instance->intenstiy[0]=instance->intenstiy[0]+value;
		instance->effectsLeft[0]->setIntensity(instance->intenstiy[0]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(6, [](int8_t value){
		instance->intenstiy[1]=instance->intenstiy[1]+value;
		instance->effectsLeft[1]->setIntensity(instance->intenstiy[1]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(5, [](int8_t value){
		instance->intenstiy[2]=instance->intenstiy[2]+value;
		instance->effectsLeft[2]->setIntensity(instance->intenstiy[2]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(4, [](int8_t value){
		instance->intenstiy[3]=instance->intenstiy[3]+value;
		instance->effectsRight[0]->setIntensity(instance->intenstiy[3]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(3, [](int8_t value){
		instance->intenstiy[4]=instance->intenstiy[4]+value;
		instance->effectsRight[1]->setIntensity(instance->intenstiy[4]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(2, [](int8_t value){
		instance->intenstiy[5]=instance->intenstiy[5]+value;
		instance->effectsRight[2]->setIntensity(instance->intenstiy[5]);
		instance->draw();
		instance->screen.commit();
	});
}


void MixScreen::MixScreen::stop(){
	InputJayD::getInstance()->removeBtnPressCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(1);
	InputJayD::getInstance()->removeEncoderMovedCallback(1);
	InputJayD::getInstance()->removeEncoderMovedCallback(5);
	InputJayD::getInstance()->removeEncoderMovedCallback(6);
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