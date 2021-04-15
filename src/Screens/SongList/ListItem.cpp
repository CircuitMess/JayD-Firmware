#include "ListItem.h"

SongList::ListItem::ListItem(ElementContainer *parent, String songName) : CustomElement(parent, 150, 15), songName(songName), path(songName){

	this->songName = songName.substring(songName.lastIndexOf('/') + 1, songName.lastIndexOf('.'));

	FontWriter u8f = getSprite()->startU8g2Fonts();
	u8f.setFont(u8g2_font_profont12_tf);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setFontMode(1);
	nameLength = u8f.getUTF8Width(this->songName.c_str());
	scrollCursor = 2;
	if(nameLength >= (getWidth() - 4)){
		scrolling = true;
		currentTime = millis();
	}else{
		scrolling = false;
	}
}

void SongList::ListItem::draw(){
	FontWriter u8f = getSprite()->startU8g2Fonts();

	u8f.setFont(u8g2_font_profont12_tf);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setFontMode(1);
	if(selected) {
		getSprite()->drawRect(getTotalX() - 2, getTotalY() - 4, getWidth() + 4, getHeight() + 6, TFT_LIGHTGREY);//treba ubaciti ikonicu za scrolanje
	}
	if(scrolling && selected){
		String temp = songName;
		while(scrollCursor + (nameLength - u8f.getUTF8Width(temp.c_str())) < 2){
			temp.remove(0, 1);
		}
		int32_t correctedCursor = scrollCursor + (nameLength - u8f.getUTF8Width(temp.c_str()));
		while(correctedCursor + u8f.getUTF8Width(temp.c_str()) >= (int)(getWidth() - 2)){
			temp.remove(temp.length() - 1);
		}
		u8f.setCursor(getTotalX() + correctedCursor, getTotalY() + 9);
		u8f.print(temp);

		temp = songName;
		if(scrollCursor + scrollOffset + nameLength > getWidth() - 2) return;
		while((scrollCursor + scrollOffset + nameLength) + (nameLength - u8f.getUTF8Width(temp.c_str())) < 2 && temp.length() > 0){
			temp.remove(0, 1);
		}
		correctedCursor = (scrollCursor + scrollOffset + nameLength) + (nameLength - u8f.getUTF8Width(temp.c_str()));
		while(correctedCursor + u8f.getUTF8Width(temp.c_str()) >= (int)(getWidth() - 2) && temp.length() > 0){
			temp.remove(temp.length() - 1);
		}
		u8f.setCursor(getTotalX() + correctedCursor, getTotalY() + 9);
		u8f.print(temp);
	}else{
		u8f.setCursor(getTotalX() + scrollCursor, getTotalY() + 9);
		u8f.print(songName);
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
