#include "MatrixPopUpPicker.h"
#include <JayD.hpp>
#include <Loop/LoopManager.h>
#include <Input/InputJayD.h>

MixScreen::MatrixPopUpPicker* MixScreen::MatrixPopUpPicker::instance = nullptr;


MixScreen::MatrixPopUpPicker::MatrixPopUpPicker(Context& context) : Modal(context, 100, 100),
																	screenLayout(&screen, VERTICAL),
																	parent(&context){

	instance = this;

	buildUI();
}

MixScreen::MatrixPopUpPicker::~MatrixPopUpPicker(){

}

void MixScreen::MatrixPopUpPicker::start(){
	draw();
	screen.commit();
	LoopManager::addListener(this);
	InputJayD::getInstance()->setEncoderMovedCallback(ENC_MID, [](int8_t value){
		if(instance == nullptr) return;
		instance->bigMatrixNumber += value;
		if(instance->bigMatrixNumber < 1){
			instance->bigMatrixNumber = 20;
		}else if(instance->bigMatrixNumber > 20){
			instance->bigMatrixNumber = 1;
		}
		instance->openGif(instance->bigMatrixNumber);
	});
	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;
		Context* parent = instance->parent;
		instance->stop();
		delete instance;
		parent->unpack();
		parent->start();
	});


	//matrixManager.matrixBig.startAnimation(new Animation(new fs::File(SPIFFS.open(buffer))), true);

}

void MixScreen::MatrixPopUpPicker::stop(){
	InputJayD::getInstance()->removeBtnPressCallback(BTN_MID);
	InputJayD::getInstance()->removeEncoderMovedCallback(ENC_MID);
	LoopManager::removeListener(this);


}


void MixScreen::MatrixPopUpPicker::unpack(){
	Context::unpack();
	//sprintf(buffer, "/matrixGIF/%s%d.gif", "big", bigMatrixNumber);
	openGif(bigMatrixNumber);
	Serial.println(bigMatrixNumber);

}

void MixScreen::MatrixPopUpPicker::draw(){
	screen.draw();
	matrixGif->push();

	/*screen.getSprite()->fillTriangle(screen.getTotalX() + 83, screen.getTotalY() + 46 , screen.getTotalX() + 97,
									 screen.getTotalY() + 39,
									 screen.getTotalX() + 83, screen.getTotalY() + 53 , TFT_WHITE);
	screen.getSprite()->drawTriangle(screen.getTotalX() + 83, screen.getTotalY() + 46 , screen.getTotalX() + 97,
									 screen.getTotalY() + 39,
									 screen.getTotalX() + 83, screen.getTotalY() + 53 , TFT_WHITE);*/


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
	Serial.println(bufferGif);
	matrixGif = new GIFAnimatedSprite(screenLayout.getSprite(), fs::File(SPIFFS.open(bufferGif)));
	matrixGif->setXY(screenLayout.getTotalX() + 17, screenLayout.getTotalY() + 12);
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




