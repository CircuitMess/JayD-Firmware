#include "PlayPause.h"
#include "Bitmaps/play.hpp"
#include "Bitmaps/fw.hpp"
#include "Bitmaps/rew.hpp"
#include "Bitmaps/pause.hpp"

Playback::PlayPause::PlayPause(ElementContainer *parent) : CustomElement(parent, 10, 10){

}

void Playback::PlayPause::togglePlaying(){
	playing=!playing;
}

void Playback::PlayPause::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->drawCircle(78, 93, 17, TFT_WHITE);
	getSprite()->drawIcon(playing ? play : pauza, getTotalX()+72, getTotalY()+84, 14, 18, 1, TFT_BLACK);
	getSprite()->drawIcon(rew, getTotalX()+44, getTotalY()+90, 10, 8, 1, TFT_BLACK);
	getSprite()->drawIcon(fw, getTotalX()+102, getTotalY()+90, 10, 8, 1, TFT_BLACK);
}

void Playback::PlayPause::setPlaying(bool playing){
	PlayPause::playing = playing;
}
