#ifndef NIBBLE_HWTEST_H
#define NIBBLE_HWTEST_H
#include <Display/Display.h>
#include <Util/Vector.h>
#include <Input/InputJayD.h>
#include <FS.h>

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
	static bool soundTest();
	static bool matrixTest();


	void beeping();
};


#endif