#include "BtnTestElement.hpp"


BtnTestElement::BtnTestElement(ElementContainer *parent) : CustomElement(parent,70,20){

}

void BtnTestElement::btnPress(){
if(!pressed){
	pressed= true;
}
}

void BtnTestElement::draw(){
	getSprite()->fillCircle(getTotalX()+15,getTotalY()+40,3,!pressed ? TFT_DARKGREY:TFT_GREEN);
}
