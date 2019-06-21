/**
 *  GlyphAtlas.cpp
 *  ONScripter-RU
 *
 *  Glyph map in a form of unified atlas for fast rendering.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Components/GlyphAtlas.hpp"
#include "Engine/Graphics/GPU.hpp"

void GlyphAtlasNode::reset(int w, int h) {
	rect = SDL_Rect{0, 0, w, h};
	left.reset();
	right.reset();
	exists = false;
}

SDL_Rect *GlyphAtlasNode::insert(int w, int h) {
	if (left) {
		// We're not a leaf
		auto newNode = left->insert(w, h);
		if (newNode)
			return newNode;
		return right->insert(w, h);
	}

	// We can't insert here, the entire space is used
	if (exists)
		return nullptr;

	// We can't insert here, the space is too small
	if (w > rect.w || h > rect.h)
		return nullptr;

	// The size is perfect, insert here
	if (w == rect.w && h == rect.h) {
		// let's set exists here...
		exists = true;
		return &rect;
	}

	// We have "more than" enough room here so we must split the space
	left  = std::make_unique<GlyphAtlasNode>();
	right = std::make_unique<GlyphAtlasNode>();

	// Decide which way to split
	auto dw = rect.w - w;
	auto dh = rect.h - h;
	if (dw > dh) {
		left->rect  = SDL_Rect{rect.x, rect.y, w, rect.h};
		right->rect = SDL_Rect{rect.x + w, rect.y, dw, rect.h};
	} else {
		left->rect  = SDL_Rect{rect.x, rect.y, rect.w, h};
		right->rect = SDL_Rect{rect.x, rect.y + h, rect.w, dh};
	}

	// Insert into left node (has sufficient space)
	return left->insert(w, h);
}

int GlyphAtlasController::ownInit() {
	atlas = gpu.createImage(width, height, 4);
	GPU_GetTarget(atlas);
	return 0;
}

int GlyphAtlasController::ownDeinit() {
	if (atlas)
		gpu.freeImage(atlas);
	return 0;
}

bool GlyphAtlasController::add(int w, int h, GPU_Rect &pos) {
	auto rect = root.insert(w, h);
	if (!rect) {
		return false;
	}
	pos.x = rect->x;
	pos.y = rect->y;
	pos.w = rect->w;
	pos.h = rect->h;
	return true;
}

void GlyphAtlasController::reset() {
	root.reset(width, height);
	gpu.clearWholeTarget(atlas->target);
}
