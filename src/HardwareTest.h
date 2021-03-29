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

#define MAX_BRIGHTNESS 256
#define NUM_LED 16*9

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

	static bool nuvotonTest();
	static bool sdTest();
	static bool matrixTest();
	static bool soundTest();

	void visualMatrixTest();
	void auditorySoundTest();

	void beeping();
};


#endif