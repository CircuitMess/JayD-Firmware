#ifndef NIBBLE_HWTEST_H
#define NIBBLE_HWTEST_H
#include <Display/Display.h>
#include <Util/Vector.h>
#include <Input/InputJayD.h>
#include <FS.h>
#include "JayD.hpp"
#include <Util/Task.h>
#include "AudioLib/OutputI2S.h"
#include "AudioLib/SourceWAV.h"

struct Test {
	bool (*test)();
	const char* name;
};

class HardwareTest
{
public:
	HardwareTest(Display& display);
	void start();

private:
	Sprite *canvas;
	Display *display;

	static HardwareTest* test;
	Vector<Test> tests;
	const char* currentTest;

	void log(const char* property, char* value);
	void log(const char* property, float value);
	void log(const char* property, double value);
	void log(const char* property, bool value);
	void log(const char *property, uint32_t value);

	static bool psram();
	static bool nuvotonTest();
	static bool sdTest();
	static bool matrixTest();
	static bool SPIFFSTest();

	void visualMatrixTest();
	void auditorySoundTest();

	void beeping();
};


#endif