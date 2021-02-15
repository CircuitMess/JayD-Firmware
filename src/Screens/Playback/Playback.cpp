#include <InputLib/InputJayD.h>
#include "Playback.h"

Playback *Playback::instance = nullptr;

Playback::Playback(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
									   songNameLayout(&screenLayout, HORIZONTAL),
									   timeElapsedLayout(&screenLayout, HORIZONTAL), buttonLayout(
				&screenLayout, HORIZONTAL){

	song.push_back(new SongName(&songNameLayout));

	playOrPause.push_back(new PlayPause(&buttonLayout));

	trackCount.push_back(new TrackCounter(&timeElapsedLayout));

	instance = this;
	buildUI();

}

void Playback::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setBtnPressCallback(2, [](){
		instance->playOrPause[0]->checkIfPlaying();
		instance->draw();
		instance->screen.commit();
	});
}

void Playback::stop(){
	InputJayD::getInstance()->removeBtnPressCallback(2);
}

void Playback::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void Playback::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(5);
	screenLayout.addChild(&songNameLayout);
	screenLayout.addChild(&timeElapsedLayout);
	screenLayout.addChild(&buttonLayout);

	songNameLayout.setWHType(PARENT, FIXED);
	songNameLayout.setHeight(35);
	songNameLayout.setGutter(5);
	songNameLayout.setPadding(10);
//	songNameLayout.setBorder(2, TFT_WHITE);

	songNameLayout.addChild(song[0]);

	timeElapsedLayout.setWHType(PARENT, FIXED);
	timeElapsedLayout.setHeight(20);
	timeElapsedLayout.setGutter(5);
	timeElapsedLayout.setPadding(10);
	//timeElapsedLayout.setBorder(2, TFT_WHITE);

	timeElapsedLayout.addChild(trackCount[0]);

	buttonLayout.setWHType(PARENT, FIXED);
	buttonLayout.setHeight(62);
	buttonLayout.setPadding(10);
	buttonLayout.setGutter(5);
	//buttonLayout.setBorder(2, TFT_WHITE);

	buttonLayout.addChild(playOrPause[0]);

	screenLayout.reflow();
	timeElapsedLayout.reflow();
	songNameLayout.reflow();
	buttonLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();

}

