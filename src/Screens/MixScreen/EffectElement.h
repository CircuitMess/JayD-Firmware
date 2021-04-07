#ifndef JAYD_FIRMWARE_EFFECTELEMENT_H
#define JAYD_FIRMWARE_EFFECTELEMENT_H

#include <UI/CustomElement.h>
#include <Display/AnimatedSprite.h>
#include <AudioLib/EffectType.hpp>

namespace MixScreen {
	class EffectElement : public CustomElement {
	public:
		EffectElement(ElementContainer *parent, bool mirrored);

		virtual ~EffectElement();

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

		static const String iconsNotMirrored[EffectType::COUNT];
		static const String iconsMirrored[EffectType::COUNT];
		static const String gifIcons[EffectType::COUNT];

		Color *bufferIcons= nullptr;
	};

}


#endif //JAYD_FIRMWARE_EFFECTELEMENT_H
