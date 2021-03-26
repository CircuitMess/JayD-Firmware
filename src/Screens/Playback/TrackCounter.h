#ifndef JAYD_FIRMWARE_TRACKCOUNTER_H
#define JAYD_FIRMWARE_TRACKCOUNTER_H

#include <UI/CustomElement.h>

namespace Playback {
	class TrackCounter : public CustomElement {
	public:
		TrackCounter(ElementContainer *parent);

		void draw();

		void setTotalDuration(int totalDuration);

		void setCurrentDuration(int currentDuration);

		int getCurrentDuration() const;

	private:
		int totalDuration = 0;
		int currentDuration = 0;

	};
}
#endif //JAYD_FIRMWARE_TRACKCOUNTER_H
