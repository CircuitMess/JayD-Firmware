#include <Input/InputJayD.h>
#include "Playback.h"

Playback::Playback *Playback::Playback::instance = nullptr;

Playback::Playback::Playback(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
												 songNameLayout(&screenLayout, HORIZONTAL),
												 timeElapsedLayout(&screenLayout, HORIZONTAL), buttonLayout(
				&screenLayout, HORIZONTAL), song(&songNameLayout), playOrPause(&buttonLayout),
												 trackCount(&timeElapsedLayout){


	instance = this;
	buildUI();

}

void Playback::Playback::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setBtnPressCallback(3, [](){
		if(instance == nullptr) return;
		instance->playOrPause.togglePlaying();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		if(instance == nullptr) return;
		instance->trackCount.setCurrentDuration(value);
		instance->draw();
		instance->screen.commit();
	});
}

void Playback::Playback::stop(){
	InputJayD::getInstance()->removeBtnPressCallback(3);
	InputJayD::getInstance()->removeEncoderMovedCallback(1);
}

void Playback::Playback::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void Playback::Playback::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(5);
	screenLayout.addChild(&songNameLayout);
	screenLayout.addChild(&timeElapsedLayout);
	screenLayout.addChild(&buttonLayout);

	songNameLayout.setWHType(PARENT, FIXED);
	songNameLayout.setHeight(35);
	songNameLayout.setGutter(5);
	songNameLayout.addChild(&song);

	timeElapsedLayout.setWHType(PARENT, FIXED);
	timeElapsedLayout.setHeight(20);
	timeElapsedLayout.setGutter(5);
	timeElapsedLayout.addChild(&trackCount);

	buttonLayout.setWHType(PARENT, FIXED);
	buttonLayout.setHeight(62);
	buttonLayout.setGutter(5);
	buttonLayout.addChild(&playOrPause);

	screenLayout.reflow();
	timeElapsedLayout.reflow();
	songNameLayout.reflow();
	buttonLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();

}

Playback::Playback::~Playback(){
	instance = nullptr;
}

