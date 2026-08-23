#include "DjAssistScoring.h"

#include <string.h>

namespace DjAssistScoring {

namespace {

bool betterThan(const DjAssistSuggestion& a, const DjAssistSuggestion& b){
	if(a.score != b.score) return a.score > b.score;
	return a.libraryIndex < b.libraryIndex;
}

} // namespace

DjAssistKeyRelationship classifyKeyRelationship(uint16_t deckKey, uint16_t candidateKey){
	if(deckKey == 0 || candidateKey == 0) return DJ_ASSIST_KEY_UNKNOWN;

	const uint8_t deckNumber = deckKey & 0xff;
	const uint8_t candidateNumber = candidateKey & 0xff;
	if(deckNumber < 1 || deckNumber > 12 || candidateNumber < 1 || candidateNumber > 12){
		return DJ_ASSIST_KEY_UNKNOWN;
	}

	const bool deckMinor = (deckKey & 0x100) != 0;
	const bool candidateMinor = (candidateKey & 0x100) != 0;

	if(deckNumber == candidateNumber){
		return deckMinor == candidateMinor ? DJ_ASSIST_KEY_SAME : DJ_ASSIST_KEY_RELATIVE;
	}

	if(deckMinor == candidateMinor){
		const uint8_t diff = static_cast<uint8_t>((deckNumber + 12 - candidateNumber) % 12);
		if(diff == 1 || diff == 11) return DJ_ASSIST_KEY_ADJACENT;
	}

	return DJ_ASSIST_KEY_INCOMPATIBLE;
}

bool requiredRateMilli(uint32_t deckBpmMilli, uint32_t candidateBpmMilli, uint32_t& outRateMilli){
	outRateMilli = DJ_ASSIST_RATE_UNITY_MILLI;
	if(deckBpmMilli == 0 || candidateBpmMilli == 0) return false;

	const uint64_t rate = (static_cast<uint64_t>(deckBpmMilli) * 1000ULL) / candidateBpmMilli;
	if(rate == 0 || rate > 0xffffffffULL) return false;

	outRateMilli = static_cast<uint32_t>(rate);
	return true;
}

bool identityMatches(const DjTrackIdentity& a, const DjTrackIdentity& b){
	if((a.flags & DJ_TRACK_IDENTITY_FINGERPRINT) && (b.flags & DJ_TRACK_IDENTITY_FINGERPRINT)){
		return memcmp(a.fingerprint, b.fingerprint, sizeof(a.fingerprint)) == 0;
	}
	if((a.flags & DJ_TRACK_IDENTITY_SOURCE) && (b.flags & DJ_TRACK_IDENTITY_SOURCE)){
		return memcmp(a.sourceId, b.sourceId, sizeof(a.sourceId)) == 0;
	}
	return false;
}

DjAssistSuggestion scoreEntry(
	const DjAssistLibraryEntry& entry,
	const DjAssistDeckContext& deck,
	bool isLoaded,
	bool isRecent
){
	DjAssistSuggestion suggestion;
	suggestion.libraryIndex = entry.libraryIndex;
	suggestion.identity = entry.identity;
	suggestion.rating = entry.rating;

	if(isLoaded){
		suggestion.excludeReason = DJ_ASSIST_EXCLUDE_LOADED;
		return suggestion;
	}
	if(isRecent){
		suggestion.excludeReason = DJ_ASSIST_EXCLUDE_RECENT;
		return suggestion;
	}
	if(entry.state == DJ_METADATA_CORRUPT || entry.state == DJ_METADATA_UNSUPPORTED){
		suggestion.excludeReason = DJ_ASSIST_EXCLUDE_UNSUPPORTED_METADATA;
		return suggestion;
	}

	int32_t confidence = 1000;
	uint16_t score = 0;

	const bool hasBpm = (entry.capabilities & DJ_METADATA_HAS_BPM) != 0 &&
		entry.bpmMilli > 0 && deck.bpmMilli > 0;
	if(hasBpm){
		uint32_t rateMilli = DJ_ASSIST_RATE_UNITY_MILLI;
		const bool rateOk = requiredRateMilli(deck.bpmMilli, entry.bpmMilli, rateMilli);
		suggestion.requiredRateMilli = rateMilli;
		suggestion.tempoDeltaMilli = static_cast<int32_t>(entry.bpmMilli) - static_cast<int32_t>(deck.bpmMilli);

		if(rateOk && rateMilli >= DJ_ASSIST_RATE_MIN_MILLI && rateMilli <= DJ_ASSIST_RATE_MAX_MILLI){
			if(rateMilli >= DJ_ASSIST_RATE_NARROW_MIN_MILLI && rateMilli <= DJ_ASSIST_RATE_NARROW_MAX_MILLI){
				suggestion.reasonFlags |= DJ_ASSIST_REASON_TEMPO_NARROW;
				score += 400;
			} else {
				suggestion.reasonFlags |= DJ_ASSIST_REASON_TEMPO_IN_RANGE;
				const uint32_t distance = rateMilli < DJ_ASSIST_RATE_NARROW_MIN_MILLI
					? (DJ_ASSIST_RATE_NARROW_MIN_MILLI - rateMilli)
					: (rateMilli - DJ_ASSIST_RATE_NARROW_MAX_MILLI);
				const uint32_t span = DJ_ASSIST_RATE_NARROW_MIN_MILLI - DJ_ASSIST_RATE_MIN_MILLI;
				const uint32_t clamped = distance > span ? span : distance;
				score += static_cast<uint16_t>(400 - (200 * clamped) / span);
			}
		} else {
			suggestion.reasonFlags |= DJ_ASSIST_REASON_TEMPO_OUT_OF_RANGE;
		}
	} else {
		confidence -= 300;
	}

	const bool hasKey = (entry.capabilities & DJ_METADATA_HAS_KEY) != 0 &&
		entry.key != 0 && deck.key != 0;
	if(hasKey){
		suggestion.keyRelationship = classifyKeyRelationship(deck.key, entry.key);
		switch(suggestion.keyRelationship){
			case DJ_ASSIST_KEY_SAME:
				suggestion.reasonFlags |= DJ_ASSIST_REASON_KEY_SAME;
				score += 300;
				break;
			case DJ_ASSIST_KEY_ADJACENT:
				suggestion.reasonFlags |= DJ_ASSIST_REASON_KEY_ADJACENT;
				score += 220;
				break;
			case DJ_ASSIST_KEY_RELATIVE:
				suggestion.reasonFlags |= DJ_ASSIST_REASON_KEY_RELATIVE;
				score += 200;
				break;
			case DJ_ASSIST_KEY_INCOMPATIBLE:
			case DJ_ASSIST_KEY_UNKNOWN:
			default:
				break;
		}
	} else {
		suggestion.keyRelationship = DJ_ASSIST_KEY_UNKNOWN;
		suggestion.reasonFlags |= DJ_ASSIST_REASON_KEY_UNKNOWN;
		confidence -= 200;
	}

	const bool hasRating = entry.rating <= 5;
	if(hasRating){
		if(entry.rating > 0) suggestion.reasonFlags |= DJ_ASSIST_REASON_RATING_KNOWN;
		score += static_cast<uint16_t>(entry.rating) * 20;
	} else {
		confidence -= 100;
	}

	if(entry.capabilities & DJ_METADATA_HAS_GRID){
		suggestion.reasonFlags |= DJ_ASSIST_REASON_GRID_AVAILABLE;
		score += 50;
	} else {
		confidence -= 100;
	}

	if(entry.capabilities & DJ_METADATA_HAS_PHRASES){
		suggestion.reasonFlags |= DJ_ASSIST_REASON_PHRASE_AVAILABLE;
		score += 50;
	} else {
		confidence -= 100;
	}

	const bool hasDuration = (entry.capabilities & DJ_METADATA_HAS_SOURCE_FRAMES) != 0 &&
		entry.durationFrames > 0 && entry.sampleRate > 0;
	if(hasDuration){
		static const uint64_t minSeconds = 60;
		const uint64_t seconds = entry.durationFrames / entry.sampleRate;
		if(seconds < minSeconds){
			suggestion.reasonFlags |= DJ_ASSIST_REASON_DURATION_SHORT;
		} else {
			score += 100;
		}
	} else {
		confidence -= 100;
	}

	if(confidence < 0) confidence = 0;
	if(confidence > 1000) confidence = 1000;
	suggestion.confidence = static_cast<uint16_t>(confidence);
	if(confidence < 400) suggestion.reasonFlags |= DJ_ASSIST_REASON_LOW_CONFIDENCE;

	suggestion.score = score > 1000 ? 1000 : score;
	return suggestion;
}

void mergeSuggestion(
	DjAssistSuggestion* suggestions,
	uint8_t& count,
	uint8_t capacity,
	const DjAssistSuggestion& candidate
){
	if(capacity == 0) return;

	// Re-scoring an already-ranked track (e.g. overlapping scan chunks, or a
	// track that has since become loaded/recent/unsupported) must replace,
	// not duplicate or leave stale, its entry. Do this before the exclusion
	// check below so a track that WAS ranked but is now excluded gets
	// removed rather than left stale in the list.
	for(uint8_t i = 0; i < count; i++){
		if(suggestions[i].libraryIndex != candidate.libraryIndex) continue;
		for(uint8_t j = i; static_cast<uint8_t>(j + 1) < count; j++){
			suggestions[j] = suggestions[j + 1];
		}
		count--;
		break;
	}

	if(candidate.excludeReason != DJ_ASSIST_EXCLUDE_NONE) return;

	uint8_t insertAt = count;
	for(uint8_t i = 0; i < count; i++){
		if(betterThan(candidate, suggestions[i])){
			insertAt = i;
			break;
		}
	}
	if(insertAt >= capacity) return;

	const uint8_t last = count < capacity ? count : static_cast<uint8_t>(capacity - 1);
	for(uint8_t i = last; i > insertAt; i--){
		suggestions[i] = suggestions[i - 1];
	}
	suggestions[insertAt] = candidate;
	if(count < capacity) count++;
}

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
){
	if(total == 0) return 0;

	uint16_t index = cursor % total;
	uint16_t processed = 0;
	const uint16_t limit = budget < total ? budget : total;

	for(; processed < limit; processed++){
		const DjAssistLibraryEntry& entry = entries[index];

		bool isLoaded = false;
		for(uint8_t i = 0; i < loadedCount; i++){
			if(identityMatches(entry.identity, loaded[i])){
				isLoaded = true;
				break;
			}
		}

		bool isRecent = false;
		if(!isLoaded){
			for(uint8_t i = 0; i < recentCount; i++){
				if(identityMatches(entry.identity, recent[i])){
					isRecent = true;
					break;
				}
			}
		}

		mergeSuggestion(suggestions, count, capacity, scoreEntry(entry, deck, isLoaded, isRecent));
		index = static_cast<uint16_t>((index + 1) % total);
	}

	cursor = index;
	return processed;
}

uint8_t crossfadeCurve(uint16_t stepIndex, uint16_t totalSteps){
	if(totalSteps == 0 || stepIndex >= totalSteps) return 255;
	const uint32_t value = (static_cast<uint32_t>(stepIndex) * 255U) / totalSteps;
	return static_cast<uint8_t>(value);
}

} // namespace DjAssistScoring
