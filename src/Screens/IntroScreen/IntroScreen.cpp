#include "IntroScreen.h"
#include "Bitmaps/introGIF.h"
#include <FS.h>
#include <FS/PGMFile.h>
#include <Loop/LoopManager.h>
#include "../MainMenu/MainMenu.h"


IntroScreen::IntroScreen *IntroScreen::IntroScreen::instance = nullptr;


IntroScreen::IntroScreen::IntroScreen(Display &display) : Context(display), gifIntro(screen.getSprite(), fs::File(std::make_shared<PGMFile>(intro_gif, sizeof(intro_gif)))){

	instance = this;

	gifIntro.setXY(screen.getTotalX(), screen.getTotalY());

	pack();
}

IntroScreen::IntroScreen::~IntroScreen(){
	instance = nullptr;
}

void IntroScreen::IntroScreen::draw(){
	gifIntro.push();
}

void IntroScreen::IntroScreen::start(){
	draw();
	screen.commit();

	gifIntro.setLoopDoneCallback([]{
		if(instance == nullptr) return;

		Display& display = *instance->getScreen().getDisplay();

		instance->stop();
		delete instance;

		MainMenu::MainMenu* main = new MainMenu::MainMenu(display);
		main->unpack();
		main->start();
	});

	LoopManager::addListener(this);
}

void IntroScreen::IntroScreen::stop(){
	LoopManager::removeListener(this);
}

void IntroScreen::IntroScreen::loop(uint micros){
	if(!gifIntro.newFrameReady()) return;

	draw();
	screen.commit();
}


