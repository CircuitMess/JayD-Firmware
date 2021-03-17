#ifndef JAYD_FIRMWARE_EFFECTELEMENT_HPP
#define JAYD_FIRMWARE_EFFECTELEMENT_HPP

#include <UI/CustomElement.h>
#include <Display/AnimatedSprite.h>

enum EffectType {
	LOWPASS, HIGHPASS, SPEED, REVERB, BITCRUSHER
};

namespace MixScreen {
	class EffectElement : public CustomElement {
	public:
		EffectElement(ElementContainer *parent, bool mirrored, EffectType effect);

		void draw();

		void setEffect(EffectType effect);

		void setIntensity(uint8_t intensity);

		void setSelected(bool selected);

		bool needsUpdate();
	private:
		EffectType effect;
		uint8_t intensity = 0;
		bool mirrored = false;
		bool selected = false;
		AnimatedSprite *gif;
	};

}


#endif //JAYD_FIRMWARE_EFFECTELEMENT_HPP
