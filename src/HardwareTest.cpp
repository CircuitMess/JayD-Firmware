#include "HardwareTest.h"
#include <SPI.h>
#include <SD.h>
#include "Wire.h"
#include "JayD.hpp"
#include "Devices/LEDmatrix/LEDmatrix.h"
#include "Matrix/MatrixR.h"

HardwareTest *HardwareTest::test = nullptr;

HardwareTest::HardwareTest(Display &_display) : canvas(_display.getBaseSprite()), display(&_display){

	test = this;

	tests.push_back({HardwareTest::nuvotonTest, "Nuvoton"});
	tests.push_back({HardwareTest::sdTest, "SD Card"});
	tests.push_back({HardwareTest::matrixTest, "LED Matrix"});
	tests.push_back({ HardwareTest::soundTest, "Sound File"});

	SPI.begin(18, 19, 23);
	SPI.setFrequency(60000000);

	Wire.begin(I2C_SDA, I2C_SCL);
}

void HardwareTest::start(){

	printf("\nStarting test ...\n");
	printf("CPU Freq: %d MHz\n", ESP.getCpuFreqMHz());
	printf("I2C Frequency: %zu Hz\n\n", Wire.getClock());

	canvas->clear(TFT_BLACK);
	canvas->setTextColor(TFT_GOLD);
	canvas->setTextFont(2);
	canvas->setTextSize(1);
	canvas->setCursor(0, 0);
	canvas->printCenter("JayD Hardware Test");
	canvas->setCursor(0, 10);
	canvas->println();
	display->commit();

	bool pass = true;
	for(const Test &test : tests){

		currentTest = test.name;

		canvas->setTextColor(TFT_WHITE);
		canvas->printf("%s: ", test.name);
		display->commit();

		bool result = test.test();

		canvas->setTextColor(result ? TFT_GREEN : TFT_RED);
		canvas->printf("%s\n", result ? "PASSED" : "FAILED");
		display->commit();

		if(!(pass &= result)) break;
	}

	if(pass){
		canvas->setTextColor(TFT_CYAN);
		canvas->printf("\n");
		canvas->printCenter("Test Successful!");
		display->commit();

		printf("HW test completed successfully.\n\n");
		printf("Prepare for visual & auditory test...\n");

		delay(1000);

		visualMatrixTest();
		delay(100);
		auditorySoundTest();

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
			test->log("Identification", "Passed");
			return true;
		}else{
			test->log("Identification", "Failed -> Wrong data acquired.");
			return false;
		}
	}else{
		test->log("Identification", "Failed -> Wire not available.");
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
		test->log("SD Write", "Error Opening File.");
		return false;
	}

	const char *writeBuff = "SD Card write test";
	size_t writeBuffLen = strlen(writeBuff);

	/* File write test */
	if(file.printf("%s", writeBuff) != writeBuffLen){
		test->log("SD Write","Failed writing to file.");
		return false;
	}

	file.close();

	/* File opening test */
	file = SD.open("/SDCardTest.txt", "r");
	if(!file){
		test->log("SD Read", "Error Opening File.");
		return false;
	}

	char *readBuff = static_cast<char *>(malloc(writeBuffLen * sizeof(char) * 2));

	/* File read test */
	if(file.available()){
		file.readBytes(readBuff, writeBuffLen);
	}else{
		test->log("SD Read","File Not Available.");
		free(readBuff);
		return false;
	}

	/* Compare read-write */
	if(!strcmp(writeBuff, readBuff)){
		test->log("SD Compare", "Write buffer not equal to read buffer.");
		free(readBuff);
		return false;
	}else{
		test->log("SD Card","Test Successful.");
		free(readBuff);
		return true;
	}

}

bool HardwareTest::matrixTest(){

	auto *ledMatrix = new LEDmatrixImpl(16, 9);

	/* Matrix begin test */
	if(!ledMatrix->begin(26, 27)){
		test->log("LED Matrix","Begin failed.");
		return false;
	}else{
		ledMatrix->clear();
		ledMatrix->push();

		delete ledMatrix;

		test->log("LED Matrix","Begin successful.");
		return true;
	}
}

bool HardwareTest::soundTest(){

	File file = SD.open("/SandstormCut.wav");
	/* File opening test */
	if(!(file)){
		test->log("Sound File","File Error");
		return false;
	}
	else{
		return true;
	}
}

void HardwareTest::auditorySoundTest(){

	File file;
	if(!(file = SD.open("/SandstormCut.wav"))){
		printf("File error\n");
		for(;;);
	}

	auto *wav = new SourceWAV(file);

	auto *i2s = new OutputI2S({
									  .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
									  .sample_rate = 44100,
									  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
									  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
									  .communication_format = i2s_comm_format_t(
											  I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
									  .intr_alloc_flags = 0,
									  .dma_buf_count = 6,
									  .dma_buf_len = 512,
									  .use_apll = false
							  }, i2s_pin_config, I2S_NUM_0);

	i2s->setSource(wav);

	i2s->setGain(0.2);
	i2s->start();

	/* I2S running test*/
	if(!i2s->isRunning()){
		printf("I2S not running.\n");
		for(;;);
	}

	/* Audio Task -> Core 0*/
	Task audioTask("Audio", [](Task *task){
		auto *i2s = (OutputI2S *) task->arg;
		uint32_t currTime = millis();

		while(task->running){
			if(i2s->isRunning() && (millis()-currTime) < 10000){
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

	/* Audio task running test */
	if(!audioTask.running){
		printf("Audio task not running.\n");
	}

	/* Scheduler loop */
	for(;;){
		Sched.loop(0);
		if(!audioTask.running){
			break;
		}
	}

	printf("I2S sound test completed.\n");

}

void HardwareTest::visualMatrixTest(){

	auto *ledMatrix = new LEDmatrixImpl(16, 9);

	/* Matrix begin test */
	if(!ledMatrix->begin(26, 27)){
		printf("LED Matrix begin failed.\n");
		for(;;);
	}


	ledMatrix->clear();
	ledMatrix->setBrightness(MAX_BRIGHTNESS / 2);

	/* Individual LED test */
	for(int i = 0; i < NUM_LED; ++i){

		ledMatrix->drawPixel(i, MAX_BRIGHTNESS / 2);
		ledMatrix->push();
		delay(50);
	}
	printf("Individual LED test completed.\n");

	/* Brightness change test */
	for(int i = MAX_BRIGHTNESS - 1; i >= 0; i -= 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(10);
	}
	/* Brightness change test */
	for(int i = 0; i < MAX_BRIGHTNESS - 1; i += 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(10);
	}
	/* Brightness change test */
	for(int i = MAX_BRIGHTNESS - 1; i >= 0; i -= 5){

		ledMatrix->setBrightness(i);
		ledMatrix->push();
		delay(10);
	}
	printf("Brightness test completed.\n");

	ledMatrix->clear();
	ledMatrix->push();

	printf("LED Matrix test completed.\n\n");

}


void HardwareTest::log(const char *property, char *value){
	Serial.printf("\n%s:%s:%s\n", currentTest, property, value);
}

void HardwareTest::log(const char *property, float value){
	Serial.printf("\n%s:%s:%f\n", currentTest, property, value);
}

void HardwareTest::log(const char *property, double value){
	Serial.printf("\n%s:%s:%lf\n", currentTest, property, value);
}

void HardwareTest::log(const char *property, bool value){
	Serial.printf("\n%s:%s:%d\n", currentTest, property, value ? 1 : 0);
}
