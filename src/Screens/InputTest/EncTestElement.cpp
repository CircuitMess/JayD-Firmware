#include "EncTestElement.h"

EncTestElement::EncTestElement(ElementContainer *parent, bool midLayout) : CustomElement(parent, 20, 30), midLayout(midLayout){


}

void EncTestElement::encoderMove(int8_t newValue){
	counter = min(counter + abs(newValue), 11);
	if(counter>10){
		encValueDone=true;
	}
}

void EncTestElement::draw(){
	if(!midLayout){
		getSprite()->drawCircle(getTotalX() + 12, getTotalY() + 30, 10, TFT_DARKGREY);
		for(int i=4;i<counter;i++){
			getSprite()->fillCircle(getTotalX()+12,getTotalY()+30,i,TFT_GREEN);
		}
		getSprite()->drawCircle(getTotalX() + 12, getTotalY() + 30, 4, TFT_BLACK);
		getSprite()->fillCircle(getTotalX() + 12, getTotalY() + 30, 3, !encPressed ? TFT_DARKGREY : TFT_GREEN);
	}else if(midLayout){
		getSprite()->drawCircle(getTotalX() + 52, getTotalY() + 30, 10, TFT_DARKGREY);
		for(int i=4;i<counter;i++){
			getSprite()->fillCircle(getTotalX()+52,getTotalY()+30,i,TFT_GREEN);
		}
		getSprite()->fillCircle(getTotalX() + 52, getTotalY() + 30, 4, TFT_BLACK);
		getSprite()->fillCircle(getTotalX() + 52, getTotalY() + 30, 3, !encPressed ? TFT_DARKGREY : TFT_GREEN);
	}
}

void EncTestElement::btnEncPress(){
	if(!encPressed){
		encPressed = true;
	}
}

bool EncTestElement::isEncPressed() const{
	return encPressed;
}

bool EncTestElement::isEncValueDone() const{
	return encValueDone;
}
