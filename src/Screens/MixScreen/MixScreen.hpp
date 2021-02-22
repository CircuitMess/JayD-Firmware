#ifndef JAYD_FIRMWARE_MIXSCREEN_HPP
#define JAYD_FIRMWARE_MIXSCREEN_HPP

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include "LeftSeekBar.hpp"
#include "RightSeekBar.hpp"

class MixScreen : public Context {
public:

	MixScreen(Display &display);

	void start();

	void stop();

	void draw();

private:
	static MixScreen *instance;

	LinearLayout screenLayout;
	LinearLayout leftLayout;
	LinearLayout rightLayout;

	std::vector<LeftSeekBar *> leftSeekBar;
	std::vector<RightSeekBar *> rightSeekBar;

	void buildUI();

};


#endif //JAYD_FIRMWARE_MIXSCREEN_HPP
