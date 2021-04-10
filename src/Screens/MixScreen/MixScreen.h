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
#include <AudioLib/SpeedModifier.h>
#include <unordered_map>
#include <Input/InputJayD.h>
#include <Util/Task.h>
#include <AudioLib/Systems/MixSystem.h>
#include <Matrix/VuVisualizer.h>
#include <Matrix/RoundVuVisualiser.h>
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

	private:
		static MixScreen *instance;

		fs::File f1;
		fs::File f2;
		MixSystem* system = nullptr;

		LinearLayout* screenLayout;
		LinearLayout* leftLayout;
		LinearLayout* rightLayout;

		SongSeekBar* leftSeekBar;
		SongSeekBar* rightSeekBar;

		SongName* leftSongName;
		SongName* rightSongName;

		EffectElement* effectElements[6] = { nullptr };

		bool isPlaying = false;

		void buildUI();


		void buttonPress(uint8_t id) override;
		void buttonRelease(uint8_t id) override;
		void buttonHold(uint8_t id) override;
		void encoderMove(uint8_t id, int8_t value) override;
		void potMove(uint8_t id, uint8_t value) override;

		static const std::unordered_map<uint8_t, uint8_t> mapBtn;
		static const std::unordered_map<uint8_t, uint8_t> mapEnc;

		uint32_t lastDraw = 0;

		VuVisualizer leftVu;
		VuVisualizer rightVu;
		RoundVuVisualiser midVu;

		uint8_t channel = 0;
		bool hold = false;

	};
}


#endif //JAYD_FIRMWARE_MIXSCREEN_H
