#include <Arduino.h>
#include <CircuitOS.h>
#include <TFT_eSPI.h>
#include <Display/Display.h>
#include <Input/InputGPIO.h>
#include <Input/Input.h>
#include <Loop/LoopListener.h>
#include <Loop/LoopManager.h>
#include "src/Screens/Playback/Playback.h"

#define blPin 25

Display display(160,128,-1,-1);
Playback* playback;

void setup(){
	Serial.begin(115200);
	pinMode(blPin,OUTPUT);
	digitalWrite(blPin,LOW);
	display.begin();
	playback=new Playback(display);
	playback->unpack();
	playback->start();

}

void loop(){
}