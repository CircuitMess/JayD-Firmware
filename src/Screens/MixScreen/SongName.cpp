#include "SongName.h"

MixScreen::SongName::SongName(ElementContainer *parent) : CustomElement(parent, 79, 10){

}

void MixScreen::SongName::draw(){
	FontWriter u8f = getSprite()->startU8g2Fonts();
	u8f.setFont(u8g2_font_HelvetiPixel_tr);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setFontMode(1);
	if(scrolling){
		String temp = songName;
		while(scrollCursor + (nameLength - u8f.getUTF8Width(temp.c_str())) < 0){
			temp.remove(0, 1);
		}
		int32_t correctedCursor = scrollCursor + (nameLength - u8f.getUTF8Width(temp.c_str()));
		while(correctedCursor + u8f.getUTF8Width(temp.c_str()) >= (int)(getWidth())){
			temp.remove(temp.length() - 1);
		}
		u8f.setCursor(getTotalX() + correctedCursor, getTotalY() - 6);
		u8f.print(temp);

		temp = songName;
		if(scrollCursor + 20 + nameLength > getWidth()) return;
		while((scrollCursor + 20 + nameLength) + (nameLength - u8f.getUTF8Width(temp.c_str())) < 0 && temp.length() > 0){
			temp.remove(0, 1);
		}
		correctedCursor = (scrollCursor + 20 + nameLength) + (nameLength - u8f.getUTF8Width(temp.c_str()));
		while(correctedCursor + u8f.getUTF8Width(temp.c_str()) >= (int)(getWidth()) && temp.length() > 0){
			temp.remove(temp.length() - 1);
		}
		u8f.setCursor(getTotalX() + correctedCursor, getTotalY() - 6);
		u8f.print(temp);
	}else{
		u8f.setCursor(getTotalX() + scrollCursor, getTotalY() - 6);
		u8f.print(songName);
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

	FontWriter u8f = getSprite()->startU8g2Fonts();
	u8f.setFont(u8g2_font_HelvetiPixel_tr);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setFontMode(1);
	nameLength = u8f.getUTF8Width(songName.c_str());
	if(nameLength >= (getWidth() - 2)){
		scrolling = true;
		currentTime = millis();
		scrollCursor = 10;
	}else{
		scrolling = false;
		scrollCursor = ((int)(getWidth()) - 2 - nameLength) / 2;
	}
}
