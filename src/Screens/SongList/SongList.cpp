#include <Input/InputJayD.h>
#include "SongList.h"

SongList::SongList *SongList::SongList::instance = nullptr;

SongList::SongList::SongList(Display &display) : Context(display),
												 scrollLayout(&screen),
												 list(&scrollLayout, VERTICAL){

	for(int i = 0; i < 10; i++){
		songs.push_back(new ListItem(&list, "song"));
	}

	instance = this;
	buildUI();
	songs[selectedElement]->setSelected(true);

}

void SongList::SongList::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		if(instance == nullptr) return;
		instance->songs[instance->selectedElement]->setSelected(false);
		instance->selectedElement += value;
		if(instance->selectedElement < 0){
			instance->selectedElement = 0;
		}else if(instance->selectedElement >= instance->songs.size()){
			instance->selectedElement = instance->songs.size() - 1;
		}
		instance->songs[instance->selectedElement]->setSelected(true);
		instance->scrollLayout.scrollIntoView(instance->selectedElement,2);
		instance->draw();
		instance->screen.commit();
	});
	instance->draw();
	instance->screen.commit();
}

void SongList::SongList::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(1);

}

void SongList::SongList::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void SongList::SongList::buildUI(){
	scrollLayout.setWHType(PARENT, PARENT);
	//scrollLayout.setBorder(1, TFT_RED);
	scrollLayout.addChild(&list);

	list.setWHType(PARENT, CHILDREN);
	list.setPadding(5);
	list.setGutter(10);
	//list.setBorder(1, TFT_RED);
	for(int i = 0; i < songs.size(); i++){
		list.addChild(songs[i]);
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
