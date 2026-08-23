#ifndef JAYD_FIRMWARE_DJASSISTSCORING_H
#define JAYD_FIRMWARE_DJASSISTSCORING_H

#include "DjAssistTypes.h"

// Pure, bounded, deterministic scoring/ranking helpers. No IO, no heap use -
// every function operates only on already-resolved, caller-owned memory.
namespace DjAssistScoring {

// Camelot-equivalent relationship between two encoded keys (0 = unknown).
DjAssistKeyRelationship classifyKeyRelationship(uint16_t deckKey, uint16_t candidateKey);

// Fixed-point (1000 == 1.0x) rate required to play candidateBpmMilli at
// deckBpmMilli. Returns false (and leaves outRateMilli at unity) when either
// BPM is unknown/zero or the ratio would not fit a checked uint32_t.
bool requiredRateMilli(uint32_t deckBpmMilli, uint32_t candidateBpmMilli, uint32_t& outRateMilli);

// True when both identities carry the same kind of evidence (fingerprint or
// source id) and that evidence matches. Identities with no comparable
// evidence never match - exclusion is never claimed without proof.
bool identityMatches(const DjTrackIdentity& a, const DjTrackIdentity& b);

// Scores one library entry against a deck context. Missing capabilities
// reduce confidence rather than reject the track; only loaded/recent/
// corrupt-or-unsupported entries are excluded (see DjAssistExcludeReason).
DjAssistSuggestion scoreEntry(
	const DjAssistLibraryEntry& entry,
	const DjAssistDeckContext& deck,
	bool isLoaded,
	bool isRecent
);

// Inserts `candidate` into the sorted (descending score, ascending
// libraryIndex tie-break) `suggestions[0..count)` array of `capacity`,
// growing `count` up to `capacity`. Excluded candidates are dropped. Bounded
// O(capacity) work, no allocation.
void mergeSuggestion(
	DjAssistSuggestion* suggestions,
	uint8_t& count,
	uint8_t capacity,
	const DjAssistSuggestion& candidate
);

// Processes up to `budget` entries of `entries[0..total)` starting at
// `cursor` (wrapping), scoring and merging each into `suggestions`, then
// advances `cursor`. Bounded incremental work suitable for calling outside
// the audio path across many ticks; running it in one large budget or many
// small ones over the same full pass yields an identical final top-N.
// Returns the number of entries actually processed this call.
uint16_t scanTick(
	const DjAssistLibraryEntry* entries,
	uint16_t total,
	uint16_t& cursor,
	uint16_t budget,
	const DjAssistDeckContext& deck,
	const DjTrackIdentity* loaded,
	uint8_t loadedCount,
	const DjTrackIdentity* recent,
	uint8_t recentCount,
	DjAssistSuggestion* suggestions,
	uint8_t& count,
	uint8_t capacity
);

// Linear crossfade curve sample in [0,255]. stepIndex >= totalSteps (or
// totalSteps == 0) saturates at 255; checked against divide-by-zero.
uint8_t crossfadeCurve(uint16_t stepIndex, uint16_t totalSteps);

} // namespace DjAssistScoring

#endif
