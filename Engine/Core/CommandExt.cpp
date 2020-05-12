/**
 *  CommandExt.cpp
 *  ONScripter-RU
 *
 *  Command executer for core extended commands (RU part).
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Joystick.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Layers/ObjectFall.hpp"
#include "Engine/Layers/Media.hpp"
#include "Engine/Layers/Subtitle.hpp"
#include "Resources/Support/Version.hpp"
#include "Support/Unicode.hpp"
#include "Support/FileIO.hpp"

#if defined(IOS) && defined(USE_OBJC)
#include "Support/Apple/UIKitWrapper.hpp"
#elif defined(DROID)
#include "Support/Droid/DroidProfile.hpp"
#endif

#ifdef IOS
#include <malloc/malloc.h>
#endif

int ONScripter::zOrderOverridePreserveCommand() {
	preserve = !preserve;
	return RET_CONTINUE;
}

int ONScripter::zOrderOverrideCommand() {
	bool is_lsp2 = (script_h.isName("z_order_override2"));

	int sprite_num    = script_h.readInt();
	int override_to   = script_h.readInt();
	AnimationInfo &si = is_lsp2 ? sprite2_info[sprite_num] : sprite_info[sprite_num];

	dynamicProperties.addSpriteProperty(&si, sprite_num, is_lsp2, true, SPRITE_PROPERTY_Z_ORDER, override_to);

	if ((sprite_num <= z_order_hud) != (si.z_order_override <= z_order_hud)) {
		errorAndExit("You can't use z_order_override to move a sprite between scene and hud.");
	}

	return RET_CONTINUE;
}

int ONScripter::wheelvalueCommand() {
	script_h.readVariable();
	script_h.pushVariable();
	script_h.setInt(&script_h.pushed_variable, last_wheelscroll);
	last_wheelscroll = 0;
	return RET_CONTINUE;
}

int ONScripter::waitlipsCommand() {
	if (skip_mode & SKIP_SUPERSKIP)
		return RET_CONTINUE;
	bool mustWait{false};
	for (const auto &act : fetchConstantRefreshActions<LipsAnimationAction>()) {
		if (act->expired() || act->terminated) {
			mustWait = true;
			break;
		}
	}
	if (!mustWait)
		return RET_CONTINUE;

	WaitAction *action{WaitAction::create()};
	action->event_mode = WAIT_WAIT2_MODE;
	action->clock.setCountdown(1);
	Lock lock(&registeredCRActions);
	registeredCRActions.emplace_back(action);

	return RET_CONTINUE;
}

int ONScripter::waitvoiceCommand() {

	// Syntax (a bitmask):
	// waitvoice 0 -> waits for channel 0, can be skipped
	// waitvoice 1 -> waits for channel 0, cannot be skipped
	// waitvoice 2 -> waits for channel 0, can be skipped, ignores voice status
	// waitvoice 3 -> waits for channel 0, cannot be skipped, ignores voice status

	int mask             = script_h.readInt();
	bool uninterruptible = mask & 1;
	bool ignoreVoicePlay = mask & 2;
	int extraDelay       = ignore_voicedelay ? 0 : voicedelay_time;

	if (script_h.hasMoreArgs()) {
		extraDelay = script_h.readInt();
	}

	if (!ignoreVoicePlay && (!ons.wave_sample[0] || !Mix_Playing(0) || Mix_Paused(0))) {
		extraDelay = 0; // Ignore the extra wait to fulfil short voice/skip needs
	}

	WaitVoiceAction *action{WaitVoiceAction::create()};
	action->event_mode   = (automode_flag || uninterruptible) ? WAIT_VOICE_MODE : WAIT_VOICE_MODE | WAIT_INPUT_MODE;
	action->voiceDelayMs = (skip_mode & SKIP_SUPERSKIP) || (skip_mode & SKIP_NORMAL) ? 0 : extraDelay;

	Lock lock(&ons.registeredCRActions);
	registeredCRActions.emplace_back(action);

	return RET_CONTINUE;
}

int ONScripter::waitvideoCommand() {
	auto layer = getLayer<MediaLayer>(video_layer);

	while (layer && layer->isPlaying(false) && video_skip_mode != VideoSkip::NotPlaying) {
		waitEvent(0);
	}

	return RET_CONTINUE;
}

int ONScripter::lvStopCommand() {
	stopLvPlayback();

	return RET_CONTINUE;
}

int ONScripter::lvSetLogCommand() {
	auto ch = validChannel(script_h.readInt());
	std::string file(script_h.readFilePath());
	bool eob = true;

	if (script_h.hasMoreArgs()) {
		eob = script_h.readInt(); //end of block (1 == end)
		if (!eob) {
			if (!script_h.logState.tmpVoiceGroupStarted) {
				script_h.logState.tmpVoiceGroupStarted = true;
				script_h.logState.tmpVoices.emplace_back();
			}
			script_h.logState.tmpVoices.back()[ch] = file;
			return RET_CONTINUE;
		} else if (script_h.logState.tmpVoiceGroupStarted) {
			script_h.logState.tmpVoices.back()[ch] = file;
			script_h.logState.tmpVoiceGroupStarted = false;
			script_h.logState.tmpVoices.emplace_back();
			return RET_CONTINUE;
		}
	}

	script_h.logState.tmpVoices.emplace_back();
	script_h.logState.tmpVoices.back()[ch] = file;

	return RET_CONTINUE;
}

int ONScripter::lvPlayCommand() {
	int scrollableId = validSprite(script_h.readInt()); //tree sprite number
	int vol          = script_h.readInt();              //voice volume from config

	stopLvPlayback();

	AnimationInfo &ai = sprite_info[scrollableId];
	if (!ai.scrollableInfo.isSpecialScrollable)
		errorAndExit("scrollable_get_hovered_elem called on something that's not a scrollable");

	auto &tree = dataTrees[ai.scrollableInfo.elementTreeIndex];
	auto &elem = tree.getById(ai.scrollableInfo.hoveredElement);

	if (!elem.has("log"))
		errorAndExit("Inadequate tree");

	script_h.logState.currVoiceDialogueLabelIndex = script_h.logState.logEntryIndexToLabelIndex(std::stoi(elem["log"].value));
	script_h.logState.currVoiceSet                = -1;
	script_h.logState.currVoiceVolume             = vol;

	startLvPlayback();

	return RET_CONTINUE;
}

int ONScripter::vvSetLogCommand() {
	script_h.logState.tmpVolume = script_h.readInt(); // voice volume for the dialogue
	return RET_CONTINUE;
}

int ONScripter::videovolCommand() {
	video_volume = validVolume(script_h.readInt());
	setVolume(MIX_VIDEO_CHANNEL, video_volume, volume_on_flag);

	return RET_CONTINUE;
}

// verify_files %ret,"file"[,$list]
// -4 old hash
// -3 invalid hash
// -2 unsupported file
// -1 no file
//  0 no error
//  1 game file validation failed
int ONScripter::verifyFilesCommand() {
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> fileInfo;

	script_h.readVariable();
	script_h.pushVariable();

	if (!readIniFile(script_h.readFilePath(), fileInfo)) {
		script_h.setInt(&script_h.pushed_variable, -1);
		while (script_h.hasMoreArgs()) script_h.readVariable();
		return RET_CONTINUE; //dummy
	}

	auto info = fileInfo.find("info");
	auto data = fileInfo.find("data");

	std::string passedDate;

	bool looksFine{false};

	if (info != fileInfo.end() && data != fileInfo.end()) {
		auto &infoNode = info->second;
		auto game      = infoNode.find("game");
		auto hash      = infoNode.find("hash");
		auto ver       = infoNode.find("ver");
		auto apiver    = infoNode.find("apiver");
		auto date      = infoNode.find("date");

		if (game != infoNode.end() && hash != infoNode.end() && ver != infoNode.end() && apiver != infoNode.end() &&
		    date != infoNode.end() && game->second.size() > 0 && hash->second == "size" &&
			ver->second == ONS_VERSION && apiver->second == ONS_API) {

			// Try an entire match or a wild-card match.
			looksFine = game->second.find(script_h.game_identifier) != std::string::npos;
			if (!looksFine && game->second[game->second.size()-1] == '*')
				looksFine = script_h.game_identifier.compare(0, game->second.size()-1, game->second, 0, game->second.size()-1) == 0;
			passedDate = date->second;
		}
	}

	if (!looksFine) {
		script_h.setInt(&script_h.pushed_variable, -2);
		while (script_h.hasMoreArgs()) script_h.readVariable();
		return RET_CONTINUE; //dummy
	}

	// This is rather straight-forward, but our thread model needs changes anyway...

	ons.preventExit(true);

	try {
		if (passedDate != "ignore" && time(nullptr) > static_cast<time_t>(std::stoull(passedDate)) + 7 * 24 * 3600) {
			script_h.setInt(&script_h.pushed_variable, -4);
			while (script_h.hasMoreArgs()) script_h.readVariable();
			return RET_CONTINUE; //dummy
		}

		std::vector<std::string> missing, modified;
		std::string failures;
		std::atomic_flag working = ATOMIC_FLAG_INIT;

		working.test_and_set(std::memory_order_relaxed);

		auto verifyFiles = [&working, &data, &missing, &modified, &failures]() {
			for (auto &entry : data->second) {
				size_t size, readSize;

				size = static_cast<size_t>(std::stoull(entry.second));

				auto filename = entry.first;
				translatePathSlashes(filename);
				auto path = ons.script_h.reader->completePath(filename.c_str(), FileType::File, &readSize);

				if (path) {
					freearr(&path);
					if (size != readSize) {
						failures += "{c:FFA500:" + filename + "}\n";
						modified.emplace_back(filename);
					}
				} else {
					failures += "{c:FF0000:" + filename + "}\n";
					missing.emplace_back(filename);
				}
			}

			working.clear(std::memory_order_release);
			return 0;
		};

		auto verifyThread = SDL_CreateThread(cmp::lambda_ptr<int>(verifyFiles), "Verification", &verifyFiles);
		if (verifyThread) {
			SDL_DetachThread(verifyThread);
		} else {
			sendToLog(LogLevel::Warn, "Failed to create verification thread...\n");
			verifyFiles();
		}

		auto delay = 1000 / (ons.game_fps ? ons.game_fps : DEFAULT_FPS);
		do {
			waitEvent(delay);
		} while (working.test_and_set(std::memory_order_acquire));

		if (!missing.empty()) {
			sendToLog(LogLevel::Error, "Missing files\n");
			for (auto &filename : missing)
				sendToLog(LogLevel::Error, "%s\n", filename.c_str());
		}

		if (!modified.empty()) {
			sendToLog(LogLevel::Error, "Modified files\n");
			for (auto &filename : modified)
				sendToLog(LogLevel::Error, "%s\n", filename.c_str());
		}

		script_h.setInt(&script_h.pushed_variable, !failures.empty());

		if (script_h.hasMoreArgs()) {
			script_h.readVariable();
			script_h.setStr(&script_h.getVariableData(script_h.current_variable.var_no).str, failures.c_str());
		}

	} catch (...) { // std::invalid_argument and std::out_of_range
		script_h.setInt(&script_h.pushed_variable, -3);
		while (script_h.hasMoreArgs()) script_h.readVariable();
	}

	ons.preventExit(false);

	return RET_CONTINUE;
}

int ONScripter::useTextGradientsCommand() {
	use_text_gradients                      = (script_h.readInt() == 1);
	sentence_font.changeStyle().is_gradient = use_text_gradients;
	name_font.changeStyle().is_gradient     = use_text_gradients;
	return RET_CONTINUE;
}

int ONScripter::useTextGradientsForSpritesCommand() {
	use_text_gradients_for_sprites = (script_h.readInt() == 1);
	return RET_CONTINUE;
}

int ONScripter::treeSetCommand() {
	bool returnVal = script_h.isName("tree_setra") || script_h.isName("tree_seta");
	bool rawValues = script_h.isName("tree_setra") || script_h.isName("tree_setr");
	bool foundAssign{false};

	if (returnVal) {
		script_h.readVariable();
		script_h.pushVariable();
	}

	int no = validTree(script_h.readInt());

	std::deque<std::string> params;
	std::deque<std::string> values;
	bool valuesSection{false};
	bool lastItem{false};

	do {
		std::string item;
		bool equalsFound{false};

		if (valuesSection) {
			item = script_h.readRaw();
		} else {
			script_h.readVariable();
			if (script_h.current_variable.type == VariableInfo::TypeInt || script_h.current_variable.type == VariableInfo::TypeArray) {
				item = std::to_string(script_h.getIntVariable(&script_h.current_variable));
			} else if (script_h.current_variable.type == VariableInfo::TypeStr) {
				item = script_h.getVariableData(script_h.current_variable.var_no).str;
			} else {
				item = script_h.readStr();
				if (item == "=") {
					equalsFound = true;
					// Chop the tree at the param list before = (e.g. in {a,b,c,=,d,e}, subtree c will be chopped from {a,b,c}, including removing node c itself)
					dataTrees[no].prune(params);
					// All following params will be treated as (array) values at node c,
					// so we end the param list with auto (e.g. {a,b,c,auto}) for each value, to create a sequential list
					item        = "auto";
					foundAssign = true;
				}
			}
		}
		lastItem = !(script_h.hasMoreArgs());
		if (lastItem && !equalsFound)
			valuesSection = true; // Last item goes to the values section (except if it was =, in that case, we have no values)
		((valuesSection) ? values : params).push_back(item);
		if (equalsFound)
			valuesSection = true; // Further params after = go to the values section
	} while (!lastItem);

	if (!foundAssign && rawValues) {
		errorAndExit("Attempted to use raw arguments with no assignment operator");
		return RET_CONTINUE; //dummy
	}

	int result{0};
	for (std::string &value : values) {
		result = dataTrees[no].setValue(params, value);
	}
	if (returnVal)
		script_h.setInt(&script_h.pushed_variable, result);

	for (auto sp : sprites(SPRITE_LSP)) {
		if (sp->scrollableInfo.isSpecialScrollable)
			dirtySpriteRect(sp->id, false);
	}

	return RET_CONTINUE;
}

int ONScripter::treeGetCommand() {

	script_h.readVariable();
	script_h.pushVariable();

	int no = validTree(script_h.readInt());

	std::deque<std::string> params;
	do {
		script_h.readVariable();
		if (script_h.current_variable.type == VariableInfo::TypeInt || script_h.current_variable.type == VariableInfo::TypeArray) {
			params.emplace_back(std::to_string(script_h.getIntVariable(&script_h.current_variable)));
		} else if (script_h.current_variable.type == VariableInfo::TypeStr) {
			params.emplace_back(script_h.getVariableData(script_h.current_variable.var_no).str);
		} else {
			params.emplace_back(script_h.readStr());
		}
	} while (script_h.hasMoreArgs());

	bool label = params.back() == "log";

	std::string res = dataTrees[no].getValue(params);

	/* This should be removed.
	 * Let's not pretend we store labels in trees.
	 * "log" contains a log entry index not a label index.
	 * If you want a label, write and call a proper "get_log_label" function to retrieve the label based on the log index ID.
	 */
	if (label) {
		// return the label name, not its id
		int id = std::stoi(res);
		res    = "*";
		res += script_h.getLabelByLogEntryIndex(id)->name;
	}

	script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, res.c_str());

	return RET_CONTINUE;
}

int ONScripter::treeExecuteCommand() {

	// In case we will reexecute from the inside
	currentCommandPosition.set(script_h.getCurrent());

	int no = validTree(script_h.readInt());

	dataTrees[no].accept(std::make_shared<StringTree::StringTreeExecuter>());

	currentCommandPosition.unset();

	return RET_CONTINUE;
}

int ONScripter::treeClearCommand() {
	int tree = validTree(script_h.readInt());

	//	if (tree == logInfo.treeId) {
	//		logInfo.dialogueStatus.clear();
	//		logInfo.lastDialogue = -1;
	//	}

	dataTrees[tree].clear();

	for (auto sptr : sprites(SPRITE_LSP)) {
		if (sptr->scrollableInfo.isSpecialScrollable)
			dirtySpriteRect(sptr);
	}

	return RET_CONTINUE;
}

int ONScripter::textFadeDurationCommand() {
	bool temp = (script_h.isName("text_fade_t"));
	int fade  = script_h.readInt();

	if (temp)
		dlgCtrl.textFadeDuration.set(fade);
	else
		text_fade_duration = fade;

	return RET_CONTINUE;
}

int ONScripter::textDisplaySpeedCommand() {
	bool temp = (script_h.isName("text_speed_t"));
	int speed = script_h.readInt();

	if (temp)
		dlgCtrl.textDisplaySpeed.set(speed);
	else
		text_display_speed = speed;

	return RET_CONTINUE;
}

int ONScripter::stopwatchCommand() {
	const char *buf = script_h.readStr();
	printClock(buf);
	commandExecutionTime = 0;
	return RET_CONTINUE;
}

int ONScripter::spritesetPosCommand() {
	// Parameters: spriteset number; mask sprite number
	int no               = script_h.readInt();
	spritesets[no].pos.x = script_h.readInt();
	spritesets[no].pos.y = script_h.readInt();
	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
	// technically only the rect of canvas_width x canvas_height *offset by spriteset pos* needs filling.
	// That might make it a little less. But generally these three operations are a little expensive.
	return RET_CONTINUE;
}

int ONScripter::spritesetMaskCommand() {
	// Parameters: spriteset number; mask sprite number
	int no                          = script_h.readInt();
	spritesets[no].maskSpriteNumber = script_h.readInt();
	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
	return RET_CONTINUE;
}

// Parameters: spriteset number; (optionally) 1 for on, 0 for off (default on)
int ONScripter::spritesetEnableCommand() {
	int no      = script_h.readInt();
	bool enable = true;
	if (script_h.hasMoreArgs())
		enable = script_h.readInt();
	spritesets[no].setEnable(enable);
	spritesets[no].id = no;
	//sendToLog(LogLevel::Info, "spritesets[%i].enable=%i\n", no, enable);
	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
	return RET_CONTINUE;
}

int ONScripter::spritesetBlurCommand() {
	// Parameters: spriteset number; blur factor (may be as high as... 4000? or more?)
	int no              = script_h.readInt();
	spritesets[no].blur = script_h.readInt();
	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
	return RET_CONTINUE;
}

int ONScripter::spritesetAlphaCommand() {
	// Parameters: spriteset number; alpha out of 255
	int no               = script_h.readInt();
	spritesets[no].trans = script_h.readInt();
	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
	return RET_CONTINUE;
}

// sptwait property,spritenumber
int ONScripter::spritePropertyWaitCommand() {
	bool is_lsp2 = (script_h.isName("sptwait2"));

	int property = 0;
	for (unsigned int i = 0; i < dynamicSpritePropertyNames.size(); i++) {
		if (script_h.compareString(dynamicSpritePropertyNames.at(i))) {
			script_h.readName();
			property = i;
			break;
		}
	}

	// Read sprite number.
	int sprite_num        = script_h.readInt();
	AnimationInfo *sprite = is_lsp2 ? &sprite2_info[sprite_num] : &sprite_info[sprite_num];

	dynamicProperties.waitOnSpriteProperty(sprite, property);

	return RET_CONTINUE;
}

// spt property,spritenumber,value?,duration?,equation?
//	 for properties, see the SPRITE_PROPERTY_ enum or dynamicSpritePropertyNames.
//   for equations, see the MOTION_EQUATION_ enum
int ONScripter::spritePropertyCommand() {
	bool is_lsp2 = (script_h.isName("spt2") || script_h.isName("aspt2"));
	bool is_abs  = (script_h.isName("aspt") || script_h.isName("aspt2"));

	int property = 0;
	for (unsigned int i = 0; i < dynamicSpritePropertyNames.size(); i++) {
		if (script_h.compareString(dynamicSpritePropertyNames.at(i))) {
			script_h.readName();
			property = i;
			break;
		}
	}

	// Read sprite number.
	int sprite_num        = script_h.readInt();
	AnimationInfo *sprite = is_lsp2 ? &sprite2_info[sprite_num] : &sprite_info[sprite_num];

	int value    = 0;
	int duration = 0;
	int equation = MOTION_EQUATION_LINEAR;
	if (script_h.hasMoreArgs())
		value = script_h.readInt();
	if (script_h.hasMoreArgs())
		duration = script_h.readInt();
	if (script_h.hasMoreArgs())
		equation = script_h.readInt();

	dynamicProperties.addSpriteProperty(sprite, sprite_num, is_lsp2, is_abs, property, value, duration, equation);

	return RET_CONTINUE;
}

int ONScripter::snapLogCommand() {
	int scrollable_id = validSprite(script_h.readInt());

	AnimationInfo &ai                 = sprite_info[scrollable_id];
	AnimationInfo::ScrollableInfo &si = ai.scrollableInfo;
	if (!si.isSpecialScrollable) {
		errorAndExit("Not a special scrollable");
		return RET_CONTINUE;
	}

	unsigned int labelId;
	LabelInfo *label;
	bool force_current = false;
	bool instant_snap  = true;
	bool snap_top      = false;
	bool scroll_max    = false;

	if (script_h.hasMoreArgs()) {
		label = script_h.lookupLabel(script_h.readLabel() + 1);
	} else {
		label = current_label_info;
		if (!callStack.empty())
			label = callStack.front().label;
	}

	if (script_h.hasMoreArgs()) {
		force_current = script_h.readInt();
	}

	if (script_h.hasMoreArgs()) {
		instant_snap = script_h.readInt();
	}

	if (script_h.hasMoreArgs()) {
		snap_top = script_h.readInt();
	}

	if (script_h.hasMoreArgs()) {
		scroll_max = script_h.readInt();
	}

	if (si.layoutedElements == 0) {
		// Nothing to do
		return RET_CONTINUE;
	}

	StringTree &tree = dataTrees[si.elementTreeIndex];

	labelId = script_h.getLabelIndex(label);

	// If this is inefficient, it is because we are traversing a structure primarily intended for script access and GUI layout.
	// It's not the best structure for storing data for ONS to efficiently access.
	// Could introduce a second intermediate data structure. :)
	// TODO : Change to iterating over logState.logEntries instead...
	long curElem{-1};
	for (auto it = tree.insertionOrder.begin(); it != tree.insertionOrder.end(); ++it) {
		StringTree &t = tree[*it];
		if (t.has("log") && script_h.logState.logEntryIndexToLabelIndex(std::stoi(t["log"].value)) == labelId) {
			curElem = it - tree.insertionOrder.begin();
		}
	}

	// We found an element that is invisible onscreen, scroll to max.
	if (curElem != -1 && scroll_max && !script_h.logState.readLabels[labelId]) {
		curElem = -1;
	}

	if (curElem == -1) {
		if (scroll_max)
			snapScrollableByOffset(&sprite_info[scrollable_id], INT_MAX);
		return RET_CONTINUE;
	}

	if (script_h.logState.readLabels[labelId]) {
		snapScrollableToElement(&sprite_info[scrollable_id], curElem, snap_top ? AnimationInfo::ScrollSnap::TOP : AnimationInfo::ScrollSnap::BOTTOM, instant_snap);
	} else if (!force_current) {
		snapScrollableToElement(&sprite_info[scrollable_id], curElem - 1, snap_top ? AnimationInfo::ScrollSnap::TOP : AnimationInfo::ScrollSnap::BOTTOM, instant_snap);
	}

	return RET_CONTINUE;
}

int ONScripter::setLogCommand() {
	int treeNo      = validTree(script_h.readInt());
	const char *log = script_h.readStr();
	std::string res;
	bool jumpable = true;

	if (*log == '\0')
		res = dlgCtrl.textPart;
	else
		res = log;

	if (script_h.hasMoreArgs()) {
		res.insert(0, script_h.readStr());
	}

	if (script_h.hasMoreArgs()) {
		res.append(script_h.readStr());
	}

	if (script_h.hasMoreArgs()) {
		jumpable = script_h.readInt();
	}

	// First initialize this dialogue's log data correctly
	LabelInfo *label_info = current_label_info;
	if (!callStack.empty())
		label_info = callStack.front().label;
	unsigned int labelIndex = script_h.getLabelIndex(label_info);
	auto &data              = script_h.logState.dialogueData[labelIndex];
	data.text               = res;

	// Save voices
	data.voices = std::move(script_h.logState.tmpVoices);
	data.volume = script_h.logState.tmpVolume;
	script_h.logState.tmpVoices.clear();
	script_h.logState.tmpVolume = 100;

	data.jumpable = jumpable;

	// Add new entry to the log
	auto logEntryIndex              = script_h.logState.logEntries.size();
	std::string logEntryIndexString = std::to_string(logEntryIndex);
	LogEntry l;
	l.labelIndex       = labelIndex;
	l.choiceVectorSize = script_h.choiceState.acceptChoiceNextIndex; // Should be equal to the choice vector size except if we are superskipping
	script_h.logState.logEntries.push_back(l);

	// Now append the entry to the log tree for rendering
	std::deque<std::string> params;
	params.emplace_back(logEntryIndexString);
	params.emplace_back("log");
	dataTrees[treeNo].setValue(params, logEntryIndexString);

	return RET_CONTINUE;
}

// syntax
// scroll_exceeds %result,sprite
int ONScripter::scrollExceedsCommand() {
	bool lsp2 = script_h.isName("scroll_exceeds2");

	script_h.readVariable();
	script_h.pushVariable();

	int spriteId = validSprite(script_h.readInt());

	AnimationInfo &ai = lsp2 ? sprite2_info[spriteId] : sprite_info[spriteId];

	if (ai.exists && ai.scrollable.h && ai.pos.h > ai.scrollable.h)
		script_h.setInt(&script_h.pushed_variable, 1);
	else
		script_h.setInt(&script_h.pushed_variable, 0);

	return RET_CONTINUE;
}

// syntax
// scrollable sprite,tree,x,y,w,h
//
int ONScripter::scrollableSpriteCommand() {
	int sprNo  = validSprite(script_h.readInt());
	int treeNo = validTree(script_h.readInt());

	GPU_Rect newpos;
	newpos.x = script_h.readInt();
	newpos.y = script_h.readInt();
	newpos.w = script_h.readInt();
	newpos.h = script_h.readInt();

	backupState(&sprite_info[sprNo]);
	if (sprite_info[sprNo].exists && sprite_info[sprNo].visible) {
		dirtySpriteRect(sprNo, false);
	}
	sprite_info[sprNo].remove();

	sprite_info[sprNo].num_of_cells = 1;
	sprite_info[sprNo].visible      = false;
	sprite_info[sprNo].orig_pos = sprite_info[sprNo].pos = newpos;
	//sprite_info[sprNo].fill(64,128,32,255);
	sprite_info[sprNo].scrollableInfo.isSpecialScrollable = true;
	sprite_info[sprNo].scrollableInfo.elementTreeIndex    = treeNo;
	sprite_info[sprNo].scrollable.h                       = newpos.h;
	sprite_info[sprNo].exists                             = true;

	return RET_CONTINUE;
}

// scrollable_scroll scrollableId, rows (negative is up, positive is down)
int ONScripter::scrollableScrollCommand() {
	int scrollableId  = validSprite(script_h.readInt());
	AnimationInfo &ai = sprite_info[scrollableId];
	if (!ai.scrollableInfo.isSpecialScrollable)
		errorAndExit("scrollable_scroll called on something that's not a scrollable");
	int rows = script_h.readInt();
	snapScrollableByOffset(&ai, rows);
	return RET_CONTINUE;
}

// scrollable_get_hovered_elem returnInt,scrollableId
// Returns the lookup key for the currently hovered element on the specified scrollable.
// (This is always defined even if the mouse cursor is not currently over an element. But btnwait will not tell script of mouseclicks unless it is.)
int ONScripter::scrollableGetHoveredElementCommand() {
	script_h.readVariable();
	script_h.pushVariable();

	int scrollableId  = validSprite(script_h.readInt());
	AnimationInfo &ai = sprite_info[scrollableId];
	if (!ai.scrollableInfo.isSpecialScrollable)
		errorAndExit("scrollable_get_hovered_elem called on something that's not a scrollable");

	script_h.setInt(&script_h.pushed_variable, static_cast<int32_t>(ai.scrollableInfo.hoveredElement));

	return RET_CONTINUE;
}

// scrollable_display scrollableId
int ONScripter::scrollableDisplayCommand() {
	int sprNo = validSprite(script_h.readInt());
	dirtySpriteRect(sprNo, false);
	layoutSpecialScrollable(&sprite_info[sprNo]);
	sprite_info[sprNo].visible = true;
	return RET_CONTINUE;
}

// scrollable_cfg configItemName,scrollableId,(...name-specific parameters)
int ONScripter::scrollableConfigCommand() {
	// Get config property name
	bool match   = true;
	bool divider = false, firstmargin = false, lastmargin = false,
	     cols = false, colgap = false,
	     elembg = false, elemwidth = false, elemheight = false,
	     textmarginwidth = false, textmarginleft = false, textmarginright = false,
	     textmargintop = false, scrollbar = false, tightfit = false,
	     hovertext = false, normaltext = false, mousectrl = false;
	if (script_h.compareString("divider"))
		divider = true;
	else if (script_h.compareString("scrollbar"))
		scrollbar = true;
	else if (script_h.compareString("firstmargin"))
		firstmargin = true;
	else if (script_h.compareString("lastmargin"))
		lastmargin = true;
	else if (script_h.compareString("textmarginwidth"))
		textmarginwidth = true;
	else if (script_h.compareString("textmarginleft"))
		textmarginleft = true;
	else if (script_h.compareString("textmarginright"))
		textmarginright = true;
	else if (script_h.compareString("textmargintop"))
		textmargintop = true;
	else if (script_h.compareString("cols"))
		cols = true;
	else if (script_h.compareString("colgap"))
		colgap = true;
	else if (script_h.compareString("elembg"))
		elembg = true;
	else if (script_h.compareString("elemwidth"))
		elemwidth = true;
	else if (script_h.compareString("elemheight"))
		elemheight = true;
	else if (script_h.compareString("tightfit"))
		tightfit = true;
	else if (script_h.compareString("hovertext"))
		hovertext = true;
	else if (script_h.compareString("normaltext"))
		normaltext = true;
	else if (script_h.compareString("mousectrl"))
		mousectrl = true;
	else
		match = false;
	if (match)
		script_h.readName();

	// Get scrollable spriteID
	int sprNo                         = validSprite(script_h.readInt());
	AnimationInfo::ScrollableInfo &si = sprite_info[sprNo].scrollableInfo;

	if (divider || elembg || scrollbar) { // Specify another spriteID to be used within the scrollable
		int sprite = validSprite(script_h.readInt());
		if (divider)
			si.divider = sprite2_info[sprite].gpu_image ? &sprite2_info[sprite] : &sprite_info[sprite];
		if (elembg)
			si.elementBackground = sprite2_info[sprite].gpu_image ? &sprite2_info[sprite] : &sprite_info[sprite];
		if (scrollbar) {
			si.scrollbar       = sprite2_info[sprite].gpu_image ? &sprite2_info[sprite] : &sprite_info[sprite];
			si.scrollbarTop    = script_h.readInt();
			si.scrollbarHeight = script_h.readInt() - si.scrollbarTop;
		}
	} else if (firstmargin)
		si.firstMargin = script_h.readInt();
	else if (lastmargin)
		si.lastMargin = script_h.readInt();
	else if (cols)
		si.columns = script_h.readInt();
	else if (colgap)
		si.columnGap = script_h.readInt();
	else if (elemwidth)
		si.elementWidth = script_h.readInt();
	else if (elemheight)
		si.elementHeight = script_h.readInt();
	else if (textmarginwidth)
		si.textMarginLeft = si.textMarginRight = script_h.readInt();
	else if (textmarginleft)
		si.textMarginLeft = script_h.readInt();
	else if (textmarginright)
		si.textMarginRight = script_h.readInt();
	else if (textmargintop)
		si.textMarginTop = script_h.readInt();
	else if (tightfit)
		si.tightlyFit = script_h.readInt();
	else if (mousectrl)
		si.respondsToMouseOver = script_h.readInt();
	else if (hovertext || normaltext) {
		bool is_color{false};
		const char *buf = script_h.readColor(&is_color);
		if (!is_color)
			errorAndExit("Invalid colour");
		readColor(&(hovertext ? si.hoverMultiplier : si.normalMultipler), buf);
		(hovertext ? si.hoverGradients : si.normalGradients) = script_h.readInt();
	} else
		sendToLog(LogLevel::Error, "scrollableConfig: No configname match...\n");

	return RET_CONTINUE;
}

int ONScripter::saveresetCommand() {
	FileIO::removeDir(script_h.save_path);
	freearr(&script_h.save_path);
	relaunchCommand();
}

// gptwait property
int ONScripter::globalPropertyWaitCommand() {
	int property = 0;
	for (unsigned int i = 0; i < dynamicGlobalPropertyNames.size(); i++) {
		if (script_h.compareString(dynamicGlobalPropertyNames.at(i))) {
			script_h.readName();
			property = i;
			break;
		}
	}
	dynamicProperties.waitOnGlobalProperty(property);
	/*if (property==GLOBAL_PROPERTY_ONION_ALPHA) {
		// additionally wait for cooldown to finish
		event_mode = IDLE_EVENT_MODE;
		while (onionAlphaCooldown > 10) {
	 waitEvent(0);
		}
	 }*/
	return RET_CONTINUE;
}

// gpt property,value?,duration?,equation?
//	 for properties, see the GLOBAL_PROPERTY_ enum or dynamicGlobalPropertyNames.
//   for equations, see the MOTION_EQUATION_ enum
int ONScripter::globalPropertyCommand() {

	bool is_abs = (script_h.isName("agpt"));

	int property = 0;
	for (unsigned int i = 0; i < dynamicGlobalPropertyNames.size(); i++) {
		if (script_h.compareString(dynamicGlobalPropertyNames.at(i))) {
			script_h.readName();
			property = i;
			break;
		}
	}

	int value     = 0;
	int duration  = 0;
	int equation  = MOTION_EQUATION_LINEAR;
	bool override = false;
	if (script_h.hasMoreArgs())
		value = script_h.readInt();
	if (script_h.hasMoreArgs())
		duration = script_h.readInt();
	if (script_h.hasMoreArgs())
		equation = script_h.readInt();
	if (script_h.hasMoreArgs())
		override = script_h.readInt() == 1;

	if (value != 0 && duration != 0 && reduce_motion &&
		(property == GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE || property == GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE)) {
		dynamicProperties.addGlobalProperty(is_abs, property, value, 0, equation, override);
		value = 0;
	}

	dynamicProperties.addGlobalProperty(is_abs, property, value, duration, equation, override);

	return RET_CONTINUE;
}

int ONScripter::getvideovolCommand() {
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, video_volume);
	return RET_CONTINUE;
}

int ONScripter::superSkipCommand() {
	script_h.readVariable();

	superSkipData.dst_var = script_h.current_variable.var_no;

	std::string src(script_h.readLabel());
	superSkipData.dst_lbl = script_h.readLabel();

	if (script_h.choiceState.acceptChoiceVectorSize == -1) {
		// If it's not set, assume we can superskip to the end of the whole choice vector.
		script_h.choiceState.acceptChoiceVectorSize = static_cast<int32_t>(script_h.choiceState.choiceVector.size());
	}

	enum SuperSkipFlags {
		SUPERSKIP_FLAG_NONE          = 0,
		SUPERSKIP_FLAG_DEFER_LOADING = 1
	};

	int flags{SUPERSKIP_FLAG_NONE};
	if (script_h.hasMoreArgs()) {
		// Optional 4th parameter: flags
		flags = script_h.readInt();
	}
	deferredLoadingEnabled = flags & SUPERSKIP_FLAG_DEFER_LOADING;

	// Note: State saved before the RET_CONTINUE (watch for possible errors?)
	superSkipData.callerState = script_h.getScriptStateData();

	// Our stack should be pristine at a time of super skip
	if (!callStack.empty())
		callStack.clear();

	auto addr = script_h.lookupLabel(superSkipData.dst_lbl.c_str() + 1)->start_address;

	setCurrentLabel(src.c_str() + 1);

	if (current_label_info->start_address > addr) {
		errorAndExit("Cannot sskip backwards");
	} else if (current_label_info->start_address == addr) {
		tryEndSuperSkip(true);
	} else {
		//FIXME: add all the necessary cases
		skip_mode                 = SKIP_NORMAL | SKIP_SUPERSKIP;
		internal_slowdown_counter = 0;
		textgosub_clickstr_state  = CLICK_NONE;
		page_enter_status         = 0;
	}
	return RET_CONTINUE;
}

int ONScripter::superSkipUnsetCommand() {
	skip_mode &= ~(SKIP_SUPERSKIP | SKIP_NORMAL);

	for (AnimationInfo *s : sprites(SPRITE_LSP2)) {
		if (s->deferredLoading) {
			setupAnimationInfo(s);
			postSetupAnimationInfo(s);
		}
	}

	repaintCommand();

	deferredLoadingEnabled = false;

	return RET_CONTINUE;
}

int ONScripter::subtitleStopCommand() {
	getLayer<SubtitleLayer>(script_h.readInt())->stopPlayback();
	return RET_CONTINUE;
}

int ONScripter::subtitleLoadCommand() {
	unsigned int id   = script_h.readInt();
	std::string buf   = script_h.readFilePath();
	unsigned int rate = script_h.readInt();

	if (rate == 0 || rate >= 1000)
		errorAndExit("ssa_load: incorrect rate");

	getLayer<SubtitleLayer>(id)->loadSubtitles(buf.c_str(), rate);
	return RET_CONTINUE;
}

int ONScripter::subtitleFontCommand() {
	unsigned int id   = script_h.readInt();
	unsigned int font = script_h.readInt();

	if (font >= 10)
		errorAndExit("ssa_font: incorrect font");

	getLayer<SubtitleLayer>(id)->setFont(font);
	return RET_CONTINUE;
}

// spritesetptwait property,spritesetnumber
int ONScripter::spritesetPropertyWaitCommand() {

	int property = 0;
	for (unsigned int i = 0; i < dynamicSpritesetPropertyNames.size(); i++) {
		if (script_h.compareString(dynamicSpritesetPropertyNames.at(i))) {
			script_h.readName();
			property = i;
			break;
		}
	}
	// Read spriteset number.
	int spriteset_num = script_h.readInt();

	dynamicProperties.waitOnSpritesetProperty(spriteset_num, property);

	return RET_CONTINUE;
}

// spritesetpt property,spritesetnumber,value?,duration?,equation?
int ONScripter::spritesetPropertyCommand() {

	bool is_abs = (script_h.isName("aspritesetpt"));

	int property = 0;
	for (unsigned int i = 0; i < dynamicSpritesetPropertyNames.size(); i++) {
		if (script_h.compareString(dynamicSpritesetPropertyNames.at(i))) {
			script_h.readName();
			property = i;
			break;
		}
	}

	// Read spriteset number.
	int spriteset_num = script_h.readInt();

	int value    = 0;
	int duration = 0;
	int equation = MOTION_EQUATION_LINEAR;
	if (script_h.hasMoreArgs())
		value = script_h.readInt();
	if (script_h.hasMoreArgs())
		duration = script_h.readInt();
	if (script_h.hasMoreArgs())
		equation = script_h.readInt();

	dynamicProperties.addSpritesetProperty(spriteset_num, is_abs, property, value, duration, equation);

	return RET_CONTINUE;
}

int ONScripter::splitCommand() {
	script_h.readStr();
	const char *save_buf = script_h.saveStringBuffer();

	char delimiter = script_h.readStr()[0];

	char *token = new char[std::strlen(save_buf) + 1];
	while (script_h.hasMoreArgs()) {

		unsigned int c = 0;
		while (save_buf[c] != delimiter && save_buf[c] != '\0') c++;
		std::memcpy(token, save_buf, c);
		token[c] = '\0';

		script_h.readVariable();
		if (script_h.current_variable.type & VariableInfo::TypeInt ||
		    script_h.current_variable.type & VariableInfo::TypeArray) {
			script_h.setInt(&script_h.current_variable, static_cast<int>(std::strtol(token, nullptr, 0)));
		} else if (script_h.current_variable.type & VariableInfo::TypeStr) {
			script_h.setStr(&script_h.getVariableData(script_h.current_variable.var_no).str, token);
		}

		save_buf += c;
		if (save_buf[0] != '\0')
			save_buf++;
	}
	delete[] token;

	return RET_CONTINUE;
}

int ONScripter::smartquotesCommand() {
	uint32_t codepoint = 0;
	uint32_t state     = 0;
	const char *buf;
	uint32_t params[5];
	for (int i = 0; i < 4; i++) {
		buf = script_h.readStr();
		while (decodeUTF8(&state, &codepoint, *buf)) buf++;
		params[i] = codepoint;
	}
	if (script_h.hasMoreArgs()) {
		buf = script_h.readStr();
		while (decodeUTF8(&state, &codepoint, *buf)) buf++;
		sentence_font.smart_single_quotes_represented_by_dumb_double = true;
		sentence_font.setSmartQuotes(params[0], params[1], params[2], params[3], codepoint);
		name_font.smart_single_quotes_represented_by_dumb_double = true;
		name_font.setSmartQuotes(params[0], params[1], params[2], params[3], codepoint);
	} else {
		sentence_font.smart_single_quotes_represented_by_dumb_double = false;
		sentence_font.setSmartQuotes(params[0], params[1], params[2], params[3], 0);
		name_font.smart_single_quotes_represented_by_dumb_double = false;
		name_font.setSmartQuotes(params[0], params[1], params[2], params[3], 0);
	}
	return RET_CONTINUE;
}

// skip_unread {0,1} -- determines whether unread dialogues can be skipped.
int ONScripter::skipUnreadCommand() {
	skip_unread = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::skipModeCommand() {
	if (skip_mode & SKIP_SUPERSKIP)
		return RET_CONTINUE;

	skip_enabled = script_h.isName("skip_enable");

	if (!skip_enabled) {
		keyState.ctrl         = 0;
		skip_mode             = 0;
		eventCallbackRequired = true;
	}

	return RET_CONTINUE;
}

int ONScripter::setVoiceWaitMulCommand() {
	voicewait_multiplier = parsefloat(script_h.readStr());
	return RET_CONTINUE;
}

// sprite number, x, y (as strings)
int ONScripter::setScaleCenterCommand() {
	int sprite_num    = script_h.readInt();
	AnimationInfo &si = sprite2_info[sprite_num];

	//backupState(&si);
	dirtySpriteRect(&si);

	si.has_scale_center = true;
	si.scale_center.x   = script_h.readInt();
	si.scale_center.y   = script_h.readInt();
	//sendToLog(LogLevel::Info, "scale center x,y: %f,%f\n", si.scale_center.x, si.scale_center.y);

	UpdateAnimPosXY(&si);
	si.calcAffineMatrix(window.script_width, window.script_height);
	dirtySpriteRect(&si);

	return RET_CONTINUE;
}

// sprite number, left, top (as strings)
// LSP2-only command.
int ONScripter::setHotspotCommand() {
	int sprite_num    = script_h.readInt();
	AnimationInfo &si = sprite2_info[sprite_num];

	backupState(&si);
	dirtySpriteRect(&si);

	si.has_hotspot = true;
	si.hotspot.x   = parsefloat(script_h.readStr());
	si.hotspot.y   = parsefloat(script_h.readStr());

	//sendToLog(LogLevel::Info, "hotspot x,y: %f,%f\n", si.hotspot.x, si.hotspot.y);

	UpdateAnimPosXY(&si);
	si.calcAffineMatrix(window.script_width, window.script_height);
	dirtySpriteRect(&si);

	return RET_CONTINUE;
}

int ONScripter::setwindowDynamicCommand() {
	if (script_h.isName("setwindowd_off")) {
		wndCtrl.usingDynamicTextWindow = false;
	} else {
		//sentence_font.is_transparent = false; // bug?
		backupState(&sentence_font_info);
		sentence_font_info.setImageName(script_h.readStr());
		parseTaggedString(&sentence_font_info);
		setupAnimationInfo(&sentence_font_info);
		//sentence_font_info.blending_mode = BlendModeId::NORMAL; // unnecessary?

		wndCtrl.usingDynamicTextWindow = true;
		wndCtrl.setWindow(sentence_font_info.pos);
	}

	return RET_CONTINUE;
}

// Syntax x y w h hext varea, where hext is the column number to stretch, and varea is the vertical size of the area in which text may be rendered
int ONScripter::setwindowDynamicMainRegionCommand() {
	wndCtrl.mainRegionDimensions.x = script_h.readInt();
	wndCtrl.mainRegionDimensions.y = script_h.readInt();
	wndCtrl.mainRegionDimensions.w = script_h.readInt();
	wndCtrl.mainRegionDimensions.h = script_h.readInt();
	wndCtrl.mainRegionExtensionCol = script_h.readInt();
	return RET_CONTINUE;
}

// Syntax x y w h hext, where hext is the column number to stretch
int ONScripter::setwindowDynamicNoNameRegionCommand() {
	wndCtrl.noNameRegionDimensions.x = script_h.readInt();
	wndCtrl.noNameRegionDimensions.y = script_h.readInt();
	wndCtrl.noNameRegionDimensions.w = script_h.readInt();
	wndCtrl.noNameRegionDimensions.h = script_h.readInt();
	wndCtrl.noNameRegionExtensionCol = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::setwindowDynamicNameRegionCommand() {
	wndCtrl.nameRegionDimensions.x = script_h.readInt();
	wndCtrl.nameRegionDimensions.y = script_h.readInt();
	wndCtrl.nameRegionDimensions.w = script_h.readInt();
	wndCtrl.nameRegionDimensions.h = script_h.readInt();
	wndCtrl.nameBoxExtensionCol    = script_h.readInt();
	wndCtrl.nameBoxDividerCol      = script_h.readInt();
	wndCtrl.nameRegionExtensionCol = script_h.readInt();
	wndCtrl.nameBoxExtensionRow    = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::setwindowDynamicDimensionsCommand() {
	int x = script_h.readInt();
	int y = script_h.readInt();
	int w = script_h.readInt();
	int h = script_h.readInt();

	backupState(&sentence_font_info);

	sentence_font_info.orig_pos.x = x;
	sentence_font_info.orig_pos.y = y;
	sentence_font_info.orig_pos.w = w;
	sentence_font_info.orig_pos.h = h;

	UpdateAnimPosXY(&sentence_font_info);
	UpdateAnimPosWH(&sentence_font_info);

	wndCtrl.setWindow(sentence_font_info.pos);

	sentence_font_info.exists = true;

	addTextWindowClip(dirty_rect_hud);
	addTextWindowClip(before_dirty_rect_hud);

	sentence_font.top_xy[0]                = x;
	sentence_font.top_xy[1]                = y;
	sentence_font.changeStyle().wrap_limit = w;

	lookbackflushCommand();
	page_enter_status = 0;
	display_mode      = DISPLAY_MODE_NORMAL;

	commitVisualState();
	flush(refreshMode(), nullptr, nullptr);

	return RET_CONTINUE;
}

int ONScripter::setwindowDynamicPaddingCommand() {
	// css order
	wndCtrl.mainRegionPadding.top    = script_h.readInt();
	wndCtrl.mainRegionPadding.right  = script_h.readInt();
	wndCtrl.mainRegionPadding.bottom = script_h.readInt();
	wndCtrl.mainRegionPadding.left   = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::setwindowDynamicNamePaddingCommand() {
	// css order
	wndCtrl.nameBoxPadding.top    = script_h.readInt();
	wndCtrl.nameBoxPadding.right  = script_h.readInt();
	wndCtrl.nameBoxPadding.bottom = script_h.readInt();
	wndCtrl.nameBoxPadding.left   = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::setwindow4Command() {
	bool setwindow4 = !script_h.isName("setwindow4name");
	Fontinfo &fi    = setwindow4 ? sentence_font : name_font;

	fi.top_xy[0]            = script_h.readInt();
	fi.top_xy[1]            = script_h.readInt();
	auto &style             = fi.changeStyle();
	style.font_size         = script_h.readInt();
	style.wrap_limit        = script_h.readInt();
	style.character_spacing = script_h.readInt();
	style.line_height       = script_h.readInt();

	if (setwindow4)
		text_display_speed = script_h.readInt(); //using a nouveau parameter now

	style.is_bold      = script_h.readInt();
	style.is_italic    = script_h.readInt();
	style.is_underline = script_h.readInt();
	style.is_shadow    = script_h.readInt();
	style.is_border    = script_h.readInt();

	if (setwindow4 && !(script_h.hasMoreArgs())) {
		errorAndExit("Improper setwindow4 usage, required params are missing");
	} else if (setwindow4) {
		bool is_color = false;
		const char *buf;
		if (allow_color_type_only) {
			buf = script_h.readColor(&is_color);
			if (!is_color)
				buf = script_h.readStr();
		} else {
			buf = script_h.readStr();
			if (buf[0] == '#')
				is_color = true;
		}

		backupState(&sentence_font_info);

		sentence_font_info.deleteImage();

		if (is_color) {
			sentence_font_info.stale_image = true;
			sentence_font.is_transparent   = true;
			readColor(&sentence_font.window_color, buf);

			sentence_font_info.orig_pos.x = script_h.readInt();
			sentence_font_info.orig_pos.y = script_h.readInt();
			sentence_font_info.orig_pos.w = script_h.readInt() - sentence_font_info.orig_pos.x;
			sentence_font_info.orig_pos.h = script_h.readInt() - sentence_font_info.orig_pos.y;
			UpdateAnimPosXY(&sentence_font_info);
			UpdateAnimPosWH(&sentence_font_info);

			if (!sentence_font_info.gpu_image)
				sentence_font_info.gpu_image = gpu.createImage(sentence_font_info.pos.w, sentence_font_info.pos.h, 4);
			GPU_GetTarget(sentence_font_info.gpu_image);
			gpu.clearWholeTarget(sentence_font_info.gpu_image->target,
			                     sentence_font.window_color.x, sentence_font.window_color.y, sentence_font.window_color.z, 0xFF);
			gpu.multiplyAlpha(sentence_font_info.gpu_image);
			sentence_font_info.trans_mode    = AnimationInfo::TRANS_COPY;
			sentence_font_info.blending_mode = BlendModeId::MUL;
			sentence_font_info.trans         = 255;
		} else {
			sentence_font.is_transparent = false;
			sentence_font_info.setImageName(buf);
			parseTaggedString(&sentence_font_info);
			setupAnimationInfo(&sentence_font_info);

			sentence_font_info.orig_pos.x = script_h.readInt();
			sentence_font_info.orig_pos.y = script_h.readInt();
			UpdateAnimPosXY(&sentence_font_info);

			sentence_font.window_color       = {0xff, 0xff, 0xff};
			sentence_font_info.blending_mode = BlendModeId::NORMAL;
			if (script_h.hasMoreArgs())
				sentence_font_info.trans = script_h.readInt();
			else
				sentence_font_info.trans = 255;
		}
		sentence_font_info.exists = true;
	}

	if (script_h.hasMoreArgs())
		fi.borderPadding = script_h.readInt();

	if (setwindow4) {
		dirty_rect_hud.add(sentence_font_info.pos);

		lookbackflushCommand();
		page_enter_status = 0;
		display_mode      = DISPLAY_MODE_NORMAL;

		commitVisualState();
		flush(refreshMode(), nullptr, &sentence_font_info.pos);
	}

	return RET_CONTINUE;
}

int ONScripter::setFpsCommand() {
	game_fps = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::screenFlipCommand() {
	should_flip = script_h.readInt();
	if (should_flip)
		repaintCommand();
	return RET_CONTINUE;
}

int ONScripter::scriptMuteCommand() {
	bool request = script_h.readInt();

	if (request && volume_on_flag) {
		script_mute    = true;
		volume_on_flag = false;
		setVolumeMute(script_mute);
	} else if (!request && script_mute) {
		script_mute    = false;
		volume_on_flag = true;
		setVolumeMute(script_mute);
	}

	return RET_CONTINUE;
}

int ONScripter::rumbleCommand() {
	float strength    = script_h.readInt() / 100.0;
	int length        = script_h.readInt();
	bool didSomething = joyCtrl.rumble(strength, length);

	if (!didSomething) {
		sendToLog(LogLevel::Warn, "Unable to rumble %f,%d\n", strength, length);
	}

	return RET_CONTINUE;
}

int ONScripter::relaunchCommand() {
	sendToLog(LogLevel::Info, "Relaunching...\n");
	cleanLabel();
	ctrl.deinit();

	std::vector<char *> newArgv;
	newArgv.emplace_back(FileIO::safePath(argv[0], false, true));

	bool hasRoot = false;

	for (int i = 1; i < argc; i++) {
		newArgv.emplace_back(copystr(argv[i]));

		if ((equalstr("--root", argv[i]) || equalstr("--tmp-root", argv[i])) && i + 1 < argc) {
			hasRoot = true;
			i++;
			newArgv.emplace_back(FileIO::safePath(argv[i], true, true));
		} else if (equalstr("--save", argv[i]) && i + 1 < argc) {
			newArgv.emplace_back(FileIO::safePath(argv[i], true, true));
		}
	}

	if (!hasRoot) {
		newArgv.emplace_back(copystr("--tmp-root"));
		newArgv.emplace_back(FileIO::safePath(script_path, true, true));
	}

	newArgv.emplace_back(nullptr);

	if (!FileIO::restartApp(newArgv)) {
		window.showSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "ONScripter-RU", "Please open the application once it closes!");
		sendToLog(LogLevel::Error, "Failed to run: %s\n", newArgv[0]);
	}

	for (auto &ptr : newArgv)
		freearr(&ptr);

	ctrl.quit(0);
}

int ONScripter::regexDefineCommand() {
	unsigned index = script_h.readInt();

	if (index >= regExps.size()) {
		regExps.resize(index + 1);
	}

#ifdef USE_STD_REGEX
	regExps[index] = std::regex(script_h.readStr(), std::regex_constants::optimize);
#else
	regExps[index].first = std::unique_ptr<char[]>(copystr(script_h.readStr()));
	slre_compile(regExps[index].first.get(),
	             static_cast<int>(std::strlen(regExps[index].first.get())),
	             0, &regExps[index].second);
#endif

	return RET_CONTINUE;
}

int ONScripter::quakeApiCommand() {
	CameraMove m;

	bool old_api{false};
	bool ongoing{false};

	if (script_h.isName("quakex_t")) {
		m.moveType = CameraMove::Type::X;
	} else if (script_h.isName("quakey_t")) {
		m.moveType = CameraMove::Type::Y;
	} else if (script_h.isName("quake_t")) {
		switch (script_h.readInt()) {
			case 1:
				m.moveType = CameraMove::Type::X;
				break;
			default:
			case 2:
				m.moveType = CameraMove::Type::Y;
				break;
		}
		ongoing = true;
	} else if (script_h.isName("quakey")) {
		m.moveType = CameraMove::Type::Y;
		old_api    = true;
	} else if (script_h.isName("quakex")) {
		m.moveType = CameraMove::Type::X;
		old_api    = true;
	} else if (script_h.isName("quake")) {
		//This is what NScripter does at least
		if (std::rand() % 2)
			m.moveType = CameraMove::Type::X;
		else
			m.moveType = CameraMove::Type::Y;
		old_api = true;
	}

	int duration = 0;
	if (old_api || !ongoing) {
		m.setAmplitude(script_h.readInt());
		duration = script_h.readInt();
	} else {
		m.setAmplitude(script_h.readInt());
		m.cycleTime = script_h.readInt();
	}

	if (m.getAmplitude() > camera.center_pos.x && m.moveType == CameraMove::Type::X) {
		m.setAmplitude(camera.center_pos.x);
	} else if (m.getAmplitude() > camera.center_pos.y && m.moveType == CameraMove::Type::Y) {
		m.setAmplitude(camera.center_pos.y);
	}

	if (m.moveType == CameraMove::Type::X)
		camera.x_move = m;
	else
		camera.y_move = m;

	if (duration) {
		if (m.moveType == CameraMove::Type::X) {
			dynamicProperties.addGlobalProperty(true, GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE, 0, duration, MOTION_EQUATION_LINEAR, true);
			if (old_api) {
				dynamicProperties.waitOnGlobalProperty(GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE);
			}
		} else {
			dynamicProperties.addGlobalProperty(true, GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE, 0, duration, MOTION_EQUATION_LINEAR, true);
			if (old_api) {
				dynamicProperties.waitOnGlobalProperty(GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE);
			}
		}
	}

	//done
	return RET_CONTINUE;
}

int ONScripter::quakeendCommand() {
	if (!camera.isMoving())
		return RET_CONTINUE;

	dynamicProperties.addGlobalProperty(true, GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE, 0, 166, MOTION_EQUATION_LINEAR, true);
	dynamicProperties.addGlobalProperty(true, GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE, 0, 166, MOTION_EQUATION_LINEAR, true);
	dynamicProperties.waitOnGlobalProperty(GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE);
	dynamicProperties.waitOnGlobalProperty(GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE);

	return RET_CONTINUE;
}

int ONScripter::textAtlasCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("textatlas: not in the define section");

	use_text_atlas = true;
	glyphCache.resize(GLYPH_ATLAS_W * GLYPH_ATLAS_H);

	return RET_CONTINUE;
}

int ONScripter::profilestopCommand() {
	sendToLog(LogLevel::Info, "Profiling stop requested");

#ifdef DROID
	profileStop();
#endif

	return RET_CONTINUE;
}

int ONScripter::profilestartCommand() {
	sendToLog(LogLevel::Info, "Profiling start requested");

#ifdef DROID
	profileStart(script_h.readInt());
#else
	script_h.readInt();
#endif

	return RET_CONTINUE;
}

int ONScripter::presetdefineCommand() {
	int num = script_h.readInt();

	if (num < 0) {
		errorAndExit("preset number must in >= 0 range");
		return RET_CONTINUE;
	}

	unsigned int font_number = script_h.readInt();

	if (!sentence_font.isFontLoaded(font_number)) {
		sendToLog(LogLevel::Error, "WARN: preset %d requested font %u, which is missing, using default font!\n", num, font_number);
		font_number = 0;
	}

	presets[num].font_number = font_number;
	presets[num].preset_id   = num;
	presets[num].font_size   = script_h.readInt();

	bool is_colour;
	const char *buf;

	buf = script_h.readColor(&is_colour);
	if (!is_colour)
		return RET_CONTINUE;
	readColor(&presets[num].color, buf);

	presets[num].is_bold      = script_h.readInt();
	presets[num].is_italic    = script_h.readInt();
	presets[num].is_underline = script_h.readInt();

	presets[num].is_border    = script_h.readInt();
	presets[num].border_width = script_h.readInt();
	if (presets[num].border_width != -1)
		presets[num].border_width *= 25;
	buf = script_h.readColor(&is_colour);
	if (!is_colour)
		return RET_CONTINUE;
	readColor(&presets[num].border_color, buf);

	presets[num].is_shadow          = script_h.readInt();
	presets[num].shadow_distance[0] = script_h.readInt();
	presets[num].shadow_distance[1] = script_h.readInt();
	buf                             = script_h.readColor(&is_colour);
	if (!is_colour)
		return RET_CONTINUE;
	readColor(&presets[num].shadow_color, buf);

	presets[num].character_spacing = script_h.readInt();

	if (script_h.hasMoreArgs()) {
		presets[num].line_height = script_h.readInt();
		if (script_h.hasMoreArgs()) {
			presets[num].wrap_limit = script_h.readInt();
		}
	}

	return RET_CONTINUE;
}

int ONScripter::pastLogCommand() {
	script_h.readName();
	script_h.readVariable();
	script_h.pushVariable();

	int scrollable_id = validSprite(script_h.readInt());
	AnimationInfo &ai = sprite_info[scrollable_id];

	if (!ai.scrollableInfo.isSpecialScrollable) {
		errorAndExit("Not a special scrollable");
		return RET_CONTINUE;
	}

	auto &tree = dataTrees[ai.scrollableInfo.elementTreeIndex];
	auto &si   = ai.scrollableInfo;

	auto first       = getScrollableElementsVisibleAt(&si, tree, ai.scrollable.y);
	const char *addr = nullptr;

	// TODO Refactor. This looks like copy and pasted code.
	// Seems to set addr to the start_address of the bottom currently displayed element.
	// Should instead call a function to get the bottom currently displayed element, then look up and retrieve its address.
	for (auto it = first; it != tree.insertionOrder.end(); ++it) {
		StringTree &elem = tree[*it];

		LabelInfo *log_label = script_h.getLabelByIndex(script_h.logState.logEntryIndexToLabelIndex(std::stoi(elem["log"].value)));
		addr                 = log_label->start_address;

		bool read = script_h.logState.logEntryIndexToIsRead(std::stoi(elem["log"].value));
		if (!read)
			break;

		float w = si.elementWidth ? si.elementWidth : ai.pos.w;
		float h = si.elementHeight;

		GPU_Rect elemRect{0, 0, w, h};
		setRectForScrollableElement(&elem, elemRect);
		if (elemRect.y - ai.scrollable.y > ai.pos.h - ai.scrollableInfo.lastMargin) {
			// we're off the bottom of the visible area, break
			break;
		}
	}

	bool found = false;
	while (script_h.hasMoreArgs()) {
		const char *lbl = script_h.readLabel();
		if (!found && addr) {
			LabelInfo *label = script_h.lookupLabel(lbl + 1);
			if (label->start_address <= addr) {
				script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, lbl);
				found = true;
			}
		}
	}

	if (!found) {
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, "");
	}

	return RET_CONTINUE;
}

int ONScripter::pastLabelCommand() {
	bool label2 = script_h.isName("past_label2");

	script_h.readName();
	script_h.readVariable();
	script_h.pushVariable();

	LabelInfo *label = script_h.lookupLabel(script_h.readLabel() + 1);

	if (label2) {
		//past_label2 $res,"*lookup_label","*check1","*check2"(, ..)
		//Sets $res to the first label before the lookup_label or to ""
		LabelInfo *next = nullptr;
		do {
			next = script_h.lookupLabel(script_h.readLabel() + 1);

			if (next->start_address <= label->start_address)
				break;
		} while (script_h.hasMoreArgs());

		while (script_h.hasMoreArgs()) script_h.readStr();

		size_t len = std::strlen(next->name);
		char *buf  = new char[len + 2];
		buf[0]     = '*';
		copystr(buf + 1, next->name, len + 1);
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, buf);
		delete[] buf;
	} else {
		//past_label %res,"*label"
		//Sets %res to 1 if top-level script has gone farther than *label start address

		const char *addr = script_h.getNext();

		if (!callStack.empty()) {
			addr = callStack.front().next_script;
		}

		if (label->start_address <= addr)
			script_h.setInt(&script_h.pushed_variable, 1);
		else
			script_h.setInt(&script_h.pushed_variable, 0);
	}

	return RET_CONTINUE;
}

int ONScripter::operateConfigCommand() {
	//operate_config [u_]read,$dst,"property"
	//operate_config [u_]write,$val,"property"
	//operate_config [u_]unset,"property"
	//operate_config [u_]save

	std::string op = script_h.readName();

	auto &map = op.substr(0, 2) == "u_" ? (static_cast<void>(op.replace(0, 2, "")), user_cfg_options) : ons_cfg_options;

	auto translate = [](std::string propertyName) {
		std::unordered_map<std::string, std::string> remaps {
			{"game_script", "game-script"}
		};
		for (auto &remap : remaps)
			if (remap.first == propertyName)
				return remap.second;
		return propertyName;
	};

	if (op == "read") {
		script_h.readVariable();
		script_h.pushVariable();

		std::string propertyName(translate(script_h.readStr()));

		auto it = map.find(propertyName);
		if (it != map.end())
			script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, it->second.c_str());
		else
			script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, "undef");
	} else if (op == "write") {
		std::string propertyValue(script_h.readStr());
		std::string propertyName(translate(script_h.readStr()));
		//sendToLog(LogLevel::Error, "Attempting to modify property %s with value %s\n",propertyName.c_str(),propertyValue.c_str());
		map[propertyName] = propertyValue;
	} else if (op == "unset") {
		auto it = map.find(translate(script_h.readStr()));
		if (it != map.end())
			map.erase(it);
	} else if (op == "save") {
		std::string configData;
		// Some reasonable value we are unlikely to exceed
		configData.reserve(1024);

#ifdef WIN32
		const char *lineend = " \r\n";
#else
		const char *lineend = " \n";
#endif

		for (auto &opt : ons_cfg_options) {
			if (opt.second == "noval")
				configData += opt.first + lineend;
			else
				configData += opt.first + '=' + opt.second + lineend;
		}

		for (auto &opt : user_cfg_options)
			configData += "env[" + opt.first + "]=" + opt.second + lineend;

		auto cfgFile = std::string(ons_cfg_path) + CFG_FILE;
		auto tmpFile = cfgFile + ".tmp";

		auto configBuffer = reinterpret_cast<const uint8_t *>(configData.c_str());
		if (!FileIO::writeFile(tmpFile, configBuffer, configData.size()) ||
		    !FileIO::renameFile(tmpFile, cfgFile, true)) {
			sendToLog(LogLevel::Error, "Failed to write to %s!\n", cfgFile.c_str());
			configData = "Failed to create ons.cfg file in the game folder!\n"
			             "Make sure the game folder is not read-only and restart the game, or create ons.cfg manually with the following contents:\n\n" +
			             configData;
			ons.errorAndCont(configData.c_str(), nullptr, "I/O Warning", true, true);
		}
	}

	return RET_CONTINUE;
}

int ONScripter::nosmartquotesCommand() {
	sentence_font.resetSmartQuotes();
	name_font.resetSmartQuotes();
	return RET_CONTINUE;
}

int ONScripter::nearestJumpableLogEntryIndexCommand() {
	script_h.readVariable();
	script_h.pushVariable();

	uint32_t logEntryIndex = script_h.readInt() + 1;
	uint32_t label_index;

	do {
		logEntryIndex--;
		label_index = script_h.logState.logEntryIndexToLabelIndex(logEntryIndex);
	} while (!script_h.logState.dialogueData[label_index].jumpable);

	script_h.setInt(&script_h.pushed_variable, logEntryIndex);

	return RET_CONTINUE;
}

int ONScripter::moreramCommand() {
	int lower_limit = script_h.readInt();

	if (ram_limit <= lower_limit) {
		{
			Lock lock(&imageCache);
			imageCache.clearAll();
		}
		{
			Lock lock(&soundCache);
			soundCache.clearAll();
		}
		gpu.clearImagePools();
#ifdef IOS
		// Do it, I said!
		malloc_zone_pressure_relief(nullptr, 0);
#endif
		sendToLog(LogLevel::Info, "[Optimisation] Freed memory to avoid crashes!\n");
	}

	return RET_CONTINUE;
}

int ONScripter::markRangeReadCommand() {
	auto *start_l = script_h.lookupLabel(script_h.readLabel() + 1);
	auto *end_l   = script_h.lookupLabel(script_h.readLabel() + 1);

	int start = script_h.getLabelIndex(start_l);
	int end   = script_h.getLabelIndex(end_l);

	if (start > end)
		std::swap(start, end);

	while (start <= end) {
		script_h.logState.readLabels[start] = true;
		start++;
	}

	return RET_CONTINUE;
}

int ONScripter::markReadCommand() {
	LabelInfo *label = current_label_info;
	if (!callStack.empty())
		label = callStack.front().label;

	auto id = script_h.getLabelIndex(label);
	script_h.logState.readLabels[id] = true;

	return RET_CONTINUE;
}

int ONScripter::markAllReadCommand() {
	LabelInfo *label = current_label_info;
	if (!callStack.empty())
		label = callStack.front().label;

	auto id = script_h.getLabelIndex(label);

	for (auto &thisLogEntry : script_h.logState.logEntries) {
		// Read all before current as we may not have clicked past it.
		// This may get called from places before any log entries are present, e.g. at *start, in this case
		// we should not mark any log entries as read. Failing to do so will result in all episode messages to become
		// read after exiting via right-click menu. See: https://forum.umineko-project.org/viewtopic.php?f=6&t=339.
		if (thisLogEntry.labelIndex >= id && thisLogEntry.choiceVectorSize == script_h.choiceState.choiceVector.size())
			break;
		script_h.logState.readLabels[thisLogEntry.labelIndex] = true;
	}

	return RET_CONTINUE;
}

int ONScripter::mainGotoCommand() {
	// Clearing the stack and changing the label
	// if you jump from a subroutine you will happen to in the main script
	callStack.clear();
	return gotoCommand();
}

int ONScripter::gotoCommand() {
	setCurrentLabel(script_h.readLabel() + 1);
	tryEndSuperSkip(false);
	return RET_CONTINUE;
}

int ONScripter::mainLabelCommand() {
	script_h.readVariable();
	if(script_h.current_variable.type != VariableInfo::TypeStr) {
		errorAndExit("main_label requires a $str variable argument to return the label into");
	}

	LabelInfo *label = current_label_info;
	if (!callStack.empty())
		label = callStack.front().label;

	std::string labelName = "*";
	labelName += label->name;

	script_h.setStr(&script_h.getVariableData(script_h.current_variable.var_no).str, labelName.c_str());
	return RET_CONTINUE;
}

int ONScripter::makeChoiceCommand() {
	script_h.choiceState.acceptChoiceNextIndex++;
	script_h.choiceState.choiceVector.push_back(script_h.readInt());
	while (script_h.hasMoreArgs()) {
		script_h.choiceState.acceptChoiceNextIndex++;
		script_h.choiceState.choiceVector.push_back(script_h.readInt());
	}
	return RET_CONTINUE;
}

// lookahead regex_string,return_string_1,return_string_2,return_string_3,...,return_string_n
// Returns the values of the first capturing group in the first n matches of the regex string against the script from the current position.
int ONScripter::lookaheadCommand() {

	if (callStack.empty())
		return RET_CONTINUE;                 // failed (we were called from the main script?)
	NestInfo *tmp_nest = &callStack.front(); // first subfunction, next_script field represents our position in the main game script outside all subfunctions
	if (!tmp_nest->next_script)
		return RET_CONTINUE; // failed (I can't imagine why we'd have no script available but just in case...)

	auto &regexp  = regExps[script_h.readInt()];
	auto matchNum = script_h.readInt();

#ifdef USE_STD_REGEX // Unfortunately it is like 50 times slower :x
	bool matched = true;
	std::cmatch result;

	const char *currentLocation = tmp_nest->next_script;

	while (script_h.hasMoreArgs()) {
		if (matched) {
			// avoid extra searching on failure
			matched = std::regex_search(currentLocation, result, regexp);
		}

		if (!matched) {
			sendToLog(LogLevel::Error, "Regexp sequence matching failed!\n");
		}

		//sendToLog(LogLevel::Info, "Regexp matched: [%s]\n", result.str().c_str());
		//sendToLog(LogLevel::Info, "Current location: [%.*s]\n", 500, currentLocation);

		// feed it to the result variable
		for (int i = 1; i <= matchNum; i++) {
			script_h.readVariable();
			// feed empty string if matching failed
			script_h.setStr(&script_h.getVariableData(script_h.current_variable.var_no).str,
			                matched ? result.str(i).c_str() : "");
		}

		currentLocation += result.position() + result.length(); // Next search after the previous hit
	}
#else
	auto result = std::make_unique<slre_cap[]>(matchNum);

	int bytesScanned            = 0;
	size_t bytesConsumed        = script_h.getOffset(tmp_nest->next_script);
	size_t bytesRemaining       = script_h.getScriptLength() - bytesConsumed;
	const char *currentLocation = tmp_nest->next_script;
	std::unique_ptr<char[]> output;
	size_t outputLen = 0;

	while (script_h.hasMoreArgs()) {
		if (bytesScanned >= 0) {
			// avoid extra searching on failure
			bytesScanned = slre_match_reuse(&regexp.second, currentLocation, static_cast<int>(bytesRemaining), result.get(), matchNum);
		}

		if (bytesScanned < 0) {
			sendToLog(LogLevel::Error, "Regexp sequence matching failed: %d!\n", bytesScanned);
		}

		//sendToLog(LogLevel::Info, "Regexp matched: [%.*s]\n", result[0].len, result[0].ptr);
		//sendToLog(LogLevel::Info, "Current location: [%.*s]\n", 500, currentLocation);

		// feed it to the result variable
		for (int i = 0; i < matchNum; i++) {
			script_h.readVariable();
			// feed empty string if matching failed
			if (bytesScanned < 0) {
				script_h.setStr(&script_h.getVariableData(script_h.current_variable.var_no).str, "");
			} else {
				size_t len = result[i].len;
				if (outputLen < len + 1) {
					output    = std::make_unique<char[]>(len + 1);
					outputLen = len + 1;
				}
				auto p = output.get();
				std::memcpy(p, result[i].ptr, len);
				p[result[i].len] = '\0';
				script_h.setStr(&script_h.getVariableData(script_h.current_variable.var_no).str, p);
			}
		}

		currentLocation += bytesScanned; // Next search after the previous hit
		bytesRemaining -= bytesScanned;
	}
#endif

	return RET_CONTINUE;
}

int ONScripter::loadregCommand() {

	if (reg_loaded) {
		errorAndExit("You have already loaded the registry!");
		return RET_CONTINUE; // dummy
	}

	if (readIniFile(script_h.readFilePath(), registry)) {
		reg_loaded = true;
	} else {
		errorAndExit("Failed to load the registry file!");
	}

	return RET_CONTINUE;
}

int ONScripter::loadfromregCommand() {

	if (!reg_loaded) {
		errorAndExit("You haven't loaded any registry to receive the data from!");
	}

	script_h.readVariable();

	if (script_h.current_variable.type != VariableInfo::TypeStr)
		errorAndExit("loadfromreg: no string variable.");

	int no = script_h.current_variable.var_no;

	const char *buf = script_h.readStr();
	std::string reg_sec(buf);
	buf = script_h.readStr();

	auto key = registry.find(reg_sec);
	if (key == registry.end())
		errorAndExit("loadfromreg: no such key found ");
	auto val = registry[key->first].find(buf);
	if (val == registry[key->first].end())
		errorAndExit("loadfromreg: no such value found.");

	script_h.setStr(&script_h.getVariableData(no).str, val->second.c_str());

	return RET_CONTINUE;
}

// lips_sprite spriteNo(,characterName)
// makes this sprite animate to voices for characterName (or disables animation if no characterName is passed). One characterName at a time per sprite.
int ONScripter::lipsSpriteCommand() {
	int lipSpriteNo = validSprite(script_h.readInt());

	AnimationInfo &lsp  = sprite_info[lipSpriteNo];
	AnimationInfo &lsp2 = sprite2_info[lipSpriteNo];
	if (lsp.exists && lsp.lips_name) {
		delete[] lsp.lips_name;
		lsp.lips_name = nullptr;
	}
	if (lsp2.exists && lsp2.lips_name) {
		delete[] lsp2.lips_name;
		lsp2.lips_name = nullptr;
	}
	if (script_h.hasMoreArgs()) {
		const char *buf = script_h.readStr();
		if (lsp.exists)
			lsp.lips_name = copystr(buf);
		if (lsp2.exists)
			lsp2.lips_name = copystr(buf);
	}
	return RET_CONTINUE;
}

int ONScripter::lipsLimitsCommand() {
	speechLevels[0] = atof(script_h.readStr());
	speechLevels[1] = atof(script_h.readStr());
	sendToLog(LogLevel::Info, "Speech levels changed to %f %f\n", speechLevels[0], speechLevels[1]);
	return RET_CONTINUE;
}

// lips_channel channelNo,characterName1,characterName2...
// animates lips for characterName on audio from channel channelNo
int ONScripter::lipsChannelCommand() {
	int channelNo = script_h.readInt();

	std::vector<std::string> characters;
	do
		characters.emplace_back(script_h.readStr());
	while (script_h.hasMoreArgs());

	if (!lipsChannels[channelNo].has())
		lipsChannels[channelNo].set();

	lipsChannels[channelNo].get().characterNames = characters;

	// Note that we never unset lipsChannels[channelNo].
	// It would be nice if this could be done when characters.empty(), but I am unsure if it is thread-safe.

	return RET_CONTINUE;
}

int ONScripter::jautomodeCommand() {
	if (automode_flag)
		jumpToTilde(false);
	return RET_CONTINUE;
}

int ONScripter::jnautomodeCommand() {
	if (!automode_flag)
		jumpToTilde(false);
	return RET_CONTINUE;
}

int ONScripter::jskipSuperCommand() {
	if (skip_mode & SKIP_SUPERSKIP)
		jumpToTilde(false);
	return RET_CONTINUE;
}

int ONScripter::jnskipSuperCommand() {
	if (!(skip_mode & SKIP_SUPERSKIP))
		jumpToTilde(false);
	return RET_CONTINUE;
}

int ONScripter::jskipCommand() {
	if (skip_mode & SKIP_NORMAL || keyState.ctrl)
		jumpToTilde(false);
	return RET_CONTINUE;
}

int ONScripter::jnskipCommand() {
	if (!(skip_mode & SKIP_NORMAL || keyState.ctrl))
		jumpToTilde(false);
	return RET_CONTINUE;
}

int ONScripter::ignoreVoiceDelayCommand() {
	ignore_voicedelay = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::hyphenCarryCommand() {
	sentence_font.layoutData.newLineBehavior.duplicateHyphens = true;
	name_font.layoutData.newLineBehavior.duplicateHyphens     = true;
	return RET_CONTINUE;
}

int ONScripter::getChoiceVectorSizeCommand() {
	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, static_cast<int32_t>(script_h.choiceState.choiceVector.size()));
	return RET_CONTINUE;
}

int ONScripter::getLogDataCommand() {
	// Returns the jump label and choice vector size for a given log entry index.
	int logEntryIndex    = script_h.readInt();

	if  (logEntryIndex < 0)
		logEntryIndex = static_cast<int32_t>(script_h.logState.logEntries.size() - 1);

	int choiceVectorSize = script_h.logState.logEntries[logEntryIndex].choiceVectorSize;

	script_h.readVariable();
	if (script_h.current_variable.type != VariableInfo::TypeStr) {
		errorAndExit("get_log_data second argument (output label) must be a string");
	}

	std::string res = "*";
	res += script_h.getLabelByLogEntryIndex(logEntryIndex)->name;
	script_h.setStr(&script_h.getVariableData(script_h.current_variable.var_no).str, res.c_str());

	script_h.readVariable();
	if (script_h.current_variable.type != VariableInfo::TypeInt) {
		errorAndExit("get_log_data third argument (output choice vector size) must be an int");
	}
	script_h.setInt(&script_h.current_variable, choiceVectorSize);

	return RET_CONTINUE;
}

int ONScripter::getUniqueLogEntryIndexCommand() {
	// Gets the log entry index for a label. Only to be used for labels that appear just once in the log!
	script_h.readVariable();
	script_h.pushVariable();
	if (script_h.pushed_variable.type != VariableInfo::TypeInt) {
		errorAndExit("get_unique_log_entry_index first argument (output log entry index) must be an int");
	}

	const char *label = script_h.readLabel(); // contains label with * attached
	auto toFind       = script_h.getLabelIndex(script_h.lookupLabel(label + 1));

	for (uint32_t i = 0; i < script_h.logState.logEntries.size(); i++) {
		if (script_h.logState.logEntryIndexToLabelIndex(i) == toFind) {
			script_h.setInt(&script_h.pushed_variable, static_cast<int32_t>(i));
			return RET_CONTINUE;
		}
	}

	script_h.setInt(&script_h.pushed_variable, -1);
	return RET_CONTINUE;
}

int ONScripter::getScriptPathCommand() {
	//Syntax:
	//getscriptpath $dst,%index[,1] - for basepath

	script_h.readVariable();
	script_h.pushVariable();

	unsigned int i = script_h.readInt();
	bool base      = script_h.hasMoreArgs() ? script_h.readInt() : false;

	if (i >= script_list.size())
		errorAndExit("Script index is out of bounds");

	auto &path = script_list[i];
	size_t pos = std::string::npos;

	if (base)
		pos = path.find_last_of('.');

	if (pos != std::string::npos)
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, path.substr(0, pos).c_str());
	else
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, path.c_str());

	return RET_CONTINUE;
}

int ONScripter::getScriptNumCommand() {
	//Syntax:
	//getscriptnum %num

	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, static_cast<int>(script_list.size()));
	return RET_CONTINUE;
}

int ONScripter::getRendererNameCommand() {
	//Syntax:
	//getrenderername $dst,%index

	script_h.readVariable();
	script_h.pushVariable();

	int32_t no = script_h.readInt();
	if (no >= 0) {
		int32_t rsize = std::extent<decltype(gpu.renderers)>::value;
		if (no >= rsize)
			errorAndExit("Renderer name index is out of bounds");

		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, gpu.renderers[no].name);
	} else {
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, gpu.current_renderer->name);
	}

	return RET_CONTINUE;
}

int ONScripter::getRendererNumCommand() {
	//Syntax:
	//getrenderer %num

	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, static_cast<int>(std::extent<decltype(gpu.renderers)>::value));
	return RET_CONTINUE;
}

int ONScripter::getramCommand() {
	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, ram_limit);
	return RET_CONTINUE;
}

int ONScripter::fallCommand() {
	//Syntax:
	//fall dims, %id,%w,%h
	//fall speed, %id[,%speed]
	//fall amount, %id,%amount
	//fall wind, %id,%angle
	//fall base, %id,#colour,%w,%h[,%a]
	//fall base, %id,"picture"
	//fall pause,%id,%state
	//fall blend,%id,"mode"
	//fall amps,%id,"0.1","0.125","1"

	bool dims{false}, speed{false}, c_speed{false}, amount{false}, wind{false},
	    base{false}, pause{false}, blend{false}, amps{false}, cover{false};

	if (script_h.compareString("dims"))
		dims = true;
	else if (script_h.compareString("speed"))
		speed = true;
	else if (script_h.compareString("c_speed"))
		c_speed = true;
	else if (script_h.compareString("amount"))
		amount = true;
	else if (script_h.compareString("wind"))
		wind = true;
	else if (script_h.compareString("base"))
		base = true;
	else if (script_h.compareString("pause"))
		pause = true;
	else if (script_h.compareString("blend"))
		blend = true;
	else if (script_h.compareString("amps"))
		amps = true;
	else if (script_h.compareString("cover"))
		cover = true;
	else
		errorAndExit("Invalid fall param");

	script_h.readName();

	auto layer_id = script_h.readInt();
	auto layer    = getLayer<ObjectFallLayer>(layer_id);

	if (dims) {
		layer->setDims(script_h.readInt(), script_h.readInt());
	} else if (speed) {
		if (script_h.hasMoreArgs())
			layer->setSpeed(script_h.readInt());
		else
			layer->setSpeed();
	} else if (c_speed) {
		layer->setCustomSpeed(script_h.readInt());
	} else if (amount) {
		int property  = dynamicProperties.getRegisteredProperty("fallamount");
		auto value    = script_h.readInt();
		auto duration = script_h.hasMoreArgs() ? script_h.readInt() : 0;
		auto equation = script_h.hasMoreArgs() ? script_h.readInt() : 0;
		dynamicProperties.addCustomProperty(layer, true, property, value, duration, equation, true);
	} else if (wind) {
		layer->setWind(script_h.readInt());
	} else if (base) {
		bool is_colour{false};
		const char *buf = script_h.readColor(&is_colour);

		if (!is_colour) {
			GPU_Image *image = loadGpuImage(script_h.readFilePath());
			layer->setBaseDrop(image);
		} else {
			uchar3 colourBytes;
			readColor(&colourBytes, buf);
			SDL_Color colour{colourBytes.x, colourBytes.y, colourBytes.z, 255};
			uint32_t w = script_h.readInt();
			uint32_t h = script_h.readInt();
			if (script_h.hasMoreArgs())
				colour.a = script_h.readInt();
			layer->setBaseDrop(colour, w, h);
		}
	} else if (pause) {
		layer->setPause(script_h.readInt());
	} else if (blend) {
		BlendModeId mode{BlendModeId::NORMAL};
		std::string modeStr{script_h.readStr()};
		if (modeStr == "add")
			mode = BlendModeId::ADD;
		else if (modeStr == "normal")
			mode = BlendModeId::NORMAL;
		else if (modeStr == "sub")
			mode = BlendModeId::SUB;
		else if (modeStr == "mul")
			mode = BlendModeId::MUL;
		else if (modeStr == "alpha")
			mode = BlendModeId::ALPHA;
		else
			errorAndExit("Invalid fall blend mode");
		layer->setBlend(mode);
	} else if (amps) {
		float s = parsefloat(script_h.readStr());
		float w = parsefloat(script_h.readStr());
		float h = parsefloat(script_h.readStr());
		float r = script_h.hasMoreArgs() ? parsefloat(script_h.readStr()) : 0;
		float m = script_h.hasMoreArgs() ? parsefloat(script_h.readStr()) : 1;
		layer->setAmplifiers(s, w, h, r, m);
	} else if (cover) {
		layer->coverScreen();
	}

	return RET_CONTINUE;
}

int ONScripter::errorCommand() {
	const char *buf = script_h.readStr();
	errorAndExit(buf);
	return RET_CONTINUE;
}

// Parameters: sprite number, enable(1)/disable(0) (default 1)
int ONScripter::enableTransitionsCommand() {
	bool lsp2        = script_h.isName("enable_transitions2");
	int spriteNumber = script_h.readInt();
	bool enable      = true;
	if (script_h.hasMoreArgs())
		enable = script_h.readInt();

	AnimationInfo *ai = lsp2 ? &sprite2_info[spriteNumber] : &sprite_info[spriteNumber];

	if (enable)
		nontransitioningSprites.erase(ai);
	else
		nontransitioningSprites.insert(ai);
	return RET_CONTINUE;
}

int ONScripter::enableCustomCursors() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("enable_custom_cursors: not in the define section");

	enable_custom_cursors = true;

	return RET_CONTINUE;
}

int ONScripter::displayScreenshotCommand() {
	int lsp = validSprite(script_h.readInt());

	if (!screenshot_gpu) {
		errorAndExit("No screenshot was made to display");
		return RET_CONTINUE; //dummy
	}

	backupState(&sprite_info[lsp]);

	sprite_info[lsp].num_of_cells = 1;
	sprite_info[lsp].current_cell = 0;
	sprite_info[lsp].trans_mode   = AnimationInfo::TRANS_COPY;
	sprite_info[lsp].visible      = true;
	sprite_info[lsp].orig_pos.x   = script_h.readInt();
	sprite_info[lsp].orig_pos.y   = script_h.readInt();
	int w                         = script_h.readInt();
	int h                         = script_h.readInt();
	UpdateAnimPosXY(&sprite_info[lsp]);
	sprite_info[lsp].trans = script_h.readInt();

	//Animationinfo (setup)
	sprite_info[lsp].deleteImage();
	sprite_info[lsp].abs_flag = true;

	sprite_info[lsp].gpu_image = gpu.createImage(w, h, 3);
	GPU_GetTarget(sprite_info[lsp].gpu_image);
	gpu.copyGPUImage(screenshot_gpu, nullptr, nullptr, sprite_info[lsp].gpu_image->target, w / 2.0, h / 2.0,
	                 w / static_cast<float>(screenshot_gpu->w), h / static_cast<float>(screenshot_gpu->h), 0, true);

	sprite_info[lsp].setImage(sprite_info[lsp].gpu_image);

	sprite_info[lsp].stale_image = false;
	sprite_info[lsp].exists      = true;

	last_loaded_sprite_ind                     = (1 + last_loaded_sprite_ind) % SPRITE_NUM_LAST_LOADS;
	last_loaded_sprite[last_loaded_sprite_ind] = lsp;
	dirtySpriteRect(lsp, false);

	return RET_CONTINUE;
}

int ONScripter::dialogueSetVoiceWaitCommand() {
	dlgCtrl.currentVoiceWait = std::to_string(script_h.readInt());
	return RET_CONTINUE;
}

int ONScripter::conditionDialogueCommand() {
	size_t idx = script_h.readInt();
	if (idx >= conditions.size()) {
		size_t sz = 32;
		while (sz <= idx) sz <<= 1;
		conditions.resize(sz, false);
	}
	conditions[idx] = script_h.readInt() == 1;

	return RET_CONTINUE;
}

int ONScripter::disposeDialogueCommand() {
	if (!dlgCtrl.dialogueProcessingState.active) {
		errorAndExit("Tried to ruin dialogue state from something that is not a dialogue");
		return RET_CONTINUE; //dummy
	}

	// Ending dialogue days here; we will have any current scriptState as main and only
	dlgCtrl.scriptState.disposeMainscript(true);
	dlgCtrl.setDialogueActive(false);

	return RET_CONTINUE;
}

int ONScripter::dialogueAddEndsCommand() {
	dialogue_add_ends = script_h.readInt();

	return RET_CONTINUE;
}

int ONScripter::dialogueContinueCommand() {
	if (!dlgCtrl.dialogueProcessingState.active) {
		errorAndExit("You are not allowed to d_continue outside the scope of d2 command");
		return RET_CONTINUE; //dummy
	}

	if (dlgCtrl.suspendDialoguePasses < 0) {
		//sendToLog(LogLevel::Info, "dialogueContinueCommand dialogue event\n");
		dlgCtrl.events.emplace();
	}
	//sendToLog(LogLevel::Info, "d_continue executed\n");
	dlgCtrl.suspendDialoguePasses++;
	return RET_CONTINUE;
}

// wait_on_d n, where n is the index of the pipe character to wait on or -1 for TEXT_STATE::END
int ONScripter::waitOnDialogueCommand() {
	int index = script_h.readInt();
	//sendToLog(LogLevel::Info, "wait_on_d %d", index);
	if (dlgCtrl.dialogueProcessingState.active) {
		dlgCtrl.suspendScriptPasses[index]--;
		//sendToLog(LogLevel::Info, " reduced to %d\n", dlgCtrl.suspendScriptPasses[index]);
	} else {
		//sendToLog(LogLevel::Info, "\n");
	}
	return RET_CONTINUE;
}

int ONScripter::dialogueCommand() {

	dlgCtrl.continueScriptExecution = script_h.isName("d2");

	script_h.pushStringBuffer(0);

	if (!dlgCtrl.dialogueProcessingState.active) {
		while (effect_current) waitEvent(0); //fixes the bug with d26767, is this the ONLY place to account for?
		commitVisualState();
		dlgCtrl.dialogue_pos = script_h.getCurrent();
		dlgCtrl.feedDialogueTextData(script_h.readToEol());
	} else {
		script_h.readToEol();
	}
	return textCommand();
}

int ONScripter::dialogueNameCommand() {
	if (script_h.isName("d_name_refresh")) {
		dlgCtrl.nameLayouted = false;
		dlgCtrl.nameRenderState.clear();
		dlgCtrl.layoutName();
	} else {
		const char *buf = script_h.readStr();
		dlgCtrl.setDialogueName(buf);
	}
	return RET_CONTINUE;
}

int ONScripter::debugStrCommand() {
	const char *buf = script_h.readStr();
	sendToLog(LogLevel::Warn, "Debugger: %s\n", buf);

	return RET_CONTINUE;
}

int ONScripter::customCursorCommand() {
	if (cursor)
		SDL_FreeCursor(cursor);
	cursor = nullptr;

	std::string img = script_h.readStr();
	if (img == "arrow") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	} else if (img == "ibeam") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
	} else if (img == "wait") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	} else if (img == "cross") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
	} else if (img == "waitarrow") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAITARROW);
	} else if (img == "sizenwse") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
	} else if (img == "sizenesw") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
	} else if (img == "sizewe") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
	} else if (img == "sizens") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
	} else if (img == "sizeall") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
	} else if (img == "no") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
	} else if (img == "hand") {
		cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	}

	SDL_SetCursor(cursor);
	return RET_CONTINUE;
}

int ONScripter::countSymbolsCommand() {

	script_h.readVariable();
	script_h.pushVariable();

	if (script_h.pushed_variable.type != VariableInfo::TypeInt &&
	    script_h.pushed_variable.type != VariableInfo::TypeArray)
		errorAndExit("count_breaks: no integer variable.");

	const char *buf = script_h.readStr();
	if (!buf || (*buf == '\0')) {
		script_h.setInt(&script_h.pushed_variable, 0);
		return RET_CONTINUE;
	}

	uint32_t find_codepoint = 0;
	uint32_t codepoint      = 0;
	uint32_t state          = 0;
	int lineBreaks          = 0;

	while (decodeUTF8(&state, &find_codepoint, *buf)) buf++;

	buf = script_h.readStr();

	if (!buf || (*buf == '\0')) {
		script_h.setInt(&script_h.pushed_variable, 0);
		return RET_CONTINUE;
	}

	while (*buf != '\0') {
		state = 0;
		while (decodeUTF8(&state, &codepoint, *buf)) buf++;
		buf++;
		if (codepoint == find_codepoint)
			lineBreaks++;
	}

	script_h.setInt(&script_h.pushed_variable, lineBreaks);

	return RET_CONTINUE;
}

int ONScripter::colorModCommand() {
	bool lsp2 = false;
	if (script_h.isName("color_mod2"))
		lsp2 = true;

	//color_mod off,sprite
	//color_mod sepia,sprite
	//color_mod nega1,sprite
	//color_mod nega2,sprite
	//color_mod sprite,#colour

	/* Match the string and save it (since it has to be first parameter...) */
	const char *str_off = "off", *str_sepia = "sepia", *str_nega1 = "nega1", *str_nega2 = "nega2";
	const char *matched_string = nullptr;
	for (auto s : {str_off, str_sepia, str_nega1, str_nega2}) {
		if (script_h.compareString(s)) {
			matched_string = s;
			script_h.readName();
			break;
		}
	}

	/* Read lsp number. */
	int sprite_num = script_h.readInt();
	AnimationInfo *anim;
	if (lsp2)
		anim = &sprite2_info[sprite_num];
	else
		anim = &sprite_info[sprite_num];

	if (!anim->exists) {
		if (!matched_string) {
			uchar3 colour;
			readColor(&colour, readColorStr());
		}
		return RET_CONTINUE;
	}
	backupState(anim);

	if (anim->layer_no >= 0) {
		auto *layer = getLayer<ObjectFallLayer>(anim->layer_no, false);
		if (layer != nullptr) {
			layer->setBlend(BlendModeId::NORMAL);
		}
	}

	if (matched_string) {
		if (matched_string == str_off) {
			anim->spriteTransforms.sepia     = false;
			anim->spriteTransforms.negative1 = false;
			anim->spriteTransforms.negative2 = false;
			anim->spriteTransforms.greyscale = false;
		} else if (matched_string == str_sepia) {
			anim->spriteTransforms.sepia = true;
		} else if (matched_string == str_nega1) {
			anim->spriteTransforms.negative1 = true;
		} else if (matched_string == str_nega2) {
			anim->spriteTransforms.negative2 = true;
		}
	} else {
		uchar3 colour;
		readColor(&colour, readColorStr());
		anim->spriteTransforms.greyscale = true;
		anim->darkenHue                  = {colour.x, colour.y, colour.z, 255};
	}

	if (anim->visible)
		dirtySpriteRect(sprite_num, lsp2);
	return RET_CONTINUE;
}

int ONScripter::clearLogCommand() {
	// Clears the log state and the tree at the provided index and empties the choice vector.
	// You can also pass in a choice vector size, which will trim the choice vector to that size and remove all the log entries with a greater choice vector size.

	int treeNo       = validTree(script_h.readInt());
	StringTree &tree = dataTrees[treeNo];

	if (script_h.hasMoreArgs()) {
		// Partial clear
		LogEntry newCVS{0, static_cast<uint32_t>(script_h.readInt())};
		auto firstTooLargeIt = std::upper_bound(script_h.logState.logEntries.begin(),
		                                        script_h.logState.logEntries.end(), newCVS,
		                                        [](const LogEntry &a, const LogEntry &b) {
			                                        return a.choiceVectorSize < b.choiceVectorSize;
		                                        });

		if (firstTooLargeIt != script_h.logState.logEntries.end()) {
			auto firstTooLargeElement = firstTooLargeIt - script_h.logState.logEntries.begin();

			// Trim everything after this.
			for (size_t i = firstTooLargeElement; i < script_h.logState.logEntries.size(); i++) {
				tree.branches.erase(tree.insertionOrder[i]);
			}
			tree.insertionOrder.resize(firstTooLargeElement);
			script_h.logState.logEntries.resize(firstTooLargeElement);
		}
		script_h.choiceState.choiceVector.resize(newCVS.choiceVectorSize);
	} else {
		// Full clear
		tree.clear();
		script_h.logState.logEntries.clear();
		script_h.choiceState.choiceVector.clear();
	}
	if (script_h.choiceState.acceptChoiceNextIndex > script_h.choiceState.choiceVector.size()) {
		script_h.choiceState.acceptChoiceNextIndex = static_cast<uint32_t>(script_h.choiceState.choiceVector.size());
	}
	return RET_CONTINUE;
}

int ONScripter::clearCacheCommand() {
	// Parameters: ID

	bool image = script_h.isName("clear_cache_img");

	int id = script_h.readInt();

	if (image) {
		SDL_AtomicLock(&async.imageCacheQueue.lock);
		/*#*/ auto new_end = std::remove_if(async.imageCacheQueue.q.begin(),
		                                    async.imageCacheQueue.q.end(),
		                                    [&id](std::unique_ptr<AsyncInstruction> &i) {
			                                    return (static_cast<LoadImageCacheInstruction *>(i.get()))->id == id;
		                                    });
		/*#*/ async.imageCacheQueue.q.erase(new_end, async.imageCacheQueue.q.end());
		{
			Lock lock(&imageCache);
			imageCache.clear(id);
		}
		SDL_AtomicUnlock(&async.imageCacheQueue.lock);
	} else {
		SDL_AtomicLock(&async.soundCacheQueue.lock);
		/*#*/ auto new_end = std::remove_if(async.soundCacheQueue.q.begin(),
		                                    async.soundCacheQueue.q.end(),
		                                    [&id](std::unique_ptr<AsyncInstruction> &i) {
			                                    return (static_cast<LoadSoundCacheInstruction *>(i.get()))->id == id;
		                                    });
		/*#*/ async.soundCacheQueue.q.erase(new_end, async.soundCacheQueue.q.end());
		{
			Lock lock(&soundCache);
			soundCache.clear(id);
		}
		SDL_AtomicUnlock(&async.soundCacheQueue.lock);
	}
	return RET_CONTINUE;
}

int ONScripter::choicesToStringCommand() {
	script_h.readVariable();
	if (script_h.current_variable.type != VariableInfo::TypeStr) {
		errorAndExit("savechoices requires a string argument");
	}
	std::string ret;
	size_t end = script_h.choiceState.choiceVector.size();
	for (size_t i = 0; i < end; i++) {
		ret += std::to_string(script_h.choiceState.choiceVector[i]);
		if (i != end-1) {
			ret += ",";
		}
	}
	script_h.setStr(&script_h.getVariableData(script_h.current_variable.var_no).str, ret.c_str());
	return RET_CONTINUE;
}

int ONScripter::choicesFromStringCommand() {
	const char *buf = script_h.readStr();
	std::istringstream choices{std::string(buf)};
	std::string choice;
	script_h.choiceState.choiceVector.clear();
	while(std::getline(choices, choice, ',')) {
		script_h.choiceState.choiceVector.push_back(std::stoi(choice));
	}
	return RET_CONTINUE;
}

// Parameters: The sprite number of the CHILD image (NOT the parent image).
int ONScripter::childImageDetachCommand() {
	bool childLsp2 = script_h.isName("child_image_detach2");
	int childImage = script_h.readInt();

	AnimationInfo &c = childLsp2 ? sprite2_info[childImage] : sprite_info[childImage];
	int parentImage  = c.parentImage.no;
	bool parentLsp2  = c.parentImage.lsp2;
	AnimationInfo &p = parentLsp2 ? sprite2_info[parentImage] : sprite_info[parentImage];

	dirtySpriteRect(parentImage, parentLsp2);
	c.parentImage = {-1, false};
	dirtySpriteRect(childImage, childLsp2);

	for (auto &zlevel : p.childImages) {
		auto aiIdentifier = &zlevel.second; // zlevel = (zOrder, (spriteNumber, isLsp2))
		if (aiIdentifier->no == childImage) {
			p.childImages.erase(zlevel.first);
			break;
		}
	}
	return RET_CONTINUE;
}

// Parameters: parent sprite number; child sprite number; optional child z-ordering (default 0)
int ONScripter::childImageCommand() {
	bool lsp2          = script_h.isName("child_image2");
	int parentImage    = script_h.readInt();
	int childImage     = script_h.readInt();
	int childZOrdering = 0;
	if (script_h.hasMoreArgs()) {
		childZOrdering = script_h.readInt();
	}

	AnimationInfo &p = lsp2 ? sprite2_info[parentImage] : sprite_info[parentImage];
	AnimationInfo &c = lsp2 ? sprite2_info[childImage] : sprite_info[childImage];

	backupState(&p);
	backupState(&c);
	dirtySpriteRect(parentImage, lsp2);
	dirtySpriteRect(childImage, lsp2);
	p.childImages[childZOrdering] = {childImage, lsp2};
	c.parentImage                 = {parentImage, lsp2};

	return RET_CONTINUE;
}

int ONScripter::changeFontCommand() {

	int font = script_h.readInt();

	if (font >= 0)
		sentence_font.changeCurrentFont(font);

	if (script_h.hasMoreArgs()) {
		font = script_h.readInt();
		if (font >= 0)
			name_font.changeCurrentFont(font);
	}

	return RET_CONTINUE;
}

// cache_slot_type slotnumber,"lru|def",capacity(if lru)
int ONScripter::cacheSlotTypeCommand() {
	bool image{script_h.isName("cache_slot_img")};

	int slotnumber = script_h.readInt();
	std::string s(script_h.readStr());
	if (s.compare("lru") == 0) {
		int capacity = script_h.readInt();
		if (image) {
			Lock lock(&imageCache);
			imageCache.makeLRU(slotnumber, capacity);
		} else {
			Lock lock(&soundCache);
			soundCache.makeLRU(slotnumber, capacity);
		}
	} else if (s.compare("def") == 0) {
		if (image) {
			Lock lock(&imageCache);
			imageCache.makeUnlimited(slotnumber);
		} else {
			Lock lock(&soundCache);
			soundCache.makeUnlimited(slotnumber);
		}
	} else {
		sendToLog(LogLevel::Error, "Unknown cache slot type %s\n", s.c_str());
	}
	return RET_CONTINUE;
}

int ONScripter::asyncLoadCacheCommand() {
	// Parameters: ID, filename (no tags!), optional bool allow_rgb (true by default)
	bool image{script_h.isName("async_cache_img")};

	int id = script_h.readInt();
	std::string filename(script_h.readFilePath());

	bool allow_rgb{true};
	if (script_h.hasMoreArgs())
		allow_rgb = script_h.readInt();

	if (image) {
		async.cacheImage(id, filename, allow_rgb);
	} else {
		async.cacheSound(id, filename);
	}
	return RET_CONTINUE;
}

int ONScripter::loadCacheCommand() {
	// Parameters: ID, filename (no tags!), optional bool allow_rgb (true by default)

	bool image{script_h.isName("cache_img")};

	int id = script_h.readInt();
	std::string filename(script_h.readFilePath());

	bool allow_rgb{true};
	if (script_h.hasMoreArgs())
		allow_rgb = script_h.readInt();

	if (image) {
		loadImageIntoCache(id, filename, allow_rgb);
	} else {
		loadSoundIntoCache(id, filename);
	}
	return RET_CONTINUE;
}

int ONScripter::dropCacheCommand() {
	// drop_cache_img only.
	// Parameters: ID (or the unquoted string "all"), filename (no tags!)

	bool all{false};
	int id{0};

	if (script_h.compareString("all")) {
		script_h.readName();
		all = true;
	}
	if (!all)
		id = script_h.readInt();

	std::string filename(script_h.readFilePath());

	dropCache(all ? nullptr : &id, filename);

	return RET_CONTINUE;
}

int ONScripter::borderPaddingCommand() {
	sentence_font.borderPadding = script_h.readInt();

	if (script_h.hasMoreArgs()) {
		name_font.borderPadding = script_h.readInt();
	}

	return RET_CONTINUE;
}

int ONScripter::blurCommand() {
	blur_mode[BeforeScene] = blur_mode[AfterScene];
	blur_mode[AfterScene]  = script_h.readInt();

	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);

	return RET_CONTINUE;
}

int ONScripter::blendModeCommand() {
	bool lsp2 = false;
	if (script_h.isName("blend_mode2"))
		lsp2 = true;

	/* Match the string and save it (since it has to be first parameter...) */
	const char *str_add = "add", *str_sub = "sub", *str_mul = "mul", *str_nor = "nor";
	const char *matched_string = nullptr;
	for (auto s : {str_add, str_sub, str_mul, str_nor}) {
		if (script_h.compareString(s)) {
			matched_string = s;
			script_h.readName();
			break;
		}
	}

	/* Read lsp number. */
	int sprite_num = script_h.readInt();
	AnimationInfo *anim;
	if (lsp2)
		anim = &sprite2_info[sprite_num];
	else
		anim = &sprite_info[sprite_num];

	if (!anim->exists)
		return RET_CONTINUE;
	backupState(anim);

	if (matched_string) {
		if (matched_string == str_add && anim->blending_mode != BlendModeId::ADD) {
			anim->blending_mode = BlendModeId::ADD;
			if (anim->visible)
				dirtySpriteRect(sprite_num, lsp2);
		} else if (matched_string == str_sub && anim->blending_mode != BlendModeId::SUB) {
			anim->blending_mode = BlendModeId::SUB;
			if (anim->visible)
				dirtySpriteRect(sprite_num, lsp2);
		} else if (matched_string == str_mul && anim->blending_mode != BlendModeId::MUL) {
			anim->blending_mode = BlendModeId::MUL;
			if (anim->visible)
				dirtySpriteRect(sprite_num, lsp2);
		} else if (matched_string == str_nor && anim->blending_mode != BlendModeId::NORMAL) {
			anim->blending_mode = BlendModeId::NORMAL;
			if (anim->visible)
				dirtySpriteRect(sprite_num, lsp2);
		}
	}

	return RET_CONTINUE;
}

// abgm_prop value, duration, equation
int ONScripter::bgmPropertyCommand() {
	int value     = 0;
	int duration  = 0;
	int equation  = MOTION_EQUATION_LINEAR;
	bool override = false;
	value         = script_h.readInt();
	if (script_h.hasMoreArgs())
		duration = script_h.readInt();
	if (script_h.hasMoreArgs())
		equation = script_h.readInt();
	if (script_h.hasMoreArgs())
		override = script_h.readInt() == 1;
	dynamicProperties.addGlobalProperty(true, GLOBAL_PROPERTY_BGM_CHANNEL_VOLUME, value, duration, equation, override);
	return RET_CONTINUE;
}

// ach_prop ch, value, duration, equation
int ONScripter::mixChannelPropertyCommand() {
	auto ch       = validChannel(script_h.readInt());
	int value     = 0;
	int duration  = 0;
	int equation  = MOTION_EQUATION_LINEAR;
	bool override = false;
	if (script_h.hasMoreArgs())
		value = script_h.readInt();
	if (script_h.hasMoreArgs())
		duration = script_h.readInt();
	if (script_h.hasMoreArgs())
		equation = script_h.readInt();
	if (script_h.hasMoreArgs())
		override = script_h.readInt() == 1;
	dynamicProperties.addGlobalProperty(true, GLOBAL_PROPERTY_MIX_CHANNEL_VOLUME | ch, value, duration, equation, override);
	return RET_CONTINUE;
}

int ONScripter::mixChannelPropertyWaitCommand() {
	auto ch = validChannel(script_h.readInt());

	dynamicProperties.waitOnGlobalProperty(GLOBAL_PROPERTY_MIX_CHANNEL_VOLUME | ch);

	return RET_CONTINUE;
}

int ONScripter::bgmPropertyWaitCommand() {
	dynamicProperties.waitOnGlobalProperty(GLOBAL_PROPERTY_BGM_CHANNEL_VOLUME);

	return RET_CONTINUE;
}

// Sets the default button to hover for Arrow control mode.
int ONScripter::btnhover_dCommand() {
	auto newDefault            = script_h.readInt();
	hoveredButtonDefaultNumber = newDefault;
	return RET_CONTINUE;
}

int ONScripter::btnasyncCommand() {
	//enable/disable flag
	bool currentState{btnasync_active};
	bool newState = script_h.readInt();

	if (newState == currentState) {
		if (newState && !atomic_flag) {
			waitEvent(0);
		}
		return RET_CONTINUE;
	}

	if (newState) {
		// Enable btnasync.
		// First check if there's a ButtonMonitorAction that already exists. If so, we'll just keep that one.
		Lock lock(&registeredCRActions);
		auto existingBMAs = fetchConstantRefreshActions<ButtonMonitorAction>();
		if (existingBMAs.size() == 1) {
			dynamic_cast<ButtonMonitorAction *>(existingBMAs.front().get())->keepAlive();
		} else {
			// Set up a new ButtonMonitorAction.
			ButtonMonitorAction *action{ButtonMonitorAction::create()};
			action->event_mode = WAIT_BUTTON_MODE;
			registeredCRActions.emplace_back(action);
		}
		btnasync_draw_required = true;
	}

	else {
		// End btnasync.
		// Kill ButtonMonitorAction.
		Lock lock(&registeredCRActions);
		for (const auto &a : fetchConstantRefreshActions<ButtonMonitorAction>()) a->terminate();
	}

	btnasync_active = newState;
	return RET_CONTINUE;
}

int ONScripter::backupDisableCommand() {
	do {
		std::string file = script_h.readFilePath();
#if defined(IOS) && defined(USE_OBJC)
		for (size_t n = 0, sz = archive_path.getPathNum(); n < sz; n++) {
			std::string currpath = archive_path.getPath(n) + file;
			if (FileIO::accessFile(currpath)) {
				backupDisable(currpath.c_str());
				sendToLog(LogLevel::Info, "[Optimisation] Should not backup %s\n", currpath.c_str());
			}
		}
#endif
	} while (script_h.hasMoreArgs());

	return RET_CONTINUE;
}

int ONScripter::apiCompatCommand() {
	// params: %dst_var, API_FEATURESET, API_COMPAT, API_PATCH
	// 1 is returned if engine is compatible
	//(check comments near API_XX definitions)

	script_h.readVariable();
	script_h.pushVariable();

	bool features{script_h.readInt() <= API_FEATURESET};
	bool compat{script_h.readInt() == API_COMPAT};
	bool patch{script_h.readInt() <= API_PATCH};

	script_h.setInt(&script_h.pushed_variable, features && compat && patch);

	return RET_CONTINUE;
}

int ONScripter::aliasFontCommand() {
	Fontinfo::FontAlias type = Fontinfo::FontAlias::Italic;
	if (script_h.isName("bold_font"))
		type = Fontinfo::FontAlias::Bold;
	else if (script_h.isName("bold_italic_font"))
		type = Fontinfo::FontAlias::BoldItalic;

	int from = script_h.readInt();
	int to   = script_h.readInt();
	sentence_font.aliasFont(type, from, to);

	return RET_CONTINUE;
}

// Allows you to explicitly specify the choice vector size at which to stop superskip.
int ONScripter::acceptChoiceVectorSizeCommand() {
	script_h.choiceState.acceptChoiceVectorSize = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::acceptChoiceNextIndexCommand() {
	script_h.choiceState.acceptChoiceNextIndex = script_h.readInt();
	return RET_CONTINUE;
}

int ONScripter::acceptChoiceCommand() {
	int branchToFollow;
	if (script_h.choiceState.acceptChoiceNextIndex >= static_cast<uint32_t>(script_h.choiceState.acceptChoiceVectorSize)) {
		// We are out of entries in the choice vector.
		// We must terminate super skip early.
		branchToFollow = -1;
	} else {
		branchToFollow = script_h.choiceState.choiceVector[script_h.choiceState.acceptChoiceNextIndex++];
	}

	script_h.readVariable();
	if (branchToFollow == -1) {
		// Terminate super skip early
		tryEndSuperSkip(true); // you better be in super skip if you're using this command :)
	} else {
		// Return the branch to follow into the passed variable.
		if (script_h.current_variable.type != VariableInfo::TypeInt) {
			errorAndExit("Invalid argument for accept_choice: takes an int argument to output the choice to");
		}
		script_h.setInt(&script_h.current_variable, branchToFollow);
	}

	return RET_CONTINUE;
}

int ONScripter::atomicCommand() {
	atomic_flag = script_h.readInt();
	return RET_CONTINUE;
}
