#ifndef JAYD_FIRMWARE_MATRIXPOPUPPICKER_H
#define JAYD_FIRMWARE_MATRIXPOPUPPICKER_H

#include <Arduino.h>
#include <CircuitOS.h>
#include <Support/Modal.h>
#include <Display/GIFAnimatedSprite.h>
#include <UI/LinearLayout.h>
#include "../../InputKeys.h"

class MatrixAnimGIF;

namespace MixScreen {
	class MixScreen;

	class MatrixPopUpPicker : public Modal, public LoopListener, public InputListener {
	public:
		MatrixPopUpPicker(Context& context);

		void draw() override;

		void start() override;

		void stop() override;

		virtual ~MatrixPopUpPicker();

		void unpack() override;

		void loop(uint micros) override;
		void pack() override;


	private:
		static MatrixPopUpPicker* instance;

		LinearLayout screenLayout;

		GIFAnimatedSprite* gif = nullptr;
		MatrixAnimGIF* anim = nullptr;

		int8_t bigMatrixNumber = 2;

		MixScreen* parent = nullptr;

		void buildUI();

		void openGif(uint8_t gifNum);
		void btnEnc(uint8_t i) override;
		void enc(uint8_t i, int8_t value) override;
	};
}
#endif
