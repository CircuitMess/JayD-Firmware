#ifndef JAYD_FIRMWARE_TRACKCOUNTER_HPP
#define JAYD_FIRMWARE_TRACKCOUNTER_HPP

#include <UI/CustomElement.h>

class TrackCounter : public CustomElement {
public:
	TrackCounter(ElementContainer *parent);

	void draw();

private:

	int minutes = 0;
	int seconds = 0;
	bool pressed = false;

};

#endif //JAYD_FIRMWARE_TRACKCOUNTER_HPP
