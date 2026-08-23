#ifndef JAYD_FIRMWARE_DJASSISTSESSIONPORT_H
#define JAYD_FIRMWARE_DJASSISTSESSIONPORT_H

#include "DjAssistTypes.h"

// Arduino-free seam between DjAssistController and DjSession, extracted so
// the REAL controller logic (buildGuard()/resolveBoundary()/fillWorkerStep()/
// tickTransition()/tickRollback()/the actuator) can be driven by a host test
// against a fake session, instead of only ever being validated by the real
// firmware build. This is not a parallel reimplementation: every method
// here mirrors an existing DjSession method of the exact same name/
// signature/constness, and DjSession implements this interface directly
// (see DjSession.h) - the port only narrows what DjAssistController is
// allowed to see, it does not change what DjSession does.
class DjAssistSessionPort {
public:
	virtual ~DjAssistSessionPort() {}

	// Actuator surface (DjAssistSessionActuator, DjAssistController.cpp).
	virtual DjSubmitResult setPlaying(uint8_t deck, bool playing, DjCommandOrigin origin) = 0;
	virtual DjSubmitResult setSync(uint8_t deck, bool armed, int8_t masterDeck, DjCommandOrigin origin) = 0;
	virtual DjSubmitResult setMix(uint8_t mix, DjCommandOrigin origin) = 0;
	virtual void assistTrackCommand(uint32_t commandId) = 0;
	virtual DjCommandStatus assistTrackedStatus(uint32_t commandId) = 0;
	virtual bool copySnapshot(DjSnapshot& snapshot) = 0;

	// Bounded candidate-table data source (DjAssistController::fillWorkerStep()/
	// candidateTableReady()/tickSuggestions()). assistMetadataRevision() is
	// a DEDICATED, purely-internal monotonic counter - distinct from
	// DjSession's externally-meaningful library generation (semantic
	// library identity, e.g. from LibraryIndex) - bumped on every reader
	// mutation attempt (refresh that actually swaps the reader,
	// invalidate, shutdown), even when the external generation/key value
	// passed in happens to be unchanged (e.g. a same-generation sidecar
	// file replacement, or a loss+reopen cycle landing back on the same
	// external generation). Candidate-table freshness must key off THIS
	// counter, never the external generation, so those cases are never
	// mistaken for "nothing changed".
	virtual uint32_t assistMetadataRevision() = 0;
	virtual uint32_t assistTrackCount() = 0;
	virtual bool assistTrackEntry(uint32_t index, DjAssistLibraryEntry& outEntry, uint32_t& outRevision) = 0;

	// Boundary/guard inputs (DjAssistController::buildGuard()/resolveBoundary()).
	virtual bool mediaPresent() const = 0;
	virtual uint64_t deckElapsedFrames(uint8_t deck) const = 0;
	virtual bool nextDownbeatFrame(uint8_t deck, uint64_t currentFrame, uint64_t& outFrame) const = 0;
	virtual bool nextPhraseFrame(uint8_t deck, uint64_t currentFrame, uint64_t& outFrame) = 0;
	virtual DjAssistIntentGenerations assistIntentGenerationsSnapshot() = 0;
	virtual void assistPurgePendingSystemCommands(uint8_t deck) = 0;
};

#endif
