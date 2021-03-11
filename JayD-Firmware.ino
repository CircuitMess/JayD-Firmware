#include <Arduino.h>
#include <CircuitOS.h>
#include <TFT_eSPI.h>
#include <Display/Display.h>
#include <Input/InputGPIO.h>
#include <Input/Input.h>
#include <Loop/LoopListener.h>
#include <Loop/LoopManager.h>
#include "src/Screens/IntroScreen/IntroScreen.h"
#include <JayD.hpp>
#include <Input/InputJayD.h>

#define blPin 25

Display display(160, 128, -1, -1);
IntroScreen::IntroScreen *introScreen;

void setup(){
	Serial.begin(115200);
	pinMode(blPin, OUTPUT);
	digitalWrite(blPin, LOW);
	display.begin();
	LoopManager::addListener(new InputJayD());
	introScreen=new IntroScreen::IntroScreen(display);
	LoopManager::addListener(introScreen);
	introScreen->unpack();
	introScreen->start();

}

void loop(){
	LoopManager::loop();
}