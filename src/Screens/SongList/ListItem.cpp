#include "ListItem.h"

ListItem::ListItem(ElementContainer *parent) : CustomElement(parent, 10, 10){


}

void ListItem::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->setCursor(getTotalX(), getTotalY());
	getSprite()->println("song");
}