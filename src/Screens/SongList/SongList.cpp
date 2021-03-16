#include <Input/InputJayD.h>
#include <SD.h>
#include "SongList.h"
#include <JayD.hpp>

SongList::SongList *SongList::SongList::instance = nullptr;

SongList::SongList::SongList(Display &display) : Context(display),
												 scrollLayout(&screen),
												 list(&scrollLayout, VERTICAL){

	instance = this;

	SD.end();
	insertedSD = SD.begin(22, SPI);

	if(insertedSD){
		populateList();
		songs[selectedElement]->setSelected(true);
	}

	buildUI();
}

void SongList::SongList::populateList(){
	File root = SD.open("/");
	File f;
	while(f = root.openNextFile()){
		if(f.isDirectory()) continue;
		if(!String(f.name()).endsWith(".wav")) continue;

		songs.push_back(new ListItem(&list, f.name()));

		f.close();
	}
	root.close();
}

void SongList::SongList::start(){
	InputJayD::getInstance()->setEncoderMovedCallback(ENC_L1, [](int8_t value){
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

	InputJayD::getInstance()->setBtnPressCallback(BTN_L1, [](){
		if(instance == nullptr) return;

		instance->pop(new String(instance->songs[instance->selectedElement]->getName()));
	});

	draw();
	screen.commit();
}

void SongList::SongList::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(ENC_L1);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_L1);
}

void SongList::SongList::draw(){
	Sprite* canvas = screen.getSprite();

	canvas->clear(TFT_BLACK);
	canvas->setTextFont(1);
	canvas->setTextSize(2);
	canvas->setTextColor(TFT_WHITE);

	if(!insertedSD){
		canvas->setCursor(0, 50);
		canvas->printCenter("Not inserted");
	}else if(songs.empty()){
		canvas->setCursor(0, 50);
		canvas->printCenter("Empty");
	}else{
		screen.draw();
	}

	canvas->fillRect(0, 0, 160, 18, TFT_RED);
	canvas->setTextFont(1);
	canvas->setTextSize(2);
	canvas->setTextColor(TFT_WHITE);
	canvas->setCursor(0, 1);
	canvas->printCenter("SD card");
}

void SongList::SongList::buildUI(){
	scrollLayout.setWHType(PARENT, FIXED);
	scrollLayout.setHeight(110);
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
	scrollLayout.setY(18);
	pack();
}

SongList::SongList::~SongList(){
	instance = nullptr;
}
