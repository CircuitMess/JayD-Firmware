#ifndef JAYD_FIRMWARE_MIXCONTROLSTATE_H
#define JAYD_FIRMWARE_MIXCONTROLSTATE_H

#include <stdint.h>

enum MixControlBank : uint8_t {
	MIX_BANK_MIX,
	MIX_BANK_CUES,
	MIX_BANK_BROWSE,
	MIX_BANK_LOOPSYNC,
	MIX_BANK_ASSIST,
	MIX_BANK_COUNT
};

enum MixPaletteItem : uint8_t {
	MIX_PALETTE_MIX,
	MIX_PALETTE_CUES,
	MIX_PALETTE_BROWSE,
	MIX_PALETTE_LOOPSYNC,
	MIX_PALETTE_ASSIST,
	MIX_PALETTE_MATRIX,
	MIX_PALETTE_RESCAN,
	MIX_PALETTE_SETTINGS,
	MIX_PALETTE_EXIT,
	MIX_PALETTE_COUNT
};

struct MixControlState {
	MixControlBank bank = MIX_BANK_MIX;
	uint8_t cuePage = 0;
	bool paletteOpen = false;
	uint8_t paletteSelection = MIX_PALETTE_MIX;
	// Browsing cursor into the current Coach suggestion list (ASSIST bank
	// only, informational - the physical "confirm" action always targets
	// whatever is already loaded on the other deck, not this cursor).
	uint8_t assistSuggestion = 0;

	void openPalette(){
		paletteOpen = true;
		paletteSelection = static_cast<uint8_t>(bank);
	}

	void movePalette(int8_t amount){
		int16_t selection = paletteSelection + amount;
		while(selection < 0) selection += MIX_PALETTE_COUNT;
		paletteSelection = selection % MIX_PALETTE_COUNT;
	}

	MixPaletteItem confirmPalette(){
		const MixPaletteItem selected = static_cast<MixPaletteItem>(paletteSelection);
		if(selected <= MIX_PALETTE_ASSIST) bank = static_cast<MixControlBank>(selected);
		paletteOpen = false;
		return selected;
	}

	void moveCuePage(int8_t amount){
		int8_t page = cuePage + amount;
		if(page < 0) page = 0;
		if(page > 2) page = 2;
		cuePage = page;
	}

	void moveAssistSuggestion(int8_t amount, uint8_t suggestionCount){
		if(suggestionCount == 0){
			assistSuggestion = 0;
			return;
		}
		int16_t selection = assistSuggestion + amount;
		while(selection < 0) selection += suggestionCount;
		assistSuggestion = selection % suggestionCount;
	}

	int8_t cueForEncoder(uint8_t encoder) const{
		if(encoder >= 6) return -1;
		const uint8_t cue = cuePage * 3 + encoder % 3;
		return cue < 8 ? cue : -1;
	}

	static int8_t browseDeckForButton(uint8_t button){
		return button < 2 ? button : -1;
	}

	static bool encoderChordsEnabled(){
		return false;
	}

	static bool suppressDeckReleaseAfterChord(){
		return true;
	}
};

#endif
