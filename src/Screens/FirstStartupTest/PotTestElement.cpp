#include "PotTestElement.hpp"

PotTestElement::PotTestElement(ElementContainer *parent, bool horizontal) : CustomElement(parent, 65, 20), horizontal(horizontal){


}

void PotTestElement::potMove(uint8_t newValue){
	value = newValue;
	if(counter < 100){
		counter++;
	}else{
		counter = 100;
		done = true;
	}
}

void PotTestElement::draw(){
	if(!horizontal){
		getSprite()->drawRect(getTotalX() - 8, getTotalY() + 2, 2, 55, !done? TFT_DARKGREY:TFT_GREEN);
		getSprite()->drawRect(getTotalX() - 8, getTotalY() + 2, 2, (((float) counter) / 100.0f) * 55.0f, TFT_GREEN);
		getSprite()->drawRect(getTotalX() - 10, getTotalY() + 1, 6, 2, !done? TFT_DARKGREY:TFT_GREEN);
		getSprite()->drawRect(getTotalX() - 10, getTotalY() + 56, 6, 2, !done? TFT_DARKGREY:TFT_GREEN);
		getSprite()->drawRect(getTotalX() - 10, getTotalY() + 56 - (((float) value) / 255.0f) * 59.0f, 6, 2, TFT_GREEN);
	}
	if(horizontal){
		getSprite()->drawRect(getTotalX() + 25, getTotalY() + 22, 55, 2, !done? TFT_DARKGREY:TFT_GREEN);
		getSprite()->drawRect(getTotalX() + 25, getTotalY() + 22, (((float) counter) / 100.0f) * 55.0f, 2, TFT_GREEN);
		getSprite()->drawRect(getTotalX() + 25, getTotalY() + 20, 2, 6, !done? TFT_DARKGREY:TFT_GREEN);
		getSprite()->drawRect(getTotalX() + 80, getTotalY() + 20, 2, 6, !done? TFT_DARKGREY:TFT_GREEN);
		getSprite()->drawRect(getTotalX() + 25 + (((float) value) / 255.0f) * 60.0f, getTotalY() + 20, 2, 6, TFT_GREEN);
	}
}
