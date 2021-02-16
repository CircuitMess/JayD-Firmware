#include "TrackCounter.hpp"

TrackCounter::TrackCounter(ElementContainer *parent) : CustomElement(parent, 10, 10){

}


void TrackCounter::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setCursor(getTotalX(), getTotalY());
	getSprite()->fillRoundRect(getTotalX(), getTotalY() + 10, currentDuration, 8, 5, TFT_GREENYELLOW);
	getSprite()->drawRoundRect(getTotalX(), getTotalY() + 10, 140, 8, 5, TFT_WHITE);
	getSprite()->printf("%02d:%02d", currentDuration/60, currentDuration - (currentDuration / 60)*60);
	getSprite()->setCursor(getTotalX() + 110, getTotalY());
	getSprite()->printf("%02d:%02d", totalDuration / 60, totalDuration - (totalDuration / 60)*60);

}

void TrackCounter::setTotalDuration(int totalDuration){
	TrackCounter::totalDuration = totalDuration;
}

void TrackCounter::setCurrentDuration(int currentDuration){
	TrackCounter::currentDuration = currentDuration;
}


