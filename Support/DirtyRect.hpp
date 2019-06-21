/**
 *  DirtyRect.hpp
 *  ONScripter-RU
 *
 *  Invalid region on image target which should be updated.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>

struct DirtyRect {
	void setDimension(const SDL_Point &canvas, const GPU_Rect &camera_center);
	void add(GPU_Rect src);
	void clear();
	void fill(int w, int h);
	bool isEmpty();

	GPU_Rect calcBoundingBox(GPU_Rect src1, GPU_Rect &src2);

	SDL_Point canvas_dim{};
	GPU_Rect camera_center_pos{};
	GPU_Rect bounding_box{};
	GPU_Rect bounding_box_script{};
};
