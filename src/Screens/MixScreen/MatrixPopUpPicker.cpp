#include "MatrixPopUpPicker.h"
#include <JayD.hpp>
#include <Loop/LoopManager.h>
#include <Input/InputJayD.h>
#include "MixScreen.h"

MixScreen::MatrixPopUpPicker* MixScreen::MatrixPopUpPicker::instance = nullptr;


MixScreen::MatrixPopUpPicker::MatrixPopUpPicker(Context& context) : Modal(context, 100, 100),
																	screenLayout(&screen, VERTICAL),
																	parent(static_cast<MixScreen*>(&context)){

	instance = this;

	buildUI();
}

MixScreen::MatrixPopUpPicker::~MatrixPopUpPicker(){

}

void MixScreen::MatrixPopUpPicker::btnEnc(uint8_t i){
	if(i != 6) return;

	MixScreen* parent = this->parent;
	int8_t matrixAnimationNumber = bigMatrixNumber;

	stop();
	delete this;

	parent->unpack();
	parent->start();

	if(matrixAnimationNumber == 2){
		matrixManager.matrixBig.stopAnimation();
		parent->startBigVu();
	}
}

void MixScreen::MatrixPopUpPicker::enc(uint8_t i, int8_t value){
	if(i != 6) return;

	bigMatrixNumber += value;
	if(bigMatrixNumber < 1){
		bigMatrixNumber = 20;
	}else if(bigMatrixNumber > 20){
		bigMatrixNumber = 1;
	}

	openGif(bigMatrixNumber);
}

void MixScreen::MatrixPopUpPicker::start(){
	LoopManager::addListener(this);
	Input.addListener(this);
	draw();
	screen.commit();
}

void MixScreen::MatrixPopUpPicker::stop(){
	Input.removeListener(this);
	LoopManager::removeListener(this);
}


void MixScreen::MatrixPopUpPicker::unpack(){
	Context::unpack();
	openGif(bigMatrixNumber);
}

void MixScreen::MatrixPopUpPicker::draw(){
	screen.draw();
	screen.getSprite()->clear(C_RGB(52, 204, 235));
	screen.getSprite()->drawRect(screen.getTotalX(),screen.getTotalY(),100,100,TFT_BLACK);
	screen.getSprite()->setTextColor(TFT_BLACK);
	screen.getSprite()->setTextSize(1);
	screen.getSprite()->setTextFont(1);
	screen.getSprite()->setCursor(screen.getTotalX()+25 , screen.getTotalY()+2);
	screen.getSprite()->println("Choose an    animation mode");
	matrixGif->push();

	screen.getSprite()->fillTriangle(screen.getTotalX() + 88, screen.getTotalY() + 54, screen.getTotalX() + 97,
									 screen.getTotalY() + 50,
									 screen.getTotalX() + 88, screen.getTotalY() + 46, TFT_WHITE);
	screen.getSprite()->drawTriangle(screen.getTotalX() + 88, screen.getTotalY() + 55, screen.getTotalX() + 97,
									 screen.getTotalY() + 50,
									 screen.getTotalX() + 88, screen.getTotalY() + 46, TFT_BLACK);

	screen.getSprite()->fillTriangle(screen.getTotalX() + 12, screen.getTotalY() + 54, screen.getTotalX() + 3,
									 screen.getTotalY() + 50,
									 screen.getTotalX() + 12, screen.getTotalY() + 46, TFT_WHITE);
	screen.getSprite()->drawTriangle(screen.getTotalX() + 12, screen.getTotalY() + 54, screen.getTotalX() + 3,
									 screen.getTotalY() + 50,
									 screen.getTotalX() + 12, screen.getTotalY() + 46, TFT_BLACK);
}

void MixScreen::MatrixPopUpPicker::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.reflow();
	screen.repos();
}

void MixScreen::MatrixPopUpPicker::openGif(uint8_t gifNum){
	delete matrixGif;
	char bufferGif[25];
	sprintf(bufferGif, "/matrixGIF/%s%d.gif", "big", gifNum);
	matrixManager.matrixBig.startAnimation(new Animation(new fs::File(SPIFFS.open(bufferGif))), true);
	matrixGif = new GIFAnimatedSprite(screenLayout.getSprite(), fs::File(SPIFFS.open(bufferGif)));
	matrixGif->setXY(screenLayout.getTotalX() + 19, screenLayout.getTotalY() + 19);
	matrixGif->setScale(8);
	matrixGif->setLoopDoneCallback([](){
		instance->matrixGif->reset();
	});

}


void MixScreen::MatrixPopUpPicker::loop(uint micros){
	if(matrixGif->newFrameReady()){
		draw();
		screen.commit();
	}
}
