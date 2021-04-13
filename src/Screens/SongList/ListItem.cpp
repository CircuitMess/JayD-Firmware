#include "ListItem.h"

SongList::ListItem::ListItem(ElementContainer *parent, String songName) : CustomElement(parent, 150, 15), songName(songName){

}

void SongList::ListItem::draw(){

	FontWriter u8f = getSprite()->startU8g2Fonts();

	u8f.setFont(u8g2_font_profont12_tf);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setFontMode(1);

	u8f.setCursor(getTotalX()+2, getTotalY()+9);
	u8f.println(songName.substring(1));

	if(selected){
		getSprite()->drawRect(getTotalX()-2,getTotalY()-4,getWidth()+4,getHeight()+6,TFT_LIGHTGREY);//treba ubaciti ikonicu za scrolanje
	}
}

void SongList::ListItem::setSelected(bool selected){
	ListItem::selected = selected;
}

const String& SongList::ListItem::getName() const{
	return songName;
}
