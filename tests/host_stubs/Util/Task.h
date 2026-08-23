#ifndef JAYD_HOST_STUB_UTIL_TASK_H
#define JAYD_HOST_STUB_UTIL_TASK_H

// Host-test stand-in for CircuitOS's Util/Task.h. Real DjAssistController::
// begin() constructs one of these and calls start() to launch its
// background candidate-fill loop (fillTaskTrampoline()'s
// `while(task->running) { self->fillWorkerStep(); delay(20); }`); this stub
// intentionally never spawns a thread or invokes the stored function at
// all, matching the review's explicit split of the "background Task
// wrapper" from the real per-step logic under test - DjAssistIntegrationSelfCheck
// (a friend of DjAssistController) drives fillWorkerStep() itself, directly
// and deterministically, one bounded call at a time. start()/stop() only
// track `running` so begin()/end()'s real bookkeeping still executes
// unmodified.
#include <cstddef>
#include <cstdint>
#include <string>

class Task {
public:
	Task(std::string taskName, void (*fun)(Task*), size_t stackSize = 2048, void* arg = nullptr){
		(void) taskName;
		(void) fun;
		(void) stackSize;
		this->arg = arg;
	}

	void start(uint8_t priority = 0, int8_t core = -1){
		(void) priority;
		(void) core;
		running = true; // deliberately never invokes func - see class doc comment.
	}

	void stop(bool wait = false){
		(void) wait;
		running = false;
	}

	void kill(){
		running = false;
	}

	bool isStopped() const { return !running; }

	bool running = false;
	void* arg = nullptr;
};

#endif
