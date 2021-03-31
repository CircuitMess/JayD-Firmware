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
#include <JayD.hpp>
#include <Input/InputJayD.h>
#include <WiFi.h>
#include <SD.h>

#define blPin 25

Display display(160, 128, -1, -1);

void setup(){
	Serial.begin(115200);
	pinMode(blPin, OUTPUT);
	digitalWrite(blPin, HIGH);

	WiFi.mode(WIFI_OFF);
	btStop();

	disableCore0WDT();
	disableCore1WDT();

	SPI.begin(18, 19, 23);
	SPI.setFrequency(60000000);
	if(!SD.begin(22, SPI)){
		Serial.println("No SD card");
		//for(;;);
	}
	if(!SPIFFS.begin()){
		Serial.println("SPIFFS error");
	}
	display.begin();

	if(!LEDmatrix.begin(26, 27)){
		Serial.println("couldn't start matrix");
		for(;;);
	}
	LoopManager::addListener(&matrixManager);
	LoopManager::addListener(new InputJayD());
	InputJayD::getInstance()->begin();

	Context* introScreen = new IntroScreen::IntroScreen(display);
	introScreen->unpack();
	introScreen->start();

	digitalWrite(blPin, LOW);
}

void loop(){
	LoopManager::loop();
}