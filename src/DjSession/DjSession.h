#ifndef JAYD_FIRMWARE_DJSESSION_H
#define JAYD_FIRMWARE_DJSESSION_H

#include <AudioLib/InfoGenerator.h>
#include <AudioLib/Systems/MixSystem.h>
#include <FS.h>
#include <Loop/LoopListener.h>
#include <Sync/Mutex.h>
#include <atomic>
#include "../Metadata/JaydMetadata.h"
#include "../DjAssist/DjAssistController.h"
#include "../DjAssist/DjAssistSessionPort.h"
#include "DjSessionState.h"

class DjSession : public LoopListener, public DjAssistSessionPort {
public:
	static DjSession* begin(uint8_t leftGain, uint8_t rightGain, uint8_t mix);
	static DjSession* get();
	static void end();

	DjSubmitResult submit(DjCommand command);
	DjSubmitResult loadDeck(
		uint8_t deck,
		const char* path,
		DjCommandOrigin origin,
		const DjTrackIdentity* identity = nullptr
	);
	DjSubmitResult setPlaying(uint8_t deck, bool playing, DjCommandOrigin origin);
	DjSubmitResult seek(uint8_t deck, uint16_t seconds, DjCommandOrigin origin);
	DjSubmitResult setGain(uint8_t deck, uint8_t gain, DjCommandOrigin origin);
	DjSubmitResult setMix(uint8_t mix, DjCommandOrigin origin);
	DjSubmitResult setEffectType(uint8_t deck, uint8_t slot, uint8_t type, DjCommandOrigin origin);
	DjSubmitResult setEffectIntensity(uint8_t deck, uint8_t slot, uint8_t intensity, DjCommandOrigin origin);
	DjSubmitResult setRecording(bool recording, DjCommandOrigin origin);
	DjSubmitResult setQuantize(uint8_t deck, DjQuantizeResolution resolution, DjCommandOrigin origin);
	DjSubmitResult loopEngage(uint8_t deck, DjLoopLength length, DjCommandOrigin origin);
	DjSubmitResult loopDisengage(uint8_t deck, DjCommandOrigin origin);
	DjSubmitResult loopReloop(uint8_t deck, DjCommandOrigin origin);
	// masterDeck: -1 requests auto-master (the other deck); otherwise an explicit deck index.
	DjSubmitResult setSync(uint8_t deck, bool armed, int8_t masterDeck, DjCommandOrigin origin);
	DjSubmitResult setCue(uint8_t deck, uint8_t cue, DjCommandOrigin origin);
	DjSubmitResult triggerCue(uint8_t deck, uint8_t cue, DjCommandOrigin origin);
	DjSubmitResult clearCue(uint8_t deck, uint8_t cue, DjCommandOrigin origin);
#if defined(JAYD_ENABLE_WIRELESS)
	DjSubmitResult requestPairing(DjCommandOrigin origin);
#endif

	// Coach/one-shot-transition control surface. Always present (the
	// physical Assist bank and browser/API use these too, not gated on
	// wireless).
	DjSubmitResult assistSetMode(bool coachEnabled, DjCommandOrigin origin);
	DjSubmitResult assistArmTransition(
		uint8_t fromDeck,
		uint8_t toDeck,
		uint32_t libraryIndex,
		const DjTrackIdentity& targetIdentity,
		uint8_t crossfadeBeats,
		bool startAtBoundary,
		bool tempoLock,
		DjCommandOrigin origin
	);
	DjSubmitResult assistCancelTransition(DjCommandOrigin origin);
	// Bounded, POD copy of current Coach/transition state - safe for a
	// browser/API payload or physical-bank UI cache.
	bool copyAssistSnapshot(DjAssistSnapshot& snapshot) const;

	// Bounded candidate-table data source for DjAssistController, backed by
	// the same already-indexed metadata reader used by resolveMetadata() -
	// never a fresh file read outside these bounded, mutex-guarded calls.
	// assistMetadataRevision() is a lock-free atomic load (see
	// metadataRevision below): every reader-mutating call (refresh that
	// actually swaps the reader, invalidate, shutdown) bumps it BEFORE
	// mutating the reader, so a caller can safely re-read it a second time
	// immediately adjacent to its own unrelated lock/commit (e.g.
	// DjAssistController::fillWorkerStep()'s candidateMutex_ critical
	// section) with zero risk of lock-ordering/deadlock against
	// metadataMutex, closing the review's "check-then-lock gap" for good.
	// Deliberately a SEPARATE counter from libraryGeneration (the
	// externally-meaningful semantic library identity, e.g. from
	// LibraryIndex): a same-generation sidecar file replacement, or a
	// metadata loss+reopen cycle that happens to land back on the same
	// external generation, must still invalidate the candidate table, and
	// libraryGeneration alone cannot distinguish those cases from "nothing
	// changed".
	uint32_t assistMetadataRevision() override;
	uint32_t assistTrackCount() override;
	// outRevision reports the exact metadataRevision this read was
	// performed under, captured atomically (single metadataMutex
	// acquisition) with the entry read itself - not a separate before/
	// after probe - so a concurrent refreshLibraryMetadata()/
	// invalidateLibraryMetadata() swapping the reader mid-scan can never
	// leave outRevision and outEntry describing two different underlying
	// reader states (the review's "check-then-lock gap"). Set regardless
	// of whether the read itself succeeds, so callers can still detect a
	// stale pass on a failed/corrupt record.
	bool assistTrackEntry(uint32_t index, DjAssistLibraryEntry& outEntry, uint32_t& outRevision) override;
	// Cheap, in-memory downbeat hint from the already-built beat grid,
	// vs. the bounded but real SD read behind nextPhraseFrame() - callers
	// are expected to throttle the latter (see DjAssistController).
	bool mediaPresent() const override;
	uint64_t deckElapsedFrames(uint8_t deck) const override;
	bool nextDownbeatFrame(uint8_t deck, uint64_t currentFrame, uint64_t& outFrame) const override;
	bool nextPhraseFrame(uint8_t deck, uint64_t currentFrame, uint64_t& outFrame) override;

	// Durable (never evictable, unlike the bounded recentResults ring)
	// single-slot outcome tracker for the one in-flight command the Coach
	// transition/rollback state machine is ever watching at a time (the
	// state machine is strictly sequential - only one command is submitted
	// and awaited per step). assistTrackCommand() must be called
	// immediately after a successful submit(); assistTrackedStatus()
	// returns DJ_COMMAND_PENDING until that exact command id resolves via
	// the normal loop()/finishWithDiagnostics() path, regardless of how
	// many other commands are processed (and evicted from recentResults)
	// in between.
	void assistTrackCommand(uint32_t commandId) override;
	DjCommandStatus assistTrackedStatus(uint32_t commandId) override;
	// Monotonic non-system ("user") intent generations for the global mix
	// and per-deck play/sync channels, bumped by admitAssistCommand()
	// (called from submit()) the instant such a command is admitted -
	// supersede-replace or fresh push - not when it later applies. The
	// Coach controller captures this snapshot at arm() time and compares
	// it on every guard/rollback check; any change to the relevant field
	// means the user has touched that control since arming, durably and
	// without regard to recentResults ring eviction. See
	// DjAssistIntentGenerations (DjSessionState.h).
	DjAssistIntentGenerations assistIntentGenerationsSnapshot() override;
	// Removes every currently-queued SYSTEM-origin SET_PLAYING/SET_SYNC
	// for `deck` plus every queued SYSTEM-origin SET_MIX (mix is deck-
	// agnostic, always purged), finishing each through the normal
	// SUPERSEDED bookkeeping (commandResults + assistTracked). Called by
	// the Coach controller exactly once per failure/cancel episode, for
	// both plan decks, before computing rollback phases - so a
	// cancelled/failed transition can never be followed by a stale queued
	// Assist write landing after rollback has already restored safe
	// state.
	void assistPurgePendingSystemCommands(uint8_t deck) override;

	bool copySnapshot(DjSnapshot& snapshot) override;
	bool hasPendingLoad();
	bool libraryWorkAllowed();
	DjMetadataState refreshLibraryMetadata(uint32_t generation, uint64_t libraryKey);
	void invalidateLibraryMetadata();
	DjMetadataState lookupTrackMetadata(
		const char* path,
		DjTrackMetadataSnapshot& metadata,
		const DjTrackIdentity* identity = nullptr
	);
	bool copyCue(uint8_t deck, uint16_t index, JaydMetadata::Cue& cue);
	bool copyGrid(uint8_t deck, uint16_t index, JaydMetadata::Grid& grid);
	bool copyPhrase(uint8_t deck, uint16_t index, JaydMetadata::Phrase& phrase);
	void attachView(InfoGenerator* left, InfoGenerator* right, InfoGenerator* output);
	void detachView();
	void loop(uint micros) override;

private:
	DjSession(uint8_t leftGain, uint8_t rightGain, uint8_t mix);
	~DjSession() override;

	static DjSession* instance;
	static uint64_t bootId;
	static uint32_t sessionCounter;
	static bool orphanRecoveryDone;

	MixSystem* system = nullptr;
	fs::File files[DJ_DECK_COUNT];
	char paths[DJ_DECK_COUNT][DJ_PATH_CAPACITY] = {};
	uint8_t gains[DJ_DECK_COUNT] = { 255, 255 };
	uint8_t mix = 127;
	DjEffectState effectState;
	DjCueState cues;
	bool ending = false;
	bool viewAttached = false;
	InfoGenerator* viewInfo[3] = {};

	Mutex commandMutex;
	Mutex snapshotMutex;
	Mutex metadataMutex;
	DjCommandQueue commandQueue;
	DjCommandResults commandResults;
	DjSnapshotBuffers snapshots;
	uint32_t nextCommandId = 0;
	uint32_t queueDrops = 0;
	uint64_t snapshotSeq = 0;
	uint32_t sessionId = 0;
	JaydMetadata::Reader metadataReader;
	JaydMetadata::Status metadataReaderStatus = JaydMetadata::Status::Missing;
	// Externally-meaningful semantic library identity (e.g. from
	// LibraryIndex), used for deck-metadata association/snapshot/command
	// tagging elsewhere in this class. Still atomic/lock-free-readable for
	// those existing lock-free comparisons, but no longer exposed to
	// DjAssistController for candidate-table freshness - see
	// metadataRevision below and assistMetadataRevision()'s doc comment
	// for why the two must not be conflated.
	std::atomic<uint32_t> libraryGeneration { 0 };
	// Dedicated, purely-internal monotonic revision counter for candidate-
	// table freshness (see assistMetadataRevision()/assistTrackEntry()) -
	// deliberately separate from libraryGeneration above (external
	// semantic library identity). Bumped before EVERY reader-mutating
	// attempt: refreshLibraryMetadata()'s reload path, invalidateLibraryMetadata(),
	// and shutdown() - including when the external generation/key passed
	// to refreshLibraryMetadata() is unchanged but the underlying file
	// content isn't (metadataFileKey differs), and including a metadata
	// loss+reopen cycle that happens to land back on the same external
	// generation. Never assigned from the external generation/key values.
	std::atomic<uint32_t> metadataRevision { 0 };
	uint64_t libraryKey = 0;
	uint64_t metadataFileKey = 0;
	bool metadataInitialized = false;
	JaydMetadata::Track metadataTracks[DJ_DECK_COUNT] = {};
	DjDeckMetadataState deckMetadata[DJ_DECK_COUNT];

	DjBeatGrid grids[DJ_DECK_COUNT];
	DjLoopEngine loopEngines[DJ_DECK_COUNT];
	DjSyncController syncControllers[DJ_DECK_COUNT];
	DjQuantizeResolution quantizeResolution[DJ_DECK_COUNT] = { DJ_QUANTIZE_OFF, DJ_QUANTIZE_OFF };
	bool syncArmed[DJ_DECK_COUNT] = {};
	int8_t syncMasterDeck[DJ_DECK_COUNT] = { -1, -1 }; // -1 = auto (the other deck)
	DjCommandError syncLastError[DJ_DECK_COUNT] = {};

	DjRecordingSnapshot recordingSnapshot;
	DjRecordingState lastRecordingState = DJ_RECORDING_IDLE;
	// Set to the specific storage-layer error when finalizeRecording() fails
	// (naming space exhausted vs. rename I/O failure); DJ_RECORDING_ERROR_NONE
	// otherwise. Overrides a library-reported success once set.
	DjRecordingError finalizeError = DJ_RECORDING_ERROR_NONE;
#if defined(JAYD_ENABLE_WIRELESS)
	uint32_t pairingGeneration = 0;
#endif

	// Durable single-slot command-outcome tracker for DjAssist (see
	// assistTrackCommand()/assistTrackedStatus()) - deliberately separate
	// from commandResults' bounded/evictable ring. Struct definition lives
	// in DjSessionState.h (top-level, host-includable) so admitAssistCommand()
	// there can update it at admission time, not only when a command later
	// pops/finishes in loop().
	DjAssistTrackedCommand assistTracked;
	// Bumped by admitAssistCommand() (called from submit()) the instant a
	// non-system-origin mix/play/sync command is admitted - supersede-
	// replace or fresh push - not when it later applies (see
	// assistIntentGenerationsSnapshot()).
	DjAssistIntentGenerations assistIntentGenerations;

	DjAssistController assistController;
	void tickAssist();

	DjCommandError validate(const DjCommand& command) const;
	bool hasDeck(uint8_t deck) const;
	bool apply(const DjCommand& command, DjCommandError& error, DjCommandStatus& status, DjCommandResult& diagnostics);
	bool applyLoad(const DjCommand& command, DjCommandError& error);
	bool buildGrid(uint8_t deck);
	uint8_t resolveMasterDeck(uint8_t followerDeck) const;
	void tickLoops();
	void tickSync();
	void pollRecording();
	static DjRecordingState mapRecordingState(RecordingState state);
	static DjRecordingError mapRecordingError(RecordingError error);
	DjMetadataState resolveMetadata(
		const char* path,
		uint32_t generation,
		uint64_t key,
		const DjTrackIdentity* identity,
		JaydMetadata::Track& track,
		DjTrackMetadataSnapshot& metadata
	);
	static DjMetadataState metadataState(JaydMetadata::Status status);
	void publishSnapshot();
	void shutdown();
};

#endif
