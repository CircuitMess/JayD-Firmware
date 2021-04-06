#include "PlayPause.h"
#include <FS/CompressedFile.h>
#include <SPIFFS.h>


Playback::PlayPause::PlayPause(ElementContainer *parent) : CustomElement(parent, 160, 10){
	fs::File fw = SPIFFS.open("/data/playback/playPausePictures/fw.raw");
	fs::File rew = SPIFFS.open("/data/playback/playPausePictures/rew.raw");
	fs::File play = SPIFFS.open("/data/playback/playPausePictures/play.raw");
	fs::File pause = SPIFFS.open("/data/playback/playPausePictures/pause.raw");
	fs::File picture[]={};
	picture[0] = fw;
	picture[1]=rew;
	picture[2]=play;
	picture[3]=pause;
	for(int i=0;i<4;i++){
		buffer[i] = static_cast<Color *>(ps_malloc((i==0 || i==1)?(10*8*2):(14*18*2)));
		if(buffer[i] == nullptr){
			Serial.println("PlayPause pictures unpack error");
			return;
		}
		picture[i].seek(0);
		picture[i].read(reinterpret_cast<uint8_t *>(buffer[i]), (i==0 || i==1)?(10*8*2):(14*18*2));
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


