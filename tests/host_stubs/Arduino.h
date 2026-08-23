#ifndef JAYD_HOST_STUB_ARDUINO_H
#define JAYD_HOST_STUB_ARDUINO_H

// Minimal host-test stand-in for the ESP32 Arduino core header. It exists
// solely so DjAssistController.cpp (unmodified, real production source) can
// be compiled by DjAssistIntegrationSelfCheck instead of only the real
// firmware build (see DjAssistController.h's doc comment and
// DjAssistSessionPort.h). Declares only the handful of free symbols that
// file actually uses: micros(), delay(), ps_malloc(), and the `byte` alias
// used by Util/Task.h's stub. Definitions live in host_arduino_shim.cpp so
// this header can be included from multiple translation units without an
// ODR violation.

#include <cstddef>
#include <cstdint>

typedef uint8_t byte;

unsigned long micros();
void delay(uint32_t milliseconds);
void* ps_malloc(size_t size);

// Test-control only (not part of the real Arduino.h): lets
// DjAssistIntegrationSelfCheck advance the deterministic host clock
// micros() reads, instead of relying on wall-clock time, so crossfade-ramp
// timing assertions are reproducible.
void hostStubSetMicros(unsigned long value);
void hostStubAdvanceMicros(unsigned long deltaMicros);

#endif
