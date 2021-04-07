#include <SD.h>
#include "SongList.h"
#include <JayD.hpp>
#include <Loop/LoopManager.h>
#include <SPIFFS.h>
#include <FS/CompressedFile.h>

SongList::SongList *SongList::SongList::instance = nullptr;

SongList::SongList::SongList(Display &display) : Context(display){

	instance = this;

	scrollLayout = new ScrollLayout(&getScreen());
	list = new LinearLayout(scrollLayout, VERTICAL);

	fs::File file = SPIFFS.open("/SongListBackground.raw.hs");

	background = CompressedFile::open(file, 10, 9);

	buildUI();
	pack();

	SD.end();
	insertedSD = SD.begin(22, SPI);

	if(insertedSD){
		populateList();
		if(!songs.empty()){
			songs[selectedElement]->setSelected(true);
		}
	}

}

SongList::SongList::~SongList(){
	instance = nullptr;
}

void SongList::SongList::populateList(){
	File root = SD.open("/");
	File f;

	if(!songs.empty()){

		for(int i = 0; i < songs.size(); i++){
			delete songs[i];
		}
		list->getChildren().clear();
		songs.clear();
	}


	while(f = root.openNextFile()){
		if(f.isDirectory()) continue;
		if(!String(f.name()).endsWith(".wav")) continue;

		songs.push_back(new ListItem(list, f.name()));

		f.close();
	}
	root.close();


	for(int i = 0; i < songs.size(); i++){
		list->addChild(songs[i]);
	}

	list->reflow();
	list->repos();
}

void SongList::SongList::loop(uint t){

	if(millis() - prevSDCheck > sdCardInterval){

		SD.end();
		bool ins = SD.begin(22, SPI);

		if(insertedSD != ins){
			insertedSD = ins;

			if(insertedSD){
				populateList();
				if(!songs.empty()){
					selectedElement = 0;
					songs[selectedElement]->setSelected(true);
				}
				//scrollLayout->setY(18);
				scrollLayout->scrollIntoView(selectedElement,2);
			}
		}
		prevSDCheck = millis();
	}

	draw();
	screen.commit();
}

void SongList::SongList::start(){

	InputJayD::getInstance()->setEncoderMovedCallback(ENC_L1, [](int8_t value){
		if(instance == nullptr) return;

		if(!instance->songs.empty()){
			instance->songs[instance->selectedElement]->setSelected(false);
			instance->selectedElement += value;
			if(instance->selectedElement < 0){
				instance->selectedElement = 0;
			}else if(instance->selectedElement >= instance->songs.size()){
				instance->selectedElement = instance->songs.size() - 1;
			}

			instance->songs[instance->selectedElement]->setSelected(true);

			instance->scrollLayout->scrollIntoView(instance->selectedElement, 2);
			instance->draw();
			instance->screen.commit();
		}
		else{
			return;
		}
	});

	InputJayD::getInstance()->setBtnPressCallback(BTN_L1, [](){
		if(instance == nullptr) return;

		instance->pop(new String(instance->songs[instance->selectedElement]->getName()));
	});

	LoopManager::addListener(this);

	draw();
	screen.commit();
}

void SongList::SongList::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(ENC_L1);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_L1);
	LoopManager::removeListener(this);
}

void SongList::SongList::draw(){
	Sprite *canvas = screen.getSprite();

	canvas->drawIcon(buffer, 0, 0, 160, 128, 1);
	canvas->setTextFont(2);
	canvas->setTextSize(1);
	canvas->setTextColor(TFT_WHITE);

	if(!insertedSD){
		canvas->setCursor(0, 55);
		canvas->printCenter("Not inserted!");
	}else if(songs.empty()){
		canvas->setCursor(0, 55);
		canvas->printCenter("Empty!");
	}else{
		screen.draw();
	}

	canvas->fillRect(0, 0, 160, 18, TFT_LIGHTGREY);
	canvas->setTextFont(2);
	canvas->setTextSize(1);
	canvas->setTextColor(TFT_BLACK);
	canvas->setCursor(0, 1);
	canvas->printCenter("SD card");

}

void SongList::SongList::buildUI(){
	scrollLayout->setWHType(PARENT, FIXED);
	scrollLayout->setHeight(110);
	scrollLayout->addChild(list);

	list->setWHType(PARENT, CHILDREN);
	list->setPadding(5);
	list->setGutter(10);

	if(!songs.empty()){
		for(int i = 0; i < songs.size(); i++){
			list->addChild(songs[i]);
		}
	}

	scrollLayout->reflow();
	list->reflow();

	screen.addChild(scrollLayout);
	screen.repos();
	scrollLayout->setY(18);
}

void SongList::SongList::pack(){
	Context::pack();
	free(buffer);
}

void SongList::SongList::unpack(){
	Context::unpack();

	buffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(buffer == nullptr){
		Serial.println("Error");
	}
	background.seek(0);
	background.read(reinterpret_cast<uint8_t *>(buffer), 160 * 128 * 2);
}
