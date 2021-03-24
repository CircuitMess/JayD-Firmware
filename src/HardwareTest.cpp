#include "HardwareTest.h"
#include <SPI.h>
#include <SD.h>
#include "Wire.h"
#include "JayD.hpp"



HardwareTest* HardwareTest::test = nullptr;

HardwareTest::HardwareTest(Display &_display) : canvas(_display.getBaseSprite()), display(&_display)
{
	test = this;

	tests.push_back({ HardwareTest::nuvotonTest, "Nuvoton"});
	tests.push_back({ HardwareTest::sdTest, "SDCard"});
	tests.push_back({ HardwareTest::soundTest, "Sound"});
	tests.push_back({ HardwareTest::matrixTest, "LEDMatrix"});

	SPI.begin(18, 19, 23);
	SPI.setFrequency(60000000);

	Wire.begin(I2C_SDA, I2C_SCL);
}

void HardwareTest::start()
{
	printf("\nStarting test ...\n");
	printf("CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
	printf("I2C Frequency: %zu Hz\n\n", Wire.getClock());

	canvas->clear(TFT_BLACK);
	canvas->setTextColor(TFT_WHITE);
	canvas->setTextFont(2);
	canvas->setTextSize(1);
	canvas->setCursor(0, 0);
	canvas->printCenter("TESTING MODE");
	canvas->setCursor(0, 10);
	canvas->println();
	display->commit();

	bool pass = true;
	for(const Test& test : tests){
		currentTest = test.name;

		canvas->setTextColor(TFT_BLACK);
		canvas->printf("%s: ", test.name);
		display->commit();

		bool result = test.test();

		canvas->setTextColor(result ? TFT_GREEN : TFT_RED);
		canvas->printf("%s\n", result ? "PASS" : "FAIL");
		display->commit();

		if(!(pass &= result)) break;
	}

	if(pass){
		printf("Test completed successfully.");
		for(;;);

	}else{
		printf("Test failed at %s checkpoint.\n", currentTest);
		for(;;);
	}
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
		printf("Wire transmission failed.\n");
		return false;
	}

	/* Nuvoton Identification */
	Wire.beginTransmission(JDNV_ADDR);
	Wire.write(BYTE_IDENTIFY);
	Wire.endTransmission();
	Wire.requestFrom(JDNV_ADDR, 1);
	if(Wire.available()){
		if(Wire.read() == JDNV_ADDR){
			printf("Nuvoton identification Successful\n");
			return true;
		}
		else{
			printf("Nuvoton identification Failed -> Wrong data acquired\n");
			return false;
		}
	}else{
		printf("Nuvoton identification Failed -> Wire not available\n");
	}

}

bool HardwareTest::sdTest(){

	if(!SD.begin(22, SPI)){
		printf("SD Card Not Recognized.\n");
		return false;
	}

	fs::File file = SD.open("/SDCardTest.txt","w");


	/*if(){
		return true;
	}
	else{
		return false;
	}*/

	return true;

}

bool HardwareTest::soundTest(){

	return true;

}

bool HardwareTest::matrixTest(){

	return true;
}



void HardwareTest::log(const char* property, char* value){
	Serial.printf("%s:%s:%s\n", currentTest, property, value);
}

void HardwareTest::log(const char* property, float value){
	Serial.printf("%s:%s:%f\n", currentTest, property, value);
}

void HardwareTest::log(const char* property, double value){
	Serial.printf("%s:%s:%lf\n", currentTest, property, value);
}

void HardwareTest::log(const char* property, bool value){
	Serial.printf("%s:%s:%d\n", currentTest, property, value ? 1 : 0);
}
