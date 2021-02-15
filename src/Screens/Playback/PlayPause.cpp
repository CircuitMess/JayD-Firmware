#include "PlayPause.h"
#include "Bitmaps/play.hpp"
#include "Bitmaps/fw.hpp"
#include "Bitmaps/rew.hpp"
#include "Bitmaps/pause.hpp"

PlayPause::PlayPause(ElementContainer *parent) : CustomElement(parent, 10, 10){

}

void PlayPause::checkIfPlaying(){
	if(!pressed){
		pressed = true;
	}else if(pressed){
		pressed = false;
	}
}

void PlayPause::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->drawCircle(78, 93, 17, TFT_WHITE);
	if(pressed){
		getSprite()->drawIcon(play, 73, 84, 14, 18, 1, TFT_BLACK);
	}else if(!pressed){
		getSprite()->drawIcon(pauza, 72, 84, 14, 18, 1, TFT_BLACK);
	}
	getSprite()->drawIcon(rew, 44, 90, 10, 8, 1, TFT_BLACK);
	getSprite()->drawIcon(fw, 102, 90, 10, 8, 1, TFT_BLACK);
}