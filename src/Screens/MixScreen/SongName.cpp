#include "SongName.h"
#include "../../Fonts.h"

MixScreen::SongName::SongName(ElementContainer *parent) : CustomElement(parent, 79, 10){

}

void MixScreen::SongName::draw(){

	auto canvas = getSprite();
	canvas->setFont(&u8g2_font_HelvetiPixel_tr);
	canvas->setTextColor(TFT_WHITE);



	if(scrolling){
		String temp = songName;
		while(scrollCursor + (nameLength - canvas->textWidth(temp.c_str())) < 0){
			temp.remove(0, 1);
		}
		int32_t correctedCursor = scrollCursor + (nameLength - canvas->textWidth(temp.c_str()));
		while(correctedCursor + canvas->textWidth(temp.c_str()) >= (int)(getWidth())){
			temp.remove(temp.length() - 1);
		}

		canvas->drawString(temp, getTotalX() + correctedCursor, getTotalY() - 6);

		temp = songName;
		if(scrollCursor + scrollOffset + nameLength > getWidth()) return;
		while((scrollCursor + scrollOffset + nameLength) + (nameLength - canvas->textWidth(temp.c_str())) < 0 && temp.length() > 0){
			temp.remove(0, 1);
		}
		correctedCursor = (scrollCursor + scrollOffset + nameLength) + (nameLength - canvas->textWidth(temp.c_str()));
		while(correctedCursor + canvas->textWidth(temp.c_str()) >= (int)(getWidth()) && temp.length() > 0){
			temp.remove(temp.length() - 1);
		}

		canvas->drawString(temp, getTotalX() + correctedCursor, getTotalY() - 6);
	}else{
		canvas->drawString(songName, getTotalX() + scrollCursor, getTotalY() - 6);
	}
}

bool MixScreen::SongName::checkScrollUpdate(){
	if(!scrolling) return false;

	if(currentTime + scrollSpeed <= millis()){
		currentTime = millis();
		scrollCursor-=1;
		if(scrollCursor <= -nameLength){
			scrollCursor = scrollOffset;
		}
		return true;
	}
}

void MixScreen::SongName::setSongName(const String& songName){
	SongName::songName = songName;

	auto canvas = getSprite();
	canvas->setFont(&u8g2_font_HelvetiPixel_tr);
	canvas->setTextColor(TFT_WHITE);

	nameLength = canvas->textWidth(songName.c_str());
	if(nameLength >= (getWidth() - 2)){
		scrolling = true;
		currentTime = millis();
		scrollCursor = 10;
	}else{
		scrolling = false;
		scrollCursor = ((int)(getWidth()) - 2 - nameLength) / 2;
	}
}
