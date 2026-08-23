#include "Arduino.h"

#include <chrono>
#include <cstdlib>
#include <thread>

namespace {
unsigned long g_hostMicros = 0;
}

unsigned long micros(){
	return g_hostMicros;
}

void delay(uint32_t milliseconds){
	// Deterministic tests never depend on wall-clock time passing here -
	// they either advance the fake clock explicitly via
	// hostStubAdvanceMicros() or step DjAssistController's fill worker
	// directly one bounded call at a time (DjAssistFillWorker::
	// useManualSteppingForTest defaults to true - see that header).
	// A handful of scenarios now opt a real background std::thread into
	// this exact loop (DjAssistFillWorker::run()'s `stepFn_(); delay(20);`
	// cadence) to exercise a genuine begin()/end()-vs-in-flight-step race
	// (see testEndBlocksUntilInFlightPortCallReleased); a real sleep here
	// keeps that thread from busy-spinning at 100% CPU between steps
	// instead of actually yielding, without affecting any single-threaded
	// test (which never calls delay() from more than one thread, if at
	// all).
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void* ps_malloc(size_t size){
	return malloc(size);
}

void hostStubSetMicros(unsigned long value){
	g_hostMicros = value;
}

void hostStubAdvanceMicros(unsigned long deltaMicros){
	g_hostMicros += deltaMicros;
}
