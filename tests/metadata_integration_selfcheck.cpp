#include <assert.h>
#include "../src/DjSession/DjSessionState.h"
#include "../src/Screens/SongList/LibraryIndex.h"

static DjTrackMetadataSnapshot metadata(uint32_t generation, uint32_t bpm){
	DjTrackMetadataSnapshot value = {};
	value.state = DJ_METADATA_VALID;
	value.libraryGeneration = generation;
	value.capabilities = DJ_METADATA_HAS_SOURCE_FRAMES |
		DJ_METADATA_HAS_BPM |
		DJ_METADATA_HAS_CUES;
	value.sourceSampleRate = 44100;
	value.sourceDurationFrames = 441000;
	value.bpmMilli = bpm;
	value.cueCount = 2;
	return value;
}

int main(){
	DjDeckMetadataState deck;
	const DjTrackMetadataSnapshot first = metadata(7, 128000);
	assert(deck.commitIfLoaded(first, true, true));
	assert(deck.attached());
	assert(deck.snapshot().bpmMilli == 128000);

	const DjTrackMetadataSnapshot replacement = metadata(7, 130000);
	assert(!deck.commitIfLoaded(replacement, true, false));
	assert(deck.attached());
	assert(deck.snapshot().bpmMilli == 128000);

	assert(deck.commitIfLoaded(replacement, true, true));
	assert(deck.attached());
	assert(deck.snapshot().bpmMilli == 130000);

	deck.invalidate(DJ_METADATA_STALE, 8);
	assert(!deck.attached());
	assert(deck.snapshot().state == DJ_METADATA_STALE);
	assert(deck.snapshot().libraryGeneration == 8);

	DjSnapshotBuffers snapshots;
	DjSnapshot source = {};
	source.sessionActive = true;
	source.decks[0].metadata = first;
	snapshots.publish(source);
	DjSnapshot copy = {};
	snapshots.copy(copy);
	source.decks[0].metadata = replacement;
	snapshots.publish(source);
	assert(copy.decks[0].metadata.bpmMilli == 128000);
	assert(copy.decks[0].metadata.sourceDurationFrames == 441000);

	DjSnapshot playback = {};
	assert(djAllowsLibraryWork(playback));
	playback.decks[0].playing = true;
	assert(!djAllowsLibraryWork(playback));
	playback.decks[0].playing = false;
	playback.decks[1].playing = true;
	assert(!djAllowsLibraryWork(playback));
	playback.decks[1].playing = false;
	// Recording status moved from a flat DjSnapshot::recording bool to the
	// richer DjRecordingSnapshot (see DjSessionState.h); djAllowsLibraryWork()
	// now gates on the same STARTING/ACTIVE/STOPPING busy span that
	// djRecordingStartBusy() uses for new start commands.
	playback.recordingInfo.state = DJ_RECORDING_ACTIVE;
	assert(!djAllowsLibraryWork(playback));

	LibraryIndex::VerifiedIdentity identity = {};
	const LibraryIndex::CardIdentity card{
		LibraryIndex::IdentityStrength::Weak,
		3,
		32ULL * 1024 * 1024 * 1024,
		31914983424ULL
	};
	assert(LibraryIndex::updateVerifiedIdentity(
		identity,
		card,
		true,
		7,
		0x11111111
	) == LibraryIndex::IdentityAction::Refresh);
	DjDeckMetadataState playingDeck;
	assert(playingDeck.commitIfLoaded(first, true, true));

	assert(LibraryIndex::updateVerifiedIdentity(
		identity,
		card,
		false,
		0,
		0
	) == LibraryIndex::IdentityAction::Preserve);
	assert(identity.generation == 7);
	assert(identity.payloadCrc == 0x11111111);
	assert(playingDeck.attached());

	const LibraryIndex::CardIdentity replacementCard{
		LibraryIndex::IdentityStrength::Weak,
		4,
		card.cardSize,
		card.volumeSize
	};
	LibraryIndex::VerifiedIdentity changedCardIdentity = identity;
	assert(LibraryIndex::updateVerifiedIdentity(
		changedCardIdentity,
		replacementCard,
		false,
		0,
		0
	) == LibraryIndex::IdentityAction::Invalidate);
	assert(!changedCardIdentity.valid);

	assert(LibraryIndex::updateVerifiedIdentity(
		identity,
		card,
		true,
		8,
		0x22222222
	) == LibraryIndex::IdentityAction::Refresh);
	playingDeck.invalidate(DJ_METADATA_STALE, identity.generation);
	assert(!playingDeck.attached());
	assert(playingDeck.snapshot().libraryGeneration == 8);
	return 0;
}
