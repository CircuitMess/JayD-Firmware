#include "TrackCounter.h"
#include "../../Fonts.h"

Playback::TrackCounter::TrackCounter(ElementContainer *parent) : CustomElement(parent, 10, 10){

}


void Playback::TrackCounter::draw(){
	float fillSeekBar;
	if(currentDuration==0){
		fillSeekBar=0;
	}
	else {
		fillSeekBar=(((float) currentDuration) / (float)totalDuration ) * 140.0f;
	}

	getSprite()->fillRoundRect(getTotalX()+10, getTotalY() + 10, fillSeekBar, 8, 3, TFT_GREENYELLOW);
	getSprite()->drawRoundRect(getTotalX()+10, getTotalY() + 10, 140, 8, 3, TFT_WHITE);
	auto canvas = getSprite();
	canvas->setFont(&u8g2_font_HelvetiPixel_tr);
	canvas->setTextColor(TFT_WHITE);
	canvas->setTextDatum(BL_DATUM);

	char buf[50];
	sprintf(buf, "%02d:%02d", currentDuration / 60, currentDuration - (currentDuration / 60) * 60);
	canvas->drawString(buf, getTotalX()+10, getTotalY() + 5);

	sprintf(buf, "%02d:%02d", totalDuration / 60, totalDuration - (totalDuration / 60) * 60);
	canvas->drawString(buf, getTotalX() + 117, getTotalY() + 5);
	canvas->setTextDatum(TL_DATUM);

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

int Playback::TrackCounter::getCurrentDuration() const{
	return currentDuration;
}

int Playback::TrackCounter::getTotalDuration() const {
	return totalDuration;
}



