#include <InputLib/InputJayD.h>
#include "SongList.h"

SongList *SongList::instance = nullptr;

SongList::SongList(Display &display) : Context(display), screenLayout(&screen, VERTICAL), scrollLayout(&screenLayout),
									   list(&scrollLayout, VERTICAL){

	for(int i = 0; i < 10; i++){
		song.push_back(new ListItem(&list));
	}

	instance = this;
	buildUI();

}

void SongList::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		int scrollVal = value * 20;
		if(value == 0) return;
		if((instance->scrollLayout.getScrollY() >= instance->scrollLayout.getMaxScrollY()) && (value > 0) ||
		   ((instance->scrollLayout.getScrollY() == 0) && value < 0)){
			scrollVal = 0;
		}
		instance->scrollLayout.setScroll(0, instance->scrollLayout.getScrollY() + scrollVal);
		instance->draw();
		instance->screen.commit();
	});
}

void SongList::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(1);
}

void SongList::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void SongList::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setPadding(2);
	screenLayout.setGutter(2);
	screenLayout.addChild(&scrollLayout);

	scrollLayout.setWHType(PARENT, FIXED);
	scrollLayout.setHeight(20);
	scrollLayout.setBorder(1, TFT_RED);
	scrollLayout.addChild(&list);

	list.setWHType(PARENT, CHILDREN);
	list.setPadding(5);
	list.setGutter(10);
	//list.setBorder(1, TFT_RED);
	for(int i=0; i < song.size(); i++){
		list.addChild(song[i]);
	}

	screenLayout.reflow();
	scrollLayout.reflow();
	list.reflow();

	screen.addChild(&screenLayout);
	screen.repos();
	pack();
}