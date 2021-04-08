#include <Input/InputJayD.h>
#include <JayD.hpp>
#include "MainMenu.h"
#include <Loop/LoopManager.h>
#include <FS/CompressedFile.h>
#include <SPIFFS.h>

MainMenu::MainMenu *MainMenu::MainMenu::instance = nullptr;
MatrixPartition *partitions[4] = {&matrixManager.matrixL, &matrixManager.matrixR, &matrixManager.matrixMid, &matrixManager.matrixBig};
const char *partitionNames[4] = {"left", "right", "mid", "big"};

MainMenu::MainMenu::MainMenu(Display &display) : Context(display), screenLayout(new LinearLayout(&screen, HORIZONTAL)){


	for(int i = 0; i < 3; i++){
		item.push_back(new MainMenuItem(screenLayout, static_cast<MenuItemType>(i)));
	}

	backgroundPicture = CompressedFile::open(SPIFFS.open("/mainMenuBackground.raw.hs"), 14, 10);
	jayDlogo = CompressedFile::open(SPIFFS.open("/jayD_logo.raw.hs"), 8, 7);

	instance = this;
	instance->item[0]->isSelected(true);
	buildUI();

	for(uint8_t j = 0; j < 4; j++){
		for(uint8_t i = 1; i < totalAnimations + 1; i++){
			gifData[j].unusedIdleAnimations.push_back(i);
		}
	}
	//pack();
}

MainMenu::MainMenu::~MainMenu(){
	backgroundPicture.close();
	jayDlogo.close();
	instance = nullptr;
	free(backgroundBuffer);
	free(logoBuffer);

}

void MainMenu::MainMenu::start(){
	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		if(instance == nullptr) return;
		if(value == 0) return;
		instance->itemNum = instance->itemNum + value;

		if(instance->itemNum < 0){
			instance->itemNum = instance->item.size() - 1;
		}else if(instance->itemNum >= instance->item.size()){
			instance->itemNum = 0;
		}

		for(MainMenuItem *i : instance->item){
			i->isSelected(false);
		}
		instance->item[instance->itemNum]->isSelected(true);

		instance->draw();
		instance->screen.commit();
	});

	InputJayD::getInstance()->setBtnPressCallback(BTN_MID, [](){
		if(instance == nullptr) return;

		Display &display = *instance->getScreen().getDisplay();
		int8_t selected = instance->itemNum;
		Serial.println(selected);

		instance->stop();
		delete instance;

		if(selected == 0){
			//Playback::Playback* playback = new Playback::Playback(display);

			/*SongList::SongList* list = new SongList::SongList(display);
			list->unpack();
			list->start();*/
		}
	});

	for(uint8_t j = 0; j < 4; j++){
		startRandomAnimation(j);
	}

	jumpTime = 0;
	draw();
	screen.commit();

	LoopManager::addListener(this);
}

void MainMenu::MainMenu::stop(){
	matrixManager.stopAnimation();
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(BTN_MID);
}

void MainMenu::MainMenu::draw(){
	screen.getSprite()->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);
	screen.getSprite()->drawIcon(logoBuffer,screen.getTotalX()+48 + sin((float) jumpTime / 500000.0f) * 20,screen.getTotalY()+8,64,24,1);
	screen.draw();

}

void MainMenu::MainMenu::buildUI(){

	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(31);

	for(int i = 0; i < item.size(); i++){
		screenLayout->addChild(item[i]);
	}

	screenLayout->reflow();

	screen.addChild(screenLayout);
	screen.repos();
}

void MainMenu::MainMenu::loop(uint micros){
	jumpTime += micros;

	for(uint8_t i = 0; i < 4; i++){
		MatrixPartition *partition = partitions[i];
		if(partition->getAnimationCompletionRate() >= 99.0 && !gifData[i].animationLoopDone){
			gifData[i].animationLoopCounter++;
			gifData[i].animationLoopDone = true;
			if(gifData[i].animationLoopCounter > gifData[i].requiredAnimationLoops - 1){
				startRandomAnimation(i);
			}
		}
		if(partition->getAnimationCompletionRate() <= 1){
			gifData[i].animationLoopDone = false;
		}
	}
	

	bool update = false;
	for(const auto &i : item){
		update |= i->needsUpdate();
	}

	update |= floor(sin((float) (jumpTime - micros) / 500000.0f)) != floor(sin((float) jumpTime / 500000.0f));

	if(update){
		draw();
		screen.commit();
	}
}

void MainMenu::MainMenu::pack(){
	Context::pack();
	free(backgroundBuffer);
	backgroundBuffer = nullptr;
	free(logoBuffer);
	logoBuffer= nullptr;
}

void MainMenu::MainMenu::unpack(){
	Context::unpack();
	backgroundBuffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("MainMenu background picture unpack error");
		return;
	}

	backgroundPicture.seek(0);
	backgroundPicture.read(reinterpret_cast<uint8_t *>(backgroundBuffer), (160 * 128 * 2));

	logoBuffer = static_cast<Color *>(ps_malloc(45 * 42 * 2));
	if(logoBuffer == nullptr){
		Serial.println("MainMenu background picture unpack error");
		return;
	}

	jayDlogo.seek(0);
	jayDlogo.read(reinterpret_cast<uint8_t *>(logoBuffer), (45 * 42 * 2));
}

void MainMenu::MainMenu::startRandomAnimation(uint8_t i){
	uint animationIndex = 0;
	uint randomIndex = random(0, gifData[i].unusedIdleAnimations.size());
	animationIndex = gifData[i].unusedIdleAnimations[randomIndex];
	gifData[i].unusedIdleAnimations.erase(gifData[i].unusedIdleAnimations.begin() + randomIndex);

	gifData[i].usedIdleAnimations.push_back(animationIndex);
	if(gifData[i].usedIdleAnimations.size() == (int(totalAnimations / 2) + 1)){
		gifData[i].unusedIdleAnimations.push_back(gifData[i].usedIdleAnimations[0]);
		gifData[i].usedIdleAnimations.erase(gifData[i].usedIdleAnimations.begin());
	}
	char buffer[25];
	sprintf(buffer, "/matrixGIF/%s%d.gif", partitionNames[i], animationIndex);
	gifData[i].requiredAnimationLoops = 3;
	Serial.println(buffer);
	delay(5);
	partitions[i]->startAnimation(new Animation(new File(SPIFFS.open(buffer))), true);
	gifData[i].animationLoopCounter = 0;
}
