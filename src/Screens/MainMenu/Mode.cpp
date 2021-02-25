#include "Mode.h"

MainMenu::Mode::Mode(ElementContainer *parent) : CustomElement(parent, 20, 20){


}

void MainMenu::Mode::draw(){
	getSprite()->setTextFont(1);
	getSprite()->setTextColor(TFT_WHITE);
	getSprite()->setTextSize(2);

	if(selected){
		getSprite()->fillRect(getTotalX()+25,getTotalY()+50,25,25,TFT_DARKGREY);//treba ubaciti ikonicu za scrolanje
	}

	getSprite()->fillRect(getTotalX()+25,getTotalY()+50,20,20,TFT_BLUE);

}

void MainMenu::Mode::setSelected(bool selected){
	Mode::selected = selected;
}

