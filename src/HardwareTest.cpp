#include "HardwareTest.h"
#include <SPI.h>
#include <SD.h>
#include <Settings.h>
#include "Wire.h"
#include <JayD.h>
#include "SPIFFS.h"
#include "HWTestSPIFFS.hpp"
#include "HWTestSD.hpp"
#include <Util/HWRevision.h>

HardwareTest *HardwareTest::test = nullptr;

HardwareTest::HardwareTest(Display &_display) : canvas(_display.getBaseSprite()), display(&_display){

	test = this;

	tests.push_back({HardwareTest::psram, "PSRAM"});
	tests.push_back({HardwareTest::nuvotonTest, "Nuvoton"});
	tests.push_back({HardwareTest::sdTest, "SD Card"});
	tests.push_back({HardwareTest::sdTest, "SD Data"});
	tests.push_back({HardwareTest::matrixTest, "LED Matrix"});
	tests.push_back({HardwareTest::SPIFFSTest, "SPIFFS"});
	tests.push_back({HardwareTest::hwRevision, "HW rev"});

	SPI.begin(18, 19, 23);
	SPI.setFrequency(60000000);

	Wire.begin(I2C_SDA, I2C_SCL);
}

void HardwareTest::start(){
	Serial.println();
	uint64_t mac = ESP.getEfuseMac();
	uint32_t upper = mac >> 32;
	uint32_t lower = mac & 0xffffffff;
	Serial.printf("TEST:begin:%x%x\n", upper, lower);

	canvas->clear(TFT_BLACK);
	canvas->setTextColor(TFT_GOLD);
	canvas->setTextFont(2);
	canvas->setTextSize(1);
	canvas->setCursor(display->getWidth()/2, 0);
	canvas->printCenter("JayD Hardware Test");
	canvas->setCursor(display->getWidth()/2, 8);
	canvas->println();
	canvas->setTextFont(1);
	display->commit();
	canvas->setTextDatum(textdatum_t::top_center);


	bool pass = true;
	for(const Test &test : tests){

		currentTest = test.name;

		canvas->setTextColor(TFT_WHITE);
		canvas->printf("%s: ", test.name);
		display->commit();

		bool result = test.test();

		canvas->setTextColor(result ? TFT_SILVER : TFT_ORANGE);
		canvas->printf("%s\n", result ? "PASS" : "FAIL");
		display->commit();

		if(!(pass &= result)) break;
	}

	canvas->print("\n\n");
	canvas->setTextColor(pass ? TFT_BLUE : TFT_ORANGE);
	canvas->printCenter(pass ? "All OK!" : "FAIL!");
	display->commit();

	if(pass){
		Serial.println("TEST:pass");
		postTestPass();
	}else{
		Serial.printf("TEST:fail:%s\n", currentTest);
		postTestFail();
	}
}

void HardwareTest::postTestPass(){
	File file = SPIFFS.open("/Test.aac");
	PlaybackSystem playback(file);
	playback.setVolume(255);
	playback.start();

	uint32_t beepTime = millis();

	int ledVal = 255;
	int ledDir = -1;
	uint32_t ledTime = 0;

	bool painted = false;
	const auto color = TFT_GREEN;
	uint32_t flashTime = 0;
	for(;;){
		if(millis() - flashTime >= 500){
			for(int x = 0; x < canvas->width(); x++){
				for(int y = 0; y <  canvas->height(); y++){
					if(!painted && canvas->readPixel(x, y) == TFT_BLACK){
						canvas->drawPixel(x, y, color);
					}else if(painted && canvas->readPixel(x, y) == color){
						canvas->drawPixel(x, y, TFT_BLACK);
					}
				}
			}

			flashTime = millis();
			painted = !painted;
			display->commit();
		}

		if(millis() - ledTime >= 15){
			ledTime = millis();

			LEDmatrix.clear({ 255, 255, 255, (uint8_t) ledVal });
			LEDmatrix.push();

			ledVal += ledDir;
			if(ledVal == 255 || ledVal == 100){
				ledDir *= -1;
			}
		}

		if(millis() - beepTime >= 1000){
			beepTime = millis();
			playback.stop();
			playback.start();
		}

		LoopManager::loop();
		Sched.loop(0);
	}
}

void HardwareTest::postTestFail(){
	bool painted = false;
	const auto color = TFT_RED;
	auto flashTime = 0;

	for(;;){
		if(millis() - flashTime < 500) continue;

		for(int x = 0; x < canvas->width(); x++){
			for(int y = 0; y <  canvas->height(); y++){
				if(!painted && canvas->readPixel(x, y) == TFT_BLACK){
					canvas->drawPixel(x, y, color);
				}else if(painted && canvas->readPixel(x, y) == color){
					canvas->drawPixel(x, y, TFT_BLACK);
				}
			}
		}

		flashTime = millis();
		painted = !painted;
		display->commit();
	}
}

bool HardwareTest::psram(){
	if(!(psramFound() && psramInit())) return false;

	uint32_t total = ESP.getPsramSize();

	if(total != 4194236){
		test->log("total", total);
		return false;
	}

	return true;
}

bool HardwareTest::nuvotonTest(){

	pinMode(JDNV_PIN_RESET, OUTPUT);
	//digitalWrite(JDNV_PIN_RESET, HIGH);

	/* Nuvoton reset */
	digitalWrite(JDNV_PIN_RESET, LOW);
	delay(50);
	digitalWrite(JDNV_PIN_RESET, HIGH);
	delay(500);

	/* Test i2c transmission */
	Wire.beginTransmission(JDNV_ADDR);
	delay(5);
	if(Wire.endTransmission() != 0){
		test->log("Wire Transmission","Failed");
		return false;
	}

	/* Nuvoton Identification */
	Wire.beginTransmission(JDNV_ADDR);
	Wire.write(BYTE_IDENTIFY);
	Wire.endTransmission();
	Wire.requestFrom(JDNV_ADDR, 1);
	if(Wire.available()){
		if(Wire.read() == JDNV_ADDR){
			return true;
		}else{
			test->log("Identification Failed", "Wrong data acquired");
			return false;
		}
	}else{
		test->log("Identification Failed", "Wire not available");
	}
}

bool HardwareTest::sdTest(){

	/* SD begin test */
	if(!SD.begin(22, SPI)){
		test->log("SD Card","Not Recognized");
		return false;
	}

	/* File opening test */
	fs::File file = SD.open("/SDCardTest.txt", "w");
	if(!file){
		test->log("File Opening Error", file.name());
		return false;
	}

	const char *writeBuff = "SD Card write test";
	size_t writeBuffLen = strlen(writeBuff);
	size_t testBuffLen = file.printf("%s", writeBuff);

	/* File write test */
	if( testBuffLen != writeBuffLen){
		char logBuffer[100];
		sprintf(logBuffer, "Expected %zu, got %zu",writeBuffLen,testBuffLen);
		test->log("File Write failed",logBuffer);
		return false;
	}

	file.close();

	/* File opening test */
	file = SD.open("/SDCardTest.txt", "r");
	if(!file){
		test->log("File Opening Error", file.name());
		return false;
	}

	char *readBuff = static_cast<char *>(malloc(writeBuffLen * sizeof(char) * 2));

	/* File read test */
	if(file.available()){
		file.readBytes(readBuff, writeBuffLen);
	}else{
		test->log("Read Error","File Not Available.");
		free(readBuff);
		return false;
	}

	/* Compare read-write */
	if(!strcmp(writeBuff, readBuff)){
		char logBuffer[100];
		sprintf(logBuffer, "Expected %s, got %s",writeBuff,readBuff);
		test->log("Compare Error", logBuffer);
		free(readBuff);
		return false;
	}

	file.close();
	SD.remove("/SDCardTest.txt");

	free(readBuff);
	return true;
}

bool HardwareTest::sdData(){
	for(const auto & check : SDSizes){
		File f = SD.open(String("/") + check.name);
		if(!f){
			test->log("Failed opening", check.name);
			f.close();
			return false;
		}

		if(f.size() != check.size){
			char logBuffer[100];
			sprintf(logBuffer, "%s - expected %d, got %lu", check.name.c_str(), check.size, f.size());
			test->log("Size mismatch", logBuffer);
			f.close();
			return false;
		}

		f.close();
	}

	return true;
}

bool HardwareTest::matrixTest(){

	Wire.beginTransmission(0x74);

	if(Wire.endTransmission() != 0){
		test->log("Begin","Failed");
		return false;
	}
	return true;
}

bool HardwareTest::SPIFFSTest(){

	File file;

	/* SPIFFS begin test */
	if(!SPIFFS.begin()){
		test->log("Begin","Failed");
		return false;
	}

	for(const auto & check : SPIFFSChecksums){

		file = SPIFFS.open(check.name);

		if(!file){
			test->log("File open error", check.name);
			return false;
		}

		char buff;
		uint32_t fileBytesSum = 0;

		while(file.readBytes(&buff,1)){

			fileBytesSum+=buff;
		}

		if(fileBytesSum != check.sum){
			char logBuffer[100];
			sprintf(logBuffer, "%s - expected %d, got %d", check.name.c_str(), check.sum, fileBytesSum);
			test->log("Checksum mismatch", logBuffer);
			file.close();
			return false;
		}

		file.close();
	}

	return true;
}

bool HardwareTest::hwRevision(){
	const auto rev = HWRevision::get();
	if(rev != 0){
		test->canvas->printf("Fused: ");
		test->canvas->setTextColor(TFT_GOLD);
		test->canvas->printf("%d ", rev);
		test->canvas->setTextColor(TFT_WHITE);

		return rev == CurrentVersion;
	}

	HWRevision::write(CurrentVersion);
	HWRevision::commit();

	return true;
}

void HardwareTest::log(const char *property, char *value){
	Serial.printf("\n%s:%s:%s\n", currentTest, property, value);
}

void HardwareTest::log(const char *property, float value){
	Serial.printf("\n%s:%s:%f\n", currentTest, property, value);
}

void HardwareTest::log(const char *property, uint32_t value){
	Serial.printf("\n%s:%s:%u\n", currentTest, property, value);
}

void HardwareTest::log(const char *property, double value){
	Serial.printf("\n%s:%s:%lf\n", currentTest, property, value);
}

void HardwareTest::log(const char *property, bool value){
	Serial.printf("\n%s:%s:%d\n", currentTest, property, value ? 1 : 0);
}
