#include "SongName.h"

Playback::SongName::SongName(ElementContainer *parent) : CustomElement(parent,40,20){


}

void Playback::SongName::draw(){

	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(2);
	getSprite()->setCursor(getTotalX(), getTotalY() + 10);
	getSprite()->printCenter(songName);

}

void Playback::SongName::setSongName(const String &songName){
	SongName::songName = songName;
}
