#include "EffectElement.h"
#include "gif/lowpass.hpp"
#include "gif/highpass.hpp"
#include "gif/speed.hpp"
#include "gif/reverb.hpp"
#include "gif/walkietalkie.hpp"
#include "Bitmaps/lowpassFadeRed.hpp"
#include "Bitmaps/highpassFadeRed.hpp"
#include "Bitmaps/speedFadeRed.hpp"
#include "Bitmaps/reverbFadeRed.hpp"
#include "Bitmaps/bitcrusherFadeRed.hpp"
#include "Bitmaps/lowpassFadeBlue.hpp"
#include "Bitmaps/highpassFadeBlue.hpp"
#include "Bitmaps/speedFadeBlue.hpp"
#include "Bitmaps/bitcrusherFadeBlue.hpp"
#include "Bitmaps/reverbFadeBlue.hpp"

#include <FS.h>
#include <FS/PGMFile.h>


const uint16_t* const MixScreen::EffectElement::iconsNotMirrored[] = { nullptr, speed_fade_red, lowpass_fade, highpass_fade_red, reverb_fade_red, bitcrusher_fade_red };
const uint16_t* const MixScreen::EffectElement::iconsMirrored[] = { nullptr, speed_fade_blue, lowpass_fade_blue, highpass_fade_blue, reverb_fade_blue, bitcrusher_fade_blue };

const uint8_t* const MixScreen::EffectElement::gifIcons[]  =  { nullptr, mix_speed_gif, mix_lowpass_gif, mix_highpass_gif, mix_reverb_gif, mix_walkietalkie_gif };
const size_t MixScreen::EffectElement::gifIconSizes[] = { 0, sizeof(mix_speed_gif), sizeof(mix_lowpass_gif), sizeof(mix_highpass_gif), sizeof(mix_reverb_gif), sizeof(mix_walkietalkie_gif),  };

MixScreen::EffectElement::EffectElement(ElementContainer *parent, bool mirrored) : CustomElement(parent, 10, 10), mirrored(mirrored){

}

void MixScreen::EffectElement::setupGif(){
	if(gif == nullptr) return;

	gif->setMaskingColor(TFT_BLACK);
	gif->setLoop(true);
	gif->nextFrame();
	gif->setXY(getTotalX() + 61 * mirrored, getTotalY() + 25);
}

void MixScreen::EffectElement::draw(){
	if(effect != EffectType::NONE){
		if(selected){
			gif->nextFrame();
			gif->push();

			/*if(mirrored){
				getSprite()->fillRect(getTotalX(), getTotalY() + 25, 77, 18, C_RGB(157, 194, 255));
			}else{
				getSprite()->fillRect(getTotalX(), getTotalY() + 25, 77, 18, C_RGB(248, 147, 121));
			}*/

		}else{
			getSprite()->drawIcon((mirrored ? iconsMirrored : iconsNotMirrored)[effect], getTotalX() + 61 * mirrored, getTotalY() + 25, 16, 16, 1, TFT_BLACK);
		}
	}

	getSprite()->fillRoundRect(getTotalX() + 6 + 19 * !mirrored, getTotalY() + 30, (((float) intensity) / 255.0f) * 45.0f, 8, 3, TFT_GREENYELLOW);
	getSprite()->drawRoundRect(getTotalX() + 6 + 19 * !mirrored, getTotalY() + 30, 45, 8, 3, TFT_WHITE);
}

void MixScreen::EffectElement::repos(){
	CustomElement::repos();
	if(gif != nullptr){
		gif->setXY(getTotalX() + 61 * mirrored, getTotalY() + 25);
	}
}

void MixScreen::EffectElement::setType(EffectType effect){
	EffectElement::effect = effect;
	delete gif;
	gif = nullptr;

	if(effect != EffectType::NONE){
		gif = new AnimatedSprite(getSprite(), PGMFile::open(gifIcons[effect], gifIconSizes[effect]));
		setupGif();
	}
}


bool MixScreen::EffectElement::needsUpdate(){
	return selected && gif != nullptr && gif->checkFrame();
}

void MixScreen::EffectElement::setSelected(bool selected){
	EffectElement::selected = selected;

	if(selected && gif != nullptr){
		gif->reset();
	}
}

bool MixScreen::EffectElement::isSelected() const{
	return selected;
}

void MixScreen::EffectElement::setIntensity(uint8_t intensity){
	EffectElement::intensity = intensity;
}

uint8_t MixScreen::EffectElement::getIntensity() const{
	return intensity;
}

EffectType MixScreen::EffectElement::getType() const{
	return effect;
}
