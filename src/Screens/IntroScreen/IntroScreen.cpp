#include "IntroScreen.h"
#include <FS.h>
#include <Loop/LoopManager.h>
#include <FS/CompressedFile.h>
#include "../MainMenu/MainMenu.h"
#include <SPIFFS.h>
#include <AudioLib/Systems/PlaybackSystem.h>
#include <Settings.h>
#include <JayD.hpp>


IntroScreen::IntroScreen *IntroScreen::IntroScreen::instance = nullptr;


IntroScreen::IntroScreen::IntroScreen(Display &display) : Context(display){
	instance = this;

	fs::File f = SPIFFS.open("/intro.g565.hs");
	if(!f){
		Serial.println("Error opening intro gif");
		return;
	}

	gif = new AnimatedSprite(screen.getSprite(), CompressedFile::open(f, 9, 8, 33734));
	gif->setSwapBytes(true);
	gif->setXY(0, 0);

	IntroScreen::pack();
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

	introSong = SPIFFS.open("/intro.aac");
	playback = new PlaybackSystem(introSong);
	playback->setVolume(Settings.get().volumeLevel);
	playback->start();

	LoopManager::addListener(this);
	matrixManager.startRandom();

	draw();
	screen.commit();
}

void IntroScreen::IntroScreen::stop(){
	LoopManager::removeListener(this);
	playback->stop();
	delete playback;
	introSong.close();
}

void IntroScreen::IntroScreen::loop(uint micros){
	if(!gif || !gif->checkFrame()) return;

	draw();
	screen.commit();
}


