#include <Input/InputJayD.h>
#include <SD.h>
#include <JayD.h>
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
	if(seekTime != 0 && millis() - seekTime >= 50){
		seekTime = 0;
		system->seek(trackCount->getCurrentDuration());

		if(wasRunning){
			system->start();
		}
	}

	bool update = songName->checkScrollUpdate();

	if(system->getElapsed() != trackCount->getCurrentDuration() && seekTime == 0){
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
	songName->setSongName(name.substring(name.lastIndexOf('/') + 1, name.length() - 4));
	trackCount->setTotalDuration(0);
	trackCount->setCurrentDuration(0);
	playOrPause->setPlaying(false);

	draw();
	screen.commit();

	uint8_t potMidVal = InputJayD::getInstance()->getPotValue(POT_MID);
	matrixManager.matrixMid.clear();
	uint8_t total = ((float) potMidVal / 255.0f) * (float) (12);
	for(int i = 0; i <= total + 1; i++){
		for(int j = 0; j < 2; j++){
			matrixManager.matrixMid.drawPixel(i, j, MatrixPixel::White);
		}
	}
	matrixManager.matrixMid.push();

	system = new PlaybackSystem(file);
	system->setVolume(InputJayD::getInstance()->getPotValue(POT_MID));
	system->start();

	playOrPause->setPlaying(true);

	Input.addListener(this);
	InputJayD::getInstance()->addListener(this);
	LoopManager::addListener(this);
	lastDraw = 0;
}

void Playback::Playback::stop(){
	Input.removeListener(this);
	InputJayD::getInstance()->removeListener(this);

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

	trackCount->setY(trackCount->getY() + 3);
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

void Playback::Playback::potMove(uint8_t id, uint8_t value) {
	if(id == POT_MID) {
		if (system) {
			system->setVolume(value);
			matrixManager.matrixMid.clear();
			uint8_t total = ((float) value / 255.0f) * (float) (12);
			for (int i = 0; i <= total + 1; i++) {
				for (int j = 0; j < 2; j++) {
					matrixManager.matrixMid.drawPixel(i, j, MatrixPixel::White);
				}
			}
			matrixManager.matrixMid.push();
		}
	}
}

void Playback::Playback::btnEnc(uint8_t i) {
	if(i == 6) {
		if (playing) {
			system->stop();
			playing = false;
		} else {
			system->start();
			playing = true;
		}
		playOrPause->setPlaying(playing);
		drawQueued = true;
	}
}

void Playback::Playback::enc(uint8_t id, int8_t value) {
	if(id == 6) {
		if(seekTime == 0){
			wasRunning = system->isRunning();
			system->stop();
		}

		seekTime = millis();

		uint16_t seekTime = constrain(trackCount->getCurrentDuration() + value, 0, system->getDuration());
		trackCount->setCurrentDuration(seekTime);

		drawQueued = true;
	}
}

void Playback::Playback::encTwoTop() {
	(new SongList::SongList(*getScreen().getDisplay()))->push(this);
}
