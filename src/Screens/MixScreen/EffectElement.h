#ifndef JAYD_FIRMWARE_EFFECTELEMENT_H
#define JAYD_FIRMWARE_EFFECTELEMENT_H

#include <UI/CustomElement.h>
#include <Display/AnimatedSprite.h>
#include <AudioLib/EffectType.hpp>

namespace MixScreen {
	class EffectElement : public CustomElement {
	public:
		EffectElement(ElementContainer *parent, bool mirrored);

		void draw();

		void setType(EffectType effect);
		EffectType getType() const;

		void setIntensity(uint8_t intensity);
		uint8_t getIntensity() const;

		void setSelected(bool selected);
		bool isSelected() const;

		bool needsUpdate();
		void repos() override;
	private:
		EffectType effect = EffectType::NONE;
		uint8_t intensity = 0;
		bool mirrored = false;
		bool selected = false;
		AnimatedSprite *gif = nullptr;

		void setupGif();

		static const uint16_t* const iconsNotMirrored[EffectType::COUNT];
		static const uint16_t* const iconsMirrored[EffectType::COUNT];
		static const uint8_t* const gifIcons[EffectType::COUNT];
		static const size_t gifIconSizes[EffectType::COUNT];
	};

}


#endif //JAYD_FIRMWARE_EFFECTELEMENT_H
