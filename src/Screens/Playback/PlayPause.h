#ifndef JAYD_FIRMWARE_PLAYPAUSE_H
#define JAYD_FIRMWARE_PLAYPAUSE_H

#include <UI/CustomElement.h>

class PlayPause : public CustomElement {
public:
	PlayPause(ElementContainer *parent);

	void checkIfPlaying();

	void draw();

private:
	bool pressed = false;
};


#endif //JAYD_FIRMWARE_PLAYPAUSE_H
