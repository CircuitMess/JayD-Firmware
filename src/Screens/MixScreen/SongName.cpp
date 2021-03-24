#include "SongName.h"

MixScreen::SongName::SongName(ElementContainer *parent, String songName) : CustomElement(parent, 5, 4),
																		   songName(songName){

}

void MixScreen::SongName::draw(){

	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);
	getSprite()->setCursor(getTotalX() + 13, getTotalY() - 12);
	getSprite()->println(songName);

}
