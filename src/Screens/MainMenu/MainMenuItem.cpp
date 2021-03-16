#include "MainMenuItem.h"
#include "Bitmaps/settings.h"
#include "Bitmaps/dj.h"
#include "Bitmaps/playback.h"
#include "Bitmaps/djGIF.h"
#include "Bitmaps/playbackGIF.h"
#include "Bitmaps/settingsGIF.h"
#include <FS/PGMFile.h>


uint16_t *icons[] = {playback, dj, settings};

unsigned char *gifIcons[] = {playback_gif, dj_gif, settings_gif};

size_t gifIconsSize[] = {sizeof(playback_gif), sizeof(dj_gif), sizeof(settings_gif)};

MainMenu::MainMenuItem::MainMenuItem(ElementContainer *parent, MenuItemType type) : CustomElement(parent, 20, 20), type(type),
																					gif(getSprite(), fs::File(std::make_shared<PGMFile>(gifIcons[type], gifIconsSize[type]))){

}



void MainMenu::MainMenuItem::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);

	if(selected){
		gif.setXY(getTotalX(), getTotalY() + 40);
		gif.push();
	}
	else{
		getSprite()->drawIcon(icons[type], getTotalX(), getTotalY() + 45, 45, 42, 1, TFT_TRANSPARENT);
	}

}

void MainMenu::MainMenuItem::isSelected(bool selected){
	MainMenuItem::selected = selected;
}

bool MainMenu::MainMenuItem::needsUpdate(){
	if(selected){
		return gif.newFrameReady();
	}else{
		return false;
	}
}







