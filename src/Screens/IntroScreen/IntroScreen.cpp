#include "IntroScreen.h"
#include <FS.h>
#include <Loop/LoopManager.h>
#include <FS/CompressedFile.h>
#include "../MainMenu/MainMenu.h"
#include <SPIFFS.h>
#include <AudioLib/Systems/PlaybackSystem.h>
#include <Settings.h>


IntroScreen::IntroScreen *IntroScreen::IntroScreen::instance = nullptr;


IntroScreen::IntroScreen::IntroScreen(Display &display) : Context(display){
	instance = this;

	fs::File f = SPIFFS.open("/intro.g565.hs");
	if(!f){
		Serial.println("Error opening intro gif");
		pack();
		return;
	}

	gif = new AnimatedSprite(screen.getSprite(), CompressedFile::open(f, 9, 8, 34537));
	gif->setSwapBytes(true);
	gif->setXY(0, 0);

	pack();
}

IntroScreen::IntroScreen::~IntroScreen(){
	delete gif;
	instance = nullptr;
}

void IntroScreen::IntroScreen::draw(){
	gif->nextFrame();
	gif->push();
}

void IntroScreen::IntroScreen::start(){
	draw();
	screen.commit();

	if(!gif) return;

	gif->setLoopDoneCallback([]{
		if(instance == nullptr) return;

		Display& display = *instance->getScreen().getDisplay();

		instance->stop();
		delete instance;

		MainMenu::MainMenu* main = new MainMenu::MainMenu(display);
		main->unpack();
		main->start();
	});

	f1 = SPIFFS.open("/intro.aac");
	playback = new PlaybackSystem(f1);
	playback->setVolume(Settings.get().volumeLevel);
	playback->setRepeat(true);
	playback->start();

	LoopManager::addListener(this);
}

void IntroScreen::IntroScreen::stop(){
	LoopManager::removeListener(this);
	playback->stop();
	delete playback;
}

void IntroScreen::IntroScreen::loop(uint micros){
	if(!gif || !gif->checkFrame()) return;

	draw();
	screen.commit();
}


