#include <Arduino.h>
#include <CircuitOS.h>
#include <JayD.h>
#include <Display/Display.h>
#include <Settings.h>
#include <Loop/LoopManager.h>
#include <Input/InputJayD.h>
#include "src/InputKeys.h"
#include "src/HardwareTest.h"
#include "src/Screens/IntroScreen/IntroScreen.h"
#include "src/Screens/InputTest/InputTest.h"

bool checkJig(){
	pinMode(PIN_BL, INPUT_PULLUP);
	return digitalRead(PIN_BL) == LOW;
}

void launch(){
	Context* introScreen = new IntroScreen::IntroScreen(JayD.getDisplay());
	introScreen->unpack();
	introScreen->start();
}

void setup(){
	Serial.begin(115200);

	if(checkJig()){
		pinMode(PIN_BL, OUTPUT);
		digitalWrite(PIN_BL, LOW);

		Display display(160, 128, -1, -1);
		display.getTft()->setPanel(JayDDisplay::panel1());
		display.begin();

		HardwareTest test(display);
		test.start();

		for(;;);
	}

	pinMode(PIN_BL, OUTPUT);
	digitalWrite(PIN_BL, HIGH);

	JayD.begin();

	InputJayD::getInstance()->addListener(&Input);
	LoopManager::addListener(&Input);

	Context::setDeleteOnPop(true);

	if(!Settings.get().inputTested){
		InputTest::InputTest* test = new InputTest::InputTest(JayD.getDisplay());
		test->setDoneCallback([](InputTest::InputTest* test){
			Settings.get().inputTested = true;
			Settings.store();

			ESP.restart();
		});

		test->unpack();
		test->start();
	}else{
		launch();
	}

	digitalWrite(PIN_BL, LOW);
}

void loop(){
	LoopManager::loop();
}