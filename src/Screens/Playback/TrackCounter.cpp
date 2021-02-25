#include "TrackCounter.hpp"

Playback::TrackCounter::TrackCounter(ElementContainer *parent) : CustomElement(parent, 10, 10){

}


void Playback::TrackCounter::draw(){
	float fillSeekBar;
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setCursor(getTotalX()+10, getTotalY());
	if(currentDuration==0){
		fillSeekBar=0;
	}
	else {
		fillSeekBar=(((float) currentDuration) / (float)totalDuration ) * 140.0f;
	}
	getSprite()->fillRoundRect(getTotalX()+10, getTotalY() + 10, fillSeekBar, 8, 5, TFT_GREENYELLOW);
	getSprite()->drawRoundRect(getTotalX()+10, getTotalY() + 10, 140, 8, 5, TFT_WHITE);
	getSprite()->printf("%02d:%02d", currentDuration / 60, currentDuration - (currentDuration / 60) * 60);
	getSprite()->setCursor(getTotalX() + 117, getTotalY());
	getSprite()->printf("%02d:%02d", totalDuration / 60, totalDuration - (totalDuration / 60) * 60);

}

void Playback::TrackCounter::setTotalDuration(int totalDuration){
	TrackCounter::totalDuration = totalDuration;
}

void Playback::TrackCounter::setCurrentDuration(int currentDuration){
	if(currentDuration < 0){
		TrackCounter::currentDuration = 0;
	}else if(currentDuration > totalDuration){
		TrackCounter::currentDuration = totalDuration;
	}else{
		TrackCounter::currentDuration = currentDuration;
	}
}



