#include "SongSeekBar.h"
#include "Bitmaps/pause_dj.hpp"
#include "Bitmaps/play_dj.hpp"

MixScreen::SongSeekBar::SongSeekBar(ElementContainer *parent): CustomElement(parent, 10, 10){

}


void MixScreen::SongSeekBar::setPlaying(bool playing){
	SongSeekBar::playing = playing;
}

void MixScreen::SongSeekBar::draw(){
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);

	if(playing)
		getSprite()->drawIcon(pause_dj, getTotalX()+35, getTotalY()+26, 5, 6, 1, TFT_BLACK);
	if(!playing){
		getSprite()->drawIcon(play_dj, getTotalX()+35, getTotalY()+26, 5, 6, 1, TFT_BLACK);
	}

	getSprite()->setCursor(getTotalX(), getTotalY()+25);
	getSprite()->printf("%02d:%02d", currentDuration/60, currentDuration - (currentDuration / 60)*60);
	getSprite()->setCursor(getTotalX()+45, getTotalY()+25);
	getSprite()->printf("%02d:%02d", totalDuration / 60, totalDuration - (totalDuration / 60)*60);

	getSprite()->fillRoundRect(getTotalX()+2,getTotalY()+40,72,10,2,TFT_BLACK);
	getSprite()->fillRect(getTotalX()+2,getTotalY()+44,72,2,C_RGB(4,211,35));
	getSprite()->drawRoundRect(getTotalX()+2,getTotalY()+40,72,10,2,TFT_WHITE);
	getSprite()->fillRect(getTotalX()+currentDuration,getTotalY()+41,4,7,TFT_WHITE);
	getSprite()->drawRect(getTotalX()+currentDuration,getTotalY()+41,4,7,TFT_BLACK);

}
void MixScreen::SongSeekBar::setTotalDuration(int totalDuration){
	SongSeekBar::totalDuration = totalDuration;
}

void MixScreen::SongSeekBar::setCurrentDuration(int currentDuration){
	if(currentDuration<totalDuration){
		SongSeekBar::currentDuration = currentDuration;
	}else if (currentDuration>=totalDuration){
		SongSeekBar::currentDuration = totalDuration;

	}
}