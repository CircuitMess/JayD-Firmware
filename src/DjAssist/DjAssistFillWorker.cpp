#include "DjAssistFillWorker.h"

#include <Arduino.h>

#ifndef JAYD_ASSIST_HOST_TASK_SEAM
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

DjAssistFillWorker::DjAssistFillWorker(){
}

DjAssistFillWorker::~DjAssistFillWorker(){
	end();
}

void DjAssistFillWorker::run(){
	entered_.store(true);
	while(!stopRequested_.load()){
		stepFn_(stepArg_);
		delay(20); // gentle background cadence; never audio-critical.
	}
	exited_.store(true);
}

void DjAssistFillWorker::trampoline(void* self){
	static_cast<DjAssistFillWorker*>(self)->run();
#ifndef JAYD_ASSIST_HOST_TASK_SEAM
	vTaskDelete(nullptr); // self-delete; never returns.
#endif
}

bool DjAssistFillWorker::begin(void (*stepFn)(void*), void* arg, const char* name, size_t stackSize){
	if(launched_) return true;
	if(forceLaunchFailureForTest){
		forceLaunchFailureForTest = false; // one-shot.
		launched_ = false;
		return false;
	}

	stepFn_ = stepFn;
	stepArg_ = arg;
	stopRequested_.store(false);
	entered_.store(false);
	exited_.store(false);

#ifdef JAYD_ASSIST_HOST_TASK_SEAM
	(void) name;
	(void) stackSize;
	if(useManualSteppingForTest){
		// Matches the previous host Util/Task.h stub exactly: report
		// success but never actually run stepFn_ ourselves - the friend
		// test drives fillWorkerStep() directly, deterministically.
		launched_ = true;
		return true;
	}
	try{
		thread_ = new std::thread(&DjAssistFillWorker::trampoline, this);
	}catch(...){
		thread_ = nullptr;
		launched_ = false;
		return false;
	}
	launched_ = true;
	return true;
#else
	// Raw FreeRTOS, not CircuitOS's Task class (see class doc comment).
	// pxCreatedTask may be null - the task self-deletes via
	// vTaskDelete(nullptr) once its own loop exits, so no handle needs to
	// be retained afterward.
	const BaseType_t created = xTaskCreate(
		&DjAssistFillWorker::trampoline, name, stackSize, this, 0, nullptr);
	if(created != pdPASS){
		launched_ = false;
		return false;
	}
	launched_ = true;
	return true;
#endif
}

void DjAssistFillWorker::end(){
	if(!launched_) return;
	stopRequested_.store(true);

#ifdef JAYD_ASSIST_HOST_TASK_SEAM
	if(thread_){
		thread_->join();
		delete thread_;
		thread_ = nullptr;
	}
	// Manual-stepping mode never spawned a real thread - nothing to wait
	// for, matching the previous stub's immediate-return behavior.
#else
	// Guaranteed non-blocking forever: exited_ is set only by our own
	// run(), which begin() having returned true guarantees will actually
	// execute (we checked xTaskCreate's own result above), unlike
	// CircuitOS's Task::stopped.
	while(!exited_.load()){
		delay(1);
	}
#endif
	launched_ = false;
}
