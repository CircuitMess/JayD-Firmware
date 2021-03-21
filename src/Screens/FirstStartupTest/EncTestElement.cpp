#include "EncTestElement.h"

EncTestElement::EncTestElement(ElementContainer *parent, bool midLayout) : CustomElement(parent, 20, 30), midLayout(midLayout){


}

void EncTestElement::encoderMove(int8_t newValue){
	value = value + newValue;
	if(counter<9){
		counter++;
	}else{
		counter = 11;

	}
}

void EncTestElement::draw(){
	if(!midLayout){
		getSprite()->drawCircle(getTotalX() + 12, getTotalY() + 30, 10, TFT_DARKGREY);
		for(int i=4;i<counter;i++){
			getSprite()->fillCircle(getTotalX()+12,getTotalY()+30,i,TFT_GREEN);
		}
		getSprite()->drawCircle(getTotalX() + 12, getTotalY() + 30, 4, TFT_BLACK);
		getSprite()->fillCircle(getTotalX() + 12, getTotalY() + 30, 3, !pressed ? TFT_DARKGREY : TFT_GREEN);
	}else if(midLayout){
		getSprite()->drawCircle(getTotalX() + 52, getTotalY() + 30, 10, TFT_DARKGREY);
		for(int i=4;i<counter;i++){
			getSprite()->fillCircle(getTotalX()+52,getTotalY()+30,i,TFT_GREEN);
		}
		getSprite()->fillCircle(getTotalX() + 52, getTotalY() + 30, 4, TFT_BLACK);
		getSprite()->fillCircle(getTotalX() + 52, getTotalY() + 30, 3, !pressed ? TFT_DARKGREY : TFT_GREEN);
	}
}

void EncTestElement::btnEncPress(){
	if(!pressed){
		pressed = true;
	}
}
