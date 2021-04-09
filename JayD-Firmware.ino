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
#include "src/Screens/Settings/SettingsScreen.h"
#include "src/Screens/InputTest/InputTest.h"
#include <Input/InputJayD.h>
#include <WiFi.h>
#include <SD.h>
#include <Services/SDScheduler.h>
#include <SPIFFS.h>
#include <Settings.h>

#define blPin 25

Display display(160, 128, -1, -1);

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

	SPI.begin(18, 19, 23, -1);
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
	LoopManager::addListener(&matrixManager);
	LoopManager::addListener(new InputJayD());
	InputJayD::getInstance()->begin();
	bool firstTime = Settings.isInputTested();
	if(firstTime){
		InputTest::InputTest *inputTest = new InputTest::InputTest(display);
		inputTest->start();
	}else{
		Context *introScreen = new IntroScreen::IntroScreen(display);
		introScreen->unpack();
		introScreen->start();
	}
	digitalWrite(blPin, LOW);
	LoopManager::addListener(&Sched);
}

void loop(){
	LoopManager::loop();
}