#include <InputLib/InputJayD.h>
#include "SongList.h"

SongList::SongList *SongList::SongList::instance = nullptr;

SongList::SongList::SongList(Display &display) : Context(display),
												 scrollLayout(&screen),
												 list(&scrollLayout, VERTICAL){

	for(int i = 0; i < 10; i++){
		song.push_back(new ListItem(&list, "song"));
	}

	instance = this;
	buildUI();

}

void SongList::SongList::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		if(instance == nullptr) return;
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

void SongList::SongList::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(1);
}

void SongList::SongList::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void SongList::SongList::buildUI(){
	scrollLayout.setWHType(PARENT, FIXED);
	scrollLayout.setHeight(20);
	scrollLayout.setBorder(1, TFT_RED);
	scrollLayout.addChild(&list);

	list.setWHType(PARENT, CHILDREN);
	list.setPadding(5);
	list.setGutter(10);
	//list.setBorder(1, TFT_RED);
	for(int i = 0; i < song.size(); i++){
		list.addChild(song[i]);
	}

	scrollLayout.reflow();
	list.reflow();

	screen.addChild(&scrollLayout);
	screen.repos();
	pack();
}

SongList::SongList::~SongList(){
	instance = nullptr;
}
