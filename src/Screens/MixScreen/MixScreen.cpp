#include <Input/InputJayD.h>
#include <SD.h>
#include <Loop/LoopManager.h>
#include <JayD.hpp>
#include <FS/CompressedFile.h>
#include "MixScreen.h"
#include "../SongList/SongList.h"
#include "../TextInputScreen/TextInputScreen.h"

MixScreen::MixScreen* MixScreen::MixScreen::instance = nullptr;

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
	MixScreen::pack();
}

MixScreen::MixScreen::~MixScreen(){
	instance = nullptr;
	free(selectedBackgroundBuffer);
}

void MixScreen::MixScreen::pack(){
	Context::pack();
	free(selectedBackgroundBuffer);
	selectedBackgroundBuffer = nullptr;
}

void MixScreen::MixScreen::unpack(){
	Context::unpack();

	selectedBackgroundBuffer = static_cast<Color*>(ps_malloc(79 * 128 * 2));
	if(selectedBackgroundBuffer == nullptr){
		Serial.println("Selected background malloc error");
		return;
	}

	fs::File bgFile = CompressedFile::open(SPIFFS.open("/mixSelectedBg.raw.hs"), 13, 12);
	bgFile.read(reinterpret_cast<uint8_t*>(selectedBackgroundBuffer), 79 * 128 * 2);
	bgFile.close();
}

void MixScreen::MixScreen::saveRecording(){
	if(!SD.exists(MixSystem::recordPath)){
		doneRecording = false;
		return;
	}

	Task saveTask("MixSave", [](Task* task){
		String saveFilename = * (String*) task->arg;

		if(SD.exists(saveFilename)){
			SD.remove(saveFilename);
		}

		File inFile = SD.open(MixSystem::recordPath);
		File outFile = SD.open(saveFilename, "w");

		SourceWAV input(inFile);
		OutputAAC output(outFile);

		output.setSource(&input);
		output.start();

		while(output.isRunning()){
			output.loop(0);
		}

		output.stop();
		input.close();

		inFile.close();
		outFile.close();
	}, 8 * 1024, &saveFilename);

	saveTask.start(1, 0);

	while(!saveTask.isStopped()){
		if(millis() - lastDraw >= 30){
			lastDraw = millis();
			drawSaveStatus();
			screen.commit();
		}

		Sched.loop(0);
	}

	SD.remove(MixSystem::recordPath);
	doneRecording = false;
}

void MixScreen::MixScreen::returned(void* data){
	String* filename = (String*) data;

	if(doneRecording){
		saveFilename = String("/") + *filename + ".aac";
		delete filename;
		return;
	}

	if(!f1){
		f1 = SD.open(*((String*) data));
	}else if(!f2){
		f2 = SD.open(*((String*) data));
	}

	delete filename;
}

void MixScreen::MixScreen::setBigVuStarted(bool bigVuStarted){
	MixScreen::bigVuStarted = bigVuStarted;
}

void MixScreen::MixScreen::start(){
	if(doneRecording){
		lastDraw = 0;
		draw();
		screen.commit();
		saveRecording();
	}


	if(!f1 || !f2){
		(new SongList::SongList(*getScreen().getDisplay()))->push(this);
		return;
	}

	Serial.printf("F1: %s\n", f1.name());
	Serial.printf("F2: %s\n", f2.name());

	String name = f1.name();
	leftSongName->setSongName(name.substring(name.lastIndexOf('/') + 1, name.length() - 4));
	name = f2.name();
	rightSongName->setSongName(name.substring(name.lastIndexOf('/') + 1, name.length() - 4));

	system = new MixSystem(f1, f2);

	system->setVolume(0, InputJayD::getInstance()->getPotValue(POT_L));
	system->setVolume(1, InputJayD::getInstance()->getPotValue(POT_R));

	system->setChannelInfo(0, leftVu.getInfoGenerator());
	system->setChannelInfo(1, rightVu.getInfoGenerator());
	system->setChannelInfo(2, midVu.getInfoGenerator());
	if(bigVuStarted){
		startBigVu();
	}

	uint8_t potMidVal = InputJayD::getInstance()->getPotValue(POT_MID);
	system->setMix(potMidVal);
	matrixManager.matrixMid.vu(potMidVal);
	matrixManager.matrixMid.push();

	leftSeekBar->setTotalDuration(system->getDuration(0));
	rightSeekBar->setTotalDuration(system->getDuration(1));

	leftSeekBar->setPlaying(true);
	rightSeekBar->setPlaying(true);


	leftSongName->checkScrollUpdate();
	rightSongName->checkScrollUpdate();

	for(int i = 0; i < 6; i++){
		effectElements[i]->setType(NONE);
	}


	Serial.printf("System constructed. Heap: %u B, PSRAM: %u B\n", ESP.getFreeHeap(), ESP.getFreePsram());
	system->start();
	delay(1);
	Serial.printf("System started. Heap: %u B, PSRAM: %u B\n", ESP.getFreeHeap(), ESP.getFreePsram());

	LoopManager::addListener(&leftVu);
	LoopManager::addListener(&rightVu);
	LoopManager::addListener(this);

	Input.addListener(this);
	InputJayD::getInstance()->addListener(this);

	draw();
	screen.commit();
}

void MixScreen::MixScreen::stop(){
	LoopManager::removeListener(&leftVu);
	LoopManager::removeListener(&rightVu);
	LoopManager::removeListener(&midVu);
	LoopManager::removeListener(this);

	Input.removeListener(this);
	InputJayD::getInstance()->removeListener(this);

	stopBigVu();

	if(system){
		system->stop();
		delete system;
		system = nullptr;
	}

}

void MixScreen::MixScreen::draw(){
	screen.getSprite()->fillRect(79, 0, 2, 128, TFT_BLACK);
	screen.getSprite()->fillRect(leftLayout->getTotalX(), leftLayout->getTotalY(), 79, 128, C_RGB(249, 93, 2));
	screen.getSprite()->fillRect(rightLayout->getTotalX(), rightLayout->getTotalY(), 79, 128, C_RGB(3, 52, 135));
	if(!selectedChannel){
		screen.getSprite()->drawIcon(selectedBackgroundBuffer, screen.getTotalX(), screen.getTotalY(), 79, 128, 1, TFT_TRANSPARENT);
	}else{
		screen.getSprite()->drawIcon(selectedBackgroundBuffer, screen.getTotalX() + 81, screen.getTotalY(), 79, 128, 1, TFT_TRANSPARENT);
	}

	if(isRecording){
		screen.getSprite()->fillCircle(79, 64, 6, TFT_BLACK);
		screen.getSprite()->fillCircle(79, 64, 4, TFT_RED);
	}
	screen.draw();

	if(doneRecording){
		drawSaveStatus();
	}
}

void MixScreen::MixScreen::drawSaveStatus(){
	Sprite* canvas = screen.getSprite();

	canvas->fillRoundRect((screen.getWidth() - 80) / 2, (screen.getHeight() - 40) / 2, 80, 40, 2, C_RGB(52, 204, 235));
	canvas->drawRoundRect((screen.getWidth() - 80) / 2, (screen.getHeight() - 40) / 2, 80, 40, 2, TFT_BLACK);

	auto font = canvas->startU8g2Fonts();
	font.setForegroundColor(TFT_WHITE);
	font.setFont(u8g2_font_DigitalDisco_tf);
	font.setFontMode(1);
	font.setCursor((screen.getWidth() - font.getUTF8Width("Saving...")) / 2, (screen.getHeight() - 40) / 2 + 23);
	font.print("Saving...");

	canvas->fillRoundRect((screen.getWidth() - 80) / 2 + 10 + (cos((float) millis() / 200.0f)+1) / 2.0f * 45.0f, (screen.getHeight() - 40) / 2 + 30, 15, 5, 2, TFT_WHITE);
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
	if(seekTime != 0 && millis() - seekTime >= 100){
		SongSeekBar* bar = seekChannel ? rightSeekBar : leftSeekBar;

		system->seekChannel(seekChannel, bar->getCurrentDuration());

		if(wasRunning){
			system->resumeChannel(seekChannel);
		}

		seekChannel = -1;
		seekTime = 0;
	}

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
		if(seekTime == 0 || seekChannel != 0){
			leftSeekBar->setCurrentDuration(system->getElapsed(0));
			update = true;
		}
	}

	if(system && system->getElapsed(1) != rightSeekBar->getCurrentDuration()){
		if(seekTime == 0 || seekChannel != 1){
			rightSeekBar->setCurrentDuration(system->getElapsed(1));
			update = true;
		}
	}

	if(system && system->isRecording() != isRecording){
		isRecording = system->isRecording();
		update = true;
	}

	if(system && system->isChannelPaused(0) != !leftSeekBar->isPlaying() && seekTime == 0){
		leftSeekBar->setPlaying(!system->isChannelPaused(0));
		update = true;
	}

	if(system && system->isChannelPaused(1) != !rightSeekBar->isPlaying() && seekTime == 0){
		rightSeekBar->setPlaying(!system->isChannelPaused(0));
		update = true;
	}

	bool songNameUpdateL = leftSongName->checkScrollUpdate();
	bool songNameUpdateR = rightSongName->checkScrollUpdate();
	update |= songNameUpdateL | songNameUpdateR;

	uint32_t currentTime = millis();
	if((update || drawQueued) && (currentTime - lastDraw) >= (isRecording ? 200 : 50)){
		drawQueued = false;
		draw();
		screen.commit();
		lastDraw = currentTime;
	}else if(update){
		drawQueued = true;
	}
}


void MixScreen::MixScreen::potMove(uint8_t id, uint8_t value){
	if(id == POT_MID){
		system->setMix(value);
		matrixManager.matrixMid.vu(value);
		matrixManager.matrixMid.push();
	}else if(id == POT_L){
		system->setVolume(0, value);
	}else if(id == POT_R){
		system->setVolume(1, value);
	}
}

void MixScreen::MixScreen::startBigVu(){
	LoopManager::addListener(&midVu);
}

void MixScreen::MixScreen::stopBigVu(){
	LoopManager::removeListener(&midVu);
}

void MixScreen::MixScreen::encTwoBot(){
	if(system->isRecording()){
		system->stopRecording();
		doneRecording = true;

		(new TextInputScreen::TextInputScreen(*screen.getDisplay()))->push(this);
	}else{
		system->startRecording();
	}
}

void MixScreen::MixScreen::encFour(){
	pop();
}

void MixScreen::MixScreen::btnCombination(){
	stop();

	MatrixPopUpPicker* popUpPicker = new MatrixPopUpPicker(*this);
	popUpPicker->unpack();
	popUpPicker->start();
}

void MixScreen::MixScreen::btn(uint8_t i){
	SongSeekBar* bar = i == 0 ? leftSeekBar : rightSeekBar;
	if(bar->isPlaying()){
		system->pauseChannel(i);
	}else{
		system->resumeChannel(i);
	}

	bar->setPlaying(!bar->isPlaying());

	drawQueued = true;
}

void MixScreen::MixScreen::btnEnc(uint8_t i){
	if(i > 6) return;

	if(i == 6){
		selectedChannel = !selectedChannel;
	}else{
		EffectElement* effect = effectElements[i];
		effect->setSelected(!effect->isSelected());
	}

	drawQueued = true;
}

void MixScreen::MixScreen::enc(uint8_t index, int8_t value){

	if(index == 6){
		if(seekTime == 0){
			seekChannel = selectedChannel;
			wasRunning = !system->isChannelPaused(selectedChannel);
			system->pauseChannel(selectedChannel);
		}

		seekTime = millis();

		SongSeekBar* bar = seekChannel ? rightSeekBar : leftSeekBar;
		uint16_t seekTime = constrain( bar->getCurrentDuration() + value, 0, system->getDuration(selectedChannel));
		bar->setCurrentDuration(seekTime);

		drawQueued = true;
		return;
	}

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

	drawQueued = true;
}

void MixScreen::MixScreen::encBtnHold(uint8_t i){
	if(i == 6){
		// system->stop();
		stop();

		if(selectedChannel == 0){
			f1.close();
		}else{
			f2.close();
		}

		(new SongList::SongList(*getScreen().getDisplay()))->push(this);
		return;
	}
}
