#include "LeftSeekBar.hpp"
#include "Bitmaps/pause dj.hpp"
#include "Bitmaps/play dj.hpp"

void LeftSeekBar::setPressed(bool playing){
	LeftSeekBar::playing = playing;
}

void LeftSeekBar::draw(){
	if(playing)
		getSprite()->drawIcon(pause_dj, 20, 15, 5, 6, 1, TFT_BLACK);
	if(!playing){
		getSprite()->drawIcon(play_dj, 20, 15, 5, 6, 1, TFT_BLACK);
	}
	getSprite()->setCursor(getTotalX(), getTotalY()+15);
	getSprite()->printf("%02d:%02d", currentDuration/60, currentDuration - (currentDuration / 60)*60);
	getSprite()->setCursor(getTotalX()+60, getTotalY()+15);
	getSprite()->printf("%02d:%02d", totalDuration / 60, totalDuration - (totalDuration / 60)*60);

	getSprite()->fillRoundRect(20,20,70,10,2,TFT_BLACK);
	getSprite()->fillRoundRect(20,20,30,10,5,TFT_GREEN);
	getSprite()->drawRoundRect(20,20,70,10,2,TFT_WHITE);

}
void LeftSeekBar::setTotalDuration(int totalDuration){
	LeftSeekBar::totalDuration = totalDuration;
}

void LeftSeekBar::setCurrentDuration(int currentDuration){
	LeftSeekBar::currentDuration = currentDuration;
}