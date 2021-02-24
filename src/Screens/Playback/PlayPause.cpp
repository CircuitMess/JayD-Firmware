#include "PlayPause.h"
#include "Bitmaps/play.hpp"
#include "Bitmaps/fw.hpp"
#include "Bitmaps/rew.hpp"
#include "Bitmaps/pause.hpp"

Playback::PlayPause::PlayPause(ElementContainer *parent) : CustomElement(parent, 10, 10){

}

void Playback::PlayPause::togglePlaying(){
	if(!playing){
		playing = true;
	}else if(playing){
		playing = false;
	}
}

void Playback::PlayPause::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->drawCircle(78, 93, 17, TFT_WHITE);
	if(playing){
		getSprite()->drawIcon(play, 73, 84, 14, 18, 1, TFT_BLACK);
	}else if(!playing){
		getSprite()->drawIcon(pauza, 72, 84, 14, 18, 1, TFT_BLACK);
	}
	getSprite()->drawIcon(rew, 44, 90, 10, 8, 1, TFT_BLACK);
	getSprite()->drawIcon(fw, 102, 90, 10, 8, 1, TFT_BLACK);
}

void Playback::PlayPause::setPlaying(bool playing){
	PlayPause::playing = playing;
}
