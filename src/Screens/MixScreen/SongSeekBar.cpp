#include "SongSeekBar.h"


MixScreen::SongSeekBar::SongSeekBar(ElementContainer *parent) : CustomElement(parent, 10, 10){
	fs::File picture[2]={
			SPIFFS.open("/pause_dj.raw"),
			SPIFFS.open("/play_dj.raw")
	};
	for(int i=0;i<2;i++){

		buffer[i] = static_cast<Color *>(ps_malloc(5*6*2));
		if(buffer[i] == nullptr){
			Serial.println("SongSeekBar picture unpack error");
			return;
		}
		picture[i].seek(0);
		picture[i].read(reinterpret_cast<uint8_t *>(buffer[i]), (5*6*2));
	}
}


void MixScreen::SongSeekBar::setPlaying(bool playing){
	SongSeekBar::playing = playing;
}

void MixScreen::SongSeekBar::draw(){
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setTextFont(1);

	getSprite()->drawIcon(playing ? buffer[0]:buffer[1], getTotalX() + 35, getTotalY() + 26, 5, 6, 1, TFT_BLACK);

	getSprite()->setCursor(getTotalX(), getTotalY() + 25);
	getSprite()->printf("%02d:%02d", currentDuration / 60, currentDuration - (currentDuration / 60) * 60);
	getSprite()->setCursor(getTotalX() + 45, getTotalY() + 25);
	getSprite()->printf("%02d:%02d", totalDuration / 60, totalDuration - (totalDuration / 60) * 60);

	getSprite()->fillRoundRect(getTotalX() + 2, getTotalY() + 40, 72, 10, 2, TFT_BLACK);
	getSprite()->fillRect(getTotalX() + 2, getTotalY() + 44, 72, 2, C_RGB(4, 211, 35));
	getSprite()->drawRoundRect(getTotalX() + 2, getTotalY() + 40, 72, 10, 2, TFT_WHITE);
	if(currentDuration==0){
		movingCursor=0;
	}else{
		movingCursor=((float) currentDuration / (float) totalDuration) * 66.0f;
	}
	getSprite()->fillRect(getTotalX()+3 + movingCursor, getTotalY() + 41, 4,
						  8, TFT_WHITE);
	getSprite()->drawRect(getTotalX()+3 + movingCursor, getTotalY() + 41, 4,
						  8, TFT_BLACK);

}

void MixScreen::SongSeekBar::setTotalDuration(int totalDuration){
	SongSeekBar::totalDuration = totalDuration;
}

void MixScreen::SongSeekBar::setCurrentDuration(int currentDuration){
	if(currentDuration < 0){
		SongSeekBar::currentDuration = 0;
	}else if(currentDuration > totalDuration){
		SongSeekBar::currentDuration = totalDuration;
	}else{
		SongSeekBar::currentDuration = currentDuration;
	}
}

int MixScreen::SongSeekBar::getCurrentDuration() const{
	return currentDuration;
}

int MixScreen::SongSeekBar::getTotalDuration() const{
	return totalDuration;
}

MixScreen::SongSeekBar::~SongSeekBar(){
for(int i=0;i<2;i++){
	free(buffer[i]);
}
}
