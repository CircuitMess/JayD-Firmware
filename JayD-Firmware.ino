#include <Arduino.h>
#include <CircuitOS.h>
#include <JayD.hpp>
#include <TFT_eSPI.h>
#include <Display/Display.h>
#include <Input/InputGPIO.h>
#include <Input/Input.h>
#include <Loop/LoopListener.h>
#include <Loop/LoopManager.h>
#include "src/Screens/IntroScreen/IntroScreen.h"
#include "src/Screens/InputTest/InputTest.h"
#include "src/InputKeys.h"
#include <Input/InputJayD.h>
#include <WiFi.h>
#include <SD.h>
#include <Services/SDScheduler.h>
#include <SPIFFS.h>
#include <Settings.h>

#define blPin 25

Display display(160, 128, -1, -1);

void launch(){
	Context *introScreen = new IntroScreen::IntroScreen(display);
	introScreen->unpack();
	introScreen->start();
}

void setup(){
	Serial.begin(115200);

	if(psramFound()){
		Serial.printf("PSRAM init: %s, free: %d B\n", psramInit() ? "Yes" : "No", ESP.getFreePsram());
	}else{
		Serial.println("No PSRAM detected");
	}

	pinMode(blPin, OUTPUT);
	digitalWrite(blPin, HIGH);

	WiFi.mode(WIFI_OFF);
	btStop();

	disableCore0WDT();
	disableCore1WDT();

	SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);
	SPI.setFrequency(60000000);
	if(!SD.begin(22, SPI)){
		Serial.println("No SD card");
		//for(;;);
	}
	if(!SPIFFS.begin()){
		Serial.println("SPIFFS error");
	}
	display.begin();
	SPI.setFrequency(20000000);

	if(!LEDmatrix.begin(I2C_SDA, I2C_SCL)){
		Serial.println("couldn't start matrix");
		for(;;);
	}

	Settings.begin();
	LEDmatrix.setBrightness(80.0f * (float) Settings.get().brightnessLevel / 255.0f);

	Context::setDeleteOnPop(true);

	LoopManager::addListener(&Sched);
	LoopManager::addListener(&matrixManager);
	LoopManager::addListener(new InputJayD());
	InputJayD::getInstance()->begin();
	InputJayD::getInstance()->addListener(&Input);
	LoopManager::addListener(&Input);

	if(!Settings.get().inputTested){
		InputTest::InputTest* test = new InputTest::InputTest(display);
		test->setDoneCallback([](InputTest::InputTest* test){
			test->stop();
			delete test;

			Settings.get().inputTested = true;
			Settings.store();

			launch();
		});

		test->unpack();
		test->start();
	}else{
		launch();
	}

	digitalWrite(blPin, LOW);
}

void loop(){
	LoopManager::loop();
}