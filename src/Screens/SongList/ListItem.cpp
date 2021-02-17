#include "ListItem.h"

ListItem::ListItem(ElementContainer *parent) : CustomElement(parent, 10, 10){


}

void ListItem::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->setCursor(getTotalX(), getTotalY());
	if(selected){
		getSprite()->drawCircle(getTotalX(),getTotalY(),10,TFT_RED);//treba ubaciti ikonicu za scrolanje
	}
	getSprite()->println("song");
}

void ListItem::setSelected(bool selected){
	ListItem::selected = selected;
}
