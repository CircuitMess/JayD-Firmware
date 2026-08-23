#include "MatrixPopUpPicker.h"
#include <JayD.h>
#include <Loop/LoopManager.h>
#include <Input/InputJayD.h>
#include "MixScreen.h"
#include <Devices/Matrix/MatrixAnimGIF.h>
#if defined(JAYD_ENABLE_WIRELESS)
#include "../../Wireless/WirelessBringup.h"
#endif

MixScreen::MatrixPopUpPicker* MixScreen::MatrixPopUpPicker::instance = nullptr;


MixScreen::MatrixPopUpPicker::MatrixPopUpPicker(Context& context) : Modal(context, 100, 100),
																	screenLayout(&screen, VERTICAL),
																	parent(static_cast<MixScreen*>(&context)){

	instance = this;

	buildUI();
}

MixScreen::MatrixPopUpPicker::~MatrixPopUpPicker(){
	delete gif;
	instance = nullptr;
}

void MixScreen::MatrixPopUpPicker::btnEnc(uint8_t i){
	if(i != 6) return;

#if defined(JAYD_ENABLE_WIRELESS)
	if(bigMatrixNumber == 0 && !pairingRequested){
		if(parent->session &&
		   parent->session->requestPairing(DJ_ORIGIN_LOCAL_UI).accepted()){
			pairingRequested = true;
			draw();
			screen.commit();
		}
		return;
	}
#endif

	MixScreen* parent = this->parent;
	int8_t matrixAnimationNumber = bigMatrixNumber;

	stop();
	delete this;

	if(matrixAnimationNumber == 2){
		delete anim;
		parent->setBigVuStarted(true);
	}
	parent->unpack();
	parent->start();
}

void MixScreen::MatrixPopUpPicker::enc(uint8_t i, int8_t value){
	if(i != 6) return;
#if defined(JAYD_ENABLE_WIRELESS)
	if(pairingRequested) return;
#endif

	bigMatrixNumber += value;
#if defined(JAYD_ENABLE_WIRELESS)
	static constexpr int8_t firstChoice = 0;
#else
	static constexpr int8_t firstChoice = 1;
#endif
	if(bigMatrixNumber < firstChoice){
		bigMatrixNumber = 20;
	}else if(bigMatrixNumber > 20){
		bigMatrixNumber = firstChoice;
	}

	openGif(bigMatrixNumber);
}

void MixScreen::MatrixPopUpPicker::start(){
	LoopManager::addListener(this);
	Input.addListener(this);
	parent->setBigVuStarted(false);
	draw();
	screen.commit();
}

void MixScreen::MatrixPopUpPicker::stop(){
	Input.removeListener(this);
	LoopManager::removeListener(this);
}

void MixScreen::MatrixPopUpPicker::pack(){
	Context::pack();
	delete gif;
}

void MixScreen::MatrixPopUpPicker::unpack(){
	Context::unpack();
	openGif(bigMatrixNumber);
}

void MixScreen::MatrixPopUpPicker::draw(){
	screen.draw();
	screen.getSprite()->clear(C_RGB(52, 204, 235));
	screen.getSprite()->drawRect(screen.getTotalX(), screen.getTotalY(), 100, 100, TFT_BLACK);
	screen.getSprite()->setTextColor(TFT_BLACK);
	screen.getSprite()->setTextSize(1);
	screen.getSprite()->setTextFont(1);
#if defined(JAYD_ENABLE_WIRELESS)
	if(bigMatrixNumber == 0){
		screen.getSprite()->setCursor(screen.getTotalX() + 20, screen.getTotalY() + 8);
		screen.getSprite()->println("Pair remote");
		if(!pairingRequested){
			screen.getSprite()->setCursor(screen.getTotalX() + 13, screen.getTotalY() + 38);
			screen.getSprite()->println("Press center");
			screen.getSprite()->setCursor(screen.getTotalX() + 19, screen.getTotalY() + 52);
			screen.getSprite()->println("for a code");
		}else{
			WirelessPairingStatus status;
			const bool open = WirelessBringup::copyPairingStatus(status);
			pairingWasOpen |= open;
			if(open){
				screen.getSprite()->setCursor(screen.getTotalX() + 35, screen.getTotalY() + 30);
				screen.getSprite()->println("Code");
				screen.getSprite()->setTextSize(2);
				screen.getSprite()->setCursor(screen.getTotalX() + 14, screen.getTotalY() + 43);
				screen.getSprite()->println(status.code);
				screen.getSprite()->setTextSize(1);
				screen.getSprite()->setCursor(screen.getTotalX() + 31, screen.getTotalY() + 68);
				screen.getSprite()->printf("%lus left", static_cast<unsigned long>((status.remainingMs + 999) / 1000));
			}else{
				screen.getSprite()->setCursor(screen.getTotalX() + (pairingWasOpen ? 29 : 25), screen.getTotalY() + 42);
				screen.getSprite()->println(pairingWasOpen ? "Pairing closed" : "Opening...");
			}
			screen.getSprite()->setCursor(screen.getTotalX() + 12, screen.getTotalY() + 82);
			screen.getSprite()->println("Center closes");
		}
		return;
	}
#endif
	screen.getSprite()->setCursor(screen.getTotalX() + 25, screen.getTotalY() + 2);
	screen.getSprite()->println("Choose an    animation mode");
	if(gif) gif->push();

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
	delete gif;
	gif = nullptr;
	delete anim;
	anim = nullptr;

#if defined(JAYD_ENABLE_WIRELESS)
	if(gifNum == 0) return;
#endif

	char filename[25];
	sprintf(filename, "/matrixGIF/big%d.gif", gifNum);

	anim = new MatrixAnimGIF(SPIFFS.open(filename));
	matrixManager.matrixBig.startAnimation(anim);

	gif = new GIFAnimatedSprite(screenLayout.getSprite(), SPIFFS.open(filename));
	gif->setXY(screenLayout.getTotalX() + 19, screenLayout.getTotalY() + 19);
	gif->setScale(8);
	gif->setLoopMode(GIF::Infinite);
	gif->start();

}


void MixScreen::MatrixPopUpPicker::loop(uint micros){
	draw();
	screen.commit();
}
