#ifndef JAYD_WIRELESS_BRINGUP_H
#define JAYD_WIRELESS_BRINGUP_H

#include <stdint.h>

struct WirelessPairingStatus {
	bool open = false;
	uint32_t remainingMs = 0;
	char code[7] = {};
};

class WirelessBringup {
public:
	static void begin();
	static void loop();
	static bool copyPairingStatus(WirelessPairingStatus& status);
};

#endif
