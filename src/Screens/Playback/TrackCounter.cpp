#include "TrackCounter.hpp"

TrackCounter::TrackCounter(ElementContainer *parent) : CustomElement(parent, 10, 10){

}

void TrackCounter::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setCursor(getTotalX(), getTotalY());
	getSprite()->drawRoundRect(getTotalX(),getTotalY()+10,140,8,5,TFT_WHITE);
	getSprite()->fillRoundRect(getTotalX(),getTotalY()+10,20,8,5,TFT_GREENYELLOW);
	getSprite()->print("00:19");
	getSprite()->setCursor(getTotalX()+110, getTotalY());
	getSprite()->print("03:34");

}