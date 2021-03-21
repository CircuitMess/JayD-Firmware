#ifndef JAYD_FIRMWARE_TEXTINPUTSCREEN_H
#define JAYD_FIRMWARE_TEXTINPUTSCREEN_H


#include <Support/Context.h>

namespace TextInputScreen {
	class TextInputScreen : public Context {
	public:

		TextInputScreen(Display &display);

		void start();

		void stop();

		void draw();

	private:

		static TextInputScreen *instance;

		String text="";

		Sprite *sprite = screen.getSprite();

		bool capitalLetters = false;
		bool shiftLetters = false;

		int selectedIndex = 0;
	};
}

#endif //JAYD_FIRMWARE_TEXTINPUTSCREEN_H
