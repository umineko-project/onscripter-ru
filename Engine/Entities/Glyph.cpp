/**
 *  Glyph.cpp
 *  ONScripter-RU
 *
 *  Glyph entity support split in params (pointers) and values (representations).
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Entities/Glyph.hpp"
#include "Engine/Graphics/GPU.hpp"
#include "Engine/Components/GlyphAtlas.hpp"
#include "Support/FileDefs.hpp"

GlyphValues::GlyphValues(const GlyphValues &orig) {
	if (!orig.glyph_gpu || orig.glyph_gpu->w == 0 || orig.glyph_gpu->h == 0)
		glyph_gpu = nullptr;
	else
		glyph_gpu = gpu.copyImage(orig.glyph_gpu);

	if (!orig.border_gpu || orig.border_gpu->w == 0 || orig.border_gpu->h == 0)
		border_gpu = nullptr;
	else
		border_gpu = gpu.copyImage(orig.border_gpu);

	// Copy SDL Surfaces
	if (orig.bitmap) {
		bitmap = orig.bitmap;
		bitmap->refcount++;
	} else {
		bitmap = nullptr;
	}
	if (orig.border_bitmap) {
		border_bitmap = orig.border_bitmap;
		border_bitmap->refcount++;
	} else {
		border_bitmap = nullptr;
	}

	border_bitmap_offset = orig.border_bitmap_offset;

	minx          = orig.minx;
	maxx          = orig.maxx;
	miny          = orig.miny;
	maxy          = orig.maxy;
	advance       = orig.advance;
	faceAscender  = orig.faceAscender;
	faceDescender = orig.faceDescender;

	glyph_pos  = orig.glyph_pos;
	border_pos = orig.border_pos;
}

GlyphValues::~GlyphValues() {
	if (bitmap != nullptr)
		SDL_FreeSurface(bitmap);
	if (border_bitmap != nullptr)
		SDL_FreeSurface(border_bitmap);
	if (glyph_gpu != nullptr)
		gpu.freeImage(glyph_gpu);
	if (border_gpu != nullptr)
		gpu.freeImage(border_gpu);
}

bool GlyphValues::buildGPUImages(GlyphAtlasController *atlas) {
	bool ret = buildGPUImage(false, atlas);
	if (ret && border_bitmap)
		ret = buildGPUImage(true, atlas);
	return ret;
}

bool GlyphValues::buildGPUImage(bool border, GlyphAtlasController *atlas) {

	SDL_Surface *src_surface = border ? border_bitmap : bitmap;

	//No need to rebuild GPU_Image
	if ((border ? border_gpu : glyph_gpu) != nullptr)
		return true;

	if (src_surface == nullptr) {
		throw std::runtime_error("GlyphValues@buildGPUImage: No surface");
	}

	if (src_surface->w == 0 || src_surface->h == 0) {
		return true;
	}

	bool ret                    = true;
	SDL_Surface *letter_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, src_surface->w, src_surface->h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	uint8_t *src_buffer = static_cast<uint8_t *>(src_surface->pixels);
	uint8_t *alphap     = static_cast<uint8_t *>(letter_surface->pixels) + 3;

	for (int i = src_surface->h; i != 0; i--) {
		for (int j = src_surface->w; j != 0; j--, src_buffer++) {
			*alphap = *src_buffer;
			alphap += 4;
		}
		alphap += letter_surface->pitch - (src_surface->w * 4);
		src_buffer += src_surface->pitch - src_surface->w;
	}

	auto &img = border ? border_gpu : glyph_gpu;

	img = gpu.copyImageFromSurface(letter_surface);

	if (atlas) {
		GPU_Rect rect;
		if (atlas->add(img->w + 2, img->h + 2, rect)) {

			gpu.copyGPUImage(img, nullptr, &rect, atlas->atlas->target, rect.x + 1, rect.y + 1);
			gpu.simulateRead(atlas->atlas);
			if (border)
				border_pos.set(rect);
			else
				glyph_pos.set(rect);
		} else {
			sendToLog(LogLevel::Error, "GlyphValues@buildGPUImage: Texture atlas addition failed!\n");
			ret = false;
		}
		GPU_FreeImage(img);
		img = nullptr;
	}

	SDL_FreeSurface(letter_surface);

	return ret;
}
