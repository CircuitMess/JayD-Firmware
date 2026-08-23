#ifndef JAYD_HOST_STUB_SYNC_MUTEX_H
#define JAYD_HOST_STUB_SYNC_MUTEX_H

// Host-test stand-in for CircuitOS's Sync/Mutex.h. Most DjAssistIntegrationSelfCheck
// scenarios still drive DjAssistController single-threaded and
// deterministically (DjAssistFillWorker::useManualSteppingForTest defaults
// to true - see that header - so the background fill worker never actually
// runs a thread there), but some scenarios now deliberately opt into a
// genuine background std::thread for the fill worker (see
// testEndBlocksUntilInFlightPortCallReleased and related tests) to exercise
// the real begin()/end()-vs-in-flight-step handshake. A trivial no-op here
// would be a genuine, sanitizer-visible data race between that thread and
// the main test thread's own candidateMutex_-protected calls
// (tickSuggestions()/candidateTableReady()), not merely an inert stub -
// this must provide REAL mutual exclusion, so it wraps std::mutex.
#include <mutex>

class Mutex {
public:
	Mutex(){}
	~Mutex(){}

	bool lock(){ mutex_.lock(); return true; }
	void unlock(){ mutex_.unlock(); }

private:
	std::mutex mutex_;
};

#endif
