#include "PotTestElement.hpp"

PotTestElement::PotTestElement(ElementContainer *parent, bool horizontal) : CustomElement(parent, 65, 20), horizontal(horizontal){


}

void PotTestElement::potMove(uint8_t newValue){
	value = newValue;
	if(counter < 50){
		counter++;
	}else{
		counter = 50;
		potDone = true;
	}
}

void PotTestElement::draw(){
	if(!horizontal){
		getSprite()->drawRect(getTotalX() - 8, getTotalY() + 2, 2, 55, !potDone ? TFT_DARKGREY : TFT_GREEN);
		getSprite()->drawRect(getTotalX() - 8, getTotalY() + 2, 2, (((float) counter) / 50.0f) * 55.0f, TFT_GREEN);
		getSprite()->drawRect(getTotalX() - 10, getTotalY() + 1, 6, 2, !potDone ? TFT_DARKGREY : TFT_GREEN);
		getSprite()->drawRect(getTotalX() - 10, getTotalY() + 56, 6, 2, !potDone ? TFT_DARKGREY : TFT_GREEN);
		getSprite()->drawRect(getTotalX() - 10, getTotalY() + 56 - (((float) value) / 255.0f) * 55.0f, 6, 2, TFT_GREEN);
	}
	if(horizontal){
		getSprite()->drawRect(getTotalX() + 25, getTotalY() + 22, 55, 2, !potDone ? TFT_DARKGREY : TFT_GREEN);
		getSprite()->drawRect(getTotalX() + 25, getTotalY() + 22, (((float) counter) / 50.0f) * 55.0f, 2, TFT_GREEN);
		getSprite()->drawRect(getTotalX() + 25, getTotalY() + 20, 2, 6, !potDone ? TFT_DARKGREY : TFT_GREEN);
		getSprite()->drawRect(getTotalX() + 80, getTotalY() + 20, 2, 6, !potDone ? TFT_DARKGREY : TFT_GREEN);
		getSprite()->drawRect(getTotalX() + 25 + (((float) value) / 255.0f) * 55.0f, getTotalY() + 20, 2, 6, TFT_GREEN);
	}
}

bool PotTestElement::isPotDone() const{
	return potDone;
}
