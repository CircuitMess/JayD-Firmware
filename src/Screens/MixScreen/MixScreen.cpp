#include <Input/InputJayD.h>
#include <SD.h>
#include <Loop/LoopManager.h>
#include <JayD.hpp>
#include <PerfMon.h>
#include "MixScreen.h"
#include "../SongList/SongList.h"
#include <AudioLib/Effect.h>

#include <AudioLib/Effects/LowPass.h>
#include <AudioLib/Effects/HighPass.h>
#include <AudioLib/Effects/Reverb.h>
#include <AudioLib/Effects/BitCrusher.h>

MixScreen::MixScreen *MixScreen::MixScreen::instance = nullptr;

const std::unordered_map<uint8_t, uint8_t> MixScreen::MixScreen::mapBtn = {
		{ 3, 0 },
		{ 8, 1 },
		{ 7, 2 },
		{ 6, 3 },
		{ 5, 4 },
		{ 4, 5 }
};

const std::unordered_map<uint8_t, uint8_t> MixScreen::MixScreen::mapEnc = {
		{ 1, 0 },
		{ 6, 1 },
		{ 5, 2 },
		{ 4, 3 },
		{ 3, 4 },
		{ 2, 5 },
};

Effect* (*launch[])() = {
		[]() -> Effect* { return nullptr; }, // None
		[]() -> Effect* { return nullptr; }, // Speed
		[]() -> Effect* { return new LowPass(); },
		[]() -> Effect* { return new HighPass(); },
		[]() -> Effect* { return new Reverb(); },
		[]() -> Effect* { return new BitCrusher(); }
};

MixScreen::MixScreen::MixScreen(Display &display) : Context(display), screenLayout(new LinearLayout(&screen, HORIZONTAL)),
													leftLayout(new LinearLayout(screenLayout, VERTICAL)),
													rightLayout(new LinearLayout(screenLayout, VERTICAL)), leftSeekBar(new SongSeekBar(leftLayout)),
													rightSeekBar(new SongSeekBar(rightLayout)), leftSongName(new SongName(leftLayout)),
													rightSongName(new SongName(rightLayout)),
													audioTask("MixAudio", audioThread, 4096, this){


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
		f1 = SD.open(*((String*)data));
	}else if(!f2){
		f2 = SD.open(*((String*)data));
	}

	delete (String*) data;
}

void MixScreen::MixScreen::start(){
	delete selector;
	selector = nullptr;

	if(!f1){
		delete selector;
		selector = new SongList::SongList(*getScreen().getDisplay());
		selector->push(this);
		return;
	}

	if(!f2){
		delete selector;
		selector = new SongList::SongList(*getScreen().getDisplay());
		selector->push(this);
		return;
	}

	Serial.printf("F1: %s\n", f1.name());
	Serial.printf("F2: %s\n", f2.name());

	s1 = new SourceWAV(f1);
	s2 = new SourceWAV(f2);

	effector1 = new EffectProcessor(s1);
	effector2 = new EffectProcessor(s2);

	for(int i = 0; i < 3; i++){
		effector1->addEffect(nullptr);
		effector2->addEffect(nullptr);
	}

	mixer = new Mixer();
	mixer->addSource(effector1);
	mixer->addSource(effector2);
	mixer->setMixRatio(InputJayD::getInstance()->getPotValue(POT_MID));

	out = new OutputI2S({
								.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
								.sample_rate = 48000,
								.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
								.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
								.communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
								.intr_alloc_flags = 0,
								.dma_buf_count = 16,
								.dma_buf_len = 512,
								.use_apll = false
						}, i2s_pin_config, I2S_NUM_0);

	out->setGain(0.05);
	out->setSource(mixer);
	out->start();

	InputJayD::getInstance()->setBtnPressCallback(0, [](){
		if(instance == nullptr) return;
		if(!instance->isPlaying){
			instance->isPlaying = true;
		}else{
			instance->isPlaying = false;
		}
		instance->leftSeekBar->setPlaying(instance->isPlaying);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(1, [](){
		if(instance == nullptr) return;
		if(!instance->isPlaying){
			instance->isPlaying = true;
		}else{
			instance->isPlaying = false;
		}
		instance->rightSeekBar->setPlaying(instance->isPlaying);
		instance->draw();
		instance->screen.commit();
	});

	InputJayD::getInstance()->addListener(this);

	draw();
	screen.commit();

	LoopManager::addListener(this);
	audioTask.start(0, 0);
}


void MixScreen::MixScreen::stop(){
	InputJayD::getInstance()->removeListener(this);
	LoopManager::removeListener(this);
	out->stop();
	audioTask.stop();
}

void MixScreen::MixScreen::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.getSprite()->fillRect(leftLayout->getTotalX(), leftLayout->getTotalY(), 78, 128, C_RGB(249, 53, 2));
	screen.getSprite()->fillRect(rightLayout->getTotalX(), rightLayout->getTotalY(), 79, 128, C_RGB(3, 52, 135));
	screen.draw();
}

void MixScreen::MixScreen::buildUI(){
	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(1);
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

	bool songNameUpdateL = leftSongName->checkScrollUpdate();
	bool songNameUpdateR = rightSongName->checkScrollUpdate();
	if(update || songNameUpdateL || songNameUpdateR){
		draw();
		screen.commit();
	}
}

void MixScreen::MixScreen::audioThread(Task* task){
	MixScreen* instance = static_cast<MixScreen*>(task->arg);
	while(task->running){
		if(instance->out->isRunning()){
			instance->out->loop(0);
		}
	}
}

MixScreen::MixScreen::~MixScreen(){
	instance = nullptr;
}

void MixScreen::MixScreen::buttonPress(uint8_t id){
	if(mapBtn.count(id)){
		EffectElement* effect = effectElements[mapBtn.find(id)->second];
		effect->setSelected(!effect->isSelected());

		draw();
		screen.commit();

		return;
	}
}

void MixScreen::MixScreen::buttonRelease(uint8_t id){

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

			if(element->getType() == EffectType::SPEED){
				if(index < 3){
					mixer->setSource(0, effector1);
					effector1->setSource(s1);
					delete speedLeft;
					speedLeft = nullptr;
				}else{
					mixer->setSource(1, effector2);
					effector2->setSource(s2);
					delete speedRight;
					speedRight = nullptr;
				}
			}

			EffectType type = static_cast<EffectType>(e);
			element->setType(type);
			element->setIntensity(0);

			Effect* effect = index < 3 ? effector1->getEffect(index) : effector2->getEffect(index - 3);
			if(index < 3){
				effector1->setEffect(index, nullptr);
			}else{
				effector2->setEffect(index-3, nullptr);
			}
			delay(50);
			delete effect;
			effects[index] = nullptr;

			if(type == EffectType::NONE) return;
			if(type == EffectType::SPEED){
				if(index < 3){
					speedLeft = new SpeedModifier(s1);
					speedLeft->setSpeed(1);
					effector1->setSource(speedLeft);
				}else{
					speedRight = new SpeedModifier(s2);
					speedRight->setSpeed(1);
					effector2->setSource(speedRight);
				}

				element->setIntensity(85);
				return;
			}

			effect = launch[type]();
			effect->setIntensity(0);
			if(index < 3){
				effector1->setEffect(index, effect);
			}else{
				effector2->setEffect(index-3, effect);
			}

			effects[index] = effect;
		}else{
			EffectType type = element->getType();

			int16_t intensity = element->getIntensity() + value * 5;
			intensity = max((int16_t) 0, intensity);
			intensity = min((int16_t) 255, intensity);

			if(type == EffectType::NONE) return;
			element->setIntensity(intensity);

			if(type == EffectType::SPEED){
				if(index < 3){
					speedLeft->setModifier(intensity);
				}else{
					speedRight->setModifier(intensity);
				}
			}else{
				effects[index]->setIntensity(element->getIntensity());
			}
		}

		draw();
		screen.commit();

		return;
	}
}

void MixScreen::MixScreen::potMove(uint8_t id, uint8_t value){

}
