#include <Arduino.h>
#include <CircuitOS.h>
#include <TFT_eSPI.h>
#include <Display/Display.h>
#include <Input/InputGPIO.h>
#include <Input/Input.h>
#include <Loop/LoopListener.h>
#include <Loop/LoopManager.h>
#include "src/Screens/Playback/Playback.h"
#include "src/Screens/SongList/SongList.h"
#include "src/Screens/MixScreen/MixScreen.h"
#include "src/Screens/MainMenu/MainMenu.h"
#include <JayD.hpp>
#include <Input/InputJayD.h>

#define blPin 25

Display display(160, 128, -1, -1);
Playback *playback;
SongList *songList;
MixScreen::MixScreen *mixScreen;
MainMenu::MainMenu *mainMenu;

void setup(){
	Serial.begin(115200);
	pinMode(blPin, OUTPUT);
	digitalWrite(blPin, LOW);
	display.begin();
	LoopManager::addListener(new InputJayD());
	mainMenu=new MainMenu::MainMenu(display);
	mainMenu->unpack();
	mainMenu->start();

}

void loop(){
	LoopManager::loop();
}
