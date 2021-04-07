#include "MainMenuItem.h"
#include <SPIFFS.h>
#include <FS/CompressedFile.h>

String gifIcons[] = {"/playbackGIF.g565", "/djGIF.g565", "/settingsGIF.g565"};

String icons[] = {"/playback.raw.hs", "/dj.raw.hs", "/settings.raw.hs"};


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
		Serial.printf("MainMenuItem picture %s unpack error\n",icons[type].c_str());
		return;
	}
	picture.seek(0);
	picture.read(reinterpret_cast<uint8_t *>(buffer), 160 * 128 * 2);


}
MainMenu::MainMenuItem::~MainMenuItem(){
	free(buffer);
	delete gif;
}



void MainMenu::MainMenuItem::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);

	if(selected && gif){
		gif->setXY(getTotalX()+7, getTotalY() + 60);
		gif->nextFrame();
		gif->push();
	}else{
		getSprite()->drawIcon(buffer, getTotalX()+7, getTotalY() + 60, 45, 42, 1, TFT_BLACK);

	}

}

void MainMenu::MainMenuItem::isSelected(bool selected){
	MainMenuItem::selected = selected;
	if(selected){
		gif->reset();
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








