#include <JayD.hpp>
#include "InputKeys.h"

InputKeys Input;

const std::unordered_map<uint8_t, uint8_t> InputKeys::mapBtn = {
		{ BTN_L1, 0 },
		{ BTN_L2, 1 },
		{ BTN_L3, 2 },
		{ BTN_R1, 3 },
		{ BTN_R2, 4 },
		{ BTN_R3, 5 },
		{ BTN_MID, 6 }
};

const std::unordered_map<uint8_t, uint8_t> InputKeys::mapEnc = {
		{ ENC_L1, 0 },
		{ ENC_L2, 1 },
		{ ENC_L3, 2 },
		{ ENC_R1, 3 },
		{ ENC_R2, 4 },
		{ ENC_R3, 5 },
		{ ENC_MID, 6 }
};

void InputListener::btnEnc(uint8_t i){ }
void InputListener::btn(uint8_t i){ }
void InputListener::btnCombination(){ }
void InputListener::encTwoBot(){ }
void InputListener::encFour(){ }
void InputListener::enc(uint8_t id, int8_t value){ }

void InputKeys::buttonPress(uint8_t id){
	auto mapped = mapBtn.find(id);
	if(mapped == mapBtn.end()){
		btnStates[id == BTN_R] = true;

		if(btnStates[0] && btnStates[1]){
			for(auto listener : listeners){
				if(listener == nullptr) continue;
				listener->btnCombination();
			}
		}

		return;
	}

	uint8_t i = mapped->second;
	btnEncStates[i] = true;

	if(btnEncStates[0] && btnEncStates[2] && btnEncStates[3] && btnEncStates[5]){
		for(auto listener : listeners){
			if(listener == nullptr) continue;
			listener->encFour();
		}
	}
}

void InputKeys::buttonRelease(uint8_t id){
	auto mapped = mapBtn.find(id);
	if(mapped == mapBtn.end()){
		btnStates[id == BTN_R] = false;

		if(!btnStates[id == BTN_L]){
			for(auto listener : listeners){
				if(listener == nullptr) continue;
				listener->btn(id == BTN_R);
			}
		}

		return;
	}

	uint8_t index = mapped->second;
	btnEncStates[index] = false;

	bool twoButton = id == BTN_L3 || id == BTN_R3;
	for(int i = 0; i < 6; i++){
		if(i == index){
			continue;
		}else if(i == 2 || i == 5){
			twoButton &= btnEncStates[i];
		}else{
			twoButton &= !btnEncStates[i];
		}
	}

	if(twoButton){
		for(auto listener : listeners){
			if(listener == nullptr) continue;
			listener->encTwoBot();
		}

		return;
	}

	for(auto listener : listeners){
		if(listener == nullptr) continue;
		listener->btnEnc(index);
	}
}

void InputKeys::addListener(InputListener* listener){
	listeners.push_back(listener);
}

void InputKeys::removeListener(InputListener* listener){
	uint i = listeners.indexOf(listener);
	if(i == (uint) -1) return;
	listeners.remove(i);
}

void InputKeys::encoderMove(uint8_t id, int8_t value){
	auto mapped = mapEnc.find(id);
	if(mapped == mapEnc.end()) return;
	uint8_t index = mapped->second;

	for(auto listener : listeners){
		if(listener == nullptr) continue;
		listener->enc(index, value);
	}
}
