#include "MainSeekBar.hpp"
#include "Bitmaps/pause dj.hpp"
#include "Bitmaps/play dj.hpp"

void MainSeekBar::setPressed(bool isPlaying){
	MainSeekBar::isPlaying = isPlaying;
}

void MainSeekBar::draw(){
	if(isPlaying)
		getSprite()->drawIcon(pause_dj, 20, 15, 5, 6, 1, TFT_BLACK);
	if(!isPlaying){
		getSprite()->drawIcon(play_dj, 20, 15, 5, 6, 1, TFT_BLACK);
	}
}
