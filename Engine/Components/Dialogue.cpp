/**
 *  Dialogue.cpp
 *  ONScripter-RU
 *
 *  Text parsing and rendering code.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Components/Dialogue.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Fonts.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Entities/Font.hpp"
#include "Support/Unicode.hpp"

#include <SDL2/SDL_gpu.h>

#include <utility>
#include <deque>
#include <vector>
#include <string>
#include <sstream>
#include <locale>

DialogueController dlgCtrl;

// ---------------------------------------------------------
// Class functions
// ---------------------------------------------------------

int DialogueController::ownInit() {
	slre_compile(R"((\[.+?\]))", std::strlen(R"((\[.+?\]))"), 0, &regexInfo);
	return 0;
}

int DialogueController::ownDeinit() {
	// Clean the images in the dialogue controller
	setDialogueActive(false);

	return 0;
}

// ----------------------- functions related to processing, parsing, execution -----------------------------

void DialogueController::setDialogueName(const char *buf) {
	std::u16string name;
	decodeUTF8String(buf, name);
	if (dialogueName != name) {
		dialogueName = name;
		nameLayouted = false;
		if (name.empty()) {
			nameRenderState.clear();
		}
	}
}

void DialogueController::feedDialogueTextData(const char *dataStr) {
	// If this introduces speed issues... though I think it shouldn't... then find a suitable location in the headers to place a bool.
	auto it = ons.ons_cfg_options.find("dialogue-style");
	if (it != ons.ons_cfg_options.end()) {
		// Not recommended for usage. Works, but does not play nice with the current implementation of presets (which may improperly override the specified font size).
		dataPart = it->second + dataStr;
	} else {
		dataPart = dataStr;
	}
	textPart = "";
	setDialogueActive();
}

DialogueController::TEXT_STATE DialogueController::handleNextPart() {
	if (!dialogueProcessingState.layoutDone)
		layoutDialogue();

	const char *dataPartC = dataPart.c_str();

	if (dataPartC[currentTextPos] == '[') {
		TEXT_STATE commandType = TEXT_STATE::USER_CMD;
		bool cmdSubmitted{false};
		bool cmdIgnored{false};
		std::string temp;
		uint32_t codepoint;

		do {
			int len = decodeUTF8Symbol(dataPartC + currentTextPos, codepoint);
			if (codepoint == ']' || codepoint == ((cmdSubmitted) ? '*' : ' ')) {
				if (!cmdSubmitted) {
					currentCommand = temp;
					if (temp[0] == '@' || temp[0] == '|' ||
					    temp[0] == '\\' || temp[0] == '#' ||
					    temp[0] == '*') {
						commandType = TEXT_STATE::TEXT_CMD;
						if (temp[0] == '|') {
							ons.variableQueue.push("0");
							ons.variableQueue.push(currentVoiceWait);
						}
					} else if (temp[0] == '!') {
						int time{0};
						if (ons.unpackInlineCall(temp.c_str(), time) == 0) // not skippable, wait
							currentCommand = "wait";
						else //skippable, delay
							currentCommand = "delay";

						if (!ons.ignored_inline_func_lut.empty() && ons.ignored_inline_func_lut.count(currentCommand.c_str())) {
							cmdIgnored = true;
						} else {
							ons.variableQueue.push(std::to_string(time));
						}

						commandType = TEXT_STATE::SYS_CMD;
					} else if (ons.isBuiltInCommand(temp.c_str())) {
						if (temp == "gosub" || temp == "goto")
							ons.errorAndExit("Cannot use inline gosub/goto commands, use defsub");
						commandType = TEXT_STATE::SYS_CMD;
					}
					if (currentCommand.length() >= sizeof(ons.script_h.current_cmd)) {
						ons.errorAndExit("command overflow");
					}

					if (temp[0] != '!' && !ons.ignored_inline_func_lut.empty() && ons.ignored_inline_func_lut.count(currentCommand.c_str()))
						cmdIgnored = true;

					cmdSubmitted = true;
					temp.clear();
				} else {
					if (!cmdIgnored) {
						if (codepoint != ']')
							temp += ','; //more arguments to go
						ons.variableQueue.push(temp);
					}
					temp.clear();
				}
			} else if (codepoint == '\0') {
				ons.errorAndExit("Inline commands should end with ] symbol!");
			} else if (codepoint != '[') {
				for (int i = 0; i < len; i++)
					temp += *(dataPartC + currentTextPos + i);
			}
			currentTextPos += len;

			// If we have no command but we are hitting the end, recall ourselves for a new command
			if (cmdIgnored && codepoint == ']') {
				return handleNextPart();
			}

		} while (codepoint != ']');

		return commandType;
	}

	if (dataPartC[currentTextPos] == '\0') {
		return TEXT_STATE::END;
	}

	uint32_t codepoint;
	do {
		int len = decodeUTF8Symbol(dataPartC + currentTextPos, codepoint);
		currentTextPos += len;
	} while (codepoint != '\0' && codepoint != '[');
	currentTextPos--;
	dialogueRenderState.segmentIndex++;
	return TEXT_STATE::TEXT;
}

bool DialogueController::wantsControl() {
	// If we're here -- the script wants control, but we may take it.
	// Take control if there are events to process.
	return dialogueProcessingState.readyToRun && !events.empty();
}

int DialogueController::processDialogueEvents() {
	int ret{ONScripter::RET_NO_READ};
	while (!events.empty()) {
		DialogueProcessingEvent &event = events.front();
		//sendToLog(LogLevel::Info, "Processing dialogue event\n");
		// check event attributes
		if (event.firstCall)
			processDialogueInitialization();
		if (event.loanExecStart) {
			startLoanExecution();
		} else if (event.loanExecEnd) {
			endLoanExecution();
		} else if (event.dialogueInlineCommandEnd) {
			executingDialogueInlineCommand = false;
			ons.inVariableQueueSubroutine  = false;
			scriptState.disposeDialogue();
			//sendToLog(LogLevel::Info, "end of inline dialogue command dialogue event\n");
			events.emplace();
		} else {
			ret = processDialogue();
		}
		events.pop();
	}
	//return ONScripter::RET_NO_READ;
	if (dialogueProcessingState.active && !continueScriptExecution && !executingDialogueInlineCommand)
		return ONScripter::RET_NO_READ;
	return ret;
}

void DialogueController::processDialogueInitialization() {
	Lock lock(&ons.registeredCRActions);
	ons.registeredCRActions.emplace_back(TextRenderingMonitorAction::create()); // alerts controller when the final glyph starts fading in
}

void DialogueController::waitForAction() {
	if (immediatelyHandleNextPart) {
		immediatelyHandleNextPart = false;
		// if we have this, are we guaranteed to be executing an inline dialogue command... yes i think so, this is a fn that directly tells dlgCtrl to wait...
		scriptState.useMainScript();
	} else {
		// cannot call startLoanExecution() directly from here; we must RET_CONTINUE first
		//sendToLog(LogLevel::Info, "loan execution start dialogue event\n");
		events.emplace_get().loanExecStart = true;
	}
}

void DialogueController::startLoanExecution() {
	loanExecutionActive = true;
	scriptState.useMainScript();
}

void DialogueController::endLoanExecution() {
	loanExecutionActive = false;
	scriptState.useDialogue(); // don't unset, we're still halfway through a dialogue command...?
}

void ScriptState::useDialogue() {
	if (swaps % 2) {
		ons.errorAndExit("Already using dialogue command script state -- something is wonky");
	}
	if (!swaps) {
		state.set(ons.script_h.getScriptStateData());
	} else {
		ons.script_h.swapScriptStateData(state.get());
	}
	swaps++;
}

void ScriptState::useMainScript() {
	if (swaps % 2 == 0) {
		ons.errorAndExit("Already using mainscript state -- something is wonky");
	}
	ons.script_h.swapScriptStateData(state.get());
	swaps++;
}

void ScriptState::disposeDialogue(bool force) {
	if (swaps % 2) {
		// using inline dialogue atm so we need to swap it before we dispose
		ons.script_h.swapScriptStateData(state.get());
	} else if (!force) {
		ons.errorAndExit("Tried to dispose a dialogue command script state when we weren't using that state at the time... unexpected but not wrong... but we will make you fix it");
	}
	state.unset();
	swaps = 0;
}

void ScriptState::disposeMainscript(bool force) {
	// be sure you know wtf you're doing if you call this function XD it is super unsafe used wrongly
	if (swaps && swaps % 2 == 0) {
		// using mainscript atm so we need to swap it before we dispose
		ons.script_h.swapScriptStateData(state.get());
	} else if (!force) {
		ons.errorAndExit("Tried to dispose a mainscript state when we weren't using that state at the time... unexpected but not wrong... but we will make you fix it");
	}
	state.unset();
	swaps = 0;
}

int DialogueController::processDialogue() {
	//sendToLog(LogLevel::Info, "processDialogue\n");
	int res{ONScripter::RET_NO_READ};
	if (!dialogueProcessingState.active)
		return res;

	//auto currentPos = ons.script_h.getCurrent();

	immediatelyHandleNextPart = false; // sorry, can't think of a better name...

	TEXT_STATE result = handleNextPart();
	switch (result) {
		case TEXT_STATE::SYS_CMD:
			//sendToLog(LogLevel::Info, "sys_cmd dialogue event (may be removed from queue in a moment, or not...)\n");
			immediatelyHandleNextPart      = true;
			executingDialogueInlineCommand = true;

			// this line backs stack up
			//sendToLog(LogLevel::Info, "useDialogue from SYS_CMD\n");
			scriptState.useDialogue();                  //backup, because string_buffer may be cleared
			ons.setVariableQueue(true, currentCommand); // correct isName, readInt, readStr etc.

			/*res = */ ons.evaluateBuiltInCommand(currentCommand.c_str());

			ons.setVariableQueue(false);
			executingDialogueInlineCommand = false;

			// It is possible we created some blocking action in this command (ConstantRefreshAction::initialize())
			if (immediatelyHandleNextPart) {
				scriptState.disposeDialogue();
			}

			break;
		case TEXT_STATE::USER_CMD:
			//sendToLog(LogLevel::Info, "useDialogue from USER_CMD\n");
			scriptState.useDialogue();

			res = ons.ScriptParser::evaluateCommand(currentCommand.c_str(), false, true);

			executingDialogueInlineCommand = true;
			ons.inVariableQueueSubroutine  = true;
			//assert(!ons.callStack.back().dialogueEventOnReturn);
			ons.callStack.back().dialogueEventOnReturn = true;
			break;
		case DialogueController::TEXT_STATE::TEXT_CMD:
			if (currentCommand == "*") {
				if (!continueScriptExecution)
					ons.errorAndExit("Used [*] in a plain (d) dialogue, use d2 command instead");
				// check if we were already allowed past this * by script...
				if (suspendDialoguePasses > 0) {
					//sendToLog(LogLevel::Info, "* (suspendDialoguePasses) dialogue event\n");
					immediatelyHandleNextPart = true;
				}
				suspendDialoguePasses--;
				break;
			} else if (currentCommand == "#") {
				if (!continueScriptExecution)
					ons.errorAndExit("Used [#] in a plain (d) dialogue, use d2 command instead");
				suspendScriptPasses[suspendScriptIndex++]++;
				//sendToLog(LogLevel::Info, "# (suspendScriptPasses) dialogue event\n");
				immediatelyHandleNextPart = true;
				break;
			}

			//sendToLog(LogLevel::Info, "useDialogue from TEXT_CMD\n");
			scriptState.useDialogue();

			if (currentCommand == "@") {
				ons.clickWait();
				if (ons.textgosub_label)
					res = ONScripter::RET_CONTINUE;
			} else if (currentCommand == R"(\)") {
				ons.clickNewPage();
				if (ons.textgosub_label)
					res = ONScripter::RET_CONTINUE;
			} else if (currentCommand == "|") {
				immediatelyHandleNextPart = true; //will be unset by waitForAction
				//if (ons.skip_mode & ONScripter::SKIP_SUPERSKIP) break;
				executingDialogueInlineCommand = true;
				ons.setVariableQueue(true, currentCommand);
				/* res = */ ons.waitvoiceCommand();
				ons.setVariableQueue(false);
				executingDialogueInlineCommand = false;
				break;
			}

			//assert(!ons.callStack.back().dialogueEventOnReturn);
			executingDialogueInlineCommand             = true;
			ons.callStack.back().dialogueEventOnReturn = true;
			//ons.callStack.back().next_script = currentPos;
			break;
		case TEXT_STATE::TEXT:
			ons.displayDialogue();
			break;
		case TEXT_STATE::END:
			// Should be no more needed, set by setDialogueActive(false);
			//suspendScriptPasses[-1]++;

			untimeAllDialogueSegments();

			if (!(ons.skip_mode & ONScripter::SKIP_SUPERSKIP)) {
				if (wndCtrl.usingDynamicTextWindow) {
					wndCtrl.updateTextboxExtension(false);
					ons.renderDynamicTextWindow(ons.text_gpu->target, nullptr, ons.refreshMode(), ons.canvasTextWindow);
				}

				renderDialogueToTarget(ons.text_gpu->target, nullptr, ons.refreshMode(), ons.canvasTextWindow);
			}

			setDialogueActive(false);
			if (ons.dialogue_add_ends) {
				ons.clickNewPage();
				//if (ons.textgosub_label) res = ONScripter::RET_CONTINUE;
			}
			res = ONScripter::RET_CONTINUE;
			break;
	}

	if (immediatelyHandleNextPart)
		events.emplace();
	immediatelyHandleNextPart = false;

	return res;
}

void DialogueController::SegmentRenderingAction::run() {
	if (segment != dlgCtrl.dialogueRenderState.segmentIndex || (ons.skip_mode & (ONScripter::SKIP_NORMAL | ONScripter::SKIP_TO_WAIT)) || ons.keyState.ctrl) {
		dlgCtrl.untimeDialogueSegment(segment);
	}
}
bool DialogueController::SegmentRenderingAction::expired() {
	return dlgCtrl.isDialogueSegmentRendered(segment);
}
void DialogueController::SegmentRenderingAction::onExpired() {
	ConstantRefreshAction::onExpired();
}
bool DialogueController::TextRenderingMonitorAction::expired() {
	return false;
}
void DialogueController::TextRenderingMonitorAction::run() {
	if (lastCompletedSegment == dlgCtrl.dialogueRenderState.segmentIndex) {
		return;
	}
	bool r{true};
	for (auto piecePtr : dlgCtrl.dialogueRenderState.segments[dlgCtrl.dialogueRenderState.segmentIndex].getPieces()) {
		auto &piece = *piecePtr;
		for (auto &glyph : piece.charRenderBuffer) {
			if (!glyph.fadeStart.expired()) {
				r = false;
				break;
			}
			// note this checks for fadeStart -- action that monitors for the start of final glyph fadein
		}
		if (!r)
			break;
	}
	if (r) {
		lastCompletedSegment = dlgCtrl.dialogueRenderState.segmentIndex;
		//sendToLog(LogLevel::Info, "TextRenderingMonitorAction dialogue event from segment %d\n", lastCompletedSegment);
		dlgCtrl.events.emplace();
	}
}
void DialogueController::TextRenderingMonitorAction::onExpired() {
	ConstantRefreshAction::onExpired();
	// better at least put a print in
	//sendToLog(LogLevel::Info, "TextRenderingMonitorAction expired.\n");
}

int DialogueController::TextRenderingMonitorAction::eventMode() {
	if (lastCompletedSegment == dlgCtrl.dialogueRenderState.segmentIndex && (ons.clickstr_state || ons.textgosub_clickstr_state)) {
		return ONScripter::IDLE_EVENT_MODE;
	}
	return ONScripter::WAIT_TIMER_MODE | ONScripter::WAIT_SLEEP_MODE | ONScripter::WAIT_TEXTOUT_MODE;
}

// ------------------------ functions related to rendering and layout --------------------------

void DialogueController::layoutName() {
	if (ons.skip_mode & ONScripter::SKIP_SUPERSKIP)
		return; // no time to do that!! gogogogo!!
	nameRenderState.clear();
	nameRenderState.tightlyFit = FIT_MODE::FIT_NONE;
	Fontinfo fi                = ons.name_font;
	fi.clear();
	layoutSegment(nameRenderState, dialogueName, fi);
	layoutLines(nameRenderState);
	getRenderingBounds(nameRenderState); // set the bounds field, window rendering will need it
	nameLayouted = true;
}

void DialogueController::layoutDialogue() {
	//ons.printClock("renderText");

	if (!nameLayouted && !dialogueName.empty())
		layoutName();

	cmp::optional<Fontinfo> fi;
	if (!(ons.skip_mode & ONScripter::SKIP_SUPERSKIP))
		fi.set(ons.sentence_font);

	unsigned int pos{0};
	if (!(ons.skip_mode & ONScripter::SKIP_SUPERSKIP))
		dialogueRenderState.clickParts.emplace_back();

	const char *dataPartC = dataPart.c_str();
	size_t dataPartLen    = dataPart.length();

	while (true) {
		slre_cap result;

		if (pos > dataPartLen) {
			ons.errorAndExit("pos is greater than dataPartLen somehow");
		}

		int bytesScanned = slre_match_reuse(&regexInfo, dataPartC + pos, static_cast<int>(dataPartLen - pos), &result, 1);

		// Look for the text(part) prefixing the match, or to the end of the string if there was no match
		auto prefixLength = static_cast<int>(bytesScanned > 0 ? bytesScanned - result.len : dataPartLen - pos);
		if (prefixLength == 0 && bytesScanned < 0) {
			// No text part or command found, we're done
			break;
		}

		if (prefixLength > 0) {
			// Found a text part
			textPart.append(dataPartC + pos, prefixLength);
			std::u16string resultStr;
			decodeUTF8String(dataPartC + pos, resultStr, prefixLength);
			//sendToLog(LogLevel::Info, "Part: %s\n", decodeUTF16String(resultStr).c_str());

			//ons.printClock("rendering part");
			if (!(ons.skip_mode & ONScripter::SKIP_SUPERSKIP))
				layoutSegment(dialogueRenderState, resultStr, fi.get());
			//ons.printClock("rendered part");
		}

		if (bytesScanned < 0)
			break;

		// Skip past the text and the following command
		pos += bytesScanned;
		if (!(ons.skip_mode & ONScripter::SKIP_SUPERSKIP) && std::strncmp(result.ptr, "[@]", 3) == 0) {
			// Start a new click-part
			dialogueRenderState.clickParts.emplace_back();
		}

		if (pos == dataPartLen)
			break;
	}
	if (ons.skip_mode & ONScripter::SKIP_SUPERSKIP) {
		//this is not true but it should save us from processing the dialogue multiple times
		dialogueProcessingState.layoutDone = true;
		return;
	}

	layoutLines(dialogueRenderState);
	wndCtrl.updateTextboxExtension(false);

	decodeUTF8String(dataPartC, dataPartUnicode);
	dialogueProcessingState.layoutDone = true;
}

// Responsible for centering lines and putting them at sensible y positions so they don't collide due to different fontsize (also including ruby)
void DialogueController::layoutLines(TextRenderingState &state) {
	// Vertically position lines
	float y{0}, previousDescender{0}, lineHeightMultiplier{0};
	if (!state.getPieces().empty()) {
		auto &fi             = state.getPieces().front()->getPreFontInfo();
		lineHeightMultiplier = static_cast<float>(fi.style().line_height) / static_cast<float>(fi.style().font_size);
		y                    = fi.y();
	}
	for (auto &line : state.lines) {
		bool hasAtLeastOneChar{false};

		for (auto piece : line.getPieces(true)) {
			if (std::any_of(piece->charRenderBuffer.begin(), piece->charRenderBuffer.end(),
			                [](RenderBufferGlyph &g) { return !g.applyNewFontinfoHere && !g.renderRubyHere; })) {
				hasAtLeastOneChar = true;
				break;
			}
		}

		if (!hasAtLeastOneChar && !line.getPieces().empty()) {
			Fontinfo &myFi           = line.getPieces().front()->getPreFontInfo();
			const GlyphValues *glyph = myFi.renderUnicodeGlyph(','); // any random letter will do, they all have the same fontface (not glyph) ascender/descenders
			line.maxAscender         = glyph->faceAscender;
			line.maxDescender        = glyph->faceDescender;
		}

		if (!line.rubyPieces.empty())
			previousDescender /= 2;

		y += (line.maxAscender + previousDescender) * lineHeightMultiplier;
		line.position.y   = y;
		previousDescender = line.maxDescender;
	}

	// Horizontally position and scale lines
	for (auto &line : state.lines) {
		if (line.pieces.empty())
			continue;

		DialoguePiece &frontPiece = *line.pieces.front();
		DialoguePiece &backPiece  = *line.pieces.back();

		Fontinfo &fi   = backPiece.getPostFontInfo();
		auto &style    = fi.style(); //don't forget~ (speed)
		bool centered  = line.inlineOverrides.is_centered.get(style.is_centered);
		bool fitted    = line.inlineOverrides.is_fitted.get(style.is_fitted);
		int wrap_limit = line.inlineOverrides.wrap_limit.get(style.wrap_limit);

		if (!centered && !fitted)
			continue;

		// TODO for Chinese support:
		// If you want the right edge of each lines to be even instead of ragged:
		// Write & call a function to adjust spacing between glyphs on the line to fit the wrap_limit.
		// Flexible spacing between glyphs should be added as another inlineOverride alongside centered/fitted/wrap_limit/etc.
		// It may be worth splitting the various parts of this function into subfunctions before doing this.
		// It is getting unwieldy.
		// (Also take care that any new fields are properly saved/loaded in writeFontinfo and readFontinfo...)

		float xStart    = frontPiece.position.x;
		float xEnd      = backPiece.position.x + backPiece.position.w - (backPiece.borderPadding * 2);
		float xWidth    = xEnd - xStart;
		float areaWidth = wrap_limit;
		if (fitted && /* we're too large */ xWidth > areaWidth) {
			// Make it smaller
			line.horizontalResize = areaWidth / xWidth;
			xWidth                = areaWidth;
		}
		if (centered) {
			// Move us left or right as appropriate
			// (Note: Shouldn't end up moving us left, I think, because we should have been line-broken earlier in that case.)
			line.position.x += (areaWidth - xWidth) / 2.0;
		}
	}

	// Apply the line positioning to the pieces
	for (auto &line : state.lines) {
		if (line.pieces.empty())
			continue;
		float frontX = line.pieces.front()->position.x;
		for (auto piecePtr : line.getPieces(true)) {
			DialoguePiece &piece = *piecePtr;
			piece.position.x += line.position.x;
			if (line.horizontalResize != 1.0) {
				piece.position.x -= frontX;
				piece.position.x *= line.horizontalResize;
				piece.position.x += frontX;
				piece.horizontalResize = line.horizontalResize;
			}
			piece.position.y += std::floor(line.position.y - piece.getPreFontInfo().y());
			/*if (!state.tightlyFitBottomEdge)
				piece.position.h += line.maxDescender - piece.usedSpaceBelowBaseline;*/
			// we hope that we no longer need this
		}
	}

	// Set cursor positions
	for (auto &segment : state.segments) {
		if (segment.getPieces().empty())
			continue;
		auto &lastPiece          = *segment.getPieces().back();
		segment.cursorPosition.x = lastPiece.position.x + lastPiece.position.w - (lastPiece.borderPadding * 2);
		segment.cursorPosition.y = lastPiece.position.y + lastPiece.baseline;
	}
}

void DialogueController::getRenderingBounds(TextRenderingState &state, bool visiblePiecesOnly) {
	// find maximum limits over all pieces
	// If we have a centred piece on a 1920 target, which is 500 px wide, we will use a
	// (1920-500)/2 + 500 = 1210 px wide image.
	// This may not be optimal but conforms to ONScripter ABI logic: position anything you are asked to
	// at specified coordinates. Check render(...) function for more info, which uses p.position.x for
	// x rendering on a target. Also note, how the resulting image is used outside of DialogueController.
	GPU_Rect max{0, 0, 0, 0};
	bool first{true};

	int i{0};
	for (auto &seg : state.segments) {
		if (visiblePiecesOnly && &state == &dialogueRenderState && i > state.segmentIndex)
			continue;
		for (auto piecePtr : seg.getPieces()) {
			DialoguePiece &piece = *piecePtr;
			if (visiblePiecesOnly) {
				if (piece.charRenderBuffer.empty())
					continue;
				RenderBufferGlyph front{piece.charRenderBuffer.front()};
				if (!front.fadeStart.expired())
					continue;
			}

			// Update left and top edges of bounding box and stretch the length if necessary
			if (first || piece.position.x < max.x) {
				if (!first)
					max.w += max.x - piece.position.x;
				max.x = piece.position.x;
			}
			if (first || piece.position.y < max.y) {
				if (!first)
					max.h += max.y - piece.position.y;
				max.y = piece.position.y;
			}
			// Update right and bottom edges of bounding box
			if (piece.position.x + piece.position.w > max.x + max.w)
				max.w = piece.position.x + piece.position.w - max.x;
			if (piece.position.y + piece.position.h > max.y + max.h)
				max.h = piece.position.y + piece.position.h - max.y;
			first = false;
		}
		i++;
	}
	state.bounds = max;
}

void DialogueController::renderToTarget(GPU_Target *dst, GPU_Rect *dstClip, char *buf, Fontinfo *f_info, bool paddingShift, int tightlyFit) {
	std::u16string text;
	decodeUTF8String(buf, text);
	renderToTarget(dst, dstClip, text, f_info, paddingShift, tightlyFit);
}

void DialogueController::renderToTarget(GPU_Target *dst, GPU_Rect *dstClip, std::u16string &text, Fontinfo *f_info, bool paddingShift, int tightlyFit) {
	Fontinfo fi = f_info ? *f_info : ons.sentence_font;
	while (fi.styleStack.size() > 1) fi.styleStack.pop();
	TextRenderingState state;
	state.shiftSpriteDrawByBorderPadding = paddingShift;
	state.tightlyFit                     = tightlyFit;
	if (dst) {
		state.dst.target = dst;
		state.dstClip    = dstClip;
	}
	// Rendering is a three-step process.
	// Firstly, the text is parsed and wrapped, and each character is assigned its proper position, font styling, and so on.
	layoutSegment(state, text, fi);
	// Then, once we know which glyphs are on which line, line-specific processing like centering is performed.
	layoutLines(state);
	if (dst) {
		// Finally, the characters are rendered in the positions they were layouted to.
		render(state);
	} else if (dstClip) {
		getRenderingBounds(state);
		*dstClip = state.bounds; // use dstClip field as an output for size taken (dummy draw)
	}
	state.clear();
}

void DialogueController::prepareForRendering(const char *buf, Fontinfo &f_info, TextRenderingState &state, uint16_t &w, uint16_t &h) {
	w = h = 0; // Nothing by default
	std::u16string text;
	decodeUTF8String(buf, text);
	//sendToLog(LogLevel::Info, "Rendering LSP to new image: %s\n", decodeUTF16String(resultStr).c_str());
	while (f_info.styleStack.size() > 1) f_info.styleStack.pop();

	layoutSegment(state, text, f_info);
	if (state.getPieces().empty())
		return; // >D

	layoutLines(state);
	getRenderingBounds(state);

	w = state.bounds.w + state.bounds.x;
	h = state.bounds.h + state.bounds.y;
}

void DialogueController::layoutSegment(TextRenderingState &state, std::u16string text, Fontinfo &fi) {

	// Could maybe do this in constructor but let's do it here..
	if (state.lines.empty()) {
		state.lines.emplace_back();
	}
	state.segments.emplace_back();
	if (!state.clickParts.empty())
		state.clickParts.back().segments.push_back(&state.segments.back());
	while (!text.empty()) {
		std::deque<DialoguePiece> rubyPieces;

		while (text.length() >= 1 && text.at(0) == '\n') {
			text.erase(0, 1);
			fi.newLine();
			state.lines.emplace_back();
		}

		DialoguePiece piece = layoutPiece(state, text, fi, &rubyPieces);

		if (piece.inlineOverrides.startsNewRun.get(false)) {
			state.segments.back().runs.emplace_back();
			piece.inlineOverrides.startsNewRun.unset(); // FIXME (This one may be broken?)
		}

		assert(!state.segments.empty());
		assert(!state.segments.back().runs.empty());

		auto &seg         = state.segments.back();
		auto &runs        = seg.runs;
		auto &run         = runs.back();
		auto &pieces_mine = run.pieces;
		pieces_mine.push_back(piece);

		state.lines.back().pieces.push_back(&state.segments.back().runs.back().pieces.back());
		for (auto &ruby : rubyPieces) {
			state.segments.back().runs.back().rubyPieces.push_back(ruby);
			state.lines.back().rubyPieces.push_back(&state.segments.back().runs.back().rubyPieces.back());
		}
	}
}

void DialogueController::timeCurrentDialogueSegment() {
	if (dialogueRenderState.segmentIndex == -1)
		return;
	auto &seg = dialogueRenderState.segments[dialogueRenderState.segmentIndex];
	int speed = textDisplaySpeed.get(ons.text_display_speed);
	int fade  = textFadeDuration.get(ons.text_fade_duration);

	char16_t prevCodepoint = 0;

	for (auto &run : seg.runs) {
		int usedMs      = 0;
		int rubiesTimed = 0;
		for (auto &piece : run.pieces) {
			for (auto &glyph : piece.charRenderBuffer) {
				if (glyph.renderRubyHere) {
					// special char to indicate it's time to time ruby!
					DialoguePiece &thisRuby = run.rubyPieces[rubiesTimed];
					for (auto &rubyGlyph : thisRuby.charRenderBuffer) {
						//sendToLog(LogLevel::Info, "Timing RUBY glyph %d at %d\n", rubyGlyph.codepoint, usedMs);
						rubyGlyph.fadeStart.setCountdown(usedMs);
						rubyGlyph.fadeStop.setCountdown(usedMs + fade);
						rubyGlyph.fadeDuration = fade;
					}
					rubiesTimed++;
					continue;
				}
				if (glyph.applyNewFontinfoHere) {
					continue;
				}

				// crude hack to allow use of "..." instead of "…" and handle "?!" and other strange things
				// would be better if we did this on some other basis than per-glyph
				bool prevIsFinal = prevCodepoint == '.' || prevCodepoint == '?' || prevCodepoint == '!';
				if (prevIsFinal && glyph.codepoint != ' ') {
					usedMs -= ons.getCharacterPostDisplayDelay(prevCodepoint, speed);
					usedMs += ons.getCharacterPostDisplayDelay(prevCodepoint == '.' ? u'⅓' : '1', speed);
				}
				usedMs += ons.getCharacterPreDisplayDelay(glyph.codepoint, speed);
				//sendToLog(LogLevel::Info, "Timing glyph %d at %d\n", glyph.codepoint, usedMs);
				glyph.fadeStart.setCountdown(usedMs);
				glyph.fadeStop.setCountdown(usedMs + fade);
				glyph.fadeDuration = fade;
				usedMs += ons.getCharacterPostDisplayDelay(glyph.codepoint, speed);
				prevCodepoint = glyph.codepoint;
			}
		}
	}
}

void DialogueController::untimeDialogueSegment(int segment) {
	for (auto piecePtr : dlgCtrl.dialogueRenderState.segments[segment].getPieces(true)) {
		auto &piece = *piecePtr;
		for (auto &glyph : piece.charRenderBuffer) {
			glyph.fadeStart.reset();
			glyph.fadeStop.reset();
		}
	}
}

void DialogueController::untimeAllDialogueSegments() {
	for (unsigned int i = 0; i < dlgCtrl.dialogueRenderState.segments.size(); i++) {
		untimeDialogueSegment(i);
	}
}

DialoguePiece DialogueController::layoutPiece(TextRenderingState &state, std::u16string &text, Fontinfo &fontInfo, std::deque<DialoguePiece> *rubyPieces) {
	DialoguePiece p;

	// xPxLeft ranges from 0 upwards
	p.xPxLeft = fontInfo.layoutData.xPxLeft;
	p.setPreFontInfo(fontInfo);
	p.borderPadding         = fontInfo.borderPadding;
	layoutPieceTmpFreshText = text;

	// x ranges from left edge x position upwards
	auto x = fontInfo.x();
	auto y = fontInfo.y();

	// Do the layout
	bool fittedSomething = addFittingChars(p, layoutPieceTmpFreshText, text, rubyPieces, !(state.dst.target));

	// If nothing fits, we will need to newline and retry.
	if (!fittedSomething) {
		p.resetLayoutInfo();
		fontInfo.newLine();
		state.lines.emplace_back();
		p.setPreFontInfo(fontInfo);
		p.xPxLeft               = fontInfo.layoutData.xPxLeft;
		x                       = fontInfo.x();
		y                       = fontInfo.y();
		layoutPieceTmpFreshText = text;
		fittedSomething         = addFittingChars(p, layoutPieceTmpFreshText, text, rubyPieces, !(state.dst.target));
		// If it STILL doesn't fit, we are screwed.
		if (!fittedSomething) {
			ons.errorAndExit("Failed to render not fitting part of a text segment!");
		}
	}

	fontInfo   = p.getPostFontInfo();
	p.xPxRight = fontInfo.layoutData.xPxRight;

	float above{state.tightlyFit & FIT_MODE::FIT_TOP ? p.verticalSize.usedSpaceAboveBaseline : p.verticalSize.maxAscend};
	float below{state.tightlyFit & FIT_MODE::FIT_BOTTOM ? p.verticalSize.usedSpaceBelowBaseline : p.verticalSize.maxDescend};

	DialogueLine &line = state.lines.back();
	p.baseline         = above;
	p.position.x       = std::floor(x);
	p.position.y       = std::floor(y - p.baseline);
	p.position.w       = (p.xPxRight - p.xPxLeft) + p.borderPadding * 2;
	p.position.h       = above + below + p.borderPadding * 2;

	if (p.verticalSize.maxAscend > line.maxAscender)
		line.maxAscender = p.verticalSize.maxAscend;
	if (p.verticalSize.maxDescend > line.maxDescender)
		line.maxDescender = p.verticalSize.maxDescend;

	line.inlineOverrides |= p.inlineOverrides;

	text = layoutPieceTmpFreshText;

	return p;
}

void DialogueController::layoutRubyPiece(DialoguePiece &mainPiece, DialoguePiece &rubyPiece, size_t rubyStartPosition, float xFinish, bool measure) {
	auto &preFontInfo = rubyPiece.getPreFontInfo();
	auto xStart       = preFontInfo.x();
	//auto xFinish = rubyPiece.getPostFontInfo().x();
	auto y = preFontInfo.y();

	rubyPiece.xPxLeft                    = preFontInfo.layoutData.xPxLeft;
	rubyPiece.borderPadding              = preFontInfo.borderPadding;
	preFontInfo.changeStyle().wrap_limit = 99999; // don't wrap.
	auto text                            = rubyPiece.text;
	addFittingChars(rubyPiece, text, rubyPiece.text, nullptr, measure);

	float raise{0};
	for (auto it = mainPiece.charRenderBuffer.begin() + rubyStartPosition; it != mainPiece.charRenderBuffer.end(); ++it) {
		RenderBufferGlyph &r = *it;
		if (r.applyNewFontinfoHere) {
			continue;
		} // can occur in case of smart quotes
		if (r.gv->maxy > raise)
			raise = r.gv->maxy;
	}
	raise *= 1.2; // position ruby's baseline slightly higher than the topmost max-y of the spanned glyphs
	raise += rubyPiece.verticalSize.usedSpaceBelowBaseline;

	rubyPiece.xPxRight   = rubyPiece.getPostFontInfo().layoutData.xPxRight;
	rubyPiece.baseline   = rubyPiece.verticalSize.usedSpaceAboveBaseline;
	rubyPiece.position.y = y - rubyPiece.baseline - raise;
	rubyPiece.position.w = (rubyPiece.xPxRight - rubyPiece.xPxLeft) + rubyPiece.borderPadding * 2;
	rubyPiece.position.x = xStart - ((rubyPiece.xPxRight - rubyPiece.xPxLeft) - (xFinish - xStart)) / 2; // center it
	rubyPiece.position.h = rubyPiece.verticalSize.usedSpaceAboveBaseline + rubyPiece.verticalSize.usedSpaceBelowBaseline + rubyPiece.borderPadding * 2;

	// update main piece's "usedSpaceAboveBaseline" and "maxAscend" to take account of the space used by ruby
	if (rubyPiece.verticalSize.usedSpaceAboveBaseline + raise > mainPiece.verticalSize.maxAscend)
		mainPiece.verticalSize.maxAscend = rubyPiece.verticalSize.usedSpaceAboveBaseline + raise;
	if (rubyPiece.baseline + raise > mainPiece.verticalSize.usedSpaceAboveBaseline)
		mainPiece.verticalSize.usedSpaceAboveBaseline = rubyPiece.baseline + raise;

	RenderBufferGlyph rubyMarker;
	rubyMarker.renderRubyHere = true;
	// display ruby when half the main text is displayed
	size_t rubyInsertionPos = (rubyStartPosition + mainPiece.charRenderBuffer.size()) / 2;
	mainPiece.charRenderBuffer.insert(mainPiece.charRenderBuffer.begin() + rubyInsertionPos, rubyMarker);
}

void DialogueController::render(TextRenderingState &state) {
	int i = 0;
	for (auto &seg : state.segments) {
		if (&state == &dialogueRenderState && i > state.segmentIndex)
			return;
		for (auto piecePtr : seg.getPieces(true)) {
			renderPiece(state, *piecePtr);
		}
		i++;
	}
}

void DialogueController::renderDialogueToTarget(GPU_Target *dst, GPU_Rect *dstClip, int /*refresh_mode*/, bool camera) {
	GPU_Rect offset = {0, 0, static_cast<float>(ons.text_gpu->w), static_cast<float>(ons.text_gpu->h)};

	if (camera) {
		offset.x += ons.camera.center_pos.x;
		offset.y += ons.camera.center_pos.y;
	}

	if (dialogueProcessingState.active) {
		if (wndCtrl.usingDynamicTextWindow && nameLayouted) {
			wndCtrl.updateTextboxExtension(true);
			nameRenderState.dst.target = dst;
			//dlgCtrl.nameRenderState.dstClip = // think it can be ignored for debug?
			nameRenderState.offset = wndCtrl.getPrintableNameBoxRegion();
			nameRenderState.offset.x += (camera ? ons.camera.center_pos.x : 0);
			nameRenderState.offset.y += (camera ? ons.camera.center_pos.y : 0);
			nameRenderState.offset.x -= nameRenderState.bounds.x;
			nameRenderState.offset.y -= nameRenderState.bounds.y;
			render(nameRenderState);
		}

		if (dialogueRenderState.segmentIndex != -1) {
			//GPU_Rect ourClip {0,0,0,0};
			dialogueRenderState.dst.target = dst;
			//dialogueRenderState.dstClip = &ourClip;
			dialogueRenderState.offset                         = offset;
			dialogueRenderState.shiftSpriteDrawByBorderPadding = false;
			if (wndCtrl.usingDynamicTextWindow) {
				dialogueRenderState.offset.y -= wndCtrl.extension;
				/*ourClip = wndCtrl.getExtendedWindow();
				 if (camera) {
				 ourClip.x += ons.camera.center_pos.x;
				 ourClip.y += ons.camera.center_pos.y;
				 }*/
			}
			dialogueRenderState.dstClip = dstClip;

			render(dialogueRenderState);
			dialogueRenderState.dstClip = nullptr;
		}
	} else {
		if (ons.canvasTextWindow)
			offset.x = offset.y = 0;
		gpu.copyGPUImage(ons.text_gpu, nullptr, dstClip, dst, offset.x, offset.y);
	}
}

void DialogueController::renderPiece(TextRenderingState &state, DialoguePiece &piece) {
	// Shadow border
	//ons.printClock("shadow border");
	renderAddedChars(state, piece, true, true);
	// Shadow glyph
	//ons.printClock("shadow glyph");
	renderAddedChars(state, piece, false, true);
	// Regular border
	//ons.printClock("regular border");
	renderAddedChars(state, piece, true, false);
	// Regular glyph
	//ons.printClock("regular glyph");
	renderAddedChars(state, piece, false, false);
}

void DialogueController::advanceDialogueRendering(uint64_t ns) {
	if (!dialogueIsRendering)
		return;
	if (dialogueRenderState.segmentIndex == -1)
		return;

	for (int segNo = 0; segNo <= dialogueRenderState.segmentIndex; segNo++) {
		DialogueSegment &seg = dialogueRenderState.segments[segNo];
		for (auto piecePtr : seg.getPieces(true)) {
			auto &piece = *piecePtr;
			for (auto &glyph : piece.charRenderBuffer) {
				if (glyph.fadeStop.expired())
					continue;
				glyph.fadeStart.tickNanos(ns);
				glyph.fadeStop.tickNanos(ns);
			}
		}
	}

	// Never try to flush when we actually display no text
	if (ons.display_mode & ONScripter::DISPLAY_MODE_TEXT) {
		ons.addTextWindowClip(ons.before_dirty_rect_hud);
		ons.addTextWindowClip(ons.dirty_rect_hud);
		ons.flush(REFRESH_NORMAL_MODE, nullptr, nullptr, false, false, false
		          /*do NOT wait for cr, that would be mad, this is called FROM cr*/);
	}
}

/* This is the main dialogue layout function.
 * It does not do any rendering, just adds characters to the dialogue piece's buffer to be rendered later.
 * It adds as many characters to the buffer as will fit on the line without breaking any rules.
 *    (Actually, it adds characters until it has used *too much* space, and then it rewinds the state to the last point at which it was allowable to insert a line break.)
 * rhs is updated with the characters still remaining to be rendered.
 *
 * It would be nice if this function was smaller for easier editing.
 * It might be a good idea to create a TextLayoutingState class to hold the crazy amount of local variables this function uses,
 * and then pass it around a bunch of subfunctions instead of doing so much directly in here.
 *
 * --- WARNING ---
 * Be very, very cautious while modifying this function.
 * Text layouting is full of edge cases that are difficult to detect without playing through the entire game.
 * It would be very useful if we had tests to ensure nothing changes with the way dialogues are currently laid out, but unfortunately, we currently do not.
 */
bool DialogueController::addFittingChars(DialoguePiece &piece, std::u16string &rhs, const std::u16string &original, std::deque<DialoguePiece> *rubyPieces, bool measure) {
	// Initialize "last safe break point" state variables (these are what we rewind to).
	// Ruby uses a separate buffer so as not to overwrite the original one with the base text.
	auto &thisAddFittingChars      = !rubyPieces ? addRubyFittingCharsTmpLastSafeBreakRHS : addFittingCharsTmpLastSafeBreakRHS;
	thisAddFittingChars            = rhs;
	long lastSafeRenderBufferCount = -1;
	long lastSafeFontinfoIndex     = 0;
	size_t lastSafeRubyPiecesSize  = rubyPieces ? rubyPieces->size() : 0; // I think this might be nonzero sometimes?? Unsure
	auto lastSafeVerticalSize      = piece.verticalSize;

	// Get the font styling info that applies at the start of the text we've been given to render.
	Fontinfo &fontInfo = piece.getPreFontInfo();
	bool startOfLine   = fontInfo.layoutData.xPxLeft == 0;
	// Of course, the fontinfo (including color etc.) might change as we go through the text, so we keep track of the one that we are currently using.
	Fontinfo workingFontinfo(fontInfo);
	LayoutData lastSafeLayoutData(fontInfo.layoutData);
	bool skipSpaces = startOfLine;

	bool withinRubySpan{false};
	bool lastNewLine{false};
	cmp::optional<DialoguePiece> rubyPiece;
	size_t rubyPieceStartPosition{0};

	workingFontinfo.fontInfoChanged = false;

	// This function should be called whenever you are *certain* that it is safe to insert a newline ("linebreak" or "break").
	// If called before the character addition boundary (marked with a comment below), it indicates that it's safe to break before the character.
	// If called after the character addition boundary, it indicates that it's safe to break after that character (no matter what the following character is!)
	auto updateLastSafeData = [&]() {
		thisAddFittingChars       = rhs;
		lastSafeLayoutData        = workingFontinfo.layoutData;
		lastSafeRenderBufferCount = piece.charRenderBuffer.size();
		lastSafeFontinfoIndex     = piece.fontInfos.size() - 1;
		lastSafeRubyPiecesSize    = rubyPieces ? rubyPieces->size() : 0;
		lastSafeVerticalSize      = piece.verticalSize;
	};

	// We'll steadily remove characters from the start of the string-to-be-rendered (rhs) and lay them out by adding them to the charRenderBuffer with correct positions.
	while (!rhs.empty()) {
		// Consume tags, convert smart quotes, and so on, from the front of the string.
		ons.processSpecialCharacters(rhs, workingFontinfo, piece.inlineOverrides);
		// We return here with tags etc. processed from the front of rhs so that our first character is now something we can actually handle directly.

		// Tag processing might have changed the font style to be used for this and future glyphs.
		if (workingFontinfo.fontInfoChanged) {
			workingFontinfo.fontInfoChanged          = false;
			workingFontinfo.layoutData.prevCharIndex = 0; // can't kern between different fontinfos (they may have different faces)
			// The charRenderBuffer doesn't simply store the fontInfo for every individual character because that's too inefficient when the font styling rarely changes.
			// Instead, we insert a signal into the character render buffer here so the renderer knows it needs to use new font styling at this point in the text.
			piece.charRenderBuffer.emplace_back();
			piece.charRenderBuffer.back().applyNewFontinfoHere = true;
			// And then record that fontinfo in a list to be shared with the renderer. The renderer will step through it, advancing one entry in the list each time it gets a signal.
			piece.fontInfos.push_back(workingFontinfo);
		}

		auto &workingStyle = workingFontinfo.style();

		if (withinRubySpan && workingStyle.ruby_text.empty()) {
			// just exited a ruby span.
			withinRubySpan = false;
			layoutRubyPiece(piece, rubyPiece.get(), rubyPieceStartPosition, workingFontinfo.x(), measure);
			rubyPieces->push_back(rubyPiece.get());
			rubyPiece.unset();
		}

		// If we've laid out all the characters we were asked for, we're done!
		if (rhs.empty())
			break;

		// This is the Unicode codepoint (character) we need to lay out.
		uint32_t this_codepoint = static_cast<uint32_t>(rhs.at(0));

		// skip leading spaces on a new line
		if (skipSpaces) {
			if (this_codepoint == ' ') {
				rhs.erase(0, 1);
				continue;
			}
			skipSpaces = false;
		}

		if (!withinRubySpan && !workingStyle.ruby_text.empty()) {
			// just entered a ruby span.
			withinRubySpan = true;
			rubyPiece.set(DialoguePiece());
			auto &rbPiece = rubyPiece.get();
			rbPiece.setPreFontInfo(workingFontinfo);
			auto &style = rbPiece.getPreFontInfo().changeStyle();
			style.ruby_text.clear();
			style.font_size = 3 * style.font_size / 5;
			decodeUTF8String(workingFontinfo.style().ruby_text.data(), rubyPiece.get().text);
			rubyPieceStartPosition = piece.charRenderBuffer.size();
		}

		if (!withinRubySpan && !workingStyle.no_break) {
			if (ons.script_language == ScriptLanguage::English) {
				// It's OK to break a line BEFORE the following characters
				// Except that there's no breaking allowed during the no_break tag ({nobr:}) or the base text of a ruby span.
				if (this_codepoint == ' ')
					updateLastSafeData();
			} else {
				/* If we are in Chinese mode updateLastSafeData on any this_codepoint (indicating it is safe to break here), as long as:
				 * 1) this_codepoint can begin a line (requires some list of characters that cannot begin a line, see:
				 *    https://en.wikipedia.org/wiki/Line_breaking_rules_in_East_Asian_languages)
				 * 2) workingFontinfo.layoutData.last_printed_codepoint can end a line (requires some list of characters that cannot end a line)
				 * 3) the two characters do not form an unsplittable pair (numbers, etc).
				 * 4) Standard checks for no_break ({nobr:} tag) and ruby as performed elsewhere in this function.
				 * Glyph spacing is to be adjusted later in layoutLines, prior to actual rendering.
				 */
				uint32_t pre_codepoint = workingFontinfo.layoutData.last_printed_codepoint;
				if ((this_codepoint < NumBegin || this_codepoint > NumEnd || pre_codepoint < NumBegin || pre_codepoint > NumEnd) &&
				    std::find(std::begin(NotLineEnd), std::end(NotLineEnd), pre_codepoint) == std::end(NotLineEnd) &&
				    std::find(std::begin(NotLineBegin), std::end(NotLineBegin), this_codepoint) == std::end(NotLineBegin)) {
					updateLastSafeData();
				}
			}
		}

		// If we hit a newline, consume it and break (we don't cross newlines here, that job belongs to one of our callers)
		if (this_codepoint == NewLine) {
			rhs.erase(0, 1);
			updateLastSafeData();
			lastNewLine = true;
			break;
		}

		// These are not renderable.
		if (this_codepoint != ZeroWidthSpace && this_codepoint != SoftHyphen && this_codepoint != NoOp) {
			// Everything else is.
			auto render = this_codepoint;
			// But these are special character codes and we need to specify what they actually look like when rendered.
			if (render == LinebreakableAsterisk)
				render = '*';
			else if (render == OpeningCurlyBrace)
				render = '{';
			else if (render == ClosingCurlyBrace)
				render = '}';
			// It's a renderable character, so we add it to the chars-to-be-rendered.
			addCharToRenderBuffer(piece, render, workingFontinfo, measure);
			// If we ran out of room, exit the loop, and we will save the layouting data up to our last safe break.
			// Cannot run out of room if layouting in fit-to-width mode (this allows us to resize text to fit it into a certain area --
			// we allow the text to get as long as it wants here, then resize it to fit in layoutLines).
			if (!piece.inlineOverrides.is_fitted.get(workingStyle.is_fitted)) {
				if (workingFontinfo.isNoRoomFor(workingFontinfo.layoutData.newLineBehavior.terminatorAdvance)) {
					break;
				}
			}
		}

		// --------------- Character addition boundary ---------------
		rhs.erase(0, 1);
		// --------------- Character addition boundary ---------------

		// It's OK to break a line AFTER the following characters
		if (!withinRubySpan && !workingFontinfo.style().no_break && (this_codepoint == ProperHyphen || this_codepoint == EmDash || this_codepoint == HyphenMinus)) {
			if (workingFontinfo.layoutData.newLineBehavior.duplicateHyphens) {
				const GlyphValues *glyph                                                        = workingFontinfo.renderUnicodeGlyph(this_codepoint);
				workingFontinfo.layoutData.newLineBehavior.terminator                           = this_codepoint;
				workingFontinfo.layoutData.newLineBehavior.terminatorAdvance                    = glyph->advance;
				workingFontinfo.layoutData.newLineBehavior.terminatorAlreadyIncludedOnFirstLine = true;
			}

			updateLastSafeData();
		}

		// It's OK to break a line AFTER the following characters, but they will influence how you wrap the line
		if (!withinRubySpan && !workingFontinfo.style().no_break && this_codepoint == SoftHyphen) {
			workingFontinfo.layoutData.newLineBehavior.terminator                           = ProperHyphen;
			const GlyphValues *glyph                                                        = workingFontinfo.renderUnicodeGlyph(ProperHyphen);
			workingFontinfo.layoutData.newLineBehavior.terminatorAdvance                    = glyph->advance;
			workingFontinfo.layoutData.newLineBehavior.terminatorAlreadyIncludedOnFirstLine = false;
			workingFontinfo.layoutData.newLineBehavior.firstLineOnly                        = true;

			updateLastSafeData();
		}

		if (!withinRubySpan && !workingFontinfo.style().no_break && this_codepoint == LinebreakableAsterisk) {
			workingFontinfo.layoutData.newLineBehavior.terminator                           = '*';
			const GlyphValues *glyph                                                        = workingFontinfo.renderUnicodeGlyph('*');
			workingFontinfo.layoutData.newLineBehavior.terminatorAdvance                    = glyph->advance;
			workingFontinfo.layoutData.newLineBehavior.terminatorAlreadyIncludedOnFirstLine = true;

			updateLastSafeData();
		}

		if (!withinRubySpan && !workingFontinfo.style().no_break && this_codepoint == ZeroWidthSpace) {
			updateLastSafeData();
		}

		workingFontinfo.layoutData.newLineBehavior.normal();
	}

	// It's OK to break a line after printing all the characters we were asked for
	if (rhs.empty()) {
		piece.setPostFontInfo(workingFontinfo);
		lastSafeLayoutData.newLineBehavior.normal();
		updateLastSafeData();
	}

	// If we deleted at least one character from lastSafeBreakRHS...
	if (lastSafeRenderBufferCount >= 0) {
		piece.fontInfos.resize(lastSafeFontinfoIndex + 1);
		if (rubyPieces && rubyPieces->size() != lastSafeRubyPiecesSize) {
			rubyPieces->resize(lastSafeRubyPiecesSize);
		}

		piece.charRenderBuffer.resize(lastSafeRenderBufferCount);
		if (!thisAddFittingChars.empty() || lastNewLine) {
			if (lastSafeLayoutData.newLineBehavior.terminator && !lastSafeLayoutData.newLineBehavior.terminatorAlreadyIncludedOnFirstLine) {
				addCharToRenderBuffer(piece, lastSafeLayoutData.newLineBehavior.terminator, workingFontinfo, measure);
			}
			rhs = NewLine;
			if (lastSafeLayoutData.newLineBehavior.duplicatingTerminator()) {
				rhs += lastSafeLayoutData.newLineBehavior.terminator + thisAddFittingChars;
			} else {
				rhs += thisAddFittingChars;
			}
		}
		piece.getPostFontInfo().layoutData = lastSafeLayoutData;
		piece.verticalSize                 = lastSafeVerticalSize;
		return true;
	}

	// We didn't find a safe location, meaning that not even one word will fit.
	if (!startOfLine) {
		rhs = original; // This wasn't the start of a line, so we should render nothing and newline down.
		return false;
	}

	// Remove the last unfitting character
	if (!piece.charRenderBuffer.empty())
		piece.charRenderBuffer.pop_back();
	piece.setPostFontInfo(workingFontinfo);
	return true; // This was the start of a line, so, even though not one word will fit, it is safe to render the partial word.
}

// builds up charRenderBuffer and modifies fontInfo.layoutData.last_printed_codepoint and fontInfo.layoutData.xPxLeft (moving the "pen" to the right)
void DialogueController::addCharToRenderBuffer(DialoguePiece &piece, char16_t codepoint, Fontinfo &fontInfo, bool measure) {
	const GlyphValues *glyph = fontInfo.renderUnicodeGlyph(codepoint, measure);
	float plus{0}, aboveBase{glyph->maxy}, belowBase{-glyph->miny}, faceAscender{glyph->faceAscender}, faceDescender{glyph->faceDescender};

	piece.charRenderBuffer.emplace_back();
	RenderBufferGlyph &r = piece.charRenderBuffer.back();
	r.codepoint          = codepoint;
	r.layoutData         = fontInfo.layoutData;
	r.gv                 = glyph;

	plus = fontInfo.layoutData.prevCharIndex ? fontInfo.my_font()->kerning(fontInfo.layoutData.prevCharIndex, glyph->ftCharIndexCache) : 0;
	plus += fontInfo.style().character_spacing;
	plus += glyph->advance;
	fontInfo.layoutData.xPxLeft += plus;
	fontInfo.layoutData.xPxRight = std::ceil(fontInfo.layoutData.xPxLeft - glyph->advance + glyph->maxx);

	if (piece.verticalSize.usedSpaceAboveBaseline < aboveBase)
		piece.verticalSize.usedSpaceAboveBaseline = aboveBase;
	if (piece.verticalSize.usedSpaceBelowBaseline < belowBase)
		piece.verticalSize.usedSpaceBelowBaseline = belowBase;
	if (piece.verticalSize.maxAscend < faceAscender)
		piece.verticalSize.maxAscend = faceAscender;
	if (piece.verticalSize.maxDescend < faceDescender)
		piece.verticalSize.maxDescend = faceDescender;

	fontInfo.layoutData.last_printed_codepoint = codepoint;
	fontInfo.layoutData.prevCharIndex          = glyph->ftCharIndexCache;
}

void DialogueController::renderAddedChars(TextRenderingState &state, DialoguePiece &p, bool renderBorder, bool renderShadow) {
	float x         = 0;
	auto fiIterator = p.fontInfos.begin();

	// We only need a copy of the fontInfo in the renderShadow case.
	cmp::optional<Fontinfo> opt;

	for (RenderBufferGlyph &r : p.charRenderBuffer) {
		assert(fiIterator < p.fontInfos.end());

		if (renderShadow && !opt.has()) {
			opt.set(*fiIterator); // copy it
			auto &style        = opt.get().changeStyle();
			style.color        = style.shadow_color;
			style.border_color = style.shadow_color;
		}

		Fontinfo &fontInfo = opt.has() ? opt.get() : *fiIterator;
		if (r.renderRubyHere)
			continue;
		if (r.applyNewFontinfoHere) {
			++fiIterator;
			opt.unset();
			continue;
		}

		char16_t &codepoint = r.codepoint;

		const GlyphValues *glyph = fontInfo.renderUnicodeGlyph(codepoint);

		bool renderReady = (&state != &dialogueRenderState) || r.fadeStart.expired();

		// For border/shadow, only do the draw if the fontInfo style says we are supposed to
		auto &style = fontInfo.style();
		if (renderReady && !(renderBorder && !style.is_border) && !(renderShadow && !style.is_shadow)) {
			int alpha{255};

			if (&state == &dialogueRenderState && !r.fadeStop.expired() && r.fadeDuration) {
				alpha = 255 - (255 * r.fadeStop.remaining() / r.fadeDuration);
			}
			float blitx = x + glyph->minx + (renderBorder ? glyph->border_bitmap_offset.x : 0) + (renderShadow ? style.shadow_distance[0] : 0);
			float blity = p.baseline - glyph->maxy - (renderBorder ? glyph->border_bitmap_offset.y : 0) + (renderShadow ? style.shadow_distance[1] : 0);
			ons.renderGlyphValues(*glyph, state.dstClip, state.dst,
			                      state.offset.x + p.position.x + p.horizontalResize * ((state.shiftSpriteDrawByBorderPadding ? p.borderPadding : 0) + blitx),
			                      state.offset.y + p.position.y + (state.shiftSpriteDrawByBorderPadding ? p.borderPadding : 0) + blity,
			                      p.horizontalResize, renderBorder, alpha);
		}

		x += r.layoutData.prevCharIndex ? fontInfo.my_font()->kerning(r.layoutData.prevCharIndex, glyph->ftCharIndexCache) : 0;
		x += (*fiIterator).style().character_spacing;
		x += glyph->advance;
		//TODO: These 3 lines are code duplication, see addCharToRenderBuffer; extract them into a float Fontinfo::getTotalAdvance(wchar_t codepoint) method.
		// (maybe there is a better name...)
	}
}

void DialogueController::setDialogueActive(bool active) {
	//if (!isEnabled) return;

	if (active)
		dialogueProcessingState.active = true;
	else
		dialogueProcessingState = DialogueProcessingState();

	currentVoiceWait = std::to_string(static_cast<int32_t>(ons.voicewait_time * ons.voicewait_multiplier));

	if (!active) {
		// Cleanup
		dataPart.clear();
		dataPartUnicode.clear();
		endOfCommand = nullptr;
		dialogueRenderState.clear();

		currentTextPos                 = 0;
		currentPipeId                  = 0;
		continueScriptExecution        = false;
		executingDialogueInlineCommand = false;
		dialogueIsRendering            = false;

		{
			Lock lock(&ons.registeredCRActions);
			for (const auto &a : fetchConstantRefreshActions<SegmentRenderingAction>()) a->terminate();
			for (const auto &a : fetchConstantRefreshActions<TextRenderingMonitorAction>()) a->terminate();
		}

		suspendScriptPasses.clear();
		suspendScriptIndex = 0;
		// PS3 may have extraneous d_continues (i.e. 10100259)
		suspendDialoguePasses = 0;

		textDisplaySpeed.unset();
		textFadeDuration.unset();

		scriptState.disposeDialogue(true);
	}
}
