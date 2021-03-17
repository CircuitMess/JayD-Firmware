#ifndef JAYD_FIRMWARE_EFFECTS_HPP
#define JAYD_FIRMWARE_EFFECTS_HPP

#include <UI/CustomElement.h>
#include <Display/AnimatedSprite.h>

enum Effect {
	LOWPASS, HIGHPASS, SPEED, REVERB, BITCRUSHER
};

namespace MixScreen {
	class Effects : public CustomElement {
	public:
		Effects(ElementContainer *parent, bool mirrored,Effect effect);

		void draw();

		void setEffect(Effect effect);

		void setIntensity(uint8_t intensity);

		void setSelected(bool selected);

		bool needsUpdate();
	private:

		Effect effect;
		uint8_t intensity = 0;
		bool mirrored = false;
		bool selected = false;
		AnimatedSprite *gif;
	};

}


#endif //JAYD_FIRMWARE_EFFECTS_HPP
