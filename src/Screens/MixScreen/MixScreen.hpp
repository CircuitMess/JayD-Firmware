#ifndef JAYD_FIRMWARE_MIXSCREEN_HPP
#define JAYD_FIRMWARE_MIXSCREEN_HPP

#include <Support/Context.h>
#include <UI/LinearLayout.h>

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



	void buildUI();

};


#endif //JAYD_FIRMWARE_MIXSCREEN_HPP
