#include "Effects.hpp"
#include "Bitmaps/highpass.hpp"
#include "Bitmaps/lowpass.hpp"
#include "Bitmaps/walkietalkie_fade_red.hpp"

MixScreen::Effects::Effects(ElementContainer *parent, bool mirrored) : CustomElement(parent, 10, 10), mirrored(mirrored){

}

uint16_t *ikone[] = {lowpass, highpass ,walkietalkie_fade_red };

void MixScreen::Effects::setEffect(Effect effect){
	Effects::effect = effect;
}

void MixScreen::Effects::setIntensity(uint8_t intensity){
	Effects::intensity = intensity;
}

void MixScreen::Effects::draw(){
	if(!mirrored){
		getSprite()->drawIcon(ikone[effect], getTotalX(), getTotalY()+25, 16, 16, 1, TFT_BLACK);
		getSprite()->fillRoundRect(getTotalX() + 25, getTotalY() + 30, 35, 8, 5, TFT_GREENYELLOW);
		getSprite()->drawRoundRect(getTotalX() + 25, getTotalY() + 30, 45, 8, 5, TFT_WHITE);
	}
	if(mirrored){
		getSprite()->drawIcon(ikone[effect], getTotalX()+58, getTotalY()+25, 16, 16, 1, TFT_BLACK);
		getSprite()->fillRoundRect(getTotalX()+2, getTotalY() + 30, 35, 8, 5, TFT_GREENYELLOW);
		getSprite()->drawRoundRect(getTotalX()+2, getTotalY() + 30, 45, 8, 5, TFT_WHITE);
	}


}