/**
 *  DirtyRect.cpp
 *  ONScripter-RU
 *
 *  Invalid region on image target which should be updated.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Support/DirtyRect.hpp"

#include <cmath>

void DirtyRect::setDimension(const SDL_Point &canvas, const GPU_Rect &camera_center) {
	canvas_dim        = canvas;
	camera_center_pos = camera_center;
	fill(canvas_dim.x, canvas_dim.y);
}

// src given in offset coordinates with 0,0 corresponding to the top-left corner of camera_center_pos.
void DirtyRect::add(GPU_Rect src) {
	if (src.w == 0 || src.h == 0)
		return;

	if (src.x != static_cast<int>(src.x)) {
		src.x = std::floor(src.x);
		src.w++;
	}

	if (src.y != static_cast<int>(src.y)) {
		src.y = std::floor(src.y);
		src.h++;
	}

	src.x += camera_center_pos.x;
	src.y += camera_center_pos.y;

	if (src.x < 0) {
		if (src.w < -src.x)
			return;
		src.w += src.x;
		src.x = 0;
	}

	if (src.y < 0) {
		if (src.h < -src.y)
			return;
		src.h += src.y;
		src.y = 0;
	}

	if (src.x >= canvas_dim.x)
		return;
	if (src.x + src.w >= canvas_dim.x)
		src.w = canvas_dim.x - src.x;

	if (src.y >= canvas_dim.y)
		return;
	if (src.y + src.h >= canvas_dim.y)
		src.h = canvas_dim.y - src.y;

	bounding_box        = calcBoundingBox(bounding_box, src);
	bounding_box_script = bounding_box;
	bounding_box_script.x -= camera_center_pos.x;
	bounding_box_script.y -= camera_center_pos.y;
}

GPU_Rect DirtyRect::calcBoundingBox(GPU_Rect src1, GPU_Rect &src2) {
	if (src2.w == 0 || src2.h == 0)
		return src1;
	if (src1.w == 0 || src1.h == 0)
		return src2;

	if (src1.x > src2.x) {
		src1.w += src1.x - src2.x;
		src1.x = src2.x;
	}

	if (src1.y > src2.y) {
		src1.h += src1.y - src2.y;
		src1.y = src2.y;
	}

	if (src1.x + src1.w < src2.x + src2.w) {
		src1.w = src2.x + src2.w - src1.x;
	}

	if (src1.y + src1.h < src2.y + src2.h) {
		src1.h = src2.y + src2.h - src1.y;
	}

	return src1;
}

void DirtyRect::clear() {
	bounding_box.w = bounding_box.h = 0;
	bounding_box_script.w = bounding_box_script.h = 0;
}

void DirtyRect::fill(int w, int h) {
	bounding_box.x = 0;
	bounding_box.y = 0;
	bounding_box.w = w;
	bounding_box.h = h;

	bounding_box_script.x = -camera_center_pos.x;
	bounding_box_script.y = -camera_center_pos.y;
	bounding_box_script.w = w;
	bounding_box_script.h = h;
}

bool DirtyRect::isEmpty() {
	return bounding_box.w * bounding_box.h == 0;
}
