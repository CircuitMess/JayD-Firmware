#ifndef JAYD_FIRMWARE_MIXSCREEN_H
#define JAYD_FIRMWARE_MIXSCREEN_H

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include "SongSeekBar.h"
#include "SongName.h"
#include "EffectElement.h"
#include "MatrixPopUpPicker.h"
#include <AudioLib/Systems/MixSystem.h>
#include <Matrix/VuVisualizer.h>
#include <Matrix/RoundVuVisualiser.h>
#include <Input/InputJayD.h>
#include "../../InputKeys.h"

namespace MixScreen {
	class MixScreen : public Context, public LoopListener, public JayDInputListener, public InputListener {
		friend MatrixPopUpPicker;
	public:

		MixScreen(Display& display);

		void start();

		void stop();

		void draw();

		void returned(void* data) override;

		void loop(uint micros) override;

		virtual ~MixScreen();
		void pack() override;
		void unpack() override;
		void setBigVuStarted(bool bigVuStarted);

	private:
		static MixScreen* instance;

		fs::File f1;
		fs::File f2;
		Color *selectedBackgroundBuffer = nullptr;
		MixSystem* system = nullptr;

		LinearLayout* screenLayout;
		LinearLayout* leftLayout;
		LinearLayout* rightLayout;

		SongSeekBar* leftSeekBar;
		SongSeekBar* rightSeekBar;

		SongName* leftSongName;
		SongName* rightSongName;

		EffectElement* effectElements[6] = {nullptr};

		void buildUI();

		uint8_t selectedChannel = 0;
		bool isRecording = false;
		bool doneRecording = false;
		String saveFilename;
		void saveRecording();
		void drawSaveStatus();


		uint32_t lastDraw = 0;
		bool drawQueued = false;

		uint32_t seekTime = 0;
		bool wasRunning = false;
		int8_t seekChannel = -1;

		VuVisualizer leftVu;
		VuVisualizer rightVu;
		RoundVuVisualiser midVu;

		bool bigVuStarted = true;

		void startBigVu();
		void stopBigVu();

		void potMove(uint8_t id, uint8_t value) override;

		void encTwoBot() override;
		void encTwoTop() override;
		void btnCombination() override;
		void btn(uint8_t i) override;
		void btnEnc(uint8_t i) override;
		void enc(uint8_t id, int8_t value) override;
		void encBtnHold(uint8_t i) override;

	};
}


#endif //JAYD_FIRMWARE_MIXSCREEN_H
