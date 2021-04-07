#ifndef JAYD_FIRMWARE_TEXTINPUTSCREEN_H
#define JAYD_FIRMWARE_TEXTINPUTSCREEN_H


#include <Support/Context.h>
#include <FS.h>

namespace TextInputScreen {
	class TextInputScreen : public Context {
	public:

		TextInputScreen(Display &display);

		virtual ~TextInputScreen();

		void start();

		void stop();

		void draw();

		void pack() override;

		void unpack() override;

	private:

		static TextInputScreen *instance;

		String text="";

		Sprite *sprite = screen.getSprite();

		bool capitalLetters = false;
		bool shiftLetters = false;

		int selectedIndex = 0;

		fs::File background;

		Color* buffer= nullptr;
	};
}

#endif //JAYD_FIRMWARE_TEXTINPUTSCREEN_H
