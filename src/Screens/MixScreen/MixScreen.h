#ifndef JAYD_FIRMWARE_MIXSCREEN_H
#define JAYD_FIRMWARE_MIXSCREEN_H

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include "SongSeekBar.h"
#include "SongName.h"
#include "EffectElement.h"
#include <AudioLib/SourceWAV.h>
#include <AudioLib/EffectProcessor.h>
#include <AudioLib/Mixer.h>
#include <AudioLib/OutputI2S.h>
#include <unordered_map>
#include <Input/InputJayD.h>
#include <Util/Task.h>

namespace MixScreen {
	class MixScreen : public Context, public LoopListener, public JayDInputListener {
	public:

		MixScreen(Display &display);

		void start();

		void stop();

		void draw();
		void returned(void* data) override;

		void loop(uint micros) override;

		virtual ~MixScreen();

		static void audioThread(Task* task);

	private:
		static MixScreen *instance;

		Task audioTask;

		fs::File f1;
		fs::File f2;

		SourceWAV* s1;
		SourceWAV* s2;

		EffectProcessor* effector1;
		EffectProcessor* effector2;
		Mixer* mixer;
		OutputI2S* out;

		LinearLayout* screenLayout;
		LinearLayout* leftLayout;
		LinearLayout* rightLayout;

		SongSeekBar* leftSeekBar;
		SongSeekBar* rightSeekBar;

		SongName* leftSongName;
		SongName* rightSongName;

		EffectElement* effectElements[6] = { nullptr };

		bool isPlaying = false;

		Effect* effects[6] = { nullptr };

		void buildUI();


		void buttonPress(uint8_t id) override;
		void buttonRelease(uint8_t id) override;
		void buttonHold(uint8_t id) override;
		void encoderMove(uint8_t id, int8_t value) override;
		void potMove(uint8_t id, uint8_t value) override;

		static const std::unordered_map<uint8_t, uint8_t> mapBtn;
		static const std::unordered_map<uint8_t, uint8_t> mapEnc;
	};
}


#endif //JAYD_FIRMWARE_MIXSCREEN_H
