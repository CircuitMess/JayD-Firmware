#include <Arduino.h>
#include <CircuitOS.h>
#include <JayD.h>
#include <Display/Display.h>
#include <Settings.h>
#include <Loop/LoopManager.h>
#include <Input/InputJayD.h>
#include <Util/HWRevision.h>
#include "src/InputKeys.h"
#include "src/HardwareTest.h"
#if defined(JAYD_ENABLE_WIRELESS)
#include "src/Wireless/WirelessBringup.h"
#endif
#include "src/Screens/IntroScreen/IntroScreen.h"
#include "src/Screens/InputTest/InputTest.h"

bool checkJig(){
	char buf[7];
	int wp = 0;

	uint32_t start = millis();
	int c;
	while(millis() - start < 500){
		vTaskDelay(1);
		c = getchar();
		if(c == EOF) continue;
		buf[wp] = (char) c;
		wp = (wp + 1) % 7;

		for(int i = 0; i < 7; i++){
			int match = 0;
			static const char* target = "JIGTEST";

			for(int j = 0; j < 7; j++){
				match += buf[(i + j) % 7] == target[j];
			}

			if(match == 7){
				// This is important, desktop app freezes otherwise if the UART/JTAG buffer isn't emptied when it tries to write something again
				while(int c2 = getchar() != EOF) {}

				return true;
			}
		}
	}

	return false;
}

void launch(){
	Context* introScreen = new IntroScreen::IntroScreen(JayD.getDisplay());
	introScreen->unpack();
	introScreen->start();
}

void setup(){
	Serial.begin(115200);

	if(checkJig()){
		printf("Jig\n");

		JayD.initVer(2);
		JayD.begin();

		pinMode(PIN_BL, OUTPUT);
		digitalWrite(PIN_BL, LOW);

		Display display(160, 128, -1, -1);
		display.getTft()->setPanel(JayDDisplay::panel3());
		display.begin();
		CircuitOS::gd_set_old_transparency(true);

		HardwareTest test(display);
		test.start();

		for(;;);
	}else{
		printf("Hello\n");
	}

	pinMode(PIN_BL, OUTPUT);
	digitalWrite(PIN_BL, HIGH);

	JayD.begin();

	InputJayD::getInstance()->addListener(&Input);
	LoopManager::addListener(&Input);

	Context::setDeleteOnPop(true);

	if(!Settings.get().inputTested){
		if(HWRevision::get() > 0){
			Settings.get().inputTested = true;
			Settings.store();

			launch();
		}else{
			InputTest::InputTest* test = new InputTest::InputTest(JayD.getDisplay());
			test->setDoneCallback([](InputTest::InputTest* test){
				Settings.get().inputTested = true;
				Settings.store();

				ESP.restart();
			});

			test->unpack();
			test->start();
		}
	}else{
		launch();
	}

	digitalWrite(PIN_BL, LOW);
#if defined(JAYD_ENABLE_WIRELESS)
	WirelessBringup::begin();
#endif
}

void loop(){
	LoopManager::loop();
#if defined(JAYD_ENABLE_WIRELESS)
	WirelessBringup::loop();
#endif
}
