#include "EffectElement.h"
#include <SPIFFS.h>
#include <FS.h>
#include <FS/PGMFile.h>


const char* const MixScreen::EffectElement::iconsNotMirrored[] = {"/noEffectRed.raw", "/speedRed.raw", "/lowpassRed.raw", "/highpassRed.raw", "/reverbRed.raw", "/bitcrusherRed.raw"};
const char* const MixScreen::EffectElement::iconsMirrored[] = {"/noEffectBlue.raw", "/speedBlue.raw", "/lowpassBlue.raw", "/highpassBlue.raw", "/reverbBlue.raw", "/bitcrusherBlue.raw"};
const char* const MixScreen::EffectElement::gifIcons[] = {"/noEffect.g565", "/speed.g565", "/lowpass.g565", "/highpass.g565", "/reverb.g565", "/bitcrusher.g565"};

MixScreen::EffectElement::EffectElement(ElementContainer* parent, bool mirrored) : CustomElement(parent, 10, 10), mirrored(mirrored){

	icon = static_cast<Color*>(ps_malloc(16 * 16 * 2));
	if(icon == nullptr){
		Serial.println("EffectIcon pictures unpack error");
		return;
	}

	setType(NONE);
}

MixScreen::EffectElement::~EffectElement(){
	free(icon);
	delete gif;
}

void MixScreen::EffectElement::setupGif(){
	if(gif == nullptr) return;

	gif->setMaskingColor(TFT_BLACK);
	gif->setLoop(true);
	gif->nextFrame();
	gif->setXY(!mirrored ? getTotalX() + 2 : getTotalX() + 58, getTotalY() + 22);
}

void MixScreen::EffectElement::draw(){

	if(selected){
		gif->nextFrame();
		gif->push();
	}else{
		getSprite()->drawIcon(icon, !mirrored ? getTotalX() + 2 : getTotalX() + 58, getTotalY() + 22, 16, 16, 1, TFT_BLACK);
	}


	getSprite()->fillRoundRect(getTotalX() + 6 + 19 * !mirrored, getTotalY() + 27, (((float) intensity) / 255.0f) * 45.0f, 8, 3, TFT_GREENYELLOW);
	getSprite()->drawRoundRect(getTotalX() + 6 + 19 * !mirrored, getTotalY() + 27, 45, 8, 3, TFT_WHITE);
}

void MixScreen::EffectElement::repos(){
	CustomElement::repos();
	if(gif != nullptr){
		gif->setXY(!mirrored ? getTotalX() + 2 : getTotalX() + 58, getTotalY() + 22);
	}
}

void MixScreen::EffectElement::setType(EffectType effect){
	EffectElement::effect = effect;

	fs::File iconFile = SPIFFS.open(mirrored ? iconsMirrored[effect] : iconsNotMirrored[effect]);
	iconFile.seek(0);
	iconFile.read(reinterpret_cast<uint8_t*>(icon), 16 * 16 * 2);
	iconFile.close();

	delete gif;
	gif = new AnimatedSprite(getSprite(), SPIFFS.open(gifIcons[effect]));

	setupGif();
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
