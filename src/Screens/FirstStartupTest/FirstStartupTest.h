#ifndef JAYD_FIRMWARE_FIRSTSTARTUPTEST_H
#define JAYD_FIRMWARE_FIRSTSTARTUPTEST_H

#include <Support/Context.h>
#include <UI/LinearLayout.h>
#include <UI/CustomElement.h>
#include "EncTestElement.h"
#include "BtnTestElement.hpp"
#include "PotTestElement.hpp"
#include <unordered_map>

namespace FirstStartupTest {
	class FirstStartupTest : public Context {
	public:
		FirstStartupTest(Display &display);

		virtual ~FirstStartupTest();

		void start() override;

		void stop() override;

		void draw() override;


	private:

		static FirstStartupTest *instance;

		LinearLayout screenLayout;
		LinearLayout leftLayout;
		LinearLayout midLayout;
		LinearLayout rightLayout;
		LinearLayout bottomLayout;

		std::vector<EncTestElement*> leftEncBtnTest;
		std::vector<EncTestElement*> midEncBtnTest;
		std::vector<EncTestElement*> rightEncBtnTest;
		std::vector<BtnTestElement*> bottomBtnTest;
		std::vector<PotTestElement*> potMidTest;
		std::vector<PotTestElement*> potBotTest;

		void buildUI();

		void checkIfDone();

		static const std::unordered_map<uint8_t, uint8_t> mapBtn;

		int doneCounter=0;
	};
}
#endif //JAYD_FIRMWARE_FIRSTSTARTUPTEST_H
