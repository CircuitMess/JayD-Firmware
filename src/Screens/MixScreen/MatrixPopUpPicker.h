#ifndef JAYD_FIRMWARE_MATRIXPOPUPPICKER_H
#define JAYD_FIRMWARE_MATRIXPOPUPPICKER_H

#include <Arduino.h>
#include <CircuitOS.h>
#include <Support/Modal.h>
#include <Display/GIFAnimatedSprite.h>
#include <UI/LinearLayout.h>

namespace MixScreen {
	class MixScreen;

	class MatrixPopUpPicker : public Modal, public LoopListener {
	public:
		MatrixPopUpPicker(Context& context);

		void draw() override;

		void start() override;

		void stop() override;

		virtual ~MatrixPopUpPicker();

		void unpack() override;

		void loop(uint micros) override;


	private:
		static MatrixPopUpPicker* instance;

		LinearLayout screenLayout;

		GIFAnimatedSprite* matrixGif = nullptr;

		int8_t bigMatrixNumber = 2;

		MixScreen* parent = nullptr;

		void buildUI();

		void openGif(uint8_t gifNum);
	};
}
#endif
