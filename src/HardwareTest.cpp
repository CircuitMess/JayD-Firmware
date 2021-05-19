#include "HardwareTest.h"
#include <SPI.h>
#include <SD.h>
#include <AudioLib/SourceAAC.h>
#include <AudioLib/Systems/PlaybackSystem.h>
#include <Settings.h>
#include "Wire.h"
#include <JayD.h>
#include "SPIFFS.h"
#include "HWTestSPIFFS.hpp"
#include "Devices/LEDmatrix/LEDmatrix.h"
#include "HWTestSD.hpp"

HardwareTest *HardwareTest::test = nullptr;

HardwareTest::HardwareTest(Display &_display) : canvas(_display.getBaseSprite()), display(&_display){

	test = this;

	tests.push_back({HardwareTest::psram, "PSRAM"});
	tests.push_back({HardwareTest::nuvotonTest, "Nuvoton"});
	tests.push_back({HardwareTest::sdTest, "SD Card"});
	tests.push_back({HardwareTest::sdTest, "SD Data"});
	tests.push_back({HardwareTest::matrixTest, "LED Matrix"});
	tests.push_back({HardwareTest::SPIFFSTest, "SPIFFS"});

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
	canvas->setCursor(0, 0);
	canvas->printCenter("JayD Hardware Test");
	canvas->setCursor(0, 8);
	canvas->println();
	canvas->setTextFont(1);
	display->commit();

	bool pass = true;
	for(const Test &test : tests){

		currentTest = test.name;

		canvas->setTextColor(TFT_WHITE);
		canvas->printf("%s: ", test.name);
		display->commit();

		bool result = test.test();

		canvas->setTextColor(result ? TFT_GREEN : TFT_RED);
		canvas->printf("%s\n\n", result ? "PASSED" : "FAILED");
		display->commit();

		if(!(pass &= result)) break;
	}

	canvas->setCursor(0, 103);

	if(pass){
		Serial.println("TEST:pass");

		canvas->setTextColor(TFT_CYAN);
		canvas->printf("\n");
		canvas->setTextFont(2);
		canvas->printCenter("Test Successful!");
		display->commit();

		auditorySoundTest();
		visualMatrixTest();

		for(;;){
			Sched.loop(0);
		}
	}else{
		canvas->setTextColor(TFT_RED);
		Serial.printf("TEST:fail:%s\n", currentTest);
	}

	for(;;);
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
	digitalWrite(JDNV_PIN_RESET, HIGH);

	/* Nuvoton reset */
	digitalWrite(JDNV_PIN_RESET, LOW);
	delay(5);
	digitalWrite(JDNV_PIN_RESET, HIGH);
	delay(10);

	/* Test i2c transmission */
	Wire.beginTransmission(JDNV_ADDR);
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

	LEDmatrixImpl* ledMatrix = new LEDmatrixImpl(16, 9);

	/* Matrix begin test */
	if(!ledMatrix->begin(26, 27)){
		test->log("Begin","Failed");
		delete ledMatrix;
		return false;
	}else{
		ledMatrix->clear();
		ledMatrix->push();

		delete ledMatrix;
		return true;
	}
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

void HardwareTest::auditorySoundTest(){

	File file;
	if(!(file = SD.open("/The Gears of Progress - Section 31 - Tech.aac"))){
		Serial.println("ffail");
		for(;;);
	}

	Settings.get().volumeLevel = 255;
	PlaybackSystem* system = new PlaybackSystem(file);
	system->setVolume(255);
	system->start();
}

void HardwareTest::visualMatrixTest(){

	auto *ledMatrix = new LEDmatrixImpl(16, 9);

	/* Matrix begin test */
	if(!ledMatrix->begin(I2C_SDA, I2C_SCL)){
		for(;;);
	}

	const int brightness = 255;

	ledMatrix->clear();
	ledMatrix->setBrightness(150);

	/* Individual LED test */
	for(int i = 0; i < 144; ++i){
		Sched.loop(0);
		ledMatrix->drawPixel(i, brightness / 2);
		ledMatrix->push();
		delay(5);
		Sched.loop(0);
	}

	/* Brightness change test */
	for(int i = brightness - 1; i >= 0; i -= 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(5);
		Sched.loop(0);
	}
	/* Brightness change test */
	for(int i = 0; i < brightness - 1; i += 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(5);
		Sched.loop(0);
	}
	/* Brightness change test */
	for(int i = brightness - 1; i >= 0; i -= 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(5);
		Sched.loop(0);
	}

	ledMatrix->clear();
	ledMatrix->push();
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
