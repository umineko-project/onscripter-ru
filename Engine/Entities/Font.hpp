/**
 *  Font.hpp
 *  ONScripter-RU.
 *
 *  Font information storage class.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Entities/Glyph.hpp"

#include <string>
#include <stack>
#include <vector>
#include <cstdint>

// Unprintable control sequences
const uint32_t LinebreakableAsterisk{u'\xE000'};
const uint32_t OpeningCurlyBrace{u'\xE001'};
const uint32_t ClosingCurlyBrace{u'\xE002'};
const uint32_t SoftHyphen{u'\x00AD'};
const uint32_t ZeroWidthSpace{u'\x200B'};
const uint32_t NoOp{u'\xE003'};

// Printable control sequences
const uint32_t OpeningSquareBrace{u'['};
const uint32_t ClosingSquareBrace{u']'};
const uint32_t ProperHyphen{u'‐'};
const uint32_t EmDash{u'—'};
const uint32_t HyphenMinus{u'-'};
const uint32_t NewLine{u'\n'};
const uint32_t NormalQuote{u'"'};
const uint32_t CnBegin{u'\x4E00'};
const uint32_t CnEnd{u'\x9FD5'};
const uint32_t JpBegin{u'\x3040'};
const uint32_t JpEnd{u'\x31FF'};
const uint32_t NumBegin{u'\x0030'};
const uint32_t NumEnd{u'\x0039'};
//!,.:;?}·―’”′″∶、。〉》」』！％＇），．：；？｝～
static const uint32_t NotLineBegin[]{u'\x0021',u'\x002C',u'\x002E',u'\x003A',u'\x003B',u'\x003F',u'\x007D',u'\x00B7',u'\x2015',u'\x2019', u'\x201D',u'\x2032',u'\x2033',u'\x2236',u'\x3001',u'\x3002',u'\x3009',u'\x300B',u'\x300D',u'\x300F',u'\xFF01',u'\xFF05',u'\xFF07',
	u'\xFF09',u'\xFF0C',u'\xFF0E',u'\xFF1A',u'\xFF1B',u'\xFF1F',u'\xFF5D',u'\xFF5E'};
//([{·‘“《「『＄（．｛
static const uint32_t NotLineEnd[]{u'\x0028',u'\x005B',u'\x007B',u'\x00B7',u'\x2018',u'\x201C',u'\x300A',u'\x300C',u'\x300E',
	u'\xFF04',u'\xFF08',u'\xFF0E',u'\xFF5B'};

class Font;

struct NewLineBehavior {
	bool duplicateHyphens{true};
	bool terminatorAlreadyIncludedOnFirstLine{false};
	bool firstLineOnly{false};
	char16_t terminator{0};
	float terminatorAdvance{0};
	void normal() {
		terminator                           = 0;
		terminatorAdvance                    = 0;
		terminatorAlreadyIncludedOnFirstLine = false;
		firstLineOnly                        = false;
	}
	bool duplicatingTerminator() {
		if (firstLineOnly)
			return false;
		if (terminator == '*')
			return true;
		return duplicateHyphens && (terminator == L'‐' /*proper hyphen*/ || terminator == L'-');
	}
};

// For information that can change as part of the text layouting process without hitting any {}.
struct LayoutData {
	float xPxLeft;  //Real x coordinate (pen position)
	float xPxRight; // Similar to above, but accounts for the entire final glyph (for rendering rectangles etc) and takes whole-number values
	uint32_t last_printed_codepoint;
	unsigned int prevCharIndex; // last ft char index, used for kerning
	NewLineBehavior newLineBehavior;
};

// OS X pollutes the main namespace with its own FontInfo type, so we
// have to use something else.
class Fontinfo {
public:
	struct InlineOverrides {
		cmp::optional<bool> is_centered; //TODO: replace by an alignment enum (should i do this now?)
		cmp::optional<bool> is_fitted;
		cmp::optional<int> wrap_limit;
		cmp::optional<bool> startsNewRun;
		InlineOverrides &operator|=(const InlineOverrides &o) {
			is_centered |= o.is_centered;
			is_fitted |= o.is_fitted;
			wrap_limit |= o.wrap_limit;
			startsNewRun |= o.startsNewRun;
			return *this;
		}
	};

	// For data changed by {} tags in the text which influence a certain range of characters in the text.
	struct TextStyleProperties {
		unsigned int font_number{0};
		int preset_id{-1};
		uchar3 color{0xff, 0xff, 0xff};
		bool is_gradient{false};
		bool is_centered{false};
		bool is_fitted{false};

		bool is_bold{false};
		bool is_italic{false};
		bool is_underline{false}; //fixed init

		bool can_loghint{false};
		bool ignore_text{false};

		bool is_border{false}; //fixed init
		int border_width{0};
		uchar3 border_color{0, 0, 0};

		bool is_shadow{false};
		int shadow_distance[2]{0, 0};
		uchar3 shadow_color{0, 0, 0};

		bool no_break{false};

		int font_size{0};

		std::string ruby_text;

		int opened_double_quotes{0};
		int opened_single_quotes{0};
		int character_spacing{-999};
		int line_height{-1}; // collision with Fontinfo constructor, it was setting to 0
		int wrap_limit{-1};  // collision with Fontinfo constructor, it was setting to 0

		InlineOverrides inlineOverrides;

		TextStyleProperties()                            = default;
		TextStyleProperties(const TextStyleProperties &) = default;
		TextStyleProperties &operator                    =(const TextStyleProperties &props) {
            // Presets are used to combine multiple tag combinations
            // For example, we can use {p:0:text} instead of {b:{c:FF0000{o:3:text}}} (blue truth/red truth)
            // Not all the preset values are applied unconditionally.
            // Some params are preserved and certain others are optionally preserved (-1 value).

            font_number = props.font_number;
            preset_id   = props.preset_id;
            color       = props.color;

            is_centered  = props.is_centered;
            is_fitted    = props.is_fitted;
            is_bold      = props.is_bold;
            is_italic    = props.is_italic;
            is_underline = props.is_underline;
            can_loghint  = props.can_loghint;
            ignore_text  = props.ignore_text;
            is_border    = props.is_border;
            if (props.border_width != -1)
                border_width = props.border_width;
            border_color = props.border_color;
            if (props.shadow_distance[0] != -1)
                shadow_distance[0] = props.shadow_distance[0];
            if (props.shadow_distance[1] != -1)
                shadow_distance[1] = props.shadow_distance[1];
            shadow_color = props.shadow_color;
            no_break     = props.no_break;
            font_size    = props.font_size;

            character_spacing = props.character_spacing;
            if (props.line_height != -1)
                line_height = props.line_height;
            if (props.wrap_limit != -1)
                wrap_limit = props.wrap_limit;
            inlineOverrides = props.inlineOverrides;

            return *this;
		}
	};

	// Text layouting uses this to figure out when some kind of tag, etc, has changed the fontInfo
	// in a way that requires storing a new copy (does not include changes to LayoutData).
	bool fontInfoChanged{false};

	LayoutData layoutData;

	std::stack<TextStyleProperties, std::vector<TextStyleProperties>> styleStack;
	TextStyleProperties &changeStyle() {
		fontInfoChanged = true;
		return styleStack.top();
	}
	const TextStyleProperties &style() const {
		return styleStack.top();
	}

	GlyphParams getGlyphParams();
	const GlyphValues *renderUnicodeGlyph(uint32_t codepoint);

	uint32_t opening_single_quote{'\''};
	uint32_t closing_single_quote{'\''};
	uint32_t opening_double_quote{'"'};
	uint32_t closing_double_quote{'"'};
	uint32_t apostrophe{'\''};
	bool smart_single_quotes_represented_by_dumb_double{false};
	bool smart_quotes{false};

	Font *my_font();
	const char *getFontPath(unsigned int i);

	enum class FontAlias {
		Italic,
		Bold,
		BoldItalic
	};

	bool changeCurrentFont(unsigned int font, int preset_id = -1);
	bool aliasFont(FontAlias type, int from, int to);

	uchar3 on_color, off_color, nofile_color;
	uchar3 buttonMultiplyColor{0xFF, 0xFF, 0xFF};
	int top_xy[2]{0, 0};  // Top left origin
	int borderPadding{0}; // padding to use for DialogueController border space

	bool is_transparent;

	uchar3 window_color;

	void setSmartQuotes(uint32_t opening_single, uint32_t closing_single, uint32_t opening_double, uint32_t closing_double, uint32_t apost);
	void resetSmartQuotes();

	Fontinfo();
	void reset();

	float x();
	float y();

	bool isFontLoaded(unsigned int number);

	void clear();
	void newLine();

	bool isNoRoomFor(float margin = 0);
	bool isLineEmpty();
};
