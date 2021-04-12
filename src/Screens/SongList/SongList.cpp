#include <SD.h>
#include "SongList.h"
#include <JayD.hpp>
#include <Loop/LoopManager.h>
#include <SPIFFS.h>
#include <FS/CompressedFile.h>
#include <U8g2_for_TFT_eSPI.h>

SongList::SongList *SongList::SongList::instance = nullptr;

SongList::SongList::SongList(Display &display) : Context(display){

	instance = this;

	scrollLayout = new ScrollLayout(&getScreen());
	list = new LinearLayout(scrollLayout, VERTICAL);

	fs::File file = SPIFFS.open("/SongListBackground.raw.hs");

	background = CompressedFile::open(file, 10, 9);

	buildUI();
	pack();

	isOpened = true;
}

SongList::SongList::~SongList(){
	instance = nullptr;
	background.close();
	free(backgroundBuffer);
}

void SongList::SongList::populateList(){

	if(!songs.empty()){

		for(int i = 0; i < songs.size(); i++){
			delete songs[i];
		}
		list->getChildren().clear();
		songs.clear();
	}

	File root = SD.open("/");
	searchDirectories(root);
	root.close();

	for(int i = 0; i < songs.size(); i++){
		list->addChild(songs[i]);
	}

	list->reflow();
	list->repos();
}

void SongList::SongList::searchDirectories(File file){

	File f;

	while(f = file.openNextFile()){
		if(f.isDirectory()){

			searchDirectories(f);
			f.close();
			continue;
		}

		if(!String(f.name()).endsWith(".aac")) continue;

		String fileName(f.name());
		int idx = fileName.lastIndexOf('/');

		songs.push_back(new ListItem(list, fileName.substring(idx)));

		f.close();
	}
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
				scrollLayout->scrollIntoView(selectedElement, 2);
			}
			draw();
			screen.commit();
		}

		prevSDCheck = millis();
	}
}

void SongList::SongList::start(){

	InputJayD::getInstance()->setEncoderMovedCallback(ENC_MID, [](int8_t value){
		if(instance == nullptr) return;

		if(instance->songs.empty() || !instance->insertedSD ||
		   instance->songs.size() <= instance->selectedElement)
			return;

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


	});

	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;

		if(instance->songs.empty() || !instance->insertedSD ||
		   instance->songs.size() <= instance->selectedElement)
			return;

		instance->pop(new String(instance->songs[instance->selectedElement]->getName()));
	});

	SD.end();
	insertedSD = SD.begin(22, SPI);
	if(insertedSD){
		populateList();
		if(!songs.empty()){
			songs[selectedElement]->setSelected(true);
		}
	}

	LoopManager::addListener(this);

	isOpened = false;

	draw();
	screen.commit();
}

void SongList::SongList::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(ENC_MID);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_MID);
	LoopManager::removeListener(this);
}

void SongList::SongList::draw(){

	Sprite *canvas = screen.getSprite();

	FontWriter u8f = canvas->startU8g2Fonts();

	u8f.setFont(u8g2_font_DigitalDisco_tf);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setFontMode(1);

	canvas->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);

	u8f.setCursor((160 - u8f.getUTF8Width("SD card"))/2, 15);
	u8f.printf("SD card");

	if(isOpened){
		u8f.setCursor((160 - u8f.getUTF8Width("Loading..."))/2, 65);
		u8f.printf("Loading...");
		return;
	}

	if(!insertedSD){
		u8f.setCursor((160 - u8f.getUTF8Width("Not inserted!"))/2, 65);
		u8f.printf("Not inserted!");

	}else if(songs.empty()){
		u8f.setCursor((160 - u8f.getUTF8Width("Empty!"))/2, 65);
		u8f.printf("Empty!");

	}else{
		screen.draw();
	}

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
	free(backgroundBuffer);
	backgroundBuffer = nullptr;
}

void SongList::SongList::unpack(){
	Context::unpack();

	backgroundBuffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("Error");
	}
	background.seek(0);
	background.read(reinterpret_cast<uint8_t *>(backgroundBuffer), 160 * 128 * 2);
}
