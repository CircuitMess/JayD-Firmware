#include "MainMenuItem.h"
#include "../../Fonts.h"
#include <SPIFFS.h>
#include <FS/CompressedFile.h>

const char* const MainMenu::MainMenuItem::gifIcons[] = {"/playbackGIF.g565", "/djGIF.g565", "/settingsGIF.g565"};
const char* const MainMenu::MainMenuItem::icons[] = {"/playback.raw.hs", "/dj.raw.hs", "/settings.raw.hs"};
const char* const MainMenu::MainMenuItem::texts[] = { "Playback", "DJ", "Settings" };


MainMenu::MainMenuItem::MainMenuItem(ElementContainer *parent, MenuItemType type) : CustomElement(parent, 20, 20), type(type){
	gif = new AnimatedSprite(getSprite(), SPIFFS.open(gifIcons[type]));
	gif->setLoop(true);
	gif->setMaskingColor(TFT_BLACK);

	buffer = static_cast<Color *>(ps_malloc(45 * 42 * 2));
	if(buffer == nullptr){
		Serial.printf("MainMenuItem picture %s unpack error\n", icons[type]);
		return;
	}

	fs::File bgFile = CompressedFile::open(SPIFFS.open(icons[type]), 7, 6);
	bgFile.read(reinterpret_cast<uint8_t *>(buffer), 160 * 128 * 2);
	bgFile.close();
}

MainMenu::MainMenuItem::~MainMenuItem(){
	free(buffer);
	delete gif;
}


void MainMenu::MainMenuItem::draw(){
	uint32_t yPos = getTotalY() + 112;

	if(selected && gif){
		gif->setXY(getTotalX()+7, getTotalY() + 46);
		gif->nextFrame();
		gif->push();

		yPos += sin((float) (micros() - startMicros) / 200000.0f) * -6;
	}else{
		getSprite()->drawIcon(buffer, getTotalX()+7, getTotalY() + 46, 45, 42, 1, TFT_BLACK);
	}

	auto canvas = getSprite();
	canvas->setFont(&u8g2_font_HelvetiPixel_tr);
	canvas->setTextColor(TFT_WHITE);

	canvas->setTextDatum(BC_DATUM);
	canvas->drawString(texts[type], getTotalX() + 7 + 45/2, yPos);
	canvas->setTextDatum(TL_DATUM);
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








