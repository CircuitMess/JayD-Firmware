#include "SongName.h"

Playback::SongName::SongName(ElementContainer *parent) : CustomElement(parent,160,20){


}

void Playback::SongName::draw(){
	FontWriter u8f = getSprite()->startU8g2Fonts();
	u8f.setFont(u8g2_font_DigitalDisco_tf);
	u8f.setFontMode(1);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setCursor(getTotalX() + ((160 - u8f.getUTF8Width(songName.c_str())) / 2) , getTotalY() + 20);
	u8f.print(songName);
}

void Playback::SongName::setSongName(const String &songName){
	SongName::songName = songName;
}
