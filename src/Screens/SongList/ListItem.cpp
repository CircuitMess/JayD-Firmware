#include "ListItem.h"

SongList::ListItem::ListItem(ElementContainer *parent, String songName) : CustomElement(parent, 150, 15), songName(songName){


}

void SongList::ListItem::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(1);
	getSprite()->setCursor(getTotalX()+2, getTotalY()+2);
	getSprite()->println(songName);
	if(selected){
		getSprite()->drawRect(getTotalX()-2,getTotalY()-4,getWidth()+4,getHeight()+6,TFT_RED);//treba ubaciti ikonicu za scrolanje
	}

}

void SongList::ListItem::setSelected(bool selected){
	ListItem::selected = selected;
}

const String& SongList::ListItem::getName() const{
	return songName;
}
