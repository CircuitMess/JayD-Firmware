#include "Effects.hpp"
#include "Bitmaps/high_pass.hpp"
#include "Bitmaps/low_pass.hpp"
#include "Bitmaps/speed.hpp"
#include "Bitmaps/reverb.hpp"
#include "Bitmaps/bitcrusher.hpp"

MixScreen::Effects::Effects(ElementContainer *parent,bool mirrored): CustomElement(parent, 10, 10){

}
uint16_t *ikone[]={low_pass, high_pass, speed,
				   reverb, bitcrusher};

void MixScreen::Effects::setEffect(Effect effect){
	Effects::effect = effect;
}

void MixScreen::Effects::setIntensity(uint8_t intensity){
	Effects::intensity = intensity;
}

void MixScreen::Effects::draw(){
	if(!mirrored){
	//getSprite()->drawIcon(ikone[effect], getTotalX(), getTotalY()+50, 17, 15, 1, TFT_BLACK);
	getSprite()->fillRoundRect(getTotalX()+25,getTotalY()+30,35,8,5,TFT_GREENYELLOW);
	getSprite()->drawRoundRect(getTotalX()+25,getTotalY()+30,45,8,5,TFT_WHITE);
	}
	if(mirrored){
		getSprite()->fillRoundRect(getTotalX(),getTotalY(),35,8,5,TFT_WHITE);
		getSprite()->drawRoundRect(getTotalX(),getTotalY(),45,8,5,TFT_WHITE);
	}


}