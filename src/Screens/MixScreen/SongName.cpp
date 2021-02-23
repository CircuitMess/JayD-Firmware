#include "SongName.hpp"

MixScreen::SongName::SongName(ElementContainer *parent) : CustomElement(parent,5,4){

}

void MixScreen::SongName::draw(){

	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);
	getSprite()->setCursor(getTotalX()+20, getTotalY()-12);
	getSprite()->println("PJESMA");

}
