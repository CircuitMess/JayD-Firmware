#include "Mode.h"

MainMenu::Mode::Mode(ElementContainer *parent) : CustomElement(parent, 20, 20){


}

void MainMenu::Mode::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);

	if(selected){
		getSprite()->fillRect(getTotalX(),getTotalY(),35,45,TFT_DARKGREY);//treba ubaciti ikonicu za scrolanje
	}

	getSprite()->fillRect(getTotalX()+20,getTotalY()+40,20,20,TFT_BLUE);

}

void MainMenu::Mode::setSelected(bool selected){
	Mode::selected = selected;
}

