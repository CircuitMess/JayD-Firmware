#include "Effects.hpp"
#include "Bitmaps/highpass.hpp"
#include "Bitmaps/lowpass.hpp"
#include "Bitmaps/walkietalkie_fade_red.hpp"
#include "Bitmaps/speed.hpp"
#include "Bitmaps/reverb.hpp"
#include "Bitmaps/bitcrusher.hpp"

MixScreen::Effects::Effects(ElementContainer *parent, bool mirrored) : CustomElement(parent, 10, 10),
																	   mirrored(mirrored){

}

uint16_t *ikone[] = {lowpass, highpass, speed, reverb, bitcrusher};

void MixScreen::Effects::setEffect(Effect effect){
	if(effect == 0){
		Effects::effect = static_cast<Effect>(0);
	}else if(effect == 4 ){
		Effects::effect = static_cast<Effect>(4);
	}else if(effect > 0 && effect < 4){
		Effects::effect = effect;
	}
}

void MixScreen::Effects::setIntensity(uint8_t intensity){
	if(intensity <= 0){
		Effects::intensity = 0;
	}else if(intensity > 0 && intensity < 255){
		Effects::intensity = intensity;
	}else if(intensity >= 255){
		Effects::intensity = 255;
	}
}

void MixScreen::Effects::draw(){
	if(selected && !mirrored){
		getSprite()->fillRect(getTotalX(), getTotalY() + 25, 77, 18, C_RGB(248, 147, 121));
	}
	if(selected && mirrored){
		getSprite()->fillRect(getTotalX(), getTotalY() + 25, 77, 18, C_RGB(157, 194, 255));
	}
	if(!mirrored){
		getSprite()->drawIcon(ikone[effect], getTotalX(), getTotalY() + 25, 16, 16, 1, TFT_BLACK);
		getSprite()->fillRoundRect(getTotalX() + 25, getTotalY() + 30, (((float) intensity) / 255.0f) * 45.0f, 8, 5,
								   TFT_GREENYELLOW);
		getSprite()->drawRoundRect(getTotalX() + 25, getTotalY() + 30, 45, 8, 5, TFT_WHITE);
	}
	if(mirrored){
		getSprite()->drawIcon(ikone[effect], getTotalX() + 58, getTotalY() + 25, 16, 16, 1, TFT_BLACK);
		getSprite()->fillRoundRect(getTotalX() + 2, getTotalY() + 30, (((float) intensity) / 255.0f) * 45.0f, 8, 5,
								   TFT_GREENYELLOW);
		getSprite()->drawRoundRect(getTotalX() + 2, getTotalY() + 30, 45, 8, 5, TFT_WHITE);
	}


}

void MixScreen::Effects::setSelected(bool selected){
	Effects::selected = selected;
}
