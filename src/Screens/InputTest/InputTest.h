#ifndef JAYD_FIRMWARE_FIRSTSTARTUPTEST_H
#define JAYD_FIRMWARE_FIRSTSTARTUPTEST_H

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include <UI/CustomElement.h>
#include "EncTestElement.h"
#include "BtnTestElement.hpp"
#include "PotTestElement.hpp"
#include <unordered_map>
#include <Input/InputJayD.h>

namespace InputTest {
	class InputTest : public Context, public JayDInputListener {
	public:
		InputTest(Display& display);

		virtual ~InputTest();

		void start() override;

		void stop() override;

		void draw() override;

		void setDoneCallback(void (* doneCallback)(InputTest*));


	private:

		static InputTest* instance;

		LinearLayout* screenLayout;
		LinearLayout* leftLayout;
		LinearLayout* midLayout;
		LinearLayout* rightLayout;
		LinearLayout* bottomLayout;

		std::vector<EncTestElement*> leftEncBtnTest;
		std::vector<EncTestElement*> midEncBtnTest;
		std::vector<EncTestElement*> rightEncBtnTest;
		std::vector<BtnTestElement*> bottomBtnTest;
		std::vector<PotTestElement*> potMidTest;
		std::vector<PotTestElement*> potBotTest;

		void buildUI();

		void checkIfDone();

		int doneCounter = 0;

		bool checkDone = false;

		int confirmedCounter = 0;

		void buttonPress(uint8_t id) override;

		void encoderMove(uint8_t id, int8_t value) override;

		void buttonHold(uint8_t id) override;

		void buttonRelease(uint8_t id) override;

		void potMove(uint8_t id, uint8_t value) override;

		void (* doneCallback)(InputTest* test) = nullptr;

		void checkIfTestIsDone();
	};
}
#endif //JAYD_FIRMWARE_FIRSTSTARTUPTEST_H
