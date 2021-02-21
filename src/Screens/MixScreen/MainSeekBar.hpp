#ifndef JAYD_FIRMWARE_MAINSEEKBAR_HPP
#define JAYD_FIRMWARE_MAINSEEKBAR_HPP

#include <UI/CustomElement.h>

class MainSeekBar : public CustomElement {
public:
	MainSeekBar(ElementContainer *parent);

	void setPressed(bool isPlaying);

	void draw();

private:
	bool isPlaying = false;
};


#endif //JAYD_FIRMWARE_MAINSEEKBAR_HPP
