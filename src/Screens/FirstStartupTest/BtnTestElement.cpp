#include "BtnTestElement.hpp"


BtnTestElement::BtnTestElement(ElementContainer *parent) : CustomElement(parent,70,20){

}

void BtnTestElement::btnPress(){
if(!btnPressed){
	btnPressed= true;

}
}

void BtnTestElement::draw(){
	getSprite()->fillCircle(getTotalX()+15,getTotalY()+40,3, !btnPressed ? TFT_DARKGREY : TFT_GREEN);
}

bool BtnTestElement::isBtnPressed() const{
	return btnPressed;
}
