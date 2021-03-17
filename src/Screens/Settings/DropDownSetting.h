#ifndef JAYD_FIRMWARE_DROPDOWN_HPP
#define JAYD_FIRMWARE_DROPDOWN_HPP

#include "SettingsElement.h"

namespace Settings {
	class DropDownSetting : public SettingsElement {
	public:

		DropDownSetting(ElementContainer *partent, String name, std::vector<String> dropDownContent);

		void activate() override;

		void selectNext();

		void selectPrev();

	private:

		bool showDropDown = false;

		int index = 0;
		std::vector<String> dropDownContent;

	protected:
		void drawControl() override;
	};
}

#endif //JAYD_FIRMWARE_DROPDOWN_HPP
