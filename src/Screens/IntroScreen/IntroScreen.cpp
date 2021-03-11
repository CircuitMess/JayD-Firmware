#include "IntroScreen.h"
#include "Bitmaps/introGIF.h"
#include <FS.h>
#include <Display/PGMFile.h>


IntroScreen::IntroScreen *IntroScreen::IntroScreen::instance = nullptr;

IntroScreen::IntroScreen::IntroScreen(Display &display) : Context(display), gifIntro(screen.getSprite(), fs::File(std::make_shared<PGMFile>(intro_gif, sizeof(intro_gif)))){

	instance = this;
	gifIntro.setXY(screen.getTotalX(), screen.getTotalY());

	pack();

}

void IntroScreen::IntroScreen::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void IntroScreen::IntroScreen::start(){

	draw();
	screen.commit();
	gifIntro.setLoopDoneCallback([]{
		instance->stop();
		delete instance;
	});

}

void IntroScreen::IntroScreen::stop(){

}

void IntroScreen::IntroScreen::loop(uint micros){

	if(gifIntro.newFrameReady()){
	//	gifIntro.push();
		draw();
		screen.commit();
	}

}


