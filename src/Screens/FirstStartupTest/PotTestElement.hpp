#ifndef JAYD_FIRMWARE_POTTESTELEMENT_HPP
#define JAYD_FIRMWARE_POTTESTELEMENT_HPP

#include <UI/CustomElement.h>

class PotTestElement : public CustomElement {
public:
	PotTestElement(ElementContainer *parent, bool horizontal);

	void potMove(uint8_t newValue);

	void draw() override;

private:
	int value = 0;
	bool horizontal = false;
	bool done = false;
	uint8_t counter = 0;
};


#endif //JAYD_FIRMWARE_POTTESTELEMENT_HPP
