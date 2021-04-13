#ifndef JAYD_FIRMWARE_INPUTKEYS_H
#define JAYD_FIRMWARE_INPUTKEYS_H

#include <Arduino.h>
#include <Util/Vector.h>
#include <Input/InputJayD.h>
#include <unordered_map>

class InputKeys;

class InputListener {
	friend InputKeys;

private:
	virtual void btnEnc(uint8_t i);
	virtual void btn(uint8_t i);
	virtual void btnCombination();
	virtual void encTwoBot();
	virtual void encFour();
	virtual void enc(uint8_t i, int8_t value);
	virtual void encBtnHold(uint8_t i);

};

class InputKeys : public JayDInputListener, public LoopListener {
public:
	void addListener(InputListener* listener);
	void removeListener(InputListener* listener);

	void loop(uint micros) override;

private:
	static const std::unordered_map<uint8_t, uint8_t> mapBtn;
	static const std::unordered_map<uint8_t, uint8_t> mapEnc;

	void buttonPress(uint8_t id) override;
	void buttonRelease(uint8_t id) override;
	void encoderMove(uint8_t id, int8_t value) override;

	bool btnEncStates[7] = { false };
	bool btnStates[2] = { false };

	static const uint32_t holdTime = 500;
	uint32_t btnEncTime[7] = { 0 };
	bool btnEncHeld[7] = { false };

	Vector<InputListener*> listeners;
};

extern InputKeys Input;

#endif //JAYD_FIRMWARE_INPUTKEYS_H
