#include <Input/InputJayD.h>
#include <SD.h>
#include <Loop/LoopManager.h>
#include <JayD.h>
#include <FS/CompressedFile.h>
#include "MixScreen.h"
#include "../SongList/SongList.h"
#include "../Settings/SettingsScreen.h"
#include "../../Fonts.h"
#include "../../DjAssist/DjAssistScoring.h"

MixScreen::MixScreen* MixScreen::MixScreen::instance = nullptr;

MixScreen::MixScreen::MixScreen(Display& display) : Context(display),
													screenLayout(new LinearLayout(&screen, HORIZONTAL)),
													leftLayout(new LinearLayout(screenLayout, VERTICAL)),
													rightLayout(new LinearLayout(screenLayout, VERTICAL)),
													leftSeekBar(new SongSeekBar(leftLayout)),
													rightSeekBar(new SongSeekBar(rightLayout)),
													leftSongName(new SongName(leftLayout)),
													rightSongName(new SongName(rightLayout)), leftVu(&matrixManager.matrixL), rightVu(&matrixManager.matrixR),
													midVu(&matrixManager.matrixBig){

	for(int i = 0; i < 3; i++){
		effectElements[i] = new EffectElement(leftLayout, false);
	}
	for(int i = 3; i < 6; i++){
		effectElements[i] = new EffectElement(rightLayout, true);
	}

	session = DjSession::begin(
			InputJayD::getInstance()->getPotValue(POT_L),
			InputJayD::getInstance()->getPotValue(POT_R),
			InputJayD::getInstance()->getPotValue(POT_MID));
	instance = this;
	buildUI();
	MixScreen::pack();
}

MixScreen::MixScreen::~MixScreen(){
	instance = nullptr;
	if(session && session == DjSession::get()){
		DjSession::end();
		session = nullptr;
	}
	free(selectedBackgroundBuffer);
}

void MixScreen::MixScreen::pack(){
	Context::pack();
	free(selectedBackgroundBuffer);
	selectedBackgroundBuffer = nullptr;
}

void MixScreen::MixScreen::unpack(){
	Context::unpack();

	selectedBackgroundBuffer = static_cast<Color*>(ps_malloc(79 * 128 * 2));
	if(selectedBackgroundBuffer == nullptr){
		Serial.println("Selected background malloc error");
		return;
	}

	fs::File bgFile = CompressedFile::open(SPIFFS.open("/mixSelectedBg.raw.hs"), 13, 12);
	bgFile.read(reinterpret_cast<uint8_t*>(selectedBackgroundBuffer), 79 * 128 * 2);
	bgFile.close();
}

void MixScreen::MixScreen::returned(void* data){
	songListOpen = false;
	if(data == nullptr) return;
	String* filename = (String*) data;
	songListOpen = false;

	session->setGain(0, InputJayD::getInstance()->getPotValue(POT_L), DJ_ORIGIN_LOCAL_UI);
	session->setGain(1, InputJayD::getInstance()->getPotValue(POT_R), DJ_ORIGIN_LOCAL_UI);
	if(!loadChannel(loadingChannel, *filename)){
		Serial.println("MixScreen: load command rejected");
	}
	delete filename;
}


bool MixScreen::MixScreen::loadChannel(uint8_t channel, const String& path){
	if(!session) return false;
	return session->loadDeck(channel, path.c_str(), DJ_ORIGIN_LOCAL_UI).accepted();
}

void MixScreen::MixScreen::setBigVuStarted(bool bigVuStarted){
	MixScreen::bigVuStarted = bigVuStarted;
}

bool MixScreen::MixScreen::syncFromSnapshot(const DjSnapshot& snapshot, bool force){
	bool changed = force;
	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; deck++){
		const DjDeckSnapshot& deckSnapshot = snapshot.decks[deck];
		SongSeekBar* bar = deck == 0 ? leftSeekBar : rightSeekBar;
		SongName* nameLabel = deck == 0 ? leftSongName : rightSongName;

		if(force || strcmp(displayedPaths[deck], deckSnapshot.path) != 0){
			memcpy(displayedPaths[deck], deckSnapshot.path, DJ_PATH_CAPACITY);
			String name = deckSnapshot.path;
			const int slash = name.lastIndexOf('/');
			const int end = name.endsWith(".aac") ? name.length() - 4 : name.length();
			nameLabel->setSongName(name.substring(slash + 1, end));
			nameLabel->checkScrollUpdate();
			changed = true;
		}

		if(bar->getTotalDuration() != deckSnapshot.duration){
			bar->setTotalDuration(deckSnapshot.duration);
			changed = true;
		}
		if(seekTime == 0 || seekChannel != deck){
			if(bar->getCurrentDuration() != deckSnapshot.elapsed){
				bar->setCurrentDuration(deckSnapshot.elapsed);
				changed = true;
			}
			if(bar->isPlaying() != deckSnapshot.playing){
				bar->setPlaying(deckSnapshot.playing);
				changed = true;
			}
		}

		for(uint8_t slot = 0; slot < DJ_EFFECT_SLOT_COUNT; slot++){
			EffectElement* element = effectElements[deck * DJ_EFFECT_SLOT_COUNT + slot];
			const EffectType type = static_cast<EffectType>(deckSnapshot.effects[slot].type);
			if(force || element->getType() != type){
				element->setType(type);
				changed = true;
			}
			if(element->getIntensity() != deckSnapshot.effects[slot].intensity){
				element->setIntensity(deckSnapshot.effects[slot].intensity);
				changed = true;
			}
		}
	}
	if(recordingState != snapshot.recordingInfo.state){
		recordingState = snapshot.recordingInfo.state;
		changed = true;
		switch(recordingState){
			case DJ_RECORDING_STARTING:
				statusText = "STARTING RECORDING...";
				statusUntil = millis() + 2500;
				break;
			case DJ_RECORDING_ACTIVE:
				statusText = "RECORDING";
				statusUntil = millis() + 1500;
				break;
			case DJ_RECORDING_STOPPING:
				statusText = "FINALIZING RECORDING...";
				statusUntil = millis() + 2500;
				break;
			case DJ_RECORDING_COMPLETE:
				statusText = snapshot.recordingInfo.valid ? "RECORDING SAVED" : "RECORDING INVALID";
				statusUntil = millis() + 3000;
				break;
			case DJ_RECORDING_FAILED:
				statusText = recordingErrorText(snapshot.recordingInfo.error);
				statusUntil = millis() + 3000;
				break;
			default:
				break;
		}
	}
	return changed;
}

void MixScreen::MixScreen::processCommandResults(const DjSnapshot& snapshot){
	for(const auto& result : snapshot.recentResults){
		if(result.id == 0 || result.status == DJ_COMMAND_ACCEPTED || resultHandled(result.id)) continue;
		markResultHandled(result.id);
		if(result.status == DJ_COMMAND_FAILED || result.status == DJ_COMMAND_REJECTED){
			showCommandError(result.error);
		}
	}
}

bool MixScreen::MixScreen::resultHandled(uint32_t id) const{
	for(const uint32_t handled : handledResults){
		if(handled == id) return true;
	}
	return false;
}

void MixScreen::MixScreen::markResultHandled(uint32_t id){
	handledResults[handledResultNext] = id;
	handledResultNext = (handledResultNext + 1) % DJ_RECENT_RESULT_COUNT;
}

void MixScreen::MixScreen::openBrowse(){
	if(songListOpen || !session) return;
	songListOpen = true;
	browseWasOpened = true;
	(new SongList::SongList(*getScreen().getDisplay(), session))->push(this);
}

void MixScreen::MixScreen::start(){
	songListOpen = false;

	if(!session) return;
	DjSnapshot snapshot;
	session->copySnapshot(snapshot);
	for(const auto& result : snapshot.recentResults){
		if(result.id && result.status != DJ_COMMAND_ACCEPTED) markResultHandled(result.id);
	}

	const bool hasTarget = snapshot.decks[0].path[0] != '\0' || snapshot.decks[1].path[0] != '\0';
	if(!hasTarget && !browseWasOpened && !session->hasPendingLoad()){
		loadingChannel = 0;
		controls.bank = MIX_BANK_BROWSE;
		openBrowse();
		return;
	}

	session->attachView(leftVu.getInfoGenerator(), rightVu.getInfoGenerator(), midVu.getInfoGenerator());
	syncFromSnapshot(snapshot, true);
	if(bigVuStarted){
		startBigVu();
	}

	matrixManager.fillMatrixMid(snapshot.mix);
	matrixManager.matrixMid.push();

	LoopManager::addListener(&leftVu);
	LoopManager::addListener(&rightVu);
	LoopManager::addListener(this);

	Input.addListener(this);
	InputJayD::getInstance()->addListener(this);

	draw();
	screen.commit();
}

void MixScreen::MixScreen::stop(){
	LoopManager::removeListener(&leftVu);
	LoopManager::removeListener(&rightVu);
	LoopManager::removeListener(&midVu);
	LoopManager::removeListener(this);

	Input.removeListener(this);
	InputJayD::getInstance()->removeListener(this);
	if(session) session->detachView();

	if(bigVuStarted){
		stopBigVu();
	}else{
		if(!matrixManager.matrixBig.getAnimations().empty()){
			delete *matrixManager.matrixBig.getAnimations().begin();
		}
	}

}

void MixScreen::MixScreen::draw(){
	screen.getSprite()->fillRect(79, 0, 2, 128, TFT_BLACK);
	screen.getSprite()->fillRect(leftLayout->getTotalX(), leftLayout->getTotalY(), 79, 128, C_RGB(249, 93, 2));
	screen.getSprite()->fillRect(rightLayout->getTotalX(), rightLayout->getTotalY(), 79, 128, C_RGB(3, 52, 135));
	if(!selectedChannel){
		screen.getSprite()->drawIcon(selectedBackgroundBuffer, screen.getTotalX(), screen.getTotalY(), 79, 128, 1, TFT_TRANSPARENT);
	}else{
		screen.getSprite()->drawIcon(selectedBackgroundBuffer, screen.getTotalX() + 81, screen.getTotalY(), 79, 128, 1, TFT_TRANSPARENT);
	}

	if(recordingState == DJ_RECORDING_ACTIVE){
		screen.getSprite()->fillCircle(79, 64, 6, TFT_BLACK);
		screen.getSprite()->fillCircle(79, 64, 4, TFT_RED);
	}else if(recordingState == DJ_RECORDING_STARTING || recordingState == DJ_RECORDING_STOPPING){
		screen.getSprite()->fillCircle(79, 64, 6, TFT_BLACK);
		screen.getSprite()->fillCircle(79, 64, 4, TFT_ORANGE);
	}
	screen.draw();

	if(controls.bank == MIX_BANK_MIX){
		drawMixLabels();
	}else if(controls.bank == MIX_BANK_CUES){
		drawCueBank();
	}else if(controls.bank == MIX_BANK_LOOPSYNC){
		drawLoopSyncBank();
	}else if(controls.bank == MIX_BANK_ASSIST){
		drawAssistBank();
	}else{
		drawBrowseBank();
	}
	if(controls.paletteOpen) drawPalette();
	drawStatus();
}

void MixScreen::MixScreen::drawMixLabels(){
	Sprite* canvas = screen.getSprite();
	canvas->setTextFont(1);
	canvas->setTextSize(1);
	canvas->setTextDatum(TC_DATUM);
	canvas->fillRect(0, 0, 160, 13, TFT_BLACK);
	canvas->setTextColor(TFT_WHITE);
	canvas->drawString(selectedChannel == 0 ? "MIX | SELECTED DECK A" : "MIX | SELECTED DECK B", 80, 2);

	static const char* names[] = { "OFF", "SPD", "LPF", "HPF", "RVB", "BIT" };
	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; deck++){
		for(uint8_t slot = 0; slot < DJ_EFFECT_SLOT_COUNT; slot++){
			const uint8_t index = deck * DJ_EFFECT_SLOT_COUNT + slot;
			EffectElement* effect = effectElements[index];
			char label[16];
			snprintf(label, sizeof(label), "%u %s %s %u", slot + 1, names[effect->getType()],
					 effect->isSelected() ? "TYPE" : "AMT", effect->getIntensity());
			const int16_t x = deck == 0 ? 2 : 83;
			const int16_t y = 47 + slot * 25;
			canvas->fillRect(x, y, 75, 10, TFT_BLACK);
			canvas->setTextColor(TFT_WHITE);
			canvas->setTextDatum(TL_DATUM);
			canvas->drawString(label, x + 2, y + 1);
		}
	}
	canvas->setTextDatum(TL_DATUM);
}

void MixScreen::MixScreen::drawCueBank(){
	DjSnapshot snapshot;
	if(!session || !session->copySnapshot(snapshot)) return;
	Sprite* canvas = screen.getSprite();
	canvas->fillRect(0, 0, 160, 128, TFT_BLACK);
	canvas->setTextFont(1);
	canvas->setTextSize(1);
	canvas->setTextDatum(TC_DATUM);
	canvas->setTextColor(TFT_WHITE);
	char header[30];
	const uint8_t cueEnd = controls.cuePage == 2 ? 8 : controls.cuePage * 3 + 3;
	snprintf(header, sizeof(header), "CUES %u-%u | SELECTED %c", controls.cuePage * 3 + 1,
			 cueEnd, selectedChannel ? 'B' : 'A');
	canvas->drawString(header, 80, 2);

	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; deck++){
		const int16_t x = deck == 0 ? 3 : 82;
		canvas->setTextDatum(TC_DATUM);
		canvas->drawString(deck == 0 ? "DECK A" : "DECK B", x + 37, 13);
		for(uint8_t pad = 0; pad < 3; pad++){
			const int8_t cue = controls.cueForEncoder(deck * 3 + pad);
			if(cue < 0) continue;
			const DjCueSnapshot& cueState = snapshot.decks[deck].cues[cue];
			const int16_t y = 24 + pad * 30;
			canvas->drawRoundRect(x, y, 75, 26, 2, TFT_WHITE);
			char line[20];
			if(cueState.occupied){
				snprintf(line, sizeof(line), "C%u %u:%02u GO", cue + 1,
						 cueState.position / 60, cueState.position % 60);
			}else{
				snprintf(line, sizeof(line), "C%u EMPTY SET", cue + 1);
			}
			canvas->setTextDatum(MC_DATUM);
			canvas->drawString(line, x + 37, y + 13);
		}
	}
	canvas->setTextDatum(BC_DATUM);
	canvas->drawString("CENTER: PAGE/SELECT  HOLD: MENU", 80, 126);
	canvas->setTextDatum(TL_DATUM);
}

void MixScreen::MixScreen::drawBrowseBank(){
	Sprite* canvas = screen.getSprite();
	canvas->fillRect(0, 0, 160, 128, TFT_BLACK);
	canvas->setTextFont(1);
	canvas->setTextSize(1);
	canvas->setTextColor(TFT_WHITE);
	canvas->setTextDatum(MC_DATUM);
	canvas->drawString("BROWSE", 80, 20);
	canvas->drawString("PRESS CENTER TO OPEN", 80, 48);
	canvas->drawString("A BUTTON: LOAD DECK A", 80, 68);
	canvas->drawString("B BUTTON: LOAD DECK B", 80, 82);
	canvas->drawString("AUDIO KEEPS PLAYING", 80, 102);
	canvas->setTextDatum(BC_DATUM);
	canvas->drawString("HOLD CENTER: MENU", 80, 126);
	canvas->setTextDatum(TL_DATUM);
}

void MixScreen::MixScreen::drawLoopSyncBank(){
	DjSnapshot snapshot;
	if(!session || !session->copySnapshot(snapshot)) return;
	Sprite* canvas = screen.getSprite();
	canvas->fillRect(0, 0, 160, 128, TFT_BLACK);
	canvas->setTextFont(1);
	canvas->setTextSize(1);
	canvas->setTextColor(TFT_WHITE);
	canvas->setTextDatum(TC_DATUM);
	canvas->drawString("LOOP / SYNC", 80, 2);

	for(uint8_t deck = 0; deck < DJ_DECK_COUNT; deck++){
		const int16_t x = deck == 0 ? 3 : 82;
		canvas->setTextDatum(TC_DATUM);
		canvas->drawString(deck == 0 ? "DECK A" : "DECK B", x + 37, 15);

		const DjLoopSnapshot& loop = snapshot.decks[deck].loop;
		const DjSyncSnapshot& sync = snapshot.decks[deck].sync;

		char loopLine[24];
		if(loop.state == DJ_LOOP_INACTIVE){
			snprintf(loopLine, sizeof(loopLine), "LOOP: OFF");
		}else if(loop.length == DJ_LOOP_BEAT_4){
			snprintf(loopLine, sizeof(loopLine), "LOOP: 4 BEAT");
		}else if(loop.length == DJ_LOOP_BEAT_1){
			snprintf(loopLine, sizeof(loopLine), "LOOP: 1 BEAT");
		}else{
			snprintf(loopLine, sizeof(loopLine), "LOOP: 1/2 BEAT");
		}
		const char* syncLine;
		switch(sync.state){
			case DJ_SYNC_LOCKED: syncLine = "SYNC: LOCKED"; break;
			case DJ_SYNC_ARMED: syncLine = "SYNC: ARMED"; break;
			default: syncLine = "SYNC: OFF"; break;
		}

		canvas->setTextDatum(TL_DATUM);
		canvas->drawString(loopLine, x + 2, 32);
		canvas->drawString("HOLD 1/2: 1 BEAT", x + 2, 48);
		canvas->drawString("HOLD 3: 4 BEAT", x + 2, 60);
		canvas->drawString(syncLine, x + 2, 78);
		canvas->drawString("HOLD SYNC: TOGGLE", x + 2, 94);
	}

	canvas->setTextDatum(BC_DATUM);
	canvas->drawString("HOLD CENTER: MENU", 80, 126);
	canvas->setTextDatum(TL_DATUM);
}

const char* MixScreen::MixScreen::assistFailureText(DjAssistTransitionFailure failure) const{
	switch(failure){
		case DJ_ASSIST_FAIL_NONE: return "NONE";
		case DJ_ASSIST_FAIL_METADATA_LOST: return "METADATA LOST";
		case DJ_ASSIST_FAIL_COMMAND_REJECTED: return "COMMAND REJECTED";
		case DJ_ASSIST_FAIL_MEDIA_REMOVED: return "SD REMOVED";
		case DJ_ASSIST_FAIL_END_OF_TRACK: return "TRACK ENDED";
		case DJ_ASSIST_FAIL_MANUAL_OVERRIDE: return "MANUAL OVERRIDE";
		case DJ_ASSIST_FAIL_CONFLICT: return "LOOP/REC CONFLICT";
		case DJ_ASSIST_FAIL_TARGET_NOT_LOADED: return "TARGET NOT LOADED";
		case DJ_ASSIST_FAIL_TARGET_CHANGED: return "TARGET CHANGED";
		case DJ_ASSIST_FAIL_CANCELLED: return "CANCELLED";
		default: return "UNKNOWN";
	}
}

const char* MixScreen::MixScreen::assistActionText(DjAssistTransitionAction action) const{
	switch(action){
		case DJ_ASSIST_ACTION_WAIT_BOUNDARY: return "WAIT FOR WINDOW";
		case DJ_ASSIST_ACTION_START_DECK: return "START DECK";
		case DJ_ASSIST_ACTION_LOCK_TEMPO: return "LOCK TEMPO";
		case DJ_ASSIST_ACTION_ENABLE_SYNC: return "ENABLE SYNC";
		case DJ_ASSIST_ACTION_CROSSFADE: return "CROSSFADE";
		case DJ_ASSIST_ACTION_STOP_DECK: return "STOP DECK";
		case DJ_ASSIST_ACTION_RELEASE_SYNC: return "RELEASE SYNC";
		default: return "STEP";
	}
}

void MixScreen::MixScreen::drawAssistBank(){
	if(!session) return;
	DjAssistSnapshot assist;
	session->copyAssistSnapshot(assist);
	DjSnapshot snapshot;
	session->copySnapshot(snapshot);

	Sprite* canvas = screen.getSprite();
	canvas->fillRect(0, 0, 160, 128, TFT_BLACK);
	canvas->setTextFont(1);
	canvas->setTextSize(1);
	canvas->setTextColor(TFT_WHITE);
	canvas->setTextDatum(TC_DATUM);

	const char* modeText;
	switch(assist.mode){
		case DJ_ASSIST_MODE_OFF: modeText = "OFF"; break;
		case DJ_ASSIST_MODE_COACH: modeText = "COACH"; break;
		case DJ_ASSIST_MODE_TRANSITION_ARMED: modeText = "ARMED"; break;
		case DJ_ASSIST_MODE_TRANSITION_RUNNING: modeText = "RUNNING"; break;
		case DJ_ASSIST_MODE_TRANSITION_COMPLETE: modeText = "COMPLETE"; break;
		case DJ_ASSIST_MODE_TRANSITION_FAILED: modeText = "FAILED"; break;
		default: modeText = "?"; break;
	}
	char header[32];
	snprintf(header, sizeof(header), "ASSIST | %s", modeText);
	canvas->drawString(header, 80, 2);
	canvas->setTextDatum(TL_DATUM);

	if(assist.mode == DJ_ASSIST_MODE_OFF){
		canvas->setTextDatum(MC_DATUM);
		canvas->drawString("COACH ADVICE IS OFF", 80, 40);
		canvas->drawString("ENABLE FOR SUGGESTIONS", 80, 56);
		canvas->setTextDatum(TL_DATUM);
	}else if(assist.mode == DJ_ASSIST_MODE_COACH){
		const DjAssistCoachAdvice& advice = assist.advice;
		char line[32];
		if(advice.valid){
			snprintf(line, sizeof(line), "ADVICE: DECK %c", advice.suggestedDeck == 0 ? 'A' : 'B');
			canvas->drawString(line, 2, 14);
			snprintf(line, sizeof(line), "WINDOW:%s RATE:%u%%",
				(advice.warningFlags & DJ_ASSIST_WARN_NO_GRID) ? "NONE" : (advice.boundaryIsPhrase ? "PHRASE" : "BEAT"),
				(unsigned) (advice.targetRateMilli / 10));
			canvas->drawString(line, 2, 26);
			snprintf(line, sizeof(line), "FADE: %s",
				advice.crossfaderDirection < 0 ? "-> A" :
				advice.crossfaderDirection > 0 ? "-> B" : "HOLD");
			canvas->drawString(line, 2, 38);

			// Accessible text reasons, never color-only: each warning bit
			// gets its own short label so a screen reader / low-vision user
			// gets the same information a sighted user would from color.
			uint8_t warningCount = 0;
			if(advice.warningFlags & DJ_ASSIST_WARN_OUT_OF_RANGE){
				canvas->drawString("! TEMPO OUT OF RANGE", 2, 50 + warningCount++ * 10);
			}
			if(advice.warningFlags & DJ_ASSIST_WARN_NO_GRID){
				canvas->drawString("! NO BEAT GRID", 2, 50 + warningCount++ * 10);
			}
			if(advice.warningFlags & DJ_ASSIST_WARN_ENDING_SOON){
				canvas->drawString("! TRACK ENDING SOON", 2, 50 + warningCount++ * 10);
			}
			if(advice.warningFlags & DJ_ASSIST_WARN_RECORDING_ACTIVE){
				canvas->drawString("! RECORDING ACTIVE", 2, 50 + warningCount++ * 10);
			}
			if(advice.warningFlags & DJ_ASSIST_WARN_LOOP_ACTIVE){
				canvas->drawString("! LOOP ACTIVE", 2, 50 + warningCount++ * 10);
			}
			if(advice.warningFlags & DJ_ASSIST_WARN_NO_METADATA){
				canvas->drawString("! NO METADATA", 2, 50 + warningCount++ * 10);
			}
			if(warningCount == 0) canvas->drawString("NO WARNINGS", 2, 50);
		}else{
			canvas->drawString("NO ADVICE YET", 2, 20);
			canvas->drawString("LOAD + PLAY A DECK", 2, 32);
		}

		if(assist.suggestionCount > 0){
			const uint8_t index = controls.assistSuggestion < assist.suggestionCount
					? controls.assistSuggestion : 0;
			const DjAssistSuggestion& suggestion = assist.suggestions[index];
			snprintf(line, sizeof(line), "SUG %u/%u  IDX %lu", index + 1, assist.suggestionCount,
				(unsigned long) suggestion.libraryIndex);
			canvas->drawString(line, 2, 92);

			const char* keyText;
			switch(suggestion.keyRelationship){
				case DJ_ASSIST_KEY_SAME: keyText = "SAME"; break;
				case DJ_ASSIST_KEY_ADJACENT: keyText = "ADJ"; break;
				case DJ_ASSIST_KEY_RELATIVE: keyText = "REL"; break;
				case DJ_ASSIST_KEY_INCOMPATIBLE: keyText = "INCOMPAT"; break;
				default: keyText = "UNKNOWN"; break;
			}
			char sign = suggestion.tempoDeltaMilli < 0 ? '-' : '+';
			const long tempoWhole = labs(suggestion.tempoDeltaMilli) / 1000;
			snprintf(line, sizeof(line), "TEMPO:%c%ld KEY:%s CONF:%u%%",
				sign, tempoWhole, keyText, suggestion.confidence / 10);
			canvas->drawString(line, 2, 104);
		}else{
			canvas->drawString("NO CANDIDATES YET", 2, 92);
		}
	}else if(assist.mode == DJ_ASSIST_MODE_TRANSITION_ARMED || assist.mode == DJ_ASSIST_MODE_TRANSITION_RUNNING){
		const DjAssistTransitionPlan& plan = assist.plan;
		char line[32];
		snprintf(line, sizeof(line), "DECK %c -> DECK %c",
			plan.fromDeck == 0 ? 'A' : 'B', plan.toDeck == 0 ? 'A' : 'B');
		canvas->drawString(line, 2, 20);
		snprintf(line, sizeof(line), "STEP %u/%u: %s", plan.currentStep + 1, plan.stepCount,
			plan.currentStep < plan.stepCount ? assistActionText(plan.steps[plan.currentStep].action) : "DONE");
		canvas->drawString(line, 2, 36);
		snprintf(line, sizeof(line), "CROSSFADE OVER %u BEATS", plan.crossfadeBeats);
		canvas->drawString(line, 2, 52);
	}else if(assist.mode == DJ_ASSIST_MODE_TRANSITION_COMPLETE){
		canvas->setTextDatum(MC_DATUM);
		canvas->drawString("TRANSITION COMPLETE", 80, 40);
		canvas->drawString("HOLD L1 TO RESUME COACH", 80, 56);
		canvas->setTextDatum(TL_DATUM);
	}else if(assist.mode == DJ_ASSIST_MODE_TRANSITION_FAILED){
		canvas->setTextDatum(MC_DATUM);
		canvas->drawString("TRANSITION FAILED", 80, 32);
		canvas->drawString(assistFailureText(assist.plan.failure), 80, 46);
		canvas->drawString("HOLD L1 TO RESUME COACH", 80, 62);
		canvas->setTextDatum(TL_DATUM);
	}

	canvas->setTextDatum(BC_DATUM);
	if(assist.mode == DJ_ASSIST_MODE_TRANSITION_ARMED || assist.mode == DJ_ASSIST_MODE_TRANSITION_RUNNING){
		canvas->drawString("HOLD R1: CANCEL", 80, 116);
	}else{
		const uint8_t otherDeck = selectedChannel == 0 ? 1 : 0;
		char footer[32];
		snprintf(footer, sizeof(footer), "L1:COACH  R1:ARM->%c",
			otherDeck == 0 ? 'A' : 'B');
		canvas->drawString(footer, 80, 116);
	}
	canvas->drawString("CTR: SUGGESTION  HOLD: MENU", 80, 126);
	canvas->setTextDatum(TL_DATUM);
}

void MixScreen::MixScreen::assistToggleCoach(){
	if(!session) return;
	DjAssistSnapshot assist;
	session->copyAssistSnapshot(assist);
	const bool wantCoach = assist.mode != DJ_ASSIST_MODE_COACH;
	const DjSubmitResult result = session->assistSetMode(wantCoach, DJ_ORIGIN_PHYSICAL);
	if(!result.accepted()) showCommandError(result.error);
	drawQueued = true;
}

void MixScreen::MixScreen::assistArmFromSelected(){
	if(!session) return;
	DjAssistSnapshot assist;
	session->copyAssistSnapshot(assist);

	if(assist.mode == DJ_ASSIST_MODE_TRANSITION_ARMED || assist.mode == DJ_ASSIST_MODE_TRANSITION_RUNNING){
		const DjSubmitResult result = session->assistCancelTransition(DJ_ORIGIN_PHYSICAL);
		if(!result.accepted()) showCommandError(result.error);
		drawQueued = true;
		return;
	}
	if(assist.mode != DJ_ASSIST_MODE_COACH) return; // OFF/COMPLETE/FAILED: use L1 to resume Coach first.

	DjSnapshot snapshot;
	if(!session->copySnapshot(snapshot)) return;
	const uint8_t fromDeck = selectedChannel;
	const uint8_t toDeck = fromDeck == 0 ? 1 : 0;
	if(!snapshot.decks[toDeck].loaded || snapshot.decks[toDeck].metadata.state != DJ_METADATA_VALID){
		showCommandError(DJ_COMMAND_ERROR_ASSIST_REJECTED);
		return;
	}

	// The physical bank always confirms whatever is already loaded on the
	// other deck (the "loaded target" path from the spec) - loading a
	// candidate there first still goes through the existing Browse
	// workflow. Best-effort match the loaded identity against the current
	// suggestion list purely so the arm command carries a meaningful
	// libraryIndex for display/bookkeeping; the engine itself validates the
	// transition against the identity, not this index.
	uint32_t libraryIndex = 0;
	for(uint8_t i = 0; i < assist.suggestionCount; i++){
		if(DjAssistScoring::identityMatches(assist.suggestions[i].identity, snapshot.decks[toDeck].identity)){
			libraryIndex = assist.suggestions[i].libraryIndex;
			break;
		}
	}

	const DjSubmitResult result = session->assistArmTransition(
		fromDeck, toDeck, libraryIndex, snapshot.decks[toDeck].identity,
		16, true, true, DJ_ORIGIN_PHYSICAL
	);
	if(!result.accepted()) showCommandError(result.error);
	drawQueued = true;
}

void MixScreen::MixScreen::drawPalette(){
	static const char* items[] = {
			"MIX", "CUES", "BROWSE", "LOOP/SYNC", "ASSIST", "MATRIX", "RESCAN SD", "SETTINGS", "EXIT DJ"
	};
	Sprite* canvas = screen.getSprite();
	canvas->fillRect(0, 0, 160, 128, TFT_BLACK);
	canvas->setTextFont(1);
	canvas->setTextSize(1);
	canvas->setTextDatum(TC_DATUM);
	canvas->setTextColor(TFT_WHITE);
	canvas->drawString("CONTROL BANKS / ACTIONS", 80, 2);
	for(uint8_t i = 0; i < MIX_PALETTE_COUNT; i++){
		const int16_t y = 17 + i * 14;
		if(i == controls.paletteSelection){
			canvas->fillRect(8, y - 1, 144, 12, TFT_WHITE);
			canvas->setTextColor(TFT_BLACK);
		}else{
			canvas->setTextColor(TFT_WHITE);
		}
		canvas->drawString(items[i], 80, y);
	}
	canvas->setTextColor(TFT_WHITE);
	canvas->setTextDatum(BC_DATUM);
	canvas->drawString("ROTATE / PRESS CONFIRM", 80, 127);
	canvas->setTextDatum(TL_DATUM);
}

void MixScreen::MixScreen::drawStatus(){
	if(statusText.length() == 0 || millis() >= statusUntil) return;
	Sprite* canvas = screen.getSprite();
	canvas->fillRect(0, 111, 160, 17, TFT_BLACK);
	canvas->setTextFont(1);
	canvas->setTextSize(1);
	canvas->setTextColor(TFT_WHITE);
	canvas->setTextDatum(BC_DATUM);
	canvas->drawString(statusText.substring(0, 26), 80, 124);
	canvas->setTextDatum(TL_DATUM);
}

const char* MixScreen::MixScreen::commandErrorText(DjCommandError error) const{
	switch(error){
		case DJ_COMMAND_ERROR_QUEUE_FULL: return "COMMAND QUEUE FULL";
		case DJ_COMMAND_ERROR_NO_DECK: return "LOAD A DECK FIRST";
		case DJ_COMMAND_ERROR_NO_EFFECT: return "SELECT AN EFFECT";
		case DJ_COMMAND_ERROR_OPEN_FAILED: return "TRACK LOAD FAILED";
		case DJ_COMMAND_ERROR_EMPTY_CUE: return "CUE IS EMPTY";
		case DJ_COMMAND_ERROR_RECORDING_ACTIVE: return "STOP RECORDING TO LOAD";
		case DJ_COMMAND_ERROR_RECORDING_BUSY: return "RECORDING BUSY";
		case DJ_COMMAND_ERROR_RECORDING_FAILED: return "RECORDING FAILED";
		case DJ_COMMAND_ERROR_SESSION_ENDING: return "DJ SESSION ENDING";
		case DJ_COMMAND_ERROR_ASSIST_REJECTED: return "ASSIST TRANSITION REJECTED";
		case DJ_COMMAND_ERROR_ASSIST_OVERRIDE_PENDING: return "MANUAL OVERRIDE PENDING";
		default: return "COMMAND REJECTED";
	}
}

const char* MixScreen::MixScreen::recordingErrorText(DjRecordingError error) const{
	switch(error){
		case DJ_RECORDING_ERROR_SD_UNAVAILABLE: return "SD CARD UNAVAILABLE";
		case DJ_RECORDING_ERROR_OPEN_FAILED: return "RECORDING FILE ERROR";
		case DJ_RECORDING_ERROR_WRITE_FAILED: return "RECORDING WRITE FAILED";
		case DJ_RECORDING_ERROR_FINALIZE_FAILED: return "RECORDING SAVE FAILED";
		case DJ_RECORDING_ERROR_BUFFER_OVERRUN: return "RECORDING OVERRUN";
		case DJ_RECORDING_ERROR_QUEUE_FULL: return "RECORDING QUEUE FULL";
		case DJ_RECORDING_ERROR_NAME_EXHAUSTED: return "RECORDING STORAGE FULL";
		case DJ_RECORDING_ERROR_RENAME_FAILED: return "RECORDING SAVE FAILED";
		default: return "RECORDING FAILED";
	}
}

void MixScreen::MixScreen::showCommandError(DjCommandError error){
	statusText = commandErrorText(error);
	statusUntil = millis() + 2500;
	drawQueued = true;
}

void MixScreen::MixScreen::buildUI(){
	screenLayout->setWHType(PARENT, PARENT);
	screenLayout->setGutter(2);
	screenLayout->addChild(leftLayout);
	screenLayout->addChild(rightLayout);

	leftLayout->setWHType(FIXED, PARENT);
	leftLayout->setWidth(79);
	leftLayout->setGutter(10);
	leftLayout->setPadding(1);


	leftLayout->addChild(leftSeekBar);
	leftLayout->addChild(leftSongName);

	for(int i = 0; i < 3; i++){
		leftLayout->addChild(effectElements[i]);
	}


	rightLayout->setWHType(FIXED, PARENT);
	rightLayout->setWidth(79);
	rightLayout->setGutter(10);
	rightLayout->setPadding(1);


	rightLayout->addChild(rightSeekBar);
	rightLayout->addChild(rightSongName);

	for(int i = 3; i < 6; i++){
		rightLayout->addChild(effectElements[i]);
	}

	screenLayout->reflow();
	leftLayout->reflow();
	rightLayout->reflow();

	screen.addChild(screenLayout);
	screen.repos();
}

void MixScreen::MixScreen::loop(uint micros){
	if(seekTime != 0 && millis() - seekTime >= 100){
		SongSeekBar* bar = seekChannel ? rightSeekBar : leftSeekBar;

		const DjSubmitResult seekResult =
				session->seek(seekChannel, bar->getCurrentDuration(), DJ_ORIGIN_PHYSICAL);
		if(!seekResult.accepted()) showCommandError(seekResult.error);

		if(wasRunning && seekResult.accepted()){
			const DjSubmitResult playResult = session->setPlaying(seekChannel, true, DJ_ORIGIN_PHYSICAL);
			if(!playResult.accepted()) showCommandError(playResult.error);
		}

		seekChannel = -1;
		seekTime = 0;
	}

	bool update = false;
	for(const auto& element : effectElements){
		update |= element->needsUpdate();
	}

	DjSnapshot snapshot;
	if(session && session->copySnapshot(snapshot)){
		processCommandResults(snapshot);
		update |= syncFromSnapshot(snapshot);
		const bool hasTarget = snapshot.decks[0].path[0] != '\0' || snapshot.decks[1].path[0] != '\0';
		if(!hasTarget && !browseWasOpened && !songListOpen && !session->hasPendingLoad()){
			loadingChannel = 0;
			controls.bank = MIX_BANK_BROWSE;
			openBrowse();
			return;
		}
	}

	bool songNameUpdateL = leftSongName->checkScrollUpdate();
	bool songNameUpdateR = rightSongName->checkScrollUpdate();
	update |= songNameUpdateL | songNameUpdateR;

	uint32_t currentTime = millis();
	if(statusText.length() && currentTime >= statusUntil){
		statusText = "";
		update = true;
	}
	if((update || drawQueued) && (currentTime - lastDraw) >= (recordingState == DJ_RECORDING_ACTIVE ? 200 : 50)){
		drawQueued = false;
		draw();
		screen.commit();
		lastDraw = currentTime;
	}else if(update){
		drawQueued = true;
	}
}


void MixScreen::MixScreen::potMove(uint8_t id, uint8_t value){
	if(id == POT_MID){
		const DjSubmitResult result = session->setMix(value, DJ_ORIGIN_PHYSICAL);
		if(result.accepted()){
			matrixManager.fillMatrixMid(value);
			matrixManager.matrixMid.push();
		}else{
			showCommandError(result.error);
		}
	}else if(id == POT_L){
		const DjSubmitResult result = session->setGain(0, value, DJ_ORIGIN_PHYSICAL);
		if(!result.accepted()) showCommandError(result.error);
	}else if(id == POT_R){
		const DjSubmitResult result = session->setGain(1, value, DJ_ORIGIN_PHYSICAL);
		if(!result.accepted()) showCommandError(result.error);
	}
}

void MixScreen::MixScreen::startBigVu(){
	LoopManager::addListener(&midVu);
}

void MixScreen::MixScreen::stopBigVu(){
	LoopManager::removeListener(&midVu);
}

void MixScreen::MixScreen::btnCombination(){
	// Guard against re-triggering while the library hasn't yet applied the
	// previous start/stop request -- avoids duplicate toggles racing the
	// async accepted-vs-applied recording lifecycle.
	if(recordingState == DJ_RECORDING_STARTING || recordingState == DJ_RECORDING_STOPPING){
		return;
	}
	const bool wantRecording = recordingState != DJ_RECORDING_ACTIVE;
	const DjSubmitResult result = session->setRecording(wantRecording, DJ_ORIGIN_PHYSICAL);
	if(!result.accepted()) showCommandError(result.error);
}

void MixScreen::MixScreen::btn(uint8_t i){
	DjSnapshot snapshot;
	if(!session || !session->copySnapshot(snapshot) || i >= DJ_DECK_COUNT) return;
	const DjSubmitResult result =
			session->setPlaying(i, !snapshot.decks[i].playing, DJ_ORIGIN_PHYSICAL);
	if(!result.accepted()) showCommandError(result.error);
}

void MixScreen::MixScreen::btnEnc(uint8_t i){
	if(i > 6) return;

	if(i == 6){
		if(controls.paletteOpen){
			applyPaletteSelection(controls.confirmPalette());
			return;
		}
		if(controls.bank == MIX_BANK_BROWSE){
			openBrowse();
			return;
		}
		selectedChannel = !selectedChannel;
		drawQueued = true;
		return;
	}

	if(controls.bank == MIX_BANK_CUES){
		const int8_t cue = controls.cueForEncoder(i);
		if(cue < 0) return;
		DjSnapshot snapshot;
		if(!session->copySnapshot(snapshot)) return;
		const uint8_t deck = i >= 3;
		const DjSubmitResult result = snapshot.decks[deck].cues[cue].occupied
				? session->triggerCue(deck, cue, DJ_ORIGIN_PHYSICAL)
				: session->setCue(deck, cue, DJ_ORIGIN_PHYSICAL);
		if(!result.accepted()) showCommandError(result.error);
		return;
	}
	if(controls.bank != MIX_BANK_MIX) return;
	EffectElement* effect = effectElements[i];
	effect->setSelected(!effect->isSelected());
	drawQueued = true;
}

void MixScreen::MixScreen::enc(uint8_t index, int8_t value){
	if(index == 6){
		if(controls.paletteOpen){
			controls.movePalette(value);
			drawQueued = true;
			return;
		}
		if(controls.bank == MIX_BANK_CUES){
			controls.moveCuePage(value);
			drawQueued = true;
			return;
		}
		if(controls.bank == MIX_BANK_ASSIST){
			DjAssistSnapshot assist;
			if(session) session->copyAssistSnapshot(assist);
			controls.moveAssistSuggestion(value, assist.suggestionCount);
			drawQueued = true;
			return;
		}
		if(controls.bank != MIX_BANK_MIX) return;
		DjSnapshot snapshot;
		if(!session->copySnapshot(snapshot) || !snapshot.decks[selectedChannel].loaded) return;
		if(seekTime == 0){
			seekChannel = selectedChannel;
			wasRunning = snapshot.decks[selectedChannel].playing;
			const DjSubmitResult result =
					session->setPlaying(selectedChannel, false, DJ_ORIGIN_PHYSICAL);
			if(!result.accepted()){
				showCommandError(result.error);
				seekChannel = -1;
				return;
			}
		}

		seekTime = millis();

		SongSeekBar* bar = seekChannel ? rightSeekBar : leftSeekBar;
		uint16_t seekTime = constrain(bar->getCurrentDuration() + value, 0, bar->getTotalDuration());
		bar->setCurrentDuration(seekTime);

		drawQueued = true;
		return;
	}

	if(controls.bank != MIX_BANK_MIX) return;
	EffectElement* element = effectElements[index];

	if(element->isSelected()){
		int8_t e = element->getType() + value;
		if(e >= EffectType::COUNT){
			e = e % EffectType::COUNT;
		}else if(e < 0){
			while(e < 0){
				e += EffectType::COUNT;
			}
		}

		// Only one speed allowed
		if(e == EffectType::SPEED){
			for(int i = (index < 3 ? 0 : 3); i < (index < 3 ? 3 : 6); i++){
				if(i == index) continue;
				if(effectElements[i]->getType() != EffectType::SPEED) continue;

				if(value < 0){
					e = e > 0 ? e - 1 : EffectType::COUNT - 1;
				}else{
					e = (e + 1) % EffectType::COUNT;
				}

				break;
			}
		}

		EffectType type = static_cast<EffectType>(e);
		const uint8_t deck = index >= 3;
		const uint8_t slot = index % 3;
		const DjSubmitResult result =
				session->setEffectType(deck, slot, type, DJ_ORIGIN_PHYSICAL);
		if(!result.accepted()){
			showCommandError(result.error);
		}
	}else{
		EffectType type = element->getType();
		if(type == EffectType::NONE) return;

		int16_t intensity = element->getIntensity() + value * 5;
		intensity = max((int16_t) 0, intensity);
		intensity = min((int16_t) 255, intensity);

		const DjSubmitResult result =
				session->setEffectIntensity(index >= 3, index % 3, intensity, DJ_ORIGIN_PHYSICAL);
		if(!result.accepted()) showCommandError(result.error);
	}
}

void MixScreen::MixScreen::encBtnHold(uint8_t i){
	if(i == 6){
		controls.openPalette();
		drawQueued = true;
		return;
	}
	if(i >= 6) return;

	if(controls.bank == MIX_BANK_CUES){
		const int8_t cue = controls.cueForEncoder(i);
		if(cue < 0) return;
		const DjSubmitResult result = session->clearCue(i >= 3, cue, DJ_ORIGIN_PHYSICAL);
		if(!result.accepted()) showCommandError(result.error);
		return;
	}
	if(controls.bank == MIX_BANK_LOOPSYNC){
		// Per-slot encoder hold (L1-L3/R1-R3) doubles as the LOOP/SYNC bank
		// here, gated on the beat-grid capability by DjSession itself
		// (setSync/loopEngage reject rather than silently no-op when the
		// grid/BPM capability is unavailable). Only active while this bank
		// is selected via the palette, so Cues/Mix hold behavior is untouched.
		const uint8_t deck = i >= 3;
		const uint8_t slot = i % 3; // slot 0/1 -> LOOP bank, slot 2 -> SYNC/ASSIST

		DjSnapshot snapshot;
		session->copySnapshot(snapshot);

		if(slot == 2){
			const bool currentlyArmed = snapshot.decks[deck].sync.state != DJ_SYNC_OFF;
			session->setSync(deck, !currentlyArmed, -1, DJ_ORIGIN_PHYSICAL);
		}else if(snapshot.decks[deck].loop.state != DJ_LOOP_INACTIVE){
			session->loopDisengage(deck, DJ_ORIGIN_PHYSICAL);
		}else{
			const DjLoopLength length = slot == 0 ? DJ_LOOP_BEAT_1 : DJ_LOOP_BEAT_4;
			session->loopEngage(deck, length, DJ_ORIGIN_PHYSICAL);
		}

		drawQueued = true;
		return;
	}
	if(controls.bank == MIX_BANK_ASSIST){
		// Only two visible, labelled hold-actions on this bank (both shown
		// on screen every time - no hidden chord): L1 toggles Coach
		// advice-only mode on/off (also the "dismiss" action once a
		// transition finishes or fails, since setCoachEnabled() is always
		// safe to call outside ARMED/RUNNING); R1 confirms a one-shot
		// transition from the selected deck to whatever's already loaded on
		// the other deck while Coach is active, or cancels an in-flight
		// transition. Every other encoder on this bank is inert.
		if(i == 0){
			assistToggleCoach();
			return;
		}
		if(i == 3){
			assistArmFromSelected();
			return;
		}
		return;
	}
	if(controls.bank == MIX_BANK_MIX){
		const DjSubmitResult result =
				session->setEffectType(i >= 3, i % 3, EffectType::NONE, DJ_ORIGIN_PHYSICAL);
		if(!result.accepted()) showCommandError(result.error);
	}
}

bool MixScreen::MixScreen::allowsEncoderChords() const{
	return MixControlState::encoderChordsEnabled();
}

void MixScreen::MixScreen::applyPaletteSelection(MixPaletteItem item){
	switch(item){
		case MIX_PALETTE_BROWSE:
		case MIX_PALETTE_RESCAN:
			controls.bank = MIX_BANK_BROWSE;
			openBrowse();
			return;
		case MIX_PALETTE_MATRIX: {
			stop();
			MatrixPopUpPicker* popUpPicker = new MatrixPopUpPicker(*this);
			popUpPicker->unpack();
			popUpPicker->start();
			return;
		}
		case MIX_PALETTE_SETTINGS:
			(new SettingsScreen::SettingsScreen(*screen.getDisplay(), false))->push(this);
			return;
		case MIX_PALETTE_EXIT:
			if(session){
				session->detachView();
				DjSession::end();
				session = nullptr;
			}
			pop();
			return;
		default:
			drawQueued = true;
			return;
	}
}
