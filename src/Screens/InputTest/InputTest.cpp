#include <Input/InputJayD.h>
#include "InputTest.h"
#include "../IntroScreen/IntroScreen.h"
#include <Settings.h>

InputTest::InputTest* InputTest::InputTest::instance = nullptr;


InputTest::InputTest::InputTest(Display& display) : Context(display), screenLayout(new LinearLayout(&screen, HORIZONTAL)),
													leftLayout(new LinearLayout(screenLayout, VERTICAL)),
													midLayout(new LinearLayout(screenLayout, HORIZONTAL)),
													rightLayout(new LinearLayout(screenLayout, VERTICAL)),
													bottomLayout(new LinearLayout(screenLayout, HORIZONTAL)){

	for(int i = 0; i < 3; i++){
		leftEncBtnTest.push_back(new EncTestElement(leftLayout, false));
	}

	midEncBtnTest.push_back(new EncTestElement(midLayout, true));

	for(int i = 0; i < 3; i++){
		rightEncBtnTest.push_back(new EncTestElement(rightLayout, false));
	}

	for(int i = 0; i < 2; i++){
		bottomBtnTest.push_back(new BtnTestElement(bottomLayout));
	}

	for(int i = 0; i < 2; i++){
		potMidTest.push_back(new PotTestElement(midLayout, false));
	}

	potBotTest.push_back(new PotTestElement(bottomLayout, true));

	instance = this;
	buildUI();

}


void InputTest::InputTest::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->addListener(this);
	InputJayD::getInstance()->setBtnPressCallback(0, [](){
		if(instance == nullptr) return;
		if(instance->bottomBtnTest[0]->isBtnPressed()) return;
		instance->bottomBtnTest[0]->btnPress();
		if(instance->bottomBtnTest[0]->isBtnPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setBtnPressCallback(1, [](){
		if(instance == nullptr) return;
		if(instance->bottomBtnTest[1]->isBtnPressed()) return;
		instance->bottomBtnTest[1]->btnPress();
		if(instance->bottomBtnTest[1]->isBtnPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setBtnPressCallback(2, [](){
		if(instance == nullptr) return;
		if(instance->midEncBtnTest[0]->isEncPressed()) return;
		instance->midEncBtnTest[0]->btnEncPress();
		if(instance->midEncBtnTest[0]->isEncPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setBtnPressCallback(3, [](){
		if(instance == nullptr) return;
		if(instance->leftEncBtnTest[0]->isEncPressed()) return;
		instance->leftEncBtnTest[0]->btnEncPress();
		if(instance->leftEncBtnTest[0]->isEncPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setBtnPressCallback(4, [](){
		if(instance == nullptr) return;
		if(instance->rightEncBtnTest[2]->isEncPressed()) return;
		instance->rightEncBtnTest[2]->btnEncPress();
		if(instance->rightEncBtnTest[2]->isEncPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setBtnPressCallback(5, [](){
		if(instance == nullptr) return;
		if(instance->rightEncBtnTest[1]->isEncPressed()) return;
		instance->rightEncBtnTest[1]->btnEncPress();
		if(instance->rightEncBtnTest[1]->isEncPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setBtnPressCallback(6, [](){
		if(instance == nullptr) return;
		if(instance->rightEncBtnTest[0]->isEncPressed()) return;
		instance->rightEncBtnTest[0]->btnEncPress();
		if(instance->rightEncBtnTest[0]->isEncPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setBtnPressCallback(7, [](){
		if(instance == nullptr) return;
		if(instance->leftEncBtnTest[2]->isEncPressed()) return;
		instance->leftEncBtnTest[2]->btnEncPress();
		if(instance->leftEncBtnTest[2]->isEncPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setBtnPressCallback(8, [](){
		if(instance == nullptr) return;
		if(instance->leftEncBtnTest[1]->isEncPressed()) return;
		instance->leftEncBtnTest[1]->btnEncPress();
		if(instance->leftEncBtnTest[1]->isEncPressed()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setPotMovedCallback(0, [](uint8_t value){
		if(instance == nullptr) return;
		if(instance->potBotTest[0]->isPotDone()) return;
		instance->potBotTest[0]->potMove(value);
		if(instance->potBotTest[0]->isPotDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setPotMovedCallback(1, [](uint8_t value){
		if(instance == nullptr) return;
		if(instance->potMidTest[0]->isPotDone()) return;
		instance->potMidTest[0]->potMove(value);
		if(instance->potMidTest[0]->isPotDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setPotMovedCallback(2, [](uint8_t value){
		if(instance == nullptr) return;
		if(instance->potMidTest[1]->isPotDone()) return;
		instance->potMidTest[1]->potMove(value);
		if(instance->potMidTest[1]->isPotDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->midEncBtnTest[0]->isEncValueDone()) return;
		instance->midEncBtnTest[0]->encoderMove(value);
		if(instance->midEncBtnTest[0]->isEncValueDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->leftEncBtnTest[0]->isEncValueDone()) return;
		instance->leftEncBtnTest[0]->encoderMove(value);
		if(instance->leftEncBtnTest[0]->isEncValueDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(2, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->leftEncBtnTest[2]->isEncValueDone()) return;
		instance->rightEncBtnTest[2]->encoderMove(value);
		if(instance->leftEncBtnTest[2]->isEncValueDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(3, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->rightEncBtnTest[1]->isEncValueDone()) return;
		instance->rightEncBtnTest[1]->encoderMove(value);
		if(instance->rightEncBtnTest[1]->isEncValueDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(4, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->rightEncBtnTest[0]->isEncValueDone()) return;
		instance->rightEncBtnTest[0]->encoderMove(value);
		if(instance->rightEncBtnTest[0]->isEncValueDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(5, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->leftEncBtnTest[2]->isEncValueDone()) return;
		instance->leftEncBtnTest[2]->encoderMove(value);
		if(instance->leftEncBtnTest[2]->isEncValueDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(6, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->leftEncBtnTest[1]->isEncValueDone()) return;
		instance->leftEncBtnTest[1]->encoderMove(value);
		if(instance->leftEncBtnTest[1]->isEncValueDone()){
			instance->doneCounter++;
		}
		instance->draw();
		instance->screen.commit();
		instance->checkIfDone();
	});

}

void InputTest::InputTest::stop(){
	for(int i = 0; i < 9; i++){
		InputJayD::getInstance()->removeBtnPressCallback(i);
	}
	for(int i = 0; i < 3; i++){
		InputJayD::getInstance()->removePotMovedCallback(i);
	}
	for(int i = 0; i < 7; i++){
		InputJayD::getInstance()->removeEncoderMovedCallback(i);
	}
	InputJayD::getInstance()->removeListener(this);
}

void InputTest::InputTest::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screenLayout->draw();
	screenLayout->getSprite()->setTextColor(TFT_GREEN);
	screenLayout->getSprite()->setTextSize(3);
	screenLayout->getSprite()->setTextFont(1);
	screenLayout->getSprite()->setCursor(screenLayout->getTotalX() + 30, screenLayout->getTotalY() + 20);

	if(doneCounter >= 18){
		screenLayout->getSprite()->fillRect(screenLayout->getTotalX(), screenLayout->getTotalY(), 160, 128, TFT_BLACK);
		screenLayout->getSprite()->println("ALL OK\n  PRESS\n ANY KEY TO EXIT");

	}

}

void InputTest::InputTest::buildUI(){
	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(1);
	screenLayout->addChild(leftLayout);
	screenLayout->addChild(midLayout);
	screenLayout->addChild(rightLayout);
	screenLayout->addChild(bottomLayout);

	leftLayout->setWHType(FIXED, PARENT);
	leftLayout->setWidth(26);
	leftLayout->setGutter(5);

	for(int i = 0; i < leftEncBtnTest.size(); i++){
		leftLayout->addChild(leftEncBtnTest[i]);
	}

	midLayout->setWHType(FIXED, FIXED);
	midLayout->setWidth(106);
	midLayout->setHeight(60);
	midLayout->setGutter(5);

	for(int i = 0; i < midEncBtnTest.size(); i++){
		midLayout->addChild(midEncBtnTest[i]);
	}

	for(int i = 0; i < potMidTest.size(); i++){
		midLayout->addChild(potMidTest[i]);
	}


	bottomLayout->setWHType(FIXED, FIXED);
	bottomLayout->setHeight(60);
	bottomLayout->setWidth(106);
	bottomLayout->setGutter(5);

	for(int i = 0; i < bottomBtnTest.size(); i++){
		bottomLayout->addChild(bottomBtnTest[i]);
	}


	rightLayout->setWHType(FIXED, PARENT);
	rightLayout->setWidth(26);
	rightLayout->setGutter(5);

	for(int i = 0; i < rightEncBtnTest.size(); i++){
		rightLayout->addChild(rightEncBtnTest[i]);
	}

	screenLayout->reflow();
	leftLayout->reflow();
	midLayout->reflow();
	bottomLayout->reflow();
	rightLayout->reflow();

	screen.addChild(screenLayout);
	screen.repos();

	bottomLayout->setPos(27, 61);

	bottomLayout->addChild(potBotTest[0]);

}

InputTest::InputTest::~InputTest(){
	instance = nullptr;
}

void InputTest::InputTest::checkIfDone(){
	if(doneCounter >= 18){
		checkDone = true;
	}
}

void InputTest::InputTest::buttonPress(uint8_t id){
	checkIfTestIsDone();
}

void InputTest::InputTest::encoderMove(uint8_t id, int8_t value){
	checkIfTestIsDone();
}

void InputTest::InputTest::buttonHold(uint8_t id){

}


void InputTest::InputTest::buttonRelease(uint8_t id){

}

void InputTest::InputTest::potMove(uint8_t id, uint8_t value){
	checkIfTestIsDone();
}

void InputTest::InputTest::setDoneCallback(void (* doneCallback)(InputTest*)){
	InputTest::doneCallback = doneCallback;
}

void InputTest::InputTest::checkIfTestIsDone(){
	if(checkDone){
		confirmedCounter++;
		if(confirmedCounter == 2){
			if(doneCallback){
				doneCallback(this);
			}else{
				this->pop();
			}
		}
	}
}

