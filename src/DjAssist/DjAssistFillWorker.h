#ifndef JAYD_FIRMWARE_DJASSISTFILLWORKER_H
#define JAYD_FIRMWARE_DJASSISTFILLWORKER_H

#include <atomic>
#include <cstddef>

#ifdef JAYD_ASSIST_HOST_TASK_SEAM
#include <thread>
#endif

// Minimal, self-contained background-worker lifecycle wrapper purpose-built
// for DjAssistController's candidate-fill task. Deliberately bypasses
// CircuitOS's own Util/Task.h rather than patching it: that class's
// start()/stop(true) pair deadlocks forever whenever the underlying
// xTaskCreate() call fails, because Task::start() unconditionally sets
// stopped=false before attempting creation and never restores it on
// failure (confirmed by reading the real bundled Task.impl - not modified
// here, per explicit instruction). This class instead:
//
//   - actually checks the task/thread-creation result itself and reports
//     it via begin()'s bool return + launched(), so a caller knows a
//     launch failed and must not expect a background worker to ever run;
//   - tracks entered()/exited() with its own atomics, set only by the
//     worker function itself (never by CircuitOS), so end() knows
//     precisely when it is actually safe to stop waiting - and, on a
//     failed launch, end() never waits at all, closing the deadlock.
//
// On real firmware this drives the worker via raw FreeRTOS
// xTaskCreate()/self-delete - the same underlying platform primitive
// CircuitOS's own Task class itself uses under the hood - never through
// CircuitOS's Task wrapper. On host builds compiled with
// JAYD_ASSIST_HOST_TASK_SEAM (DjAssistIntegrationSelfCheck only), the
// production path instead spawns a real std::thread so that target can
// exercise the actual concurrent begin()/end()-vs-in-flight-step handshake
// (see testEndBlocksUntilInFlightPortCallReleased); useManualSteppingForTest
// defaults to true there so every pre-existing deterministic scenario
// (which drives fillWorkerStep() itself directly via friend access, one
// bounded call at a time, exactly as before) keeps working completely
// unchanged unless a specific test explicitly opts out of that default.
class DjAssistFillWorker {
public:
	DjAssistFillWorker();
	~DjAssistFillWorker();

	DjAssistFillWorker(const DjAssistFillWorker&) = delete;
	DjAssistFillWorker& operator=(const DjAssistFillWorker&) = delete;

	// Launches the worker, which calls stepFn(arg) in a loop (with a
	// gentle ~20ms cadence between calls, never audio-critical) until
	// end() is called. Returns true iff the underlying task/thread was
	// actually created; false means nothing is running and end() is
	// guaranteed to return immediately with no wait. No-op returning true
	// if already launched.
	bool begin(void (*stepFn)(void*), void* arg, const char* name, size_t stackSize = 4096);

	// Requests the worker stop, then - ONLY if a worker was actually
	// launched - waits for it to actually finish its current/last
	// iteration and exit before returning. Once end() returns, stepFn
	// will never be called again. No-op (returns immediately, no wait)
	// if begin() was never called or last reported failure. Idempotent:
	// safe to call repeatedly and from the destructor.
	void end();

	bool launched() const { return launched_; }
	bool entered() const { return entered_.load(); }
	bool exited() const { return exited_.load(); }

	// Test-only: when true, the NEXT begin() call reports failure
	// immediately (as if task/thread creation failed) without attempting
	// any real creation, then resets back to false (one-shot). Always
	// compiled in (harmless/inert unless a test sets it); lets a host
	// test deterministically prove the launch-failure/no-hang contract
	// without needing to actually exhaust OS resources.
	bool forceLaunchFailureForTest = false;

#ifdef JAYD_ASSIST_HOST_TASK_SEAM
	// Host-only: true (the default) makes begin() report success without
	// spawning any real thread - matching the previous host Util/Task.h
	// stub's behavior exactly - so the many pre-existing deterministic
	// DjAssistIntegrationSelfCheck scenarios that drive fillWorkerStep()
	// directly via friend access are entirely unaffected by this class's
	// introduction. Set to false before calling begin() to opt a
	// specific test into a genuine background std::thread instead.
	bool useManualSteppingForTest = true;
#endif

private:
	static void trampoline(void* self);
	void run();

	void (*stepFn_)(void*) = nullptr;
	void* stepArg_ = nullptr;
	bool launched_ = false;
	std::atomic<bool> stopRequested_{false};
	std::atomic<bool> entered_{false};
	std::atomic<bool> exited_{false};

#ifdef JAYD_ASSIST_HOST_TASK_SEAM
	std::thread* thread_ = nullptr;
#endif
};

#endif
