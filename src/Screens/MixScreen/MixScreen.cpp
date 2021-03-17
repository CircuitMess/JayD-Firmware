#include <Input/InputJayD.h>
#include "MixScreen.h"

MixScreen::MixScreen *MixScreen::MixScreen::instance = nullptr;

MixScreen::MixScreen::MixScreen(Display &display) : Context(display), screenLayout(&screen, HORIZONTAL),
													leftLayout(&screenLayout, VERTICAL),
													rightLayout(&screenLayout, VERTICAL), leftSeekBar(&leftLayout),
													rightSeekBar(&rightLayout), leftSongName(&leftLayout, "songLeft"),
													rightSongName(&rightLayout, "songRight"){


	for(int i = 0; i < 3; i++){
		effectsLeft.push_back(new EffectElement(&leftLayout, false, static_cast<EffectType>(i)));
	}
	for(int i = 0; i < 3; i++){
		effectsRight.push_back(new EffectElement(&rightLayout, true, static_cast<EffectType>(i)));
	}

	for(int i = 0; i < 6; ++i){
		isSelected[i] = false;
	}

	instance = this;
	buildUI();

}

void MixScreen::MixScreen::start(){
	draw();
	screen.commit();
	InputJayD::getInstance()->setBtnPressCallback(0, [](){
		if(instance == nullptr) return;
		if(!instance->isPlaying){
			instance->isPlaying = true;
		}else{
			instance->isPlaying = false;
		}
		instance->leftSeekBar.setPlaying(instance->isPlaying);
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
		instance->rightSeekBar.setPlaying(instance->isPlaying);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(3, [](){
		if(instance == nullptr) return;
		if(!instance->isSelected[0]){
			instance->isSelected[0] = true;
		}else{
			instance->isSelected[0] = false;
		}
		instance->effectsLeft[0]->setSelected(instance->isSelected[0]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(8, [](){
		if(instance == nullptr) return;
		if(!instance->isSelected[1]){
			instance->isSelected[1] = true;
		}else{
			instance->isSelected[1] = false;
		}
		instance->effectsLeft[1]->setSelected(instance->isSelected[1]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(7, [](){
		if(instance == nullptr) return;
		if(!instance->isSelected[2]){
			instance->isSelected[2] = true;
		}else{
			instance->isSelected[2] = false;
		}
		instance->effectsLeft[2]->setSelected(instance->isSelected[2]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(6, [](){
		if(instance == nullptr) return;
		if(!instance->isSelected[3]){
			instance->isSelected[3] = true;
		}else{
			instance->isSelected[3] = false;
		}
		instance->effectsRight[0]->setSelected(instance->isSelected[3]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(5, [](){
		if(instance == nullptr) return;
		if(!instance->isSelected[4]){
			instance->isSelected[4] = true;
		}else{
			instance->isSelected[4] = false;
		}
		instance->effectsRight[1]->setSelected(instance->isSelected[4]);
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(4, [](){
		if(instance == nullptr) return;
		if(!instance->isSelected[5]){
			instance->isSelected[5] = true;
		}else{
			instance->isSelected[5] = false;
		}
		instance->effectsRight[2]->setSelected(instance->isSelected[5]);
		instance->draw();
		instance->screen.commit();

	});
	InputJayD::getInstance()->setEncoderMovedCallback(1, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->isSelected[0]){
			instance->effects[0] = instance->effects[0] + value;
			if(instance->effects[0] < 0){
				instance->effects[0] = 4;
			}else if(instance->effects[0] > 4){
				instance->effects[0] = 0;
			}
			instance->effectsLeft[0]->setEffect(static_cast<EffectType>(instance->effects[0]));
			instance->draw();
			instance->screen.commit();
		}
		if(!instance->isSelected[0]){
			instance->intensity[0] = instance->intensity[0] + value * 4;
			if(instance->intensity[0] > 255){
				instance->intensity[0] = 255;
			}else if(instance->intensity[0] < 0){
				instance->intensity[0] = 0;
			}
			instance->effectsLeft[0]->setIntensity(instance->intensity[0]);
		}

		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(6, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->isSelected[1]){
			instance->effects[1] = instance->effects[1] + value;
			if(instance->effects[1] < 0){
				instance->effects[1] = 4;
			}else if(instance->effects[1] > 4){
				instance->effects[1] = 0;
			}
			instance->effectsLeft[1]->setEffect(static_cast<EffectType>(instance->effects[1]));
		}
		if(!instance->isSelected[1]){
			instance->intensity[1] = instance->intensity[1] + value * 4;
			if(instance->intensity[1] > 255){
				instance->intensity[1] = 255;
			}else if(instance->intensity[1] < 0){
				instance->intensity[1] = 0;
			}
			instance->effectsLeft[1]->setIntensity(instance->intensity[1]);
		}
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(5, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->isSelected[2]){
			instance->effects[2] = instance->effects[2] + value;
			if(instance->effects[2] < 0){
				instance->effects[2] = 4;
			}else if(instance->effects[2] > 4){
				instance->effects[2] = 0;
			}
			instance->effectsLeft[2]->setEffect(static_cast<EffectType>(instance->effects[2]));
		}
		if(!instance->isSelected[2]){
			instance->intensity[2] = instance->intensity[2] + value * 4;
			if(instance->intensity[2] > 255){
				instance->intensity[2] = 255;
			}else if(instance->intensity[2] < 0){
				instance->intensity[2] = 0;
			}
			instance->effectsLeft[2]->setIntensity(instance->intensity[2]);
		}
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(4, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->isSelected[3]){
			instance->effects[3] = instance->effects[3] + value;
			if(instance->effects[3] < 0){
				instance->effects[3] = 4;
			}else if(instance->effects[3] > 4){
				instance->effects[3] = 0;
			}
			instance->effectsRight[0]->setEffect(static_cast<EffectType>(instance->effects[3]));
		}
		if(!instance->isSelected[3]){
			instance->intensity[3] = instance->intensity[3] + value * 4;
			if(instance->intensity[3] > 255){
				instance->intensity[3] = 255;
			}else if(instance->intensity[3] < 0){
				instance->intensity[3] = 0;
			}
			instance->effectsRight[0]->setIntensity(instance->intensity[3]);
		}
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(3, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->isSelected[4]){
			instance->effects[4] = instance->effects[4] + value;
			if(instance->effects[4] < 0){
				instance->effects[4] = 4;
			}else if(instance->effects[4] > 4){
				instance->effects[4] = 0;
			}
			instance->effectsRight[1]->setEffect(static_cast<EffectType>(instance->effects[4]));
		}
		if(!instance->isSelected[4]){
			instance->intensity[4] = instance->intensity[4] + value * 4;
			if(instance->intensity[4] > 255){
				instance->intensity[4] = 255;
			}else if(instance->intensity[4] < 0){
				instance->intensity[4] = 0;
			}
			instance->effectsRight[1]->setIntensity(instance->intensity[4]);
		}
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setEncoderMovedCallback(2, [](int8_t value){
		if(instance == nullptr) return;
		if(instance->isSelected[5]){
			instance->effects[5] = instance->effects[5] + value;
			if(instance->effects[5] < 0){
				instance->effects[5] = 4;
			}else if(instance->effects[5] > 4){
				instance->effects[5] = 0;
			}
			instance->effectsRight[2]->setEffect(static_cast<EffectType>(instance->effects[5]));
		}
		if(!instance->isSelected[5]){
			instance->intensity[5] = instance->intensity[5] + value * 4;
			if(instance->intensity[5] > 255){
				instance->intensity[5] = 255;
			}else if(instance->intensity[5] < 0){
				instance->intensity[5] = 0;
			}
			instance->effectsRight[2]->setIntensity(instance->intensity[5]);
		}
		instance->draw();
		instance->screen.commit();
	});
}


void MixScreen::MixScreen::stop(){
	for(int i = 0; i <= 8; i++){
		InputJayD::getInstance()->removeBtnPressCallback(i);
	}
	for(int i = 1; i <= 6; i++){
		InputJayD::getInstance()->removeEncoderMovedCallback(i);
	}
}

void MixScreen::MixScreen::draw(){
	screen.getSprite()->clear(TFT_BLACK);
	screen.getSprite()->fillRect(leftLayout.getTotalX(), leftLayout.getTotalY(), 78, 128, C_RGB(249, 53, 2));
	screen.getSprite()->fillRect(rightLayout.getTotalX(), rightLayout.getTotalY(), 79, 128, C_RGB(3, 52, 135));
	screen.draw();
}

void MixScreen::MixScreen::buildUI(){
	screenLayout.setWHType(PARENT, PARENT);
	screenLayout.setGutter(1);
	screenLayout.addChild(&leftLayout);
	screenLayout.addChild(&rightLayout);

	leftLayout.setWHType(FIXED, PARENT);
	leftLayout.setWidth(79);
	leftLayout.setGutter(10);
	leftLayout.setPadding(1);


	leftLayout.addChild(&leftSeekBar);
	leftLayout.addChild(&leftSongName);

	for(int i = 0; i < effectsLeft.size(); i++){
		leftLayout.addChild(effectsLeft[i]);
	}


	rightLayout.setWHType(FIXED, PARENT);
	rightLayout.setWidth(79);
	rightLayout.setGutter(10);
	rightLayout.setPadding(1);


	rightLayout.addChild(&rightSeekBar);
	rightLayout.addChild(&rightSongName);

	for(int i = 0; i < effectsRight.size(); i++){
		rightLayout.addChild(effectsRight[i]);
	}


	screenLayout.reflow();
	leftLayout.reflow();
	rightLayout.reflow();

	screen.addChild(&screenLayout);
	screen.repos();


}

void MixScreen::MixScreen::loop(uint micros){
	for(int i = 0; i < effectsLeft.size(); i++){
		if(effectsLeft[i]->needsUpdate()){
			draw();
			screen.commit();
		}
	}
	for(int i = 0; i < effectsRight.size(); i++){
		if(effectsRight[i]->needsUpdate()){
			draw();
			screen.commit();
		}
	}

}

MixScreen::MixScreen::~MixScreen(){
	instance = nullptr;
}
