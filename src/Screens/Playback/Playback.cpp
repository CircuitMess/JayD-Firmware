#include <Input/InputJayD.h>
#include <SD.h>
#include <JayD.hpp>
#include <Loop/LoopManager.h>
#include "Playback.h"
#include <SPIFFS.h>
#include <FS/CompressedFile.h>

Playback::Playback *Playback::Playback::instance = nullptr;

Playback::Playback::Playback(Display& display) : Context(display), screenLayout(new LinearLayout(&screen, VERTICAL)),
												 songNameLayout(new LinearLayout(screenLayout, HORIZONTAL)),
												 timeElapsedLayout(new LinearLayout(screenLayout, HORIZONTAL)), buttonLayout(new LinearLayout(
				screenLayout, HORIZONTAL)), songName(new SongName(songNameLayout)), playOrPause(new PlayPause(buttonLayout)),
												 trackCount(new TrackCounter(timeElapsedLayout)){


	instance = this;
	buildUI();

	Playback::pack();
}

Playback::Playback::~Playback(){
	instance = nullptr;
	free(backgroundBuffer);
}

void Playback::Playback::loop(uint micros){
	bool update = songName->checkScrollUpdate();

	if(system->getElapsed() != trackCount->getCurrentDuration()){
		update = true;
		trackCount->setCurrentDuration(system->getElapsed());
	}

	if(system->getDuration() != trackCount->getTotalDuration()){
		update = true;
		trackCount->setTotalDuration(system->getDuration());
	}

	uint32_t currentTime = millis();
	if((update || drawQueued) && (currentTime - lastDraw) >= 100){
		drawQueued = false;
		draw();
		screen.commit();
	}else if(update){
		drawQueued = true;
	}
}

void Playback::Playback::returned(void *data){
	String *name = static_cast<String *>(data);
	file = SD.open(name->c_str());

	delete name;
}

void Playback::Playback::start(){
	if(!file){
		(new SongList::SongList(*screen.getDisplay()))->push(this);
		return;
	}

	String name = file.name();
	songName->setSongName(name.substring(1, name.length() - 4));
	trackCount->setTotalDuration(0);
	trackCount->setCurrentDuration(0);
	playOrPause->setPlaying(false);
	draw();
	screen.commit();
	uint8_t potMidVal = InputJayD::getInstance()->getPotValue(POT_MID);
	matrixManager.matrixMid.vu(potMidVal);
	matrixManager.matrixMid.push();
	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;
		if(instance->playing){
			instance->system->pause();
			instance->playing = false;
		}else{
			instance->system->resume();
			instance->playing = true;
		}

		instance->playOrPause->setPlaying(instance->playing);

		instance->draw();
		instance->screen.commit();
	});

	InputJayD::getInstance()->setEncoderMovedCallback(ENC_MID, [](int8_t value){
		if(instance == nullptr) return;
		uint16_t seekTime = max(0, instance->trackCount->getCurrentDuration() + value);
		if(seekTime >= 0 && seekTime <= instance->trackCount->getTotalDuration() ) {
			instance->system->seek(seekTime);
		}
	});

	InputJayD::getInstance()->setPotMovedCallback(POT_MID, [](uint8_t value){
		if(instance && instance->system){
			instance->system->setVolume(value);
			matrixManager.matrixMid.vu(value);
			matrixManager.matrixMid.push();
		}
	});

	system = new PlaybackSystem(file);
	system->setVolume(InputJayD::getInstance()->getPotValue(POT_MID));
	system->start();
	system->pause();

	playOrPause->setPlaying(false);
	trackCount->setTotalDuration(system->getDuration());

	LoopManager::addListener(this);
	lastDraw = 0;
}

void Playback::Playback::stop(){
	InputJayD::getInstance()->removeBtnPressCallback(BTN_L1);
	InputJayD::getInstance()->removeEncoderMovedCallback(ENC_L1);
	InputJayD::getInstance()->removePotMovedCallback(POT_MID);

	LoopManager::removeListener(this);

	if(system){
		system->stop();
		delete system;
		system = nullptr;
	}

	playing = false;
	file.close();
}


void Playback::Playback::draw(){
	screen.getSprite()->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);
	screen.draw();
}

void Playback::Playback::buildUI(){
	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(5);
	screenLayout->addChild(songNameLayout);
	screenLayout->addChild(timeElapsedLayout);
	screenLayout->addChild(buttonLayout);

	songNameLayout->setWHType(PARENT, FIXED);
	songNameLayout->setHeight(35);
	songNameLayout->setGutter(5);
	songNameLayout->setPadding(5);
	songNameLayout->addChild(songName);

	timeElapsedLayout->setWHType(PARENT, FIXED);
	timeElapsedLayout->setHeight(20);
	timeElapsedLayout->setGutter(5);
	timeElapsedLayout->addChild(trackCount);

	buttonLayout->setWHType(PARENT, FIXED);
	buttonLayout->setHeight(62);
	buttonLayout->setGutter(5);
	buttonLayout->addChild(playOrPause);

	screenLayout->reflow();
	timeElapsedLayout->reflow();
	songNameLayout->reflow();
	buttonLayout->reflow();

	screen.addChild(screenLayout);
	screen.repos();

}

void Playback::Playback::pack(){
	Context::pack();
	free(backgroundBuffer);
	backgroundBuffer = nullptr;
}

void Playback::Playback::unpack(){
	Context::unpack();

	backgroundBuffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("Playback background unpack error");
		return;
	}

	fs::File bgFile = CompressedFile::open(SPIFFS.open("/playbackBackground.raw.hs"), 10, 9);
	bgFile.read(reinterpret_cast<uint8_t *>(backgroundBuffer), 160 * 128 * 2);
	bgFile.close();
}

