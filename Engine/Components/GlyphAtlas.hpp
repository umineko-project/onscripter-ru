/**
 *  GlyphAtlas.hpp
 *  ONScripter-RU
 *
 *  Glyph map in a form of unified atlas for fast rendering.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>

#include <memory>

// double 4096 is a bit too much for iOS
const int NUM_GLYPH_CACHE = 2048;
const int GLYPH_ATLAS_W   = 2048;
const int GLYPH_ATLAS_H   = 4096;

class GlyphAtlasNode {
	std::unique_ptr<GlyphAtlasNode> left, right;
	SDL_Rect rect{};
	bool exists{false};

public:
	void reset(int w, int h);
	SDL_Rect *insert(int w, int h);
};

class GlyphAtlasController : public BaseController {
	GlyphAtlasNode root;
	int width, height;

protected:
	int ownInit() override;
	int ownDeinit() override;

public:
	GlyphAtlasController(int w, int h)
	    : BaseController(this), width(w), height(h) {
		root.reset(width, height);
	}

	bool add(int w, int h, GPU_Rect &pos);
	void reset();

	GPU_Image *atlas{nullptr};
};
