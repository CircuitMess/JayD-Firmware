#include "TrackCounter.h"

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
	FontWriter u8f = getSprite()->startU8g2Fonts();
	u8f.setCursor(getTotalX()+10, getTotalY() + 5);
	u8f.setFont(u8g2_font_HelvetiPixel_tr);
	u8f.setFontMode(1);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.printf("%02d:%02d", currentDuration / 60, currentDuration - (currentDuration / 60) * 60);
	u8f.setCursor(getTotalX() + 117, getTotalY() + 5);
	u8f.printf("%02d:%02d", totalDuration / 60, totalDuration - (totalDuration / 60) * 60);

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



