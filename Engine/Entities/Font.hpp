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
inline bool isEnLetter(uint32_t ch) {
	return (ch >= u'a' && ch <= 'z') || (ch >= u'A' && ch <= u'Z');
}
inline bool isNumber(uint32_t ch) {
	return (ch >= '0' && ch <= u'9');
}
inline bool isNumberOrEnLetter(uint32_t ch) {
	return isNumber(ch) || isEnLetter(ch);
}
inline bool isCJKChar(uint32_t ch) {
	return (ch >= u'\x4E00' && ch <= u'\x9FFF') || (ch >= u'\x3040' && ch <= u'\x31FF') || ch == u'\x3007';
}

// !"%'),.:;?]}¢°·»‐–—†‡•›‼⁇⁈⁉℃∶、。〃々〆〈〉《》「」『』】〕〗〙〜〞〟〻ぁぃぅぇぉっゃゅょゎゕゖ゠ァィゥェォッャュョヮヵヶ・ーヽヾㇰㇱㇲㇳㇴㇵㇶㇷㇸㇹㇺㇻㇼㇽㇾㇿ︰︱︲︳︶︸︺︼︾﹀﹂﹐﹑﹒﹓﹔﹕﹖﹗﹘﹚﹜！＂％＇），．：；？］｜｝～｠､
static const uint32_t NotLineBegin[]{u'\x21', u'\x22', u'\x25', u'\x27', u'\x29', u'\x2c', u'\x2e', u'\x3a', u'\x3b', u'\x3f', u'\x5d', u'\x7d', u'\xa2', u'\xb0', u'\xb7', u'\xbb', u'\x2010', u'\x2013', u'\x2014', u'\x2020', u'\x2021', u'\x2022', u'\x203a', u'\x203c', u'\x2047', u'\x2048', u'\x2049', u'\x2103', u'\x2236', u'\x3001', u'\x3002', u'\x3003', u'\x3005', u'\x3006', u'\x3008', u'\x3009', u'\x300a', u'\x300b', u'\x300c', u'\x300d', u'\x300e', u'\x300f', u'\x3011', u'\x3015', u'\x3017', u'\x3019', u'\x301c', u'\x301e', u'\x301f', u'\x303b', u'\x3041', u'\x3043', u'\x3045', u'\x3047', u'\x3049', u'\x3063', u'\x3083', u'\x3085', u'\x3087', u'\x308e', u'\x3095', u'\x3096', u'\x30a0', u'\x30a1', u'\x30a3', u'\x30a5', u'\x30a7', u'\x30a9', u'\x30c3', u'\x30e3', u'\x30e5', u'\x30e7', u'\x30ee', u'\x30f5', u'\x30f6', u'\x30fb', u'\x30fc', u'\x30fd', u'\x30fe', u'\x31f0', u'\x31f1', u'\x31f2', u'\x31f3', u'\x31f4', u'\x31f5', u'\x31f6', u'\x31f7', u'\x31f8', u'\x31f9', u'\x31fa', u'\x31fb', u'\x31fc', u'\x31fd', u'\x31fe', u'\x31ff', u'\xfe30', u'\xfe31', u'\xfe32', u'\xfe33', u'\xfe36', u'\xfe38', u'\xfe3a', u'\xfe3c', u'\xfe3e', u'\xfe40', u'\xfe42', u'\xfe50', u'\xfe51', u'\xfe52', u'\xfe53', u'\xfe54', u'\xfe55', u'\xfe56', u'\xfe57', u'\xfe58', u'\xfe5a', u'\xfe5c', u'\xff01', u'\xff02', u'\xff05', u'\xff07', u'\xff09', u'\xff0c', u'\xff0e', u'\xff1a', u'\xff1b', u'\xff1f', u'\xff3d', u'\xff5c', u'\xff5d', u'\xff5e', u'\xff60', u'\xff64'};
// "#$'([\{£¥«·‵々〇〈〉《》「」『【〔〖〘〝︴︵︷︹︻︽︿﹁﹃﹏﹙﹛＄（．［｛｟｠￡￥￦
static const uint32_t NotLineEnd[]{u'\x22', u'\x23', u'\x24', u'\x27', u'\x28', u'\x5b', u'\x5c', u'\x7b', u'\xa3', u'\xa5', u'\xab', u'\xb7', u'\x2035', u'\x3005', u'\x3007', u'\x3008', u'\x3009', u'\x300a', u'\x300b', u'\x300c', u'\x300d', u'\x300e', u'\x3010', u'\x3014', u'\x3016', u'\x3018', u'\x301d', u'\xfe34', u'\xfe35', u'\xfe37', u'\xfe39', u'\xfe3b', u'\xfe3d', u'\xfe3f', u'\xfe41', u'\xfe43', u'\xfe4f', u'\xfe59', u'\xfe5b', u'\xff04', u'\xff08', u'\xff0e', u'\xff3b', u'\xff5b', u'\xff5f', u'\xff60', u'\xffe1', u'\xffe5', u'\xffe6'};

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
            if (props.font_size != -1)
                font_size = props.font_size;
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
	const GlyphValues *renderUnicodeGlyph(uint32_t codepoint, bool measure = false);

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
