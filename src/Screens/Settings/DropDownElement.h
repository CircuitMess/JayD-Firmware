#ifndef JAYD_FIRMWARE_DROPDOWN_H
#define JAYD_FIRMWARE_DROPDOWN_H

#include "SettingsElement.h"

namespace SettingsScreen {
	class DropDownElement : public SettingsElement {
	public:

		DropDownElement(ElementContainer *partent, String name, std::vector<String> dropDownContent);

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
