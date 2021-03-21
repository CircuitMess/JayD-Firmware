#include <Input/InputJayD.h>
#include "FirstStartupTest.h"

FirstStartupTest::FirstStartupTest *FirstStartupTest::FirstStartupTest::instance = nullptr;

FirstStartupTest::FirstStartupTest::FirstStartupTest(Display &display) : Context(display), screenLayout(&screen, HORIZONTAL),
																		 leftLayout(&screenLayout, VERTICAL),
																		 midLayout(&screenLayout, HORIZONTAL),
																		 rightLayout(&screenLayout, VERTICAL),
																		 bottomLayout(&screenLayout, HORIZONTAL){

	for(int i = 0; i < 3; i++){
		leftEncBtnTest.push_back(new EncTestElement(&leftLayout, false));
	}

	midEncBtnTest.push_back(new EncTestElement(&midLayout, true));

	for(int i = 0; i < 3; i++){
		rightEncBtnTest.push_back(new EncTestElement(&rightLayout, false));
	}

	for(int i = 0; i < 2; i++){
		bottomBtnTest.push_back(new BtnTestElement(&bottomLayout));
	}


	for(int i = 0; i < 2; i++){
		potMidTest.push_back(new PotTestElement(&midLayout, false));
	}

	potBotTest.push_back(new PotTestElement(&bottomLayout, true));

	instance = this;
	buildUI();

}


void FirstStartupTest::FirstStartupTest::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setBtnPressCallback(0, [](){
		if(instance == nullptr) return;
		instance->bottomBtnTest[0]->btnPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(1, [](){
		if(instance == nullptr) return;
		instance->bottomBtnTest[1]->btnPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(2, [](){
		if(instance == nullptr) return;
		instance->midEncBtnTest[0]->btnEncPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(3, [](){
		if(instance == nullptr) return;
		instance->leftEncBtnTest[0]->btnEncPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(4, [](){
		if(instance == nullptr) return;
		instance->rightEncBtnTest[2]->btnEncPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(5, [](){
		if(instance == nullptr) return;
		instance->rightEncBtnTest[1]->btnEncPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(6, [](){
		if(instance == nullptr) return;
		instance->rightEncBtnTest[0]->btnEncPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(7, [](){
		if(instance == nullptr) return;
		instance->leftEncBtnTest[2]->btnEncPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(8, [](){
		if(instance == nullptr) return;
		instance->leftEncBtnTest[1]->btnEncPress();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setPotMovedCallback(0, [](uint8_t value){
		if(instance == nullptr) return;
		instance->potBotTest[0]->potMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setPotMovedCallback(1, [](uint8_t value){
		if(instance == nullptr) return;
		instance->potMidTest[0]->potMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setPotMovedCallback(2, [](uint8_t value){
		if(instance == nullptr) return;
		instance->potMidTest[1]->potMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(0,[](int8_t value){
		if(instance == nullptr) return;
		instance->midEncBtnTest[0]->encoderMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(1,[](int8_t value){
		if(instance == nullptr) return;
		instance->leftEncBtnTest[0]->encoderMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(2,[](int8_t value){
		if(instance == nullptr) return;
		instance->rightEncBtnTest[2]->encoderMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(3,[](int8_t value){
		if(instance == nullptr) return;
		instance->rightEncBtnTest[1]->encoderMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(4,[](int8_t value){
		if(instance == nullptr) return;
		instance->rightEncBtnTest[0]->encoderMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(5,[](int8_t value){
		if(instance == nullptr) return;
		instance->leftEncBtnTest[2]->encoderMove(value);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(6,[](int8_t value){
		if(instance == nullptr) return;
		instance->leftEncBtnTest[1]->encoderMove(value);
		instance->draw();
		instance->screen.commit();
	});
}

void FirstStartupTest::FirstStartupTest::stop(){
	for(int i = 0; i < 9; i++){
		InputJayD::getInstance()->removeBtnPressCallback(i);
	}
	for(int i=0;i<3;i++){
		InputJayD::getInstance()->removePotMovedCallback(i);
	}
	for(int i=0;i<7;i++){
		InputJayD::getInstance()->removeEncoderMovedCallback(i);
	}

}

void FirstStartupTest::FirstStartupTest::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.draw();
}

void FirstStartupTest::FirstStartupTest::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(1);
	screenLayout.addChild(&leftLayout);
	screenLayout.addChild(&midLayout);
	screenLayout.addChild(&rightLayout);
	screenLayout.addChild(&bottomLayout);

	leftLayout.setWHType(FIXED, PARENT);
	leftLayout.setWidth(26);
//	leftLayout.setBorder(1, TFT_RED);
	leftLayout.setGutter(5);

	for(int i = 0; i < leftEncBtnTest.size(); i++){
		leftLayout.addChild(leftEncBtnTest[i]);
	}

	midLayout.setWHType(FIXED, FIXED);
	midLayout.setWidth(106);
	midLayout.setHeight(60);
//	midLayout.setBorder(1, TFT_RED);
	midLayout.setGutter(5);

	for(int i = 0; i < midEncBtnTest.size(); i++){
		midLayout.addChild(midEncBtnTest[i]);
	}

	for(int i = 0; i < potMidTest.size(); i++){
		midLayout.addChild(potMidTest[i]);
	}


	bottomLayout.setWHType(FIXED, FIXED);
	bottomLayout.setHeight(60);
	bottomLayout.setWidth(106);
	//bottomLayout.setBorder(1, TFT_RED);
	bottomLayout.setGutter(5);

	for(int i = 0; i < bottomBtnTest.size(); i++){
		bottomLayout.addChild(bottomBtnTest[i]);
	}

	rightLayout.setWHType(FIXED, PARENT);
	rightLayout.setWidth(26);
	//rightLayout.setBorder(1, TFT_RED);
	rightLayout.setGutter(5);

	for(int i = 0; i < rightEncBtnTest.size(); i++){
		rightLayout.addChild(rightEncBtnTest[i]);
	}

	screenLayout.reflow();
	leftLayout.reflow();
	midLayout.reflow();
	bottomLayout.reflow();
	rightLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();

	bottomLayout.setPos(27, 61);

	bottomLayout.addChild(potBotTest[0]);

}

FirstStartupTest::FirstStartupTest::~FirstStartupTest(){
	instance = nullptr;
}


