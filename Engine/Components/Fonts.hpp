/**
 *  Fonts.hpp
 *  ONScripter-RU
 *
 *  Low level font control code based on freetype.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"

#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_TRUETYPE_IDS_H

#include <memory>
#include <unordered_map>

struct GlyphParams;
class BaseReader;
class GlyphValues;

class Font {
private:
	// These functions update the "face" variable to the internal ttf bold/italic/bolditalic face if that exists, otherwise to an alias.
	// They should be called before each render.
	void setBold() {
		face = (hasInternalBoldFace) ? bold_face : (bold_alias != nullptr) ? bold_alias : face;
	}
	void setItalic() {
		face = (hasInternalItalicFace) ? italic_face : (italic_alias != nullptr) ? italic_alias : face;
	}
	void setBoldItalic() {
		face = (hasInternalBoldItalicFace) ? bold_italic_face : (bold_italic_alias != nullptr) ? bold_italic_alias : face;
	}
	void setReset() {
		face = normal_face;
	}
	void drawBorder(FT_Glyph *glyph, int border);
	int border_width{0};
	int current_size{0};
	FT_Face prev_face{nullptr};

public:
	FT_Face face{nullptr}; //current_face
	FT_Face normal_face{nullptr};
	FT_Face bold_face{nullptr};
	FT_Face italic_face{nullptr};
	FT_Face bold_italic_face{nullptr};

	std::unique_ptr<char[]> path;
	bool loaded{false};

	bool hasInternalBoldFace{false};
	bool hasInternalItalicFace{false};
	bool hasInternalBoldItalicFace{false};
	FT_Face bold_alias{nullptr};
	FT_Face italic_alias{nullptr};
	FT_Face bold_italic_alias{nullptr};

	FT_Error err{0};

	SDL_Surface *freetypeToSDLSurface(FT_Bitmap *ft_bmp, SDL_Color fg, SDL_Color bg);
	GlyphValues *renderGlyph(GlyphParams *key, SDL_Color fg, SDL_Color bg);

	FT_GlyphSlot loadGlyph(uint32_t unicode, unsigned int &charIndex) {
		charIndex = FT_Get_Char_Index(face, unicode);
		err       = FT_Load_Glyph(face, charIndex,
                            FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING); // constant for now
		//sendToLog(LogLevel::Error, "FT_Load_Glyph error = 0x%X\n",err);
		return face->glyph;
	}

	void setSize(int val, unsigned int id, int preset_id);
	void setBorder(int val) {
		border_width = val;
	} // in 1/64ths
	void setStyle(bool bold, bool italic) {
		if (bold && italic) {
			setBoldItalic();
			return;
		}
		if (bold) {
			setBold();
			return;
		}
		if (italic) {
			setItalic();
			return;
		}
		setReset();
	}

	int ascent();
	int lineskip();
	float kerning(unsigned int left, unsigned int right); // expects FT indices, NOT codepoints
};

class FontsController : public BaseController {
	BaseReader **reader{nullptr};

public:
	FT_Library freetype{}; //normally private
	size_t fonts_number{0};
	size_t user_fonts_number{0};
	Font fonts[10]{};
	Font user_fonts[10]{};
	bool glyphStorageOptimisation{false};

	std::unordered_map<unsigned int, unsigned int> baseFontOverrides;
	std::unordered_map<unsigned int, std::unordered_map<unsigned int, unsigned int>> presetFontOverrides;
	std::unordered_map<unsigned int, float> baseSizeMultipliers;
	std::unordered_map<unsigned int, std::unordered_map<unsigned int, float>> presetSizeMultipliers;

	bool loadFont(Font &f, size_t i, bool user);
	void initFontOverrides(const std::string &o);
	void initFontMultiplier(const std::string &m);
	Font &getFont(unsigned int id, int preset_id = -1);
	float getMultiplier(unsigned int id, int preset_id);
	void passReader(BaseReader **br) {
		reader = br;
	}
	void passRoot(const char *root) {
		copystr(fontdir, root, sizeof(fontdir));
	}
	int ownInit() override;
	int ownDeinit() override;
	char fontdir[PATH_MAX]{};
	char userfontdir[PATH_MAX]{};
	FontsController()
	    : BaseController(this) {}
};

extern FontsController fonts;
