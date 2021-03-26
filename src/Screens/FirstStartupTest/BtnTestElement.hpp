#ifndef JAYD_FIRMWARE_BTNTESTELEMENT_HPP
#define JAYD_FIRMWARE_BTNTESTELEMENT_HPP

#include <UI/CustomElement.h>

class BtnTestElement : public CustomElement {
public:
	BtnTestElement(ElementContainer *parent);

	void btnPress();

	void draw() override;

private:
	int value=0;
	bool pressed=false;

};


#endif //JAYD_FIRMWARE_BTNTESTELEMENT_HPP
