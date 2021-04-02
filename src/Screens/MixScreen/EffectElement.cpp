#include "EffectElement.h"
#include <SPIFFS.h>
#include <FS.h>
#include <FS/PGMFile.h>


const String MixScreen::EffectElement::iconsNotMirrored[] = {"/data/mixScreen/pictures/noEffectRed.raw", "/data/mixScreen/pictures/speedRed.raw", "/data/mixScreen/pictures/lowpassRed.raw", "/data/mixScreen/pictures/highpassRed.raw", "/data/mixScreen/pictures/reverbRed.raw", "/data/mixScreen/pictures/bitrusherRed.raw"};
const String MixScreen::EffectElement::iconsMirrored[] = {"/data/mixScreen/pictures/noEffectBlue.raw", "/data/mixScreen/pictures/speedBlue.raw", "/data/mixScreen/pictures/lowpassBlue.raw", "/data/mixScreen/pictures/highpassBlue.raw", "/data/mixScreen/pictures/reverbBlue.raw", "/data/mixScreen/pictures/bitrusherBlue.raw"};
const String MixScreen::EffectElement::gifIcons[] = {"/data/mixScreen/gifs/noEffect.g565", "/data/mixScreen/gifs/speed.g565", "/data/mixScreen/gifs/lowpass.g565", "/data/mixScreen/gifs/highpass.g565", "/data/mixScreen/gifs/reverb.g565", "/data/mixScreen/gifs/bitcrusher.g565"};

MixScreen::EffectElement::EffectElement(ElementContainer *parent, bool mirrored) : CustomElement(parent, 10, 10), mirrored(mirrored){
	fs::File iconsNotMirroredFile = SPIFFS.open(iconsNotMirrored[effect]);

	bufferNotMirrored = static_cast<Color *>(ps_malloc(16 * 16 * 2));
	if(bufferNotMirrored == nullptr){
		Serial.println("EffectNotMirrored pictures unpack error");
		return;
	}
	iconsNotMirroredFile.seek(0);
	iconsNotMirroredFile.read(reinterpret_cast<uint8_t *>(bufferNotMirrored), 16 * 16 * 2);

	fs::File iconsMirroredFile = SPIFFS.open(iconsMirrored[effect]);

	bufferMirrored = static_cast<Color *>(ps_malloc(16 * 16 * 2));
	if(bufferMirrored == nullptr){
		Serial.println("EffectMirrored pictures unpack error");
		return;
	}
	iconsMirroredFile.seek(0);
	iconsMirroredFile.read(reinterpret_cast<uint8_t *>(bufferMirrored), 16 * 16 * 2);

	fs::File gifIconsFile = SPIFFS.open(gifIcons[effect]);

	if(!gifIconsFile){
		Serial.printf("Can't open file %s\n", gifIcons[effect].c_str());
		return;
	}

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
		}
*/
	}else{
		getSprite()->drawIcon((mirrored ? bufferMirrored : bufferNotMirrored), getTotalX() + 61 * mirrored, getTotalY() + 25, 16, 16, 1, TFT_BLACK);
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

MixScreen::EffectElement::~EffectElement(){
	free(bufferNotMirrored);
	free(bufferMirrored);

}
