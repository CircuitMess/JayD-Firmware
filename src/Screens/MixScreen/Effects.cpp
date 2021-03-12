#include "Effects.hpp"
#include "Bitmaps/bitcrusherFadeBlue.h"
#include "Bitmaps/bitcrusherFadeRed.h"
#include "Bitmaps/highpassFadeRed.h"
#include "Bitmaps/highpassFadeBlue.h"
#include "Bitmaps/lowpassFadeRed.h"
#include "Bitmaps/lowpassFadeBlue.h"
#include "Bitmaps/reverbFadeBlue.h"
#include "Bitmaps/reverbFadeRed.h"
#include "Bitmaps/speedFadeBlue.h"
#include "Bitmaps/speedFadeRed.h"
#include "Bitmaps/highpassGIF.h"
#include "Bitmaps/bitcrusherGIF.h"
#include "Bitmaps/lowpassGIF.h"
#include "Bitmaps/reverbGIF.h"
#include "Bitmaps/speedGIF.h"
#include <FS.h>
#include <Display/PGMFile.h>


uint16_t *iconsNotMirrored[] = {lowpass_fade, highpass_fade_red, speed_fade_red, reverb_fade_red, bitcrusher_fade_red};

uint16_t *iconsMirrored[] = {lowpass_fade_blue, highpass_fade_blue, speed_fade_blue, reverb_fade_blue, bitcrusher_fade_blue};

unsigned char *gifIcons[] = {lowpass_gif, highpass_gif, speed_gif, reverb_gif, bitcrusher_gif};

size_t gifIconsSize[] = {sizeof(lowpass_gif), sizeof(highpass_gif), sizeof(speed_gif), sizeof(reverb_gif), sizeof(bitcrusher_gif)};


MixScreen::Effects::Effects(ElementContainer *parent, bool mirrored, Effect effect) : CustomElement(parent, 10, 10), effect(effect),
																					  mirrored(mirrored){
	gif = new AnimatedSprite(getSprite(), fs::File(std::make_shared<PGMFile>(gifIcons[effect], gifIconsSize[effect])));
}


void MixScreen::Effects::setEffect(Effect effect){
	Effects::effect = effect;
	delete gif;
	gif = new AnimatedSprite(getSprite(), fs::File(std::make_shared<PGMFile>(gifIcons[effect], gifIconsSize[effect])));
}

void MixScreen::Effects::setIntensity(uint8_t intensity){
	Effects::intensity = intensity;
}

void MixScreen::Effects::draw(){
	if(selected && !mirrored){
		getSprite()->fillRect(getTotalX(), getTotalY() + 25, 77, 18, C_RGB(248, 147, 121));
		gif->setXY(getTotalX(), getTotalY() + 25);
		gif->push();
	}
	if(selected && mirrored){
		getSprite()->fillRect(getTotalX(), getTotalY() + 25, 77, 18, C_RGB(157, 194, 255));
		gif->setXY(getTotalX() + 61, getTotalY() + 25);
		gif->push();
	}
	if(!mirrored){

		getSprite()->fillRoundRect(getTotalX() + 25, getTotalY() + 30, (((float) intensity) / 255.0f) * 45.0f, 8, 5,
								   TFT_GREENYELLOW);
		getSprite()->drawRoundRect(getTotalX() + 25, getTotalY() + 30, 45, 8, 5, TFT_WHITE);
	}
	if(mirrored){

		getSprite()->fillRoundRect(getTotalX() + 6, getTotalY() + 30, (((float) intensity) / 255.0f) * 45.0f, 8, 5,
								   TFT_GREENYELLOW);
		getSprite()->drawRoundRect(getTotalX() + 6, getTotalY() + 30, 45, 8, 5, TFT_WHITE);
	}
	if(!selected && !mirrored){
		getSprite()->drawIcon(iconsNotMirrored[effect], getTotalX(), getTotalY() + 25, 16, 16, 1, TFT_BLACK);
	}
	if(!selected && mirrored){
		getSprite()->drawIcon(iconsMirrored[effect], getTotalX() + 61, getTotalY() + 25, 16, 16, 1, TFT_BLACK);
	}

}

void MixScreen::Effects::setSelected(bool selected){
	Effects::selected = selected;
}

bool MixScreen::Effects::needsUpdate(){
	if(selected){
		return gif->newFrameReady();
	}else{
		return false;
	}
}
