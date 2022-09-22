/**
 *  Dialogue.hpp
 *  ONScripter-RU
 *
 *  Text parsing and rendering code.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "External/LimitedQueue.hpp"
#include "External/slre.h"
#include "Engine/Components/Base.hpp"
#include "Engine/Entities/Font.hpp"
#include "Engine/Entities/ConstantRefresh.hpp"
#include "Engine/Handlers/Script.hpp"
#include "Support/Clock.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>

#include <utility>
#include <deque>
#include <queue>
#include <vector>
#include <string>
#include <sstream>
#include <stack>
#include <locale>
#include <cstdlib>

struct DialogueProcessingEvent {
	bool firstCall;
	bool loanExecStart;
	bool loanExecEnd;
	bool dialogueInlineCommandEnd;
	// event attributes go here
	// better not to change this struct before thinking properly
	// it is contained in a limited_queue_z, thus all initialisers are ignored and fields are zeroed
};

// ----------------------------------------

struct RenderBufferGlyph {
	Clock fadeStart, fadeStop;
	LayoutData layoutData{};
	const GlyphValues *gv{nullptr};
	int fadeDuration{0};
	char16_t codepoint{0};
	bool renderRubyHere{false};
	bool applyNewFontinfoHere{false};
};

// A piece is a partial or full line of text to be rendered (does not linebreak or wrap).
struct DialoguePiece {
	std::u16string text;

	Fontinfo &getPostFontInfo() {
		// Gets the fontInfo that applies at the end of the piece
		return fontInfos.back();
	}
	void setPostFontInfo(const Fontinfo &postFontInfo) {
		fontInfos.push_back(postFontInfo);
	}
	Fontinfo &getPreFontInfo() {
		// Gets the fontInfo that applies at the start of the piece
		return fontInfos.front();
	}
	void setPreFontInfo(const Fontinfo &preFontInfo) {
		fontInfos.clear();
		fontInfos.push_back(preFontInfo);
		inlineOverrides = preFontInfo.style().inlineOverrides;
	}

	struct VerticalSize {
		float usedSpaceAboveBaseline{0};
		float usedSpaceBelowBaseline{0};
		float maxAscend{0};
		float maxDescend{0};
	};

	Fontinfo::InlineOverrides inlineOverrides; // Tags like {a:c:} (centering) that affect not just the tagged text but the entire line that text is on
	std::vector<RenderBufferGlyph> charRenderBuffer;
	std::vector<Fontinfo> fontInfos;
	float horizontalResize{1.0};
	GPU_Rect position{0, 0, 0, 0}; // where this piece should go when it is blitted
	int borderPadding{0};          // 10 px of padding to be applied to all sides of the piece to make absolutely sure there is enough room for text border
	int baseline{0};               // where to find the baseline, in pixels counted downwards from the top of the padding
	float xPxLeft{0}, xPxRight{0};
	VerticalSize verticalSize;
	void resetLayoutInfo() {
		charRenderBuffer.clear();
		while (fontInfos.size() > 1) fontInfos.pop_back();
		xPxLeft = xPxRight = 0;
		verticalSize       = {};
		inlineOverrides    = getPreFontInfo().style().inlineOverrides;
	}
};

// A run of text is a list of pieces to be rendered one after another using some fade animation.
// It may wrap across lines, so it consists of multiple images to be rendered into different places.
// The fact that we store all these parts in separate images may be a bit inefficient and could possibly be
// replaced by a texture atlas? (separate from the one used to hold glyphs?) but
// 1) we don't have a convenient texture atlas to hand at the minute;
// 2) rendering different parts from the same image may add complexity for how to feed them through a fade shader
//    (the shader receives the whole image...)
//TODO: The pieces also need some z-order and some rendering order...?
struct DialogueRun {
	std::deque<DialoguePiece> pieces;
	std::deque<DialoguePiece> rubyPieces;
	std::deque<DialoguePiece *> getPieces(bool includeRuby = false) {
		std::deque<DialoguePiece *> ret;
		for (auto &piece : pieces) ret.push_back(&piece);
		if (includeRuby)
			for (auto &piece : rubyPieces) ret.push_back(&piece);
		return ret;
	}
};

// A segment is a rendered text to be displayed all at once using some fade animation (one individual segment
// between the | in our dataPart). It consists of usually just one DialogueRun, but may have more than one
// in the case of simultaneously displayed lines. Each run is displayed concurrently.
struct DialogueSegment {
	std::deque<DialogueRun> runs{1}; // These will be rendered concurrently.
	float2 cursorPosition{0, 0};     // the position of the cursor after rendering this segment
	std::deque<DialoguePiece *> getPieces(bool includeRuby = false) {
		std::deque<DialoguePiece *> ret;
		for (auto &run : runs)
			for (auto piece : run.getPieces(includeRuby)) ret.push_back(piece);
		return ret;
	}
};

// A view of dialogue pieces organized by the line they appear on, for line layouting purposes.
struct DialogueLine {
	std::deque<DialoguePiece *> pieces;
	std::deque<DialoguePiece *> rubyPieces;
	std::deque<DialoguePiece *> getPieces(bool includeRuby = false) {
		std::deque<DialoguePiece *> ret;
		for (auto &piece : pieces) ret.push_back(piece);
		if (includeRuby)
			for (auto &piece : rubyPieces) ret.push_back(piece);
		return ret;
	}

	Fontinfo::InlineOverrides inlineOverrides;
	float horizontalResize{1.0};
	float maxAscender{0}, maxDescender{0};
	float2 position{0, 0}; // left edge and baseline offset
};

// The things between @s. Used for character counting for automode wait times.
struct DialogueClickPart {
	std::deque<DialogueSegment *> segments;
	std::deque<DialoguePiece *> getPieces(bool includeRuby = false) {
		std::deque<DialoguePiece *> ret;
		for (auto &seg : segments)
			for (auto piece : seg->getPieces(includeRuby)) ret.push_back(piece);
		return ret;
	}
	unsigned int getCharacterCount() {
		unsigned int total{0};
		for (auto piecePtr : getPieces(true)) {
			total += piecePtr->charRenderBuffer.size();
		}
		return total;
	}
	void clear() {
		segments.clear();
	}
};

struct TextRenderingState {
	struct TextRenderingDst {
		GPU_Target *target{nullptr};
		GPUBigImage *bigImage{nullptr};
	} dst;
	GPU_Rect *dstClip{nullptr};
	GPU_Rect bounds{0, 0, 0, 0};
	GPU_Rect offset{0, 0, 0, 0};
	bool shiftSpriteDrawByBorderPadding{true};
	int tightlyFit{FIT_MODE::FIT_BOTH};
	int segmentIndex{-1};
	std::deque<DialogueSegment> segments;
	std::deque<DialogueLine> lines;
	std::deque<DialogueClickPart> clickParts;
	std::deque<DialoguePiece *> getPieces(bool includeRuby = false) {
		std::deque<DialoguePiece *> ret;
		for (auto &seg : segments)
			for (auto piece : seg.getPieces(includeRuby)) ret.push_back(piece);
		return ret;
	}
	int clickPartCharacterCount() {
		if (segmentIndex == -1)
			return 0;
		// Find the click part that has our segment
		cmp::optional<DialogueClickPart *> currentPart;
		for (auto &part : clickParts)
			for (auto segmentPtr : part.segments)
				if (segmentPtr == &segments.at(segmentIndex))
					currentPart.set(&part);
		// Return how many characters are in that part (or 0 if no part found)
		return currentPart.has() ? currentPart.get()->getCharacterCount() : 0;
	}
	void clear() {
		segments.clear();
		lines.clear();
		clickParts.clear();
		dst                            = TextRenderingDst();
		dstClip                        = nullptr;
		shiftSpriteDrawByBorderPadding = true;
		segmentIndex                   = -1;
		bounds                         = GPU_Rect();
		offset                         = GPU_Rect();
	}
};

class ScriptState {
public:
	// safe helper functions so that our intent is extremely clear and we can't screw up
	void useDialogue();
	void useMainScript();
	void disposeDialogue(bool force = false); //for initialisations and spontaneous resets
	void disposeMainscript(bool force = false);

private:
	cmp::optional<ScriptHandler::ScriptLoanStorable> state;
	int swaps{0};
};

class DialogueController : public BaseController {
protected:
	int ownInit() override;
	int ownDeinit() override;

public:
	// ***** Turns the dialogue controller on and off. *****
	// ***** There is no script interface for this at  *****
	// ***** present, so just flip this bool.          *****
	//bool isEnabled {false};

	DialogueController()
	    : BaseController(this) {
		dataPart.reserve(2048);
		dataPartUnicode.reserve(2048);
		textPart.reserve(2048);
		layoutPieceTmpFreshText.reserve(2048);
		addFittingCharsTmpLastSafeBreakRHS.reserve(2048);
		addRubyFittingCharsTmpLastSafeBreakRHS.reserve(2048);
	}

	enum class TEXT_STATE {
		TEXT,
		USER_CMD,
		TEXT_CMD, //@
		SYS_CMD,
		END
	};

	class DialogueProcessingState {
	public:
		bool active{false};
		bool layoutDone{false};
		bool readyToRun{false};
		bool pretextHasBeenToldToRunOnce{false}; // intuitive bool name, but no good reason why its behavior couldn't have been simpler
	};

	class SegmentRenderingAction : public TypedConstantRefreshAction<SegmentRenderingAction> {
	public:
		int segment{-1};
		void run() override;
		bool expired() override;
		void onExpired() override;
		bool suspendsMainScript() override {
			return false;
		}
		bool suspendsDialogue() override {
			return false;
		}
	};

	class TextRenderingMonitorAction : public TypedConstantRefreshAction<TextRenderingMonitorAction> {
	public:
		int lastCompletedSegment{-1};
		std::unordered_set<int> handledEvents() override {
			return inputEventList;
		}
		int eventMode() override;
		bool expired() override;
		void run() override;
		void onExpired() override;
		bool suspendsMainScript() override {
			return false;
		}
	};

	std::string dataPart; //Contains text data for a whole dialogue with a big amount of special symbols (|) to enter cmd
	std::u16string dataPartUnicode;
	std::string textPart;

	std::u16string dialogueName;
	bool nameLayouted{false};

	TextRenderingState dialogueRenderState, nameRenderState; //-V730_NOINIT
	DialogueProcessingState dialogueProcessingState;
	void render(TextRenderingState &state);
	void renderDialogueToTarget(GPU_Target *dst, GPU_Rect *dstClip, int refresh_mode, bool camera = true);

	bool isDialogueSegmentRendered(int segment) {
		bool r{true};
		for (auto piecePtr : dialogueRenderState.segments[segment].getPieces()) {
			auto &piece = *piecePtr;
			for (auto &glyph : piece.charRenderBuffer) {
				if (!glyph.fadeStop.expired()) {
					r = false;
					break;
				}
				// note this checks for fadeStop -- action that runs dialogue rendering to total completion
			}
			if (!r)
				break;
		}
		return r;
	}
	bool isCurrentDialogueSegmentRendered() {
		return isDialogueSegmentRendered(dialogueRenderState.segmentIndex);
	}

	bool wantsControl();
	int processDialogueEvents();
	int processDialogue();
	void processDialogueInitialization();

	void waitForAction();
	void startLoanExecution();
	void endLoanExecution();

	void advanceDialogueRendering(uint64_t ns);
	void timeCurrentDialogueSegment();
	void untimeDialogueSegment(int segment);
	void untimeAllDialogueSegments();

	void setDialogueName(const char *buf);

	void feedDialogueTextData(const char *dataStr);
	TEXT_STATE handleNextPart();
	void setDialogueActive(bool active = true);
	void layoutName();
	void layoutDialogue();
	// for specialscrollable rendering
	void renderToTarget(GPU_Target *dst, GPU_Rect *dstClip, char *buf, Fontinfo *f_info = nullptr, bool paddingShift = true, int tightlyFit = FIT_MODE::FIT_BOTH);
	void renderToTarget(GPU_Target *dst, GPU_Rect *dstClip, std::u16string &text, Fontinfo *f_info = nullptr, bool paddingShift = true, int tightlyFit = FIT_MODE::FIT_BOTH);
	// for lsp drawing (returns image size and decoded text)
	void prepareForRendering(const char *buf, Fontinfo &f_info, TextRenderingState &state, uint16_t &w, uint16_t &h);

	void getRenderingBounds(TextRenderingState &state, bool visiblePiecesOnly = false);

	// State related variables
	const char *dialogue_pos{nullptr};
	std::string currentCommand;
	std::string currentVoiceWait;
	unsigned int currentTextPos{0};
	unsigned int currentPipeId{0};
	std::string gosubLabel;
	bool dialogueIsRendering{false};
	bool continueScriptExecution{false};
	bool immediatelyHandleNextPart{false};
	cmp::optional<int> textDisplaySpeed;
	cmp::optional<int> textFadeDuration;

	limited_queue_z<DialogueProcessingEvent> events;
	int suspendDialoguePasses{0};
	std::unordered_map<int, int> suspendScriptPasses;
	int suspendScriptIndex{0};
	bool executingDialogueInlineCommand{false};
	bool loanExecutionActive{false};

	ScriptState scriptState;

private:
	char *endOfCommand{nullptr};

	void layoutSegment(TextRenderingState &state, std::u16string text, Fontinfo &fi);
	DialoguePiece layoutPiece(TextRenderingState &state, std::u16string &text, Fontinfo &fontInfo, std::deque<DialoguePiece> *rubyPieces);
	void layoutRubyPiece(DialoguePiece &mainPiece, DialoguePiece &rubyPiece, size_t rubyStartPosition, float xFinish, bool measure = false);
	void layoutLines(TextRenderingState &state);
	void renderPiece(TextRenderingState &state, DialoguePiece &piece);

	bool addFittingChars(DialoguePiece &piece, std::u16string &rhs, const std::u16string &original, std::deque<DialoguePiece> *rubyPieces = nullptr, bool measure = false);
	void addCharToRenderBuffer(DialoguePiece &piece, char16_t codepoint, Fontinfo &fontInfo, bool measure = false);
	void renderAddedChars(TextRenderingState &state, DialoguePiece &p, bool renderBorder = false, bool renderShadow = false);
	void renderBorderedWord(TextRenderingState &state, SDL_Color &wordBorderColor, GPU_Rect &borderRect, int wordBorderSize) ;

	// temps with preallocated buffers
	std::u16string layoutPieceTmpFreshText;
	std::u16string addFittingCharsTmpLastSafeBreakRHS;
	std::u16string addRubyFittingCharsTmpLastSafeBreakRHS;

	slre_regex_info regexInfo{};
};

extern DialogueController dlgCtrl;
