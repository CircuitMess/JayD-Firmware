#include <SD.h>
#include "SongList.h"
#include "../MainMenu/MainMenu.h"
#include <JayD.hpp>
#include <Loop/LoopManager.h>
#include <SPIFFS.h>
#include <FS/CompressedFile.h>
#include <U8g2_for_TFT_eSPI.h>

SongList::SongList* SongList::SongList::instance = nullptr;

SongList::SongList::SongList(Display& display) : Context(display){
	instance = this;

	scrollLayout = new ScrollLayout(&getScreen());
	list = new LinearLayout(scrollLayout, VERTICAL);

	buildUI();
	SongList::pack();
}

SongList::SongList::~SongList(){
	instance = nullptr;
	free(backgroundBuffer);
}

void SongList::SongList::checkSD(){
	for(auto song : songs){
		delete song;
	}
	list->getChildren().clear();
	songs.clear();
	selectedElement = 0;
	empty = true;

	if(!insertedSD){
		insertedSD = SD.begin(22, SPI);
	}

	if(!insertedSD){
		draw();
		screen.commit();
		return;
	}

	// TODO
	// Empty card inserted, taken out, press refresh
	// SD started, opened root returns true
	File root = SD.open("/");
	insertedSD = root;
	if(!insertedSD){
		root.close();
		draw();
		screen.commit();
		return;
	}

	searchDirectories(root);
	root.close();
	empty = songs.empty();

	if(!empty){
		list->reflow();
		list->repos();
		scrollLayout->scrollIntoView(0, 5);
		songs.front()->setSelected(true);
	}

	draw();
	screen.commit();
}

void SongList::SongList::searchDirectories(File dir){
	if(!dir) return;

	File f;
	while(f = dir.openNextFile()){
		if(f.isDirectory()){
			searchDirectories(f);
			f.close();
			continue;
		}

		String name = f.name();
		name.toLowerCase();
		if(!name.endsWith(".aac") || name[name.lastIndexOf('/') + 1] == '.'){
			f.close();
			continue;
		}

		songs.push_back(new ListItem(list, f.name()));
		list->addChild(songs.back());

		f.close();
	}
}

void SongList::SongList::loop(uint t){
	if(songs[selectedElement]->checkScrollUpdate()) {
		draw();
		screen.commit();
	}
}

void SongList::SongList::start(){

	InputJayD::getInstance()->setEncoderMovedCallback(ENC_MID, [](int8_t value){
		if(instance == nullptr) return;

		if(instance->empty || !instance->insertedSD) return;

		instance->songs[instance->selectedElement]->setSelected(false);
		instance->selectedElement += value;
		if(instance->selectedElement < 0){
			instance->selectedElement = 0;
		}else if(instance->selectedElement >= instance->songs.size()){
			instance->selectedElement = instance->songs.size() - 1;
		}

		instance->songs[instance->selectedElement]->setSelected(true);

		instance->scrollLayout->scrollIntoView(instance->selectedElement, 6);
		instance->draw();
		instance->screen.commit();


	});

	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;

		if(!instance->insertedSD){
			instance->checkSD();
			return;
		}

		if(instance->empty || !instance->insertedSD || instance->songs.size() <= instance->selectedElement) return;

		String path = instance->songs[instance->selectedElement]->getName();
		fs::File file = SD.open(path);
		if(!file){
			file.close();
			SD.end();
			instance->insertedSD = false;
			instance->checkSD();
			return;
		}
		file.close();

		instance->pop(new String(path));
	});

	Input.addListener(this);
	waiting = false;
	checkSD();

	LoopManager::addListener(this);

	draw();
	screen.commit();
}

void SongList::SongList::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(ENC_MID);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_MID);
	Input.removeListener(this);
	LoopManager::removeListener(this);
}

void SongList::SongList::draw(){

	Sprite* canvas = screen.getSprite();

	FontWriter u8f = canvas->startU8g2Fonts();

	u8f.setFont(u8g2_font_DigitalDisco_tf);
	u8f.setForegroundColor(TFT_WHITE);
	u8f.setFontMode(1);

	canvas->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);

	screen.draw();

	canvas->drawIcon(backgroundBuffer, 0, 0, 160, 19, 1);

	u8f.setCursor((160 - u8f.getUTF8Width("SD card")) / 2, 15);
	u8f.printf("SD card");

	if(waiting){
		u8f.setCursor((160 - u8f.getUTF8Width("Loading...")) / 2, 65);
		u8f.printf("Loading...");
		return;
	}

	if(!insertedSD){
		u8f.setCursor((160 - u8f.getUTF8Width("Not inserted!")) / 2, 65);
		u8f.printf("Not inserted!");
	}else if(empty){
		u8f.setCursor((160 - u8f.getUTF8Width("Empty!")) / 2, 65);
		u8f.printf("Empty!");
	}
}

void SongList::SongList::buildUI(){
	scrollLayout->setWHType(PARENT, FIXED);
	scrollLayout->setHeight(110);
	scrollLayout->addChild(list);

	list->setWHType(PARENT, CHILDREN);
	list->setPadding(5);
	list->setGutter(10);

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

	waiting = true;

	backgroundBuffer = static_cast<Color*>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("SongList bg buffer error");
	}

	fs::File bgFile = CompressedFile::open(SPIFFS.open("/SongListBackground.raw.hs"), 10, 9);
	bgFile.read(reinterpret_cast<uint8_t*>(backgroundBuffer), 160 * 128 * 2);
	bgFile.close();
}

void SongList::SongList::encTwoTop(){
	delete parent;
	stop();
	delete this;
	MainMenu::MainMenu::getInstance()->unpack();
	MainMenu::MainMenu::getInstance()->start();
}
