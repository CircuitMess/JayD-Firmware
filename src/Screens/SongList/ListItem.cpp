#include "ListItem.h"

SongList::ListItem::ListItem(ElementContainer *parent, String songName) : CustomElement(parent, 10, 10), songName(songName){


}

void SongList::ListItem::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->setCursor(getTotalX(), getTotalY());
	if(selected){
		getSprite()->drawCircle(getTotalX(),getTotalY(),10,TFT_RED);//treba ubaciti ikonicu za scrolanje
	}
	getSprite()->println(songName);
}

void SongList::ListItem::setSelected(bool selected){
	ListItem::selected = selected;
}
