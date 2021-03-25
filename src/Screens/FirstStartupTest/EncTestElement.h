#ifndef JAYD_FIRMWARE_ENCTESTELEMENT_H
#define JAYD_FIRMWARE_ENCTESTELEMENT_H

#include <UI/CustomElement.h>

class EncTestElement : public CustomElement {
public:
	EncTestElement(ElementContainer *parent, bool midLayout);

	void encoderMove(int8_t newValue);

	void btnEncPress();

	void draw() override;

private:
	bool midLayout = false;
	bool pressed = false;
	uint8_t counter=5;
};

#endif //JAYD_FIRMWARE_ENCTESTELEMENT_H
