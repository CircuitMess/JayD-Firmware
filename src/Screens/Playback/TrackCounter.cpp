#include "TrackCounter.hpp"

TrackCounter::TrackCounter(ElementContainer *parent) : CustomElement(parent, 10, 10){

}

void TrackCounter::checkIfPlaying(){
if(!pressed){

}else if(pressed){

}
}

void TrackCounter::counter(){
	if((millis()/1000)>=1){
		seconds++;
	}
	if(seconds>=60){
		seconds=0;
		minutes++;
	}
}

void TrackCounter::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setCursor(getTotalX(), getTotalY());
	getSprite()->print(minutes);
	getSprite()->print(":");
	getSprite()->print(seconds);
}