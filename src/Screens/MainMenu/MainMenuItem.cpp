#include "MainMenuItem.h"
#include "Bitmaps/reset.hpp"
#include "Bitmaps/dj.hpp"
#include "Bitmaps/playback.hpp"
#include "Bitmaps/Settings_orange.hpp"
#include "Bitmaps/Playback_blue.hpp"
#include "Bitmaps/DJ_red.hpp"

MainMenu::MainMenuItem::MainMenuItem(ElementContainer *parent) : CustomElement(parent, 20, 20){

}

uint16_t *icons[] = {const_cast<uint16_t *>(playback), const_cast<uint16_t *>(dj), const_cast<uint16_t *>(reset)};

uint16_t *gifovi[] ={const_cast<uint16_t *>(playback_blue), const_cast<uint16_t *>(DJ_red), const_cast<uint16_t *>(settings_orange)};

void MainMenu::MainMenuItem::setItems(Items items){
	if(items == 0){
		MainMenuItem::items = static_cast<Items>(0);
	}else if(items == 2 ){
		MainMenuItem::items = static_cast<Items>(2);
	}else if(items > 0 && items < 2){
		MainMenuItem::items = items;
	}
}


void MainMenu::MainMenuItem::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);

	if(selected){
		getSprite()->drawIcon( gifovi[items] ,getTotalX(),getTotalY()+45,45,42,1,TFT_TRANSPARENT);
	}else{
		getSprite()->drawIcon( icons[items] ,getTotalX(),getTotalY()+45,45,42,1,TFT_TRANSPARENT);
	}

}

void MainMenu::MainMenuItem::isSelected(bool selected){
	MainMenuItem::selected = selected;
}

