#ifndef JAYD_FIRMWARE_LEFTSEEKBAR_HPP
#define JAYD_FIRMWARE_LEFTSEEKBAR_HPP

#include <UI/CustomElement.h>

class LeftSeekBar : public CustomElement {
public:
	LeftSeekBar(ElementContainer *parent);

	void setPressed(bool playing);

	void draw();

	void setTotalDuration(int totalDuration);

	void setCurrentDuration(int currentDuration);

private:
	bool playing = false;
	int totalDuration = 0;
	int currentDuration = 0;
};


#endif //JAYD_FIRMWARE_LEFTSEEKBAR_HPP
