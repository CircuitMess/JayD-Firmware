#include "MainMenuItem.h"
#include <SPIFFS.h>
#include <FS/CompressedFile.h>

String gifIcons[] = {"/playbackGIF.g565", "/djGIF.g565", "/settingsGIF.g565"};

String icons[] = {"/playback.raw.hs", "/dj.raw.hs", "/settings.raw.hs"};

String texts[] = { "Playback", "DJ", "Settings" };


MainMenu::MainMenuItem::MainMenuItem(ElementContainer *parent, MenuItemType type) : CustomElement(parent, 20, 20), type(type){

	fs::File f = SPIFFS.open(gifIcons[type]);
	if(!f){
		Serial.printf("Can't open gif file in MainMenuItem %s\n", gifIcons[type].c_str());
		return;
	}
	gif = new AnimatedSprite(getSprite(), SPIFFS.open(gifIcons[type]));
	gif->setLoop(true);
	gif->setMaskingColor(TFT_BLACK);

	fs::File iconsPictures = SPIFFS.open(icons[type]);
	fs::File picture = CompressedFile::open(iconsPictures, 7, 6);

	buffer = static_cast<Color *>(ps_malloc(45 * 42 * 2));
	if(buffer == nullptr){
		Serial.printf("MainMenuItem picture %s unpack error\n", icons[type].c_str());
		return;
	}
	picture.seek(0);
	picture.read(reinterpret_cast<uint8_t *>(buffer), 160 * 128 * 2);

	iconsPictures.close();
	picture.close();
}

MainMenu::MainMenuItem::~MainMenuItem(){
	free(buffer);
	delete gif;
}


void MainMenu::MainMenuItem::draw(){
	uint32_t yPos = getTotalY() + 107;

	if(selected && gif){
		gif->setXY(getTotalX()+7, getTotalY() + 45);
		gif->nextFrame();
		gif->push();

		yPos += sin((float) (micros() - startMicros) / 200000.0f) * -6;
	}else{
		getSprite()->drawIcon(buffer, getTotalX()+7, getTotalY() + 45, 45, 42, 1, TFT_BLACK);
	}

	FontWriter u8f = getSprite()->startU8g2Fonts();
	u8f.setFont(u8g2_font_HelvetiPixel_tr);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setFontMode(1);

	int16_t width = u8f.getUTF8Width(texts[type].c_str());
	u8f.setCursor(getTotalX() + 7 + 45/2 - width/ 2, yPos);
	u8f.print(texts[type]);
}

void MainMenu::MainMenuItem::isSelected(bool selected){
	MainMenuItem::selected = selected;
	if(selected){
		gif->reset();
		startMicros = micros();
	}else{
		gif->stop();
	}
}

bool MainMenu::MainMenuItem::needsUpdate(){
	if(selected){
		return gif->checkFrame();
	}else{
		return false;
	}
}








