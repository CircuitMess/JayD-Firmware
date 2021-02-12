#include "Buttons.h"

Buttons::Buttons(ElementContainer *parent) : CustomElement(parent, 10, 10){


}

void Buttons::checkIfPlayed(){
	if(value==0){
		value=value+1;
	}
	else if (value!=0){
		value=0;
	}
}
void Buttons::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);
	getSprite()->setCursor(getTotalX(), getTotalY());
	getSprite()->println(value);
}