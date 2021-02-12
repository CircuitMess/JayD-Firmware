#ifndef JAYD_FIRMWARE_BUTTONS_H
#define JAYD_FIRMWARE_BUTTONS_H

#include <UI/CustomElement.h>

class Buttons : public CustomElement {
public:
	Buttons(ElementContainer *parent);

	void checkIfPlayed();

	void draw();

private:
	int value = 0;
};


#endif //JAYD_FIRMWARE_BUTTONS_H
