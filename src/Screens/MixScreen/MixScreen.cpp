#include <Input/InputJayD.h>
#include <SD.h>
#include <Loop/LoopManager.h>
#include <JayD.hpp>
#include <PerfMon.h>
#include "MixScreen.h"
#include "../SongList/SongList.h"
#include <AudioLib/Effect.h>
#include <AudioLib/VuInfoGenerator.h>

#include <AudioLib/Effects/LowPass.h>
#include <AudioLib/Effects/HighPass.h>
#include <AudioLib/Effects/Reverb.h>
#include <AudioLib/Effects/BitCrusher.h>

MixScreen::MixScreen* MixScreen::MixScreen::instance = nullptr;


const std::unordered_map<uint8_t, uint8_t> MixScreen::MixScreen::mapBtn = {
		{3, 0},
		{8, 1},
		{7, 2},
		{6, 3},
		{5, 4},
		{4, 5}
};

const std::unordered_map<uint8_t, uint8_t> MixScreen::MixScreen::mapEnc = {
		{1, 0},
		{6, 1},
		{5, 2},
		{4, 3},
		{3, 4},
		{2, 5},
};


MixScreen::MixScreen::MixScreen(Display& display) : Context(display),
													screenLayout(new LinearLayout(&screen, HORIZONTAL)),
													leftLayout(new LinearLayout(screenLayout, VERTICAL)),
													rightLayout(new LinearLayout(screenLayout, VERTICAL)),
													leftSeekBar(new SongSeekBar(leftLayout)),
													rightSeekBar(new SongSeekBar(rightLayout)),
													leftSongName(new SongName(leftLayout)),
													rightSongName(new SongName(rightLayout)), leftVu(&matrixManager.matrixL), rightVu(&matrixManager.matrixR),
													midVu(&matrixManager.matrixBig){


	for(int i = 0; i < 3; i++){
		effectElements[i] = new EffectElement(leftLayout, false);
	}
	for(int i = 3; i < 6; i++){
		effectElements[i] = new EffectElement(rightLayout, true);
	}

	instance = this;
	buildUI();

}

Context* selector = nullptr;

void MixScreen::MixScreen::returned(void* data){
	if(!f1){
		f1 = SD.open(*((String*) data));
	}else if(!f2){
		f2 = SD.open(*((String*) data));
	}

	delete (String*) data;
}

void MixScreen::MixScreen::start(){
	delete selector;
	selector = nullptr;

	if(!f1){
		selector = new SongList::SongList(*getScreen().getDisplay());
		selector->push(this);
		return;
	}

	if(!f2){
		selector = new SongList::SongList(*getScreen().getDisplay());
		selector->push(this);
		return;
	}

	Serial.printf("F1: %s\n", f1.name());
	Serial.printf("F2: %s\n", f2.name());

	leftSongName->setSongName(String(f1.name()).substring(1));
	rightSongName->setSongName(String(f2.name()).substring(1));

	system = new MixSystem(f1, f2);

	system->setVolume(0, InputJayD::getInstance()->getPotValue(POT_L));
	system->setVolume(1, InputJayD::getInstance()->getPotValue(POT_R));

	system->setMix(InputJayD::getInstance()->getPotValue(POT_MID));

	leftSeekBar->setTotalDuration(system->getDuration(0));
	rightSeekBar->setTotalDuration(system->getDuration(1));

	leftSeekBar->setPlaying(false);
	rightSeekBar->setPlaying(false);

	InputJayD::getInstance()->setBtnPressCallback(0, [](){
		if(instance == nullptr) return;
		instance->isPlaying[0] = !instance->isPlaying[0];
		instance->leftSeekBar->setPlaying(instance->isPlaying[0]);
		if(instance->isPlaying[0]){
			instance->system->resumeChannel(0);
		}else{
			instance->system->pauseChannel(0);
		}
		instance->btn0Pressed = true;
		instance->prevTime = millis();
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(1, [](){
		if(instance == nullptr) return;
		instance->isPlaying[1] = !instance->isPlaying[1];
		instance->rightSeekBar->setPlaying(instance->isPlaying[1]);
		if(instance->isPlaying[1]){
			instance->system->resumeChannel(1);
		}else{
			instance->system->pauseChannel(1);
		}
		instance->btn1Pressed = true;
		instance->prevTime = millis();

		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnHeldCallback(BTN_MID, 1000, nullptr);

	InputJayD::getInstance()->addListener(this);

	leftSongName->checkScrollUpdate();
	rightSongName->checkScrollUpdate();

	draw();
	screen.commit();

	LoopManager::addListener(this);


	Serial.printf("System constructed. Heap: %u B, PSRAM: %u B\n", ESP.getFreeHeap(), ESP.getFreePsram());
	system->start();
	Serial.printf("System started. Heap: %u B, PSRAM: %u B\n", ESP.getFreeHeap(), ESP.getFreePsram());
	system->setChannelInfo(0, leftVu.getInfoGenerator());
	LoopManager::addListener(&leftVu);
	system->setChannelInfo(1, rightVu.getInfoGenerator());
	LoopManager::addListener(&rightVu);

}


void MixScreen::MixScreen::stop(){

	InputJayD::getInstance()->removeListener(this);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_L);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_R);
	InputJayD::getInstance()->removeBtnHeldCallback(BTN_MID);

	LoopManager::removeListener(this);
	LoopManager::removeListener(&leftVu);
	LoopManager::removeListener(&rightVu);
	LoopManager::removeListener(&midVu);

	if(system){
		//system->stop();
		delete system;
		system = nullptr;
	}

}

void MixScreen::MixScreen::draw(){
	screen.getSprite()->fillRect(79, 0, 2, 128, TFT_BLACK);
	screen.getSprite()->fillRect(leftLayout->getTotalX(), leftLayout->getTotalY(), 79, 128, C_RGB(249, 93, 2));
	screen.getSprite()->fillRect(rightLayout->getTotalX(), rightLayout->getTotalY(), 79, 128, C_RGB(3, 52, 135));
	if(!selectedChannel){
		screen.getSprite()->fillCircle(2, 2, 2, TFT_WHITE);
	}else{
		screen.getSprite()->fillCircle(157, 2, 2, TFT_WHITE);
	}

	if(isRecording){
		screen.getSprite()->fillCircle(79, 64, 6, TFT_BLACK);
		screen.getSprite()->fillCircle(79, 64, 4, TFT_RED);
	}
	screen.draw();
}

void MixScreen::MixScreen::buildUI(){
	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(2);
	screenLayout->addChild(leftLayout);
	screenLayout->addChild(rightLayout);

	leftLayout->setWHType(FIXED, PARENT);
	leftLayout->setWidth(79);
	leftLayout->setGutter(10);
	leftLayout->setPadding(1);


	leftLayout->addChild(leftSeekBar);
	leftLayout->addChild(leftSongName);

	for(int i = 0; i < 3; i++){
		leftLayout->addChild(effectElements[i]);
	}


	rightLayout->setWHType(FIXED, PARENT);
	rightLayout->setWidth(79);
	rightLayout->setGutter(10);
	rightLayout->setPadding(1);


	rightLayout->addChild(rightSeekBar);
	rightLayout->addChild(rightSongName);

	for(int i = 3; i < 6; i++){
		rightLayout->addChild(effectElements[i]);
	}

	screenLayout->reflow();
	leftLayout->reflow();
	rightLayout->reflow();

	screen.addChild(screenLayout);
	screen.repos();
}

void MixScreen::MixScreen::loop(uint micros){
	bool update = false;
	for(const auto& element : effectElements){
		update |= element->needsUpdate();
	}

	if(system && system->getDuration(0) != leftSeekBar->getTotalDuration()){
		leftSeekBar->setTotalDuration(system->getDuration(0));
		update = true;
	}

	if(system && system->getDuration(1) != rightSeekBar->getTotalDuration()){
		rightSeekBar->setTotalDuration(system->getDuration(1));
		update = true;
	}

	if(system && system->getElapsed(0) != leftSeekBar->getCurrentDuration()){
		leftSeekBar->setCurrentDuration(system->getElapsed(0));
		update = true;
	}

	if(system && system->getElapsed(1) != rightSeekBar->getCurrentDuration()){
		rightSeekBar->setCurrentDuration(system->getElapsed(1));
		update = true;
	}

	if(popBtnConfig == 0x0F && (millis() - prevPopBtnTime) > 100){
		Serial.println("Pop");
		popBtnConfig = 0x00;
		multipleBtnPressCheck = true;
		prevMultipleBtnTime = millis();
		pop();
	}else if(recBtnConfig == 0x03 && (millis() - prevRecBtnTime) > 100){
		isRecording = !isRecording;
		multipleBtnPressCheck = true;
		prevMultipleBtnTime = millis();
		
		recBtnConfig = 0x00;
		update = true;
		Serial.println("Rec");
	}

	bool songNameUpdateL = leftSongName->checkScrollUpdate();
	bool songNameUpdateR = rightSongName->checkScrollUpdate();
	if(update || songNameUpdateL || songNameUpdateR){
		uint32_t now = millis();
		if(lastDraw == 0 || now - lastDraw >= 50){
			draw();
			screen.commit();
			lastDraw = now;
		}
	}

	if(btn0Pressed){
		if((millis() - prevTime > 100) && btn1Pressed){
			stop();
			pack();
			MatrixPopUpPicker* popUpPicker = new MatrixPopUpPicker(*this);
			popUpPicker->unpack();
			popUpPicker->start();
			btn0Pressed = false;
			btn1Pressed = false;
		}else if((millis() - prevTime > 100) && !btn1Pressed){
			btn0Pressed = false;
			btn1Pressed = false;
		}
	}else if(btn1Pressed){
		if((millis() - prevTime > 100) && btn0Pressed){
			stop();
			pack();
			MatrixPopUpPicker* popUpPicker = new MatrixPopUpPicker(*this);
			popUpPicker->unpack();
			popUpPicker->start();
			btn0Pressed = false;
			btn1Pressed = false;
		}else if((millis() - prevTime > 100) && !btn0Pressed){
			btn0Pressed = false;
			btn1Pressed = false;
		}
	}
}

void MixScreen::MixScreen::buttonPress(uint8_t id){

	if(id == 8 || id == 5 || id == 2){
		popBtnConfig = 0x00;
	}else{

		if(millis() - prevPopBtnTime > 100){
			popBtnConfig = 0x00;
		}

		switch(id){
			case 3:
				popBtnConfig |= 0x01;
				break;
			case 7:
				popBtnConfig |= 0x02;
				break;
			case 6:
				popBtnConfig |= 0x04;
				break;
			case 4:
				popBtnConfig |= 0x08;
				break;
			default:
				break;
		}
		prevPopBtnTime = millis();
	}
	if(popBtnConfig == 0x0F)return;



	if(!(id == 4 || id == 7)){
		recBtnConfig = 0x00;
	}else{

		if(millis() - prevRecBtnTime > 100){
			recBtnConfig = 0x00;
		}

		switch(id){

			case 7:
				recBtnConfig |= 0x01;
				break;

			case 4:
				recBtnConfig |= 0x02;
				break;
			default:
				break;
		}
		prevRecBtnTime = millis();
	}
	if(recBtnConfig == 0x03)return;
}


void MixScreen::MixScreen::buttonRelease(uint8_t id){

	if(millis() - prevMultipleBtnTime > 1000){
		multipleBtnPressCheck = false;
	}

	if(!hold){

		if(mapBtn.count(id) & !multipleBtnPressCheck){
			EffectElement *effect = effectElements[mapBtn.find(id)->second];
			effect->setSelected(!effect->isSelected());

			draw();
			screen.commit();

			return;
		}

		if(id == BTN_MID){
			selectedChannel = !selectedChannel;
			draw();
			screen.commit();
			return;
		}

	}
	hold = false;

}

void MixScreen::MixScreen::buttonHold(uint8_t id){

}

void MixScreen::MixScreen::encoderMove(uint8_t id, int8_t value){
	if(mapEnc.count(id)){
		size_t index = mapEnc.find(id)->second;
		EffectElement* element = effectElements[index];

		if(element->isSelected()){
			int8_t e = element->getType() + value;
			if(e >= EffectType::COUNT){
				e = e % EffectType::COUNT;
			}else if(e < 0){
				while(e < 0){
					e += EffectType::COUNT;
				}
			}

			// Only one speed allowed
			if(e == EffectType::SPEED){
				for(int i = (index < 3 ? 0 : 3); i < (index < 3 ? 3 : 6); i++){
					if(i == index) continue;
					if(effectElements[i]->getType() != EffectType::SPEED) continue;

					if(value < 0){
						e = e > 0 ? e - 1 : EffectType::COUNT - 1;
					}else{
						e = (e + 1) % EffectType::COUNT;
					}

					break;
				}
			}

			if(element->getType() == EffectType::SPEED){
				system->removeSpeed(index >= 3);
			}

			EffectType type = static_cast<EffectType>(e);
			element->setType(type);
			element->setIntensity(0);

			if(type == EffectType::SPEED){
				system->addSpeed(index >= 3);
				element->setIntensity(255 / 2);
				return;
			}

			system->setEffect(index >= 3, index < 3 ? index : index - 3, type);
		}else{
			EffectType type = element->getType();
			if(type == EffectType::NONE) return;

			int16_t intensity = element->getIntensity() + value * 5;
			intensity = max((int16_t) 0, intensity);
			intensity = min((int16_t) 255, intensity);

			element->setIntensity(intensity);

			if(type == EffectType::SPEED){
				system->setSpeed(index >= 3, intensity);
			}else{
				system->setEffectIntensity(index >= 3, index < 3 ? index : index - 3, element->getIntensity());
			}
		}

		draw();
		screen.commit();

		return;
	}
}

void MixScreen::MixScreen::potMove(uint8_t id, uint8_t value){
	if(id == POT_MID){
		system->setMix(value);
		matrixManager.matrixMid.clear();
		matrixManager.matrixMid.vu(value);
		matrixManager.matrixMid.push();
	}else if(id == POT_L){
		system->setVolume(0, value);
	}else if(id == POT_R){
		system->setVolume(1, value);
	}
}


MixScreen::MixScreen::~MixScreen(){
	instance = nullptr;
}

void MixScreen::MixScreen::startBigVu(){
	system->setChannelInfo(2, midVu.getInfoGenerator());
	LoopManager::addListener(&midVu);

}
