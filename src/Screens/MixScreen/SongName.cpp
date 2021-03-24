#include "SongName.h"

MixScreen::SongName::SongName(ElementContainer *parent) : CustomElement(parent, 79, 10){

}

void MixScreen::SongName::draw(){
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);
	getSprite()->setTextWrap(false);
	if(scrolling){
		String temp = songName;
		while(scrollCursor + (nameLength - getSprite()->textWidth(temp)) < 0){
			temp.remove(0, 1);
		}
		int32_t correctedCursor = scrollCursor + (nameLength - getSprite()->textWidth(temp));
		while(correctedCursor + getSprite()->textWidth(temp) >= (int)(getWidth())){
			temp.remove(temp.length() - 1);
		}
		getSprite()->setCursor(getTotalX() + correctedCursor, getTotalY() - 12);
		getSprite()->print(temp);

		temp = songName;
		if(scrollCursor + 20 + nameLength > getWidth()) return;
		while((scrollCursor + 20 + nameLength) + (nameLength - getSprite()->textWidth(temp)) < 0 && temp.length() > 0){
			temp.remove(0, 1);
		}
		correctedCursor = (scrollCursor + 20 + nameLength) + (nameLength - getSprite()->textWidth(temp));
		while(correctedCursor + getSprite()->textWidth(temp) >= (int)(getWidth()) && temp.length() > 0){
			temp.remove(temp.length() - 1);
		}
		getSprite()->setCursor(getTotalX() + correctedCursor, getTotalY() - 12);
		getSprite()->print(temp);
	}else{
		getSprite()->setCursor(getTotalX() + scrollCursor, getTotalY() - 12);
		getSprite()->print(songName);
	}
}

bool MixScreen::SongName::checkScrollUpdate(){
	if(!scrolling) return false;

	if(currentTime + scrollSpeed <= millis()){
		currentTime = millis();
		scrollCursor-=1;
		if(scrollCursor <= -nameLength){
			scrollCursor = 20;
		}
		return true;
	}
}

void MixScreen::SongName::setSongName(const String& songName){
	SongName::songName = songName;

	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);
	nameLength = getSprite()->textWidth(songName);
	if(nameLength >= (getWidth() - 2)){
		scrolling = true;
		currentTime = millis();
		scrollCursor = 10;
	}else{
		scrolling = false;
		scrollCursor = ((int)(getWidth()) - 2 - nameLength) / 2;
	}
}
