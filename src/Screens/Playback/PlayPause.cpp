#include "PlayPause.h"
#include "Bitmaps/play.hpp"
#include "Bitmaps/fw.hpp"
#include "Bitmaps/rew.hpp"
#include "Bitmaps/pause.hpp"

Playback::PlayPause::PlayPause(ElementContainer *parent) : CustomElement(parent, 160, 10){

}

void Playback::PlayPause::togglePlaying(){
	playing=!playing;
}

void Playback::PlayPause::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->drawCircle(78, 93, 17, TFT_WHITE);
	if(playing){
		getSprite()->drawIcon(play, getTotalX() + getWidth() / 2 - 7, getTotalY()+19, 14, 18, 1, TFT_BLACK);
	}else if(!playing){
		getSprite()->drawIcon(pauza, getTotalX() + getWidth() / 2 - 9, getTotalY()+19, 14, 18, 1, TFT_BLACK);
	}
	getSprite()->drawIcon(rew, getTotalX()+ getWidth() / 2-35, getTotalY()+25, 10, 8, 1, TFT_BLACK);
	getSprite()->drawIcon(fw, getTotalX()+ getWidth() / 2+22, getTotalY()+25, 10, 8, 1, TFT_BLACK);
}

void Playback::PlayPause::setPlaying(bool playing){
	PlayPause::playing = playing;
}
