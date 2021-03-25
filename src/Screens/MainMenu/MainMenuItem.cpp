#include "MainMenuItem.h"
#include "Bitmaps/settings.hpp"
#include "Bitmaps/dj.hpp"
#include "Bitmaps/playback.hpp"
#include "Bitmaps/djGIF.hpp"
#include "Bitmaps/playbackGIF.hpp"
#include "Bitmaps/settingsGIF.hpp"
#include <FS/PGMFile.h>
#include <SPIFFS.h>


uint16_t *icons[] = {playback, dj, settings};

String gifIcons[] = {"/playbackGIF.g565", "/dj.g565", "/settingsGIF.g565"};

//size_t gifIconsSize[] = {sizeof(playback_gif), sizeof(dj_gif), sizeof(settings_gif)};

MainMenu::MainMenuItem::MainMenuItem(ElementContainer *parent, MenuItemType type) : CustomElement(parent, 20, 20), type(type){

	gif = new AnimatedSprite(getSprite(), SPIFFS.open(gifIcons[type]));
	gif->setLoop(true);


}


void MainMenu::MainMenuItem::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);

	if(selected){
		gif->setXY(getTotalX(), getTotalY() + 40);
		gif->nextFrame();
		gif->push();
	}else{
		getSprite()->drawIcon(icons[type], getTotalX(), getTotalY() + 45, 45, 42, 1, TFT_TRANSPARENT);
	}

}

void MainMenu::MainMenuItem::isSelected(bool selected){
	MainMenuItem::selected = selected;
}

bool MainMenu::MainMenuItem::needsUpdate(){
	if(selected){
		return gif->checkFrame();
	}else{
		return false;
	}
}

MainMenu::MainMenuItem::~MainMenuItem(){
	delete gif;
}







