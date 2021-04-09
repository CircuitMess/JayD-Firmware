#ifndef JAYD_FIRMWARE_INTROSCREEN_H
#define JAYD_FIRMWARE_INTROSCREEN_H

#include <Support/Context.h>
#include <UI/Screen.h>
#include <UI/LinearLayout.h>
#include <Loop/LoopListener.h>
#include <Display/AnimatedSprite.h>

class PlaybackSystem;


namespace IntroScreen {

	class IntroScreen: public Context, public LoopListener {
	public:

		IntroScreen(Display &display);

		virtual ~IntroScreen();

		void start();

		void stop();

		void draw();

		void loop(uint micros) override;

	private:
		static IntroScreen *instance;

		AnimatedSprite* gif = nullptr;
		PlaybackSystem* playback = nullptr;
		fs::File f1;

	};
}

#endif //JAYD_FIRMWARE_INTROSCREEN_H
