/**
 *  Font.cpp
 *  ONScripter-RU.
 *
 *  Font information storage class.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Entities/Font.hpp"
#include "Engine/Components/Fonts.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Core/ONScripter.hpp"

#include <cmath>
#include <cstdio>
#include <stack>
#include <string>
#include <sstream>
#include <unordered_map>

#define FT_CEIL(X) ((((X) + 63) & -64) / 64)

Fontinfo::Fontinfo() {
	layoutData.last_printed_codepoint = 0;
	layoutData.prevCharIndex          = 0;
	styleStack.emplace();
	on_color     = {0xff, 0xff, 0xff};
	off_color    = {0xaa, 0xaa, 0xaa};
	nofile_color = {0x55, 0x55, 0x99};

	reset();
}

GlyphParams Fontinfo::getGlyphParams() {
	// Update font with current style params
	GlyphParams gp;
	auto &s = style();
	auto &f = fonts.getFont(s.font_number, s.preset_id);

	f.setStyle(s.is_bold, s.is_italic);
	f.setSize(s.font_size, s.font_number, s.preset_id);
	f.setBorder(s.is_border ? s.border_width : 0);

	gp.font_number  = s.font_number;
	gp.preset_id    = fonts.glyphStorageOptimisation ? -1 : s.preset_id;
	gp.font_size    = s.font_size;
	gp.border_width = s.is_border ? s.border_width : 0;

	uint8_t r      = (s.color.x * buttonMultiplyColor.x) / 0xFF;
	uint8_t g      = (s.color.y * buttonMultiplyColor.y) / 0xFF;
	uint8_t b      = (s.color.z * buttonMultiplyColor.z) / 0xFF;
	gp.glyph_color = SDL_Color{r, g, b, 0xFF};

	gp.border_color = SDL_Color{s.border_color.x, s.border_color.y, s.border_color.z, 0xFF};

	gp.is_bold      = s.is_bold;
	gp.is_italic    = s.is_italic;
	gp.is_underline = false;
	gp.is_border    = s.is_border;
	gp.is_colored   = true;
	gp.is_gradient  = s.is_gradient;

	return gp;
}

// Helper method since GlyphParams is used this way pretty often
const GlyphValues *Fontinfo::renderUnicodeGlyph(uint32_t codepoint, bool measure) {
	GlyphParams p = getGlyphParams();
	p.unicode     = codepoint;
	if (measure)
		return ons.measureUnicodeGlyph(my_font(), &p);
	return ons.renderUnicodeGlyph(my_font(), &p);
}

bool Fontinfo::aliasFont(FontAlias type, int from, int to) {
	if (static_cast<size_t>(from) > fonts.fonts_number || from < 0 || !fonts.fonts[from].loaded)
		return false;
	if (static_cast<size_t>(to) > fonts.fonts_number || to < 0 || !fonts.fonts[to].loaded)
		return false;

	//sendToLog(LogLevel::Info, "aliasFont called with type %d from %d to %d\n",type,from,to);

	switch (type) {
		case FontAlias::BoldItalic:
			fonts.fonts[from].bold_italic_alias   = fonts.fonts[to].normal_face;
			fonts.getFont(from).bold_italic_alias = fonts.getFont(to).normal_face;
			break;
		case FontAlias::Bold:
			fonts.fonts[from].bold_alias   = fonts.fonts[to].normal_face;
			fonts.getFont(from).bold_alias = fonts.getFont(to).normal_face;
			break;
		case FontAlias::Italic:
			fonts.fonts[from].italic_alias   = fonts.fonts[to].normal_face;
			fonts.getFont(from).italic_alias = fonts.getFont(to).normal_face;
			break;
	}

	return true;
}

bool Fontinfo::changeCurrentFont(unsigned int font, int preset_id) {
	//sendToLog(LogLevel::Info, "changeCurrentFont(%i) called\n", font);

	if (font == style().font_number && style().preset_id == preset_id)
		return true; //let's simply return true

	if (!fonts.fonts[font].loaded || font > fonts.fonts_number)
		return false;

	changeStyle().font_number = font;
	changeStyle().preset_id   = preset_id;
	return true;
}

Font *Fontinfo::my_font() {
	return &fonts.getFont(style().font_number, style().preset_id);
}

bool Fontinfo::isFontLoaded(unsigned int number) {
	return number < fonts.fonts_number && fonts.fonts[number].loaded;
}

void Fontinfo::setSmartQuotes(uint32_t opening_single, uint32_t closing_single, uint32_t opening_double, uint32_t closing_double, uint32_t apost) {
	smart_quotes         = true;
	opening_single_quote = opening_single ? opening_single : '\'';
	closing_single_quote = closing_single ? closing_single : '\'';
	opening_double_quote = opening_double ? opening_double : '"';
	closing_double_quote = closing_double ? closing_double : '"';
	apostrophe           = apost ? apost : '\'';
}
void Fontinfo::resetSmartQuotes() {
	smart_quotes = false;
}

void Fontinfo::reset() {
	clear();

	while (styleStack.size() > 1) styleStack.pop();

	changeStyle().is_gradient = false;
	changeStyle().is_centered = false;
	changeStyle().is_fitted   = false;
	changeStyle().is_bold     = false;
	changeStyle().is_italic   = false;
	changeStyle().is_shadow   = true;
	is_transparent            = true;

	layoutData.newLineBehavior.duplicateHyphens = false;
}

float Fontinfo::x() {
	return layoutData.xPxLeft + top_xy[0];
}

float Fontinfo::y() {
	return top_xy[1];
}

void Fontinfo::clear() {
	//init here
	layoutData.xPxLeft                 = 0;
	layoutData.xPxRight                = 0;
	layoutData.last_printed_codepoint  = 0;
	layoutData.prevCharIndex           = 0;
	changeStyle().opened_single_quotes = 0;
	changeStyle().opened_double_quotes = 0;
}

void Fontinfo::newLine() {
	layoutData.xPxLeft                = 0;
	layoutData.xPxRight               = 0;
	layoutData.last_printed_codepoint = 0;
	layoutData.prevCharIndex          = 0;
}

bool Fontinfo::isNoRoomFor(float margin) {
	//TODO: implement vertical
	return layoutData.xPxLeft + margin > style().wrap_limit;
}

bool Fontinfo::isLineEmpty() {
	return std::floor(layoutData.xPxLeft) == 0;
}

const char *Fontinfo::getFontPath(unsigned int i) {
	if (i >= std::extent<decltype(fonts.fonts)>::value)
		return nullptr;

	return fonts.fonts[i].path.get();
}

// Font code

GlyphValues *Font::measureGlyph(GlyphParams *key) {
	//    ons.printClock("measureGlyph");
	GlyphValues *rv = new GlyphValues;

	// load the glyph for this font and unicode, and store the ft char index in Cache (output param) for later use
	FT_GlyphSlot glyph = loadGlyph(key->unicode, rv->ftCharIndexCache);

	if (err) {
		return rv;
	}

	FT_Glyph actual_glyph;

	err = FT_Get_Glyph(glyph, &actual_glyph);
	if (err) {
		return rv;
	}

	FT_BBox bbox;
	FT_Glyph_Get_CBox(actual_glyph, FT_GLYPH_BBOX_PIXELS, &bbox);

	if (border_width > 0 && (bbox.xMin != bbox.xMax)) {
		rv->border_bitmap_offset.x = static_cast<int>(-std::lround(border_width / 64.0));
		rv->border_bitmap_offset.y = static_cast<int>(std::lround(border_width / 64.0));
	}

	rv->minx          = bbox.xMin;
	rv->maxy          = bbox.yMax;
	rv->miny          = bbox.yMin;
	rv->maxx          = bbox.xMax;
	rv->advance       = (actual_glyph->advance.x / 65536.0);
	rv->faceAscender  = (face->size->metrics.ascender / 64.0);
	rv->faceDescender = -(face->size->metrics.descender / 64.0); // make both positive

	FT_Done_Glyph(actual_glyph);

	//    ons.printClock("measureGlyph end");
	return rv;
}

GlyphValues *Font::renderGlyph(GlyphParams *key, SDL_Color fg, SDL_Color bg) {
	GlyphValues *rv = new GlyphValues;

	// load the glyph for this font and unicode, and store the ft char index in Cache (output param) for later use
	FT_GlyphSlot glyph = loadGlyph(key->unicode, rv->ftCharIndexCache);

	if (err) {
		//sendToLog(LogLevel::Warn, "loadGlyph unsuccessful in renderGlyph\n");
		return rv;
	}

	FT_Glyph actual_glyph;
	FT_BitmapGlyph bmp_glyph;

	err = FT_Get_Glyph(glyph, &actual_glyph);
	if (err) {
		//sendToLog(LogLevel::Warn, "FT_Get_Glyph unsuccessful in renderGlyph\n");
		return rv;
	}

	SDL_Surface *glyph_border = nullptr;
	if (key->border_width > 0) {
		FT_Glyph border_glyph;
		FT_Glyph_Copy(actual_glyph, &border_glyph);
		// Turn it into bordered version.
		//sendToLog(LogLevel::Error, "Somehow we are in border for %c\n",key->unicode);
		int border_w = key->border_width * fonts.getMultiplier(key->font_number, key->preset_id);
		drawBorder(&border_glyph, border_w);
		/* Convert glyph to surface */

		if (border_glyph->format != FT_GLYPH_FORMAT_BITMAP) {
			err = FT_Glyph_To_Bitmap(&border_glyph, FT_RENDER_MODE_NORMAL, nullptr, 1);
			if (err) {
				//sendToLog(LogLevel::Error, "Crashing and burning in renderGlyph! Couldn't convert glyph to bitmap\n");
				//sendToLog(LogLevel::Error, "Bus number: 0x%X\n", err);
			}
		}
		bmp_glyph                  = reinterpret_cast<FT_BitmapGlyph>(border_glyph);
		glyph_border               = freetypeToSDLSurface(&bmp_glyph->bitmap, fg, bg); // temporary fixed color for outlines
		rv->border_bitmap_offset.x = bmp_glyph->left;
		rv->border_bitmap_offset.y = bmp_glyph->top;
		FT_Done_Glyph(border_glyph);
	}

	/* Convert glyph to surface */
	if (actual_glyph->format != FT_GLYPH_FORMAT_BITMAP) {
		err = FT_Glyph_To_Bitmap(&actual_glyph, FT_RENDER_MODE_NORMAL, nullptr, 1);
		if (err) {
			//sendToLog(LogLevel::Error, "Crashing and burning in renderGlyph! Couldn't convert glyph to bitmap\n");
		}
	}
	bmp_glyph               = reinterpret_cast<FT_BitmapGlyph>(actual_glyph);
	SDL_Surface *glyph_body = freetypeToSDLSurface(&bmp_glyph->bitmap, fg, bg);

	if (glyph_border && border_width > 0) {
		rv->border_bitmap_offset.x -= bmp_glyph->left;
		rv->border_bitmap_offset.y -= bmp_glyph->top;
	}

	rv->bitmap = glyph_body;
	if (glyph_border && border_width > 0) {
		rv->border_bitmap = glyph_border;
	}
	rv->minx          = bmp_glyph->left;
	rv->maxy          = bmp_glyph->top;
	rv->miny          = bmp_glyph->top - static_cast<int>(bmp_glyph->bitmap.rows);
	rv->maxx          = bmp_glyph->left + static_cast<int>(bmp_glyph->bitmap.width);
	rv->advance       = (actual_glyph->advance.x / 65536.0);
	rv->faceAscender  = (face->size->metrics.ascender / 64.0);
	rv->faceDescender = -(face->size->metrics.descender / 64.0); // make both positive

	FT_Done_Glyph(actual_glyph);

	//sendToLog(LogLevel::Info, "renderGlyph bitmap w,h:{%d,%d} border_bitmap w,h:{%d,%d}\n", rv.bitmap->w, rv.bitmap->h, rv.border_bitmap->w, rv.border_bitmap->h);

	// glyph->bitmap.buffer ... present? not sure if we remove it....
	//sendToLog(LogLevel::Info, "Finished creating rv.bitmap\n");

	return rv;
}

SDL_Surface *Font::freetypeToSDLSurface(FT_Bitmap *ft_bmp, SDL_Color fg, SDL_Color bg) {
	SDL_Surface *sdl_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, ft_bmp->width, ft_bmp->rows, 8, 0, 0, 0, 0);

	// Fill palette with 256 shades interpolating between foreground
	// and background colours.
	SDL_Palette *pal = sdl_surface->format->palette;
	int dr           = fg.r - bg.r;
	int dg           = fg.g - bg.g;
	int db           = fg.b - bg.b;
	for (int i = 0; i < 256; ++i) {
		pal->colors[i].r = bg.r + i * dr / 255;
		pal->colors[i].g = bg.g + i * dg / 255;
		pal->colors[i].b = bg.b + i * db / 255;
	}

	// Copy the character from the pixmap
	uint8_t *src = ft_bmp->buffer;

	uint8_t *dst = static_cast<uint8_t *>(sdl_surface->pixels);
	for (int row = 0; row < sdl_surface->h; ++row) {
		std::memcpy(dst, src, ft_bmp->pitch);
		src += ft_bmp->pitch;
		dst += sdl_surface->pitch;
	}

	return sdl_surface;
}

// Border size in 1/64ths
void Font::drawBorder(FT_Glyph *glyph, int border) {

	FT_Stroker stroker;
	err = FT_Stroker_New(fonts.freetype, &stroker);
	if (err) {
		//sendToLog(LogLevel::Error, "FT_Stroker failed\n");
		FT_Stroker_Done(stroker);
		return;
	}

	FT_Stroker_Set(stroker, border, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

	//sendToLog(LogLevel::Info, "FT_Stroker_Set border_width %d\n",border_width);

	// This border extends only outside the glyph.
	// It is not hollow. The returned glyph contains all the filled pixels of the original glyph plus some more on the outside.
	// You render the border, and then the original glyph on top afterwards.
	// Therefore, the border cannot eat into the original glyph (external border).
	//err = FT_Glyph_StrokeBorder(glyph, stroker, 0, 0);

	// This border extends also inside the glyph.
	// It is hollow, allowing the original glyph to be visible through it.
	// You render the original glyphs, and then the hollow border on top afterwards.
	// Therefore, it will eat away some of the original glyph (internal border).
	err = FT_Glyph_Stroke(glyph, stroker, 0);

	if (err) {
		//sendToLog(LogLevel::Error, "FT_Glyph_Stroke(Border) failed\n");
	}

	FT_Stroker_Done(stroker);
}

// expects FT glyph codes, NOT codepoints
float Font::kerning(unsigned int left, unsigned int right) {
	FT_Vector kern;
	err = FT_Get_Kerning(face, left, right, FT_KERNING_UNFITTED, &kern);

	return !err ? kern.x / 64.0 : 0.0;
}

int Font::ascent() {
	return FT_CEIL(FT_MulFix(face->ascender, face->size->metrics.y_scale));
}

int Font::lineskip() {
	return FT_CEIL(FT_MulFix(face->height, face->size->metrics.y_scale));
}

void Font::setSize(int val, unsigned int id, int preset_id) {
	val *= fonts.getMultiplier(id, preset_id);

	if (val != current_size || prev_face != face) {
		current_size = val;
		prev_face    = face;
		err          = FT_Set_Char_Size(face, 0, static_cast<FT_F26Dot6>(val) * 64, 0, 0);
	}
}
