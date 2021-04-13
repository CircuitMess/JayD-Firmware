#include "HardwareTest.h"
#include <SPI.h>
#include <SD.h>
#include "Wire.h"
#include "JayD.hpp"
#include "SPIFFS.h"
#include "SPIFFSFiles.hpp"
#include "Devices/LEDmatrix/LEDmatrix.h"

HardwareTest *HardwareTest::test = nullptr;

HardwareTest::HardwareTest(Display &_display) : canvas(_display.getBaseSprite()), display(&_display){

	test = this;

	tests.push_back({HardwareTest::psram, "PSRAM"});
	tests.push_back({HardwareTest::nuvotonTest, "Nuvoton"});
	tests.push_back({HardwareTest::sdTest, "SD Card"});
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
	canvas->setCursor(0, 10);
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

	if(pass){
		Serial.println("TEST:pass");

		canvas->setTextColor(TFT_CYAN);
		canvas->printf("\n");
		canvas->setTextFont(2);
		canvas->printCenter("Test Successful!");
		display->commit();

		auditorySoundTest();
		visualMatrixTest();
	}else{
		Serial.printf("TEST:fail:%s\n", currentTest);
	}

	for(;;);
}

bool HardwareTest::psram(){
	if(!(psramFound() && psramInit())) return false;

	uint32_t free = ESP.getFreePsram();

	if(free != 4194252){
		test->log("free", free);
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
		test->log("File Write", "Error Opening File.");
		return false;
	}

	const char *writeBuff = "SD Card write test";
	size_t writeBuffLen = strlen(writeBuff);

	/* File write test */
	if(file.printf("%s", writeBuff) != writeBuffLen){
		test->log("File Write","Failed writing to file.");
		return false;
	}

	file.close();

	/* File opening test */
	file = SD.open("/SDCardTest.txt", "r");
	if(!file){
		test->log("File Read", "Error Opening File.");
		return false;
	}

	char *readBuff = static_cast<char *>(malloc(writeBuffLen * sizeof(char) * 2));

	/* File read test */
	if(file.available()){
		file.readBytes(readBuff, writeBuffLen);
	}else{
		test->log("File Read","File Not Available.");
		free(readBuff);
		return false;
	}

	/* Compare read-write */
	if(!strcmp(writeBuff, readBuff)){
		test->log("Compare Files", "Write buffer not equal to read buffer.");
		free(readBuff);
		return false;
	}

	file.close();
	SD.remove("/SDCardTest.txt");

	free(readBuff);
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

	uint16_t idx = 0;

	while(files[idx].sum != 0){

		if(!SPIFFS.exists(files[idx].name)){
			test->log("File name error",files[idx].name.substring(1));
			return false;
		}
		file = SPIFFS.open(files[idx].name);

		if(!file){
			test->log("File open error",files[idx].name.substring(1));
			return false;
		}

		char buff;
		uint32_t fileBytesSum = 0;

		while(file.readBytes(&buff,1)){

			fileBytesSum+=buff;
		}

		if(fileBytesSum != files[idx].sum){
			test->log("File size error",files[idx].sum);
			return false;
		}

		idx++;
		file.close();
	}

	SPIFFS.end();

	return true;
}

void HardwareTest::auditorySoundTest(){

	File file;
	if(!(file = SD.open("/Walter.wav"))){
		for(;;);
	}

	auto *wav = new SourceWAV(file);

	auto *i2s = new OutputI2S({
									  .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
									  .sample_rate = 48000,
									  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
									  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
									  .communication_format = i2s_comm_format_t(
											  I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
									  .intr_alloc_flags = 0,
									  .dma_buf_count = 32,
									  .dma_buf_len = 512,
									  .use_apll = false
							  }, i2s_pin_config, I2S_NUM_0);

	i2s->setSource(wav);

	i2s->setGain(0.1);
	i2s->start();

	/* I2S running test*/
	if(!i2s->isRunning()){
		for(;;);
	}

	/* Audio Task -> Core 0*/
	Task audioTask("Audio", [](Task *task){
		auto *i2s = (OutputI2S *) task->arg;
		uint32_t currTime = millis();

		while(task->running){
			Sched.loop(0);

			if(i2s->isRunning() && (millis()-currTime) < 6000){
				i2s->loop(0);
			}
			else{
				i2s->stop();
				task->running = false;
			}
		}
	});

	audioTask.arg = (void *) i2s;

	audioTask.start(1, 0);
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

		ledMatrix->drawPixel(i, brightness / 2);
		ledMatrix->push();
		delay(5);
	}

	/* Brightness change test */
	for(int i = brightness - 1; i >= 0; i -= 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(5);
	}
	/* Brightness change test */
	for(int i = 0; i < brightness - 1; i += 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(5);
	}
	/* Brightness change test */
	for(int i = brightness - 1; i >= 0; i -= 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(5);
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
