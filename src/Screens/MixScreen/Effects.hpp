#ifndef JAYD_FIRMWARE_EFFECTS_HPP
#define JAYD_FIRMWARE_EFFECTS_HPP

#include <UI/CustomElement.h>

enum Effect {
	LOWPASS, HIGHPASS, SPEED, REVERB, BITCRUSHER
};

namespace MixScreen {
	class Effects : public CustomElement {
	public:
		Effects(ElementContainer *parent, bool mirrored);

		void draw();

		void setEffect(Effect effect);

		void setIntensity(uint8_t intensity);

		void setSelected(bool selected);

	private:
		Effect effect;
		uint8_t intensity = 0;
		bool mirrored = false;
		bool selected = false;
	};

}


#endif //JAYD_FIRMWARE_EFFECTS_HPP
