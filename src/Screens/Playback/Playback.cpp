#include <InputJayD.h>
#include "Playback.h"

Playback *Playback::instance = nullptr;

Playback::Playback(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
									   songNameLayout(&screenLayout, HORIZONTAL),
									   timeElapsedLayout(&screenLayout, HORIZONTAL), buttonLayout(
				&screenLayout, HORIZONTAL){

	for(int i = 0; i < 1; i++){
		song.push_back(new SongName(&songNameLayout));
	}

	for(int i = 0; i < 1; i++){
		playOrPause.push_back(new Buttons(&buttonLayout));
	}

	instance = this;
	buildUI();

}

void Playback::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setBtnPressCallback(2, [](){
		instance->playOrPause[0]->checkIfPlayed();
		instance->draw();
		instance->screen.commit();
	});
}

void Playback::stop(){

}

void Playback::draw(){
	screen.getSprite()->clear(TFT_DARKGREY);
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
	songNameLayout.setBorder(2, TFT_WHITE);

	for(int i = 0; i < song.size(); i++){
		songNameLayout.addChild(song[i]);
	}

	timeElapsedLayout.setWHType(PARENT, FIXED);
	timeElapsedLayout.setHeight(20);
	timeElapsedLayout.setGutter(5);
	timeElapsedLayout.setPadding(10);
	timeElapsedLayout.setBorder(2, TFT_WHITE);

	buttonLayout.setWHType(PARENT, FIXED);
	buttonLayout.setHeight(62);
	buttonLayout.setPadding(10);
	buttonLayout.setGutter(5);
	buttonLayout.setBorder(2, TFT_WHITE);

	for(int i = 0; i < playOrPause.size(); i++){
		buttonLayout.addChild(playOrPause[i]);
	}

	screenLayout.reflow();
	timeElapsedLayout.reflow();
	songNameLayout.reflow();
	buttonLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();

}

