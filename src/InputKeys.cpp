#include <JayD.h>
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
void InputListener::encTwoTop(){ }
void InputListener::encTwoBot(){ }
void InputListener::enc(uint8_t i, int8_t value){ }
void InputListener::encBtnHold(uint8_t i){}

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

	uint8_t index = mapped->second;
	btnEncStates[index] = true;
	btnEncTime[index] = millis();

	for(int i = 0; i < 7; i++){
		if(i == index) continue;
		if(btnEncStates[i]){
			for(int j = 0; j < 7; j++){
				btnEncTime[j] = 0;
			}
			break;
		}
	}

	if(btnEncStates[0] && btnEncStates[3]){
		twoTop = true;

		for(auto listener : listeners){
			if(listener == nullptr) continue;
			listener->encTwoTop();
			return;
		}
	}

	if(btnEncStates[2] && btnEncStates[5]){
		twoBot = true;

		for(auto listener : listeners){
			if(listener == nullptr) continue;
			listener->encTwoBot();
			return;
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
	btnEncTime[index] = 0;

	if(btnEncHeld[index]){
		btnEncHeld[index] = false;
		return;
	}

	if(twoTop && (index == 0 || index == 3)){
		twoTop = btnEncStates[0] || btnEncStates[3];
		return;
	}

	if(twoBot && (index == 2 || index == 5)){
		twoBot = btnEncStates[2] || btnEncStates[5];
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

void InputKeys::loop(uint micros){
	uint8_t pressed = 0;
	int8_t index = -1;
	for(int i = 0; i < 7; i++){
		pressed += btnEncStates[i];

		if(btnEncTime[i] != 0){
			index = i;
		}
	}

	if(pressed != 1 || index == -1) return;
	if(millis() - btnEncTime[index] < holdTime) return;

	for(auto listener : listeners){
		if(listener == nullptr) continue;
		listener->encBtnHold(index);
	}

	btnEncTime[index] = 0;
	btnEncHeld[index] = true;
}
