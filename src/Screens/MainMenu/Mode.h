#ifndef JAYD_FIRMWARE_MODE_H
#define JAYD_FIRMWARE_MODE_H

#include <UI/CustomElement.h>

namespace MainMenu {
	class Mode : public CustomElement {
	public:
		Mode(ElementContainer *parent);

		void draw();

		void setSelected(bool selected);

	private:
		bool selected = false;
	};
}
#endif //JAYD_FIRMWARE_MODE_H
