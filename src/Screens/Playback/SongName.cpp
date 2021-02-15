#include "SongName.h"

SongName::SongName(ElementContainer *parent) : CustomElement(parent,40,20){


}

void SongName::draw(){

	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->setTextFont(1);
	getSprite()->setCursor(getTotalX(), getTotalY());
	getSprite()->printCenter("CAVOGLAVE");

}