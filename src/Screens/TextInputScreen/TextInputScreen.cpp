#include <Input/InputJayD.h>
#include <FS/CompressedFile.h>
#include "TextInputScreen.h"
#include <SPIFFS.h>

TextInputScreen::TextInputScreen *TextInputScreen::TextInputScreen::instance = nullptr;

TextInputScreen::TextInputScreen::TextInputScreen(Display &display) : Context(display){

	instance = this;

	TextInputScreen::pack();
}

TextInputScreen::TextInputScreen::~TextInputScreen(){
	free(backgroundBuffer);
	instance = nullptr;
}

void TextInputScreen::TextInputScreen::start(){
	instance->draw();
	instance->screen.commit();
	InputJayD::getInstance()->setEncoderMovedCallback(0, [](int8_t value){
		instance->selectedIndex = instance->selectedIndex + value;
		if(instance->selectedIndex < 0){
			instance->selectedIndex = 30;
		}else if(instance->selectedIndex > 30){
			instance->selectedIndex = 0;
		}
		instance->draw();
		instance->screen.commit();
	});
	InputJayD::getInstance()->setBtnPressCallback(2, [](){
		if(instance->selectedIndex == 27){
			instance->text = instance->text.substring(0, instance->text.length() - 1);
		}else if(instance->selectedIndex == 30){
			instance->pop(new String(instance->text));
			return;
		}else if(instance->selectedIndex == 28){
			instance->shiftLetters = !instance->shiftLetters;
		}else if(instance->selectedIndex == 29){
			instance->capitalLetters = !instance->capitalLetters;
		}else if(instance->text.length() > 21){
			return;
		}else if(instance->selectedIndex == 26){
			instance->text += (instance->capitalLetters || instance->shiftLetters ? (char) ('A' - 33) : (char) ('a' - 65));
		}else{
			instance->text += (instance->capitalLetters || instance->shiftLetters ? (char) ('A' + instance->selectedIndex) : (char) ('a' + instance->selectedIndex));
			instance->shiftLetters = false;
		}
		instance->draw();
		instance->screen.commit();

	});
}


void TextInputScreen::TextInputScreen::stop(){
	InputJayD::getInstance()->removeEncoderMovedCallback(0);
	InputJayD::getInstance()->removeBtnPressCallback(2);
}

void TextInputScreen::TextInputScreen::draw(){
	screen.getSprite()->drawIcon(backgroundBuffer, 0, 0, 160, 128, 1);
	sprite->fillRect(2, 35, 156, 1, TFT_WHITE);
	sprite->setTextFont(1);
	sprite->setTextSize(1);
	sprite->setTextColor(TFT_WHITE);
	sprite->setCursor(5, 25);
	sprite->printf("%s", text.c_str());

	uint8_t static const rows = 4;
	uint8_t static const columns = 8;
	for(int i = 0; i < rows; i++){
		char letter;
		for(int j = 0; j < columns; j++){
			int y = 46 + i * 18;
			int x = 7 + j * 20;
			int k = i * columns + j;

			if(k == selectedIndex){
				sprite->drawRect(x - 4, y - 3, 15, 15, TFT_WHITE);
			}

			if(k == 26){
				sprite->drawRect(x - 1, y + 4, 1, 5, TFT_WHITE);
				sprite->drawRect(x - 1, y + 8, 8, 1, TFT_WHITE);
				sprite->drawRect(x + 7, y + 4, 1, 5, TFT_WHITE);
			}else if(k == 27){
				sprite->fillTriangle(x + 4, y, x, y + 4, x + 4, y + 8, TFT_WHITE);

			}else if(k == 28){
				sprite->drawTriangle(x - 1, y + 5, x + 3, y + 1, x + 7, y + 5, shiftLetters ? TFT_GREEN : TFT_WHITE);
			}else if(k == 29){
				sprite->fillTriangle(x - 1, y + 5, x + 3, y + 1, x + 7, y + 5, capitalLetters ? TFT_GREEN : TFT_WHITE);
			}else if(k == 30){
				sprite->fillTriangle(x - 1, y + 4, x + 1, y + 6, x - 1, y + 4, TFT_WHITE);
				sprite->fillTriangle(x + 6, y + 3, x + 2, y + 7, x + 7, y + 2, TFT_WHITE);
			}else if(k > 30){
				return;
			}else{
				capitalLetters || shiftLetters ? letter = 'A' + k : letter = 'a' + k;
				sprite->setTextFont(1);
				sprite->setTextSize(1);
				sprite->setTextColor(TFT_WHITE);
				sprite->setCursor(x + 1, y + 1);
				sprite->printf("%c", letter);
			}
		}
	}

}


void TextInputScreen::TextInputScreen::pack(){
	Context::pack();
	free(backgroundBuffer);
	backgroundBuffer = nullptr;
}

void TextInputScreen::TextInputScreen::unpack(){
	Context::unpack();

	backgroundBuffer = static_cast<Color *>(ps_malloc(160 * 128 * 2));
	if(backgroundBuffer == nullptr){
		Serial.println("Text input background unpack error");
		return;
	}

	fs::File bgFile = CompressedFile::open(SPIFFS.open("/backgroundBlack.raw.hs"), 10, 9);
	bgFile.read(reinterpret_cast<uint8_t *>(backgroundBuffer), 160 * 128 * 2);
	bgFile.close();
}



