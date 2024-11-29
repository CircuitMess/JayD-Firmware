#include "ListItem.h"
#include "../../Fonts.h"

SongList::ListItem::ListItem(ElementContainer *parent, String songName) : CustomElement(parent, 150, 15), songName(songName), path(songName){

	this->songName = songName.substring(songName.lastIndexOf('/') + 1, songName.lastIndexOf('.'));

	auto canvas = getSprite();
	canvas->setFont(&u8g2_font_profont12_tf);
	canvas->setTextColor(TFT_WHITE);

	nameLength = canvas->textWidth(this->songName.c_str());
	scrollCursor = 2;
	if(nameLength >= (getWidth() - 4)){
		scrolling = true;
		currentTime = millis();
	}else{
		scrolling = false;
	}
}

void SongList::ListItem::draw(){
	auto canvas = getSprite();

	canvas->setFont(&u8g2_font_profont12_tf);
	canvas->setTextColor(TFT_WHITE);
	canvas->setTextDatum(CL_DATUM);

	if(selected) {
		getSprite()->drawRect(getTotalX() - 2, getTotalY() - 4, getWidth() + 4, getHeight() + 6, TFT_LIGHTGREY);//treba ubaciti ikonicu za scrolanje
	}
	if(scrolling && selected){
		String temp = songName;
		while(scrollCursor + (nameLength - canvas->textWidth(temp.c_str())) < 2){
			temp.remove(0, 1);
		}
		int32_t correctedCursor = scrollCursor + (nameLength - canvas->textWidth(temp.c_str()));
		while(correctedCursor + canvas->textWidth(temp.c_str()) >= (int)(getWidth() - 2)){
			temp.remove(temp.length() - 1);
		}
		canvas->drawString(temp, getTotalX() + correctedCursor, getTotalY() + 9);

		temp = songName;
		if(scrollCursor + scrollOffset + nameLength > getWidth() - 2) return;
		while((scrollCursor + scrollOffset + nameLength) + (nameLength - canvas->textWidth(temp.c_str())) < 2 && temp.length() > 0){
			temp.remove(0, 1);
		}
		correctedCursor = (scrollCursor + scrollOffset + nameLength) + (nameLength - canvas->textWidth(temp.c_str()));
		while(correctedCursor + canvas->textWidth(temp.c_str()) >= (int)(getWidth() - 2) && temp.length() > 0){
			temp.remove(temp.length() - 1);
		}

		canvas->drawString(temp, getTotalX() + correctedCursor, getTotalY() + 9);
	}else{
		canvas->drawString(songName, getTotalX() + scrollCursor, getTotalY() + 9);
	}
}

void SongList::ListItem::setSelected(bool selected){
	ListItem::selected = selected;
	if(!scrolling) return;
	scrollCursor = 2;
	if(selected) {
		currentTime = millis();
	}
}

const String& SongList::ListItem::getName() const{
	return songName;
}

bool SongList::ListItem::checkScrollUpdate(){
	if(!(scrolling && selected)) return false;

	if(currentTime + scrollSpeed <= millis()){
		currentTime = millis();
		scrollCursor-=1;
		if(scrollCursor <= -nameLength + 2){
			scrollCursor = scrollOffset;
		}
		return true;
	}
}

const String& SongList::ListItem::getPath() const{
	return path;
}
