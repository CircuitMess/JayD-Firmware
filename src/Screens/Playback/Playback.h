#ifndef JAYD_FIRMWARE_PLAYBACK_H
#define JAYD_FIRMWARE_PLAYBACK_H

#include <Support/Context.h>
#include <UI/Image.h>
#include <UI/LinearLayout.h>
#include <UI/TextElement.h>
#include "SongName.hpp"

class Playback : public Context {
public:

	Playback(Display &display);

	void start();

	void stop();

	void draw();

private:
	static Playback *instance;

	LinearLayout screenLayout;
	LinearLayout songNameLayout;
	LinearLayout timeElapsedLayout;
	LinearLayout buttonLayout;

	std::vector<SongName *> song;

	void buildUI();

};


#endif //JAYD_FIRMWARE_PLAYBACK_H
