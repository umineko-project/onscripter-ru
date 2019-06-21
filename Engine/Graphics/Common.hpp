/**
 *  Common.hpp
 *  ONScripter-RU
 *
 *  Routine functions for software pixel access and transformations.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>

#include <cstdint>

#define RGBMASK 0x00ffffff
#define MEDGRAY 0x88888888

#define R_SHIFT 16
#define G_SHIFT 8
#define B_SHIFT 0
#define A_SHIFT 24

enum class BlendModeId {
	NORMAL,
	ADD,
	SUB,
	MUL, //textbox
	ALPHA,
	TOTAL
};

uint32_t getSurfacePixel(SDL_Surface *surface, int x, int y);
void setSurfacePixel(SDL_Surface *surface, int x, int y, uint32_t pixel);
int resizeSurface(SDL_Surface *src, SDL_Surface *dst);
int doClipping(GPU_Rect *dst, GPU_Rect *clip, GPU_Rect *clipped = nullptr);
void resizeImage(uint8_t *dst_buffer, int dst_width, int dst_height, int dst_total_width,
                 uint8_t *src_buffer, int src_width, int src_height, int src_total_width,
                 int byte_per_pixel, uint8_t *tmp_buffer, int tmp_total_width,
                 int num_cells = 1, bool no_interpolate = false);
