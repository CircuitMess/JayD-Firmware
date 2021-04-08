#ifndef JAYD_FIRMWARE_ENCTESTELEMENT_H
#define JAYD_FIRMWARE_ENCTESTELEMENT_H

#include <UI/CustomElement.h>

class EncTestElement : public CustomElement {
public:
	EncTestElement(ElementContainer *parent, bool midLayout);

	void encoderMove(int8_t newValue);

	void btnEncPress();

	void draw() override;

	bool isEncValueDone() const;

	bool isEncPressed() const;

private:
	bool midLayout = false;
	bool encPressed = false;
	uint8_t counter=5;
	bool encValueDone=false;
};

#endif //JAYD_FIRMWARE_ENCTESTELEMENT_H
