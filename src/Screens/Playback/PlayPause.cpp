#include "PlayPause.h"
#include <FS/CompressedFile.h>
#include <SPIFFS.h>


Playback::PlayPause::PlayPause(ElementContainer *parent) : CustomElement(parent, 160, 10){
	fs::File fw = SPIFFS.open("/fw.raw");
	fs::File rew = SPIFFS.open("/rew.raw");
	fs::File play = SPIFFS.open("/play.raw");
	fs::File pause = SPIFFS.open("/pause.raw");
	fs::File picture[]={};
	picture[0] = fw;
	picture[1]=rew;
	picture[2]=play;
	picture[3]=pause;
	for(int i=0;i<4;i++){
		buffer[i] = static_cast<Color *>(ps_malloc(160 * 128 * 2));
		if(buffer[i] == nullptr){
			Serial.println("PlayPause pictures unpack error");
			return;
		}
		picture[i].seek(0);
		picture[i].read(reinterpret_cast<uint8_t *>(buffer[i]), 160 * 128 * 2);
	}
}
Playback::PlayPause::~PlayPause(){
	for(int i=0;i<4;i++){
		free(buffer[i]);
	}
}

void Playback::PlayPause::togglePlaying(){
	playing = !playing;
}

void Playback::PlayPause::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->drawCircle(78, 93, 17, TFT_WHITE);
	if(playing){
		getSprite()->drawIcon(buffer[2], getTotalX() + getWidth() / 2 - 7, getTotalY() + 19, 14, 18, 1, TFT_BLACK);
	}else if(!playing){
		getSprite()->drawIcon(buffer[3], getTotalX() + getWidth() / 2 - 9, getTotalY() + 19, 14, 18, 1, TFT_BLACK);
	}
	getSprite()->drawIcon(buffer[1], getTotalX() + getWidth() / 2 - 35, getTotalY() + 25, 10, 8, 1, TFT_BLACK);
	getSprite()->drawIcon(buffer[0], getTotalX() + getWidth() / 2 + 22, getTotalY() + 25, 10, 8, 1, TFT_BLACK);
}

void Playback::PlayPause::setPlaying(bool playing){
	PlayPause::playing = playing;
}


