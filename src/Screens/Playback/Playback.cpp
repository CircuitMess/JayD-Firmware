#include <Input/InputJayD.h>
#include <SD.h>
#include <JayD.hpp>
#include <Loop/LoopManager.h>
#include "Playback.h"
#include "Bitmaps/playback _pozadina.hpp"
Playback::Playback *Playback::Playback::instance = nullptr;

Playback::Playback::Playback(Display &display) : Context(display), screenLayout(&screen, VERTICAL),
												 songNameLayout(&screenLayout, HORIZONTAL),
												 timeElapsedLayout(&screenLayout, HORIZONTAL), buttonLayout(
				&screenLayout, HORIZONTAL), songName(&songNameLayout), playOrPause(&buttonLayout),
												 trackCount(&timeElapsedLayout){


	instance = this;
	buildUI();

}

Playback::Playback::~Playback(){
	instance = nullptr;
}

void Playback::Playback::loop(uint micros){
	if(i2s->isRunning()){
		i2s->loop(micros);
	}

	if(((int) wav->getElapsed()) != trackCount.getCurrentDuration()){
		trackCount.setCurrentDuration(wav->getElapsed());
		draw();
		screen.commit();
	}
}

void Playback::Playback::returned(void* data){
	String* name = static_cast<String*>(data);
	file = SD.open(name->c_str());

	if(file){
		wav = new SourceWAV(file);
		trackCount.setTotalDuration(wav->getDuration());
		Serial.printf("Duration: %d\n", wav->getDuration());
		songName.setSongName(name->substring(1, name->length() - 4));
	}else{
		trackCount.setTotalDuration(0);
		songName.setSongName("-");
	}

	trackCount.setCurrentDuration(0);
	playOrPause.setPlaying(false);

	delete name;
}

void Playback::Playback::start(){
	draw();
	screen.commit();

	InputJayD::getInstance()->setBtnPressCallback(BTN_L1, [](){
		if(instance == nullptr) return;

		if(instance->playing){
			instance->i2s->stop();
			instance->playing = false;
			LoopManager::removeListener(instance);
		}else if(instance->wav){
			instance->i2s->start();
			instance->playing = true;
			LoopManager::addListener(instance);
		}

		instance->playOrPause.setPlaying(instance->playing);

		instance->draw();
		instance->screen.commit();
	});

	InputJayD::getInstance()->setEncoderMovedCallback(ENC_L1, [](int8_t value){
		if(instance == nullptr) return;

		if(instance->wav){
			instance->wav->seek(max(0, (int) instance->wav->getElapsed() + value * 100000), SeekMode::SeekSet);
			instance->trackCount.setCurrentDuration(instance->wav->getElapsed());
		}

		instance->draw();
		instance->screen.commit();
	});

	InputJayD::getInstance()->setPotMovedCallback(POT_L, [](uint8_t value){
		if(instance && instance->i2s){
			instance->i2s->setGain((float) value / 255.0f);
		}
	});

	const i2s_config_t i2s_config = {
			.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
			.sample_rate = 44100,
			.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
			.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
			.communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
			.intr_alloc_flags = 0,
			.dma_buf_count = 2,
			.dma_buf_len = 1024,
			.use_apll = false
	};

	i2s = new OutputI2S(i2s_config, i2s_pin_config, 0);
	i2s->setSource(wav);
	i2s->setGain(0.01);
}

void Playback::Playback::stop(){
	InputJayD::getInstance()->removeBtnPressCallback(BTN_L1);
	InputJayD::getInstance()->removeEncoderMovedCallback(ENC_L1);

	playing = false;

	if(i2s != nullptr){
		i2s->stop();
		delete i2s;
		i2s = nullptr;
	}

	delete wav;
	wav = nullptr;

	file.close();
}


void Playback::Playback::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screenLayout.getSprite()->drawIcon(playback_pozadina,screenLayout.getTotalX(),screenLayout.getTotalY(),160,128,1,TFT_TRANSPARENT);
	screen.draw();
}

void Playback::Playback::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(5);
	screenLayout.addChild(&songNameLayout);
	screenLayout.addChild(&timeElapsedLayout);
	screenLayout.addChild(&buttonLayout);

	songNameLayout.setWHType(PARENT, FIXED);
	songNameLayout.setHeight(35);
	songNameLayout.setGutter(5);
	songNameLayout.addChild(&songName);

	timeElapsedLayout.setWHType(PARENT, FIXED);
	timeElapsedLayout.setHeight(20);
	timeElapsedLayout.setGutter(5);
	timeElapsedLayout.addChild(&trackCount);

	buttonLayout.setWHType(PARENT, FIXED);
	buttonLayout.setHeight(62);
	buttonLayout.setGutter(5);
	buttonLayout.addChild(&playOrPause);

	screenLayout.reflow();
	timeElapsedLayout.reflow();
	songNameLayout.reflow();
	buttonLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();

}

