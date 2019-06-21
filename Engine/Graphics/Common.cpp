/**
 *  Common.hpp
 *  ONScripter-RU
 *
 *  Routine functions for software pixel access and transformations.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Graphics/Common.hpp"

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>

static unsigned long *pixel_accum     = nullptr;
static unsigned long *pixel_accum_num = nullptr;
static int pixel_accum_size           = 0;
static unsigned long tmp_acc[4];
static unsigned long tmp_acc_num[4];

static void calcWeightedSumColumnInit(uint8_t **src,
                                      int interpolation_height,
                                      int image_width, int image_height,
                                      int image_pixel_width, int byte_per_pixel) {
	int y_end = -interpolation_height / 2 + interpolation_height;

	std::memset(pixel_accum, 0, image_width * byte_per_pixel * sizeof(unsigned long));
	std::memset(pixel_accum_num, 0, image_width * byte_per_pixel * sizeof(unsigned long));
	for (int s = 0; s < byte_per_pixel; s++) {
		for (int i = 0; i < y_end - 1; i++) {
			if (i >= image_height)
				break;
			unsigned long *pa  = pixel_accum + image_width * s;
			unsigned long *pan = pixel_accum_num + image_width * s;
			uint8_t *p         = *src + image_pixel_width * i + s;
			for (int j = image_width; j != 0; j--, p += byte_per_pixel) {
				*pa++ += *p;
				(*pan++)++;
			}
		}
	}
}

static void calcWeightedSumColumn(uint8_t **src, int y,
                                  int interpolation_height,
                                  int image_width, int image_height,
                                  int image_pixel_width, int byte_per_pixel) {
	int y_start = y - interpolation_height / 2;
	int y_end   = y - interpolation_height / 2 + interpolation_height;

	for (int s = 0; s < byte_per_pixel; s++) {
		if ((y_start - 1) >= 0 && (y_start - 1) < image_height) {
			unsigned long *pa  = pixel_accum + image_width * s;
			unsigned long *pan = pixel_accum_num + image_width * s;
			uint8_t *p         = *src + image_pixel_width * (y_start - 1) + s;
			for (int j = image_width; j != 0; j--, p += byte_per_pixel) {
				*pa++ -= *p;
				(*pan++)--;
			}
		}

		if ((y_end - 1) >= 0 && (y_end - 1) < image_height) {
			unsigned long *pa  = pixel_accum + image_width * s;
			unsigned long *pan = pixel_accum_num + image_width * s;
			uint8_t *p         = *src + image_pixel_width * (y_end - 1) + s;
			for (int j = image_width; j != 0; j--, p += byte_per_pixel) {
				*pa++ += *p;
				(*pan++)++;
			}
		}
	}
}

static void calcWeightedSum(uint8_t **dst, int x_start, int x_end,
                            int image_width, int cell_start, int next_cell_start,
                            int byte_per_pixel) {
	for (int s = 0; s < byte_per_pixel; s++) {
		// avoid interpolating data from other cells or outside the image
		if (x_start >= cell_start && x_start < next_cell_start) {
			tmp_acc[s] -= pixel_accum[image_width * s + x_start];
			tmp_acc_num[s] -= pixel_accum_num[image_width * s + x_start];
		}
		if (x_end >= cell_start && x_end < next_cell_start) {
			tmp_acc[s] += pixel_accum[image_width * s + x_end];
			tmp_acc_num[s] += pixel_accum_num[image_width * s + x_end];
		}
		switch (tmp_acc_num[s]) {
			//avoid a division op if possible
			case 1:
				*(*dst)++ = static_cast<uint8_t>(tmp_acc[s]);
				break;
			case 2:
				*(*dst)++ = static_cast<uint8_t>(tmp_acc[s] >> 1);
				break;
			default:
			case 3:
				assert(tmp_acc_num[s] != 0);
				*(*dst)++ = static_cast<uint8_t>(tmp_acc[s] / tmp_acc_num[s]);
				break;
			case 4:
				*(*dst)++ = static_cast<uint8_t>(tmp_acc[s] >> 2);
				break;
		}
	}
}

void resizeImage(uint8_t *dst_buffer, int dst_width, int dst_height, int dst_total_width,
                 uint8_t *src_buffer, int src_width, int src_height, int src_total_width,
                 int byte_per_pixel, uint8_t *tmp_buffer, int tmp_total_width,
                 int num_cells, bool no_interpolate) {
	if (dst_width == 0 || dst_height == 0 ||
	    src_width < num_cells || src_height == 0)
		return;

	uint8_t *tmp_buf = tmp_buffer;
	uint8_t *src_buf = src_buffer;

	int i, j, s, c;

	int mx = 0, my = 0;

	if (src_width > 1)
		mx = byte_per_pixel;
	if (src_height > 1)
		my = tmp_total_width;

	int interpolation_width = src_width / dst_width;
	if (interpolation_width == 0)
		interpolation_width = 1;
	int interpolation_height = src_height / dst_height;
	if (interpolation_height == 0)
		interpolation_height = 1;

	int cell_width = src_width / num_cells;
	src_width      = cell_width * num_cells; //in case width is not a multiple of num_cells

	int tmp_offset = tmp_total_width - src_width * byte_per_pixel;

	if (pixel_accum_size < src_width * byte_per_pixel) {
		pixel_accum_size = src_width * byte_per_pixel;
		delete[] pixel_accum;
		pixel_accum = new unsigned long[pixel_accum_size];
		delete[] pixel_accum_num;
		pixel_accum_num = new unsigned long[pixel_accum_size];
	}
	/* smoothing */
	if (!no_interpolate && (byte_per_pixel >= 3)) {
		calcWeightedSumColumnInit(&src_buf, interpolation_height, src_width,
		                          src_height, src_total_width, byte_per_pixel);
		for (i = 0; i < src_height; i++) {
			calcWeightedSumColumn(&src_buf, i, interpolation_height, src_width,
			                      src_height, src_total_width, byte_per_pixel);
			for (c = 0; c < src_width; c += cell_width) {
				// do a separate set of smoothings for each cell,
				// to avoid interpolating data from other cells
				for (s = 0; s < byte_per_pixel; s++) {
					tmp_acc[s]     = 0;
					tmp_acc_num[s] = 0;
					for (j = 0; j < -interpolation_width / 2 + interpolation_width - 1; j++) {
						if (j >= cell_width)
							break;
						tmp_acc[s] += pixel_accum[src_width * s + c + j];
						tmp_acc_num[s] += pixel_accum_num[src_width * s + c + j];
					}
				}

				int x_start = c - interpolation_width / 2 - 1;
				int x_end   = x_start + interpolation_width;
				for (j = cell_width; j != 0; j--, x_start++, x_end++)
					calcWeightedSum(&tmp_buf, x_start, x_end,
					                src_width, c, c + cell_width,
					                byte_per_pixel);
			}
			tmp_buf += tmp_offset;
		}
	} else {
		tmp_buffer = src_buffer;
	}

	/* resampling */
	int *dst_to_src = new int[dst_width]; //lookup table for horiz resampling loop
	for (j = 0; j < dst_width; j++)
		dst_to_src[j] = (j << 3) * src_width / dst_width;
	uint8_t *dst_buf = dst_buffer;
	if (!no_interpolate && (byte_per_pixel >= 3)) {
		for (i = 0; i < dst_height; i++) {
			int y  = (i << 3) * src_height / dst_height;
			int dy = y & 0x7;
			y >>= 3;
			//avoid resampling outside the image
			int iy = 0;
			if (y < src_height - 1)
				iy = my;

			for (j = 0; j < dst_width; j++) {
				int x  = dst_to_src[j];
				int dx = x & 0x7;
				x >>= 3;
				//avoid resampling from outside the current cell
				int ix = mx;
				if (((x + 1) % cell_width) == 0)
					ix = 0;

				int k = tmp_total_width * y + x * byte_per_pixel;

				for (s = byte_per_pixel; s != 0; s--, k++) {
					unsigned int p;
					p = (8 - dx) * (8 - dy) * tmp_buffer[k];
					p += dx * (8 - dy) * tmp_buffer[k + ix];
					p += (8 - dx) * dy * tmp_buffer[k + iy];
					p += dx * dy * tmp_buffer[k + ix + iy];
					*dst_buf++ = static_cast<uint8_t>(p >> 6);
				}
			}
			for (j = dst_total_width - dst_width * byte_per_pixel; j > 0; j--)
				*dst_buf++ = 0;
		}
	} else {
		for (i = 0; i < dst_height; i++) {
			int y = (i << 3) * src_height / dst_height;
			y >>= 3;

			for (j = 0; j < dst_width; j++) {
				int x = dst_to_src[j] >> 3;
				int k = src_total_width * y + x * byte_per_pixel;

				for (s = byte_per_pixel; s != 0; s--, k++) {
					*dst_buf++ = tmp_buffer[k];
				}
			}
			for (j = dst_total_width - dst_width * byte_per_pixel; j != 0; j--)
				*dst_buf++ = 0;
		}
	}
	delete[] dst_to_src;

	/* pixels at the corners (of each cell) are preserved */
	int dst_cell_width = byte_per_pixel * dst_width / num_cells;
	cell_width *= byte_per_pixel;
	for (c = 0; c < num_cells; c++) {
		for (i = 0; i < byte_per_pixel; i++) {
			dst_buffer[c * dst_cell_width + i] = src_buffer[c * cell_width + i];
			dst_buffer[(c + 1) * dst_cell_width - byte_per_pixel + i] =
			    src_buffer[(c + 1) * cell_width - byte_per_pixel + i];
			dst_buffer[(dst_height - 1) * dst_total_width + c * dst_cell_width + i] =
			    src_buffer[(src_height - 1) * src_total_width + c * cell_width + i];
			dst_buffer[(dst_height - 1) * dst_total_width + (c + 1) * dst_cell_width - byte_per_pixel + i] =
			    src_buffer[(src_height - 1) * src_total_width + (c + 1) * cell_width - byte_per_pixel + i];
		}
	}
}

// resize 32bit surface to 32bit surface
int resizeSurface(SDL_Surface *src, SDL_Surface *dst) {
	uint32_t *src_buffer = static_cast<uint32_t *>(src->pixels);
	uint32_t *dst_buffer = static_cast<uint32_t *>(dst->pixels);

	/* size of tmp_buffer must be larger than 16 bytes */
	size_t len         = src->w * (src->h + 1) * 4 + 4;
	auto resize_buffer = std::make_unique<uint8_t[]>(len);

	resizeImage(reinterpret_cast<uint8_t *>(dst_buffer), dst->w, dst->h, dst->w * 4,
	            reinterpret_cast<uint8_t *>(src_buffer), src->w, src->h, src->w * 4,
	            4, resize_buffer.get(), src->w * 4, 1);

	return 0;
}

uint32_t getSurfacePixel(SDL_Surface *surface, int x, int y) {
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	uint8_t *p = static_cast<uint8_t *>(surface->pixels) + y * surface->pitch + x * bpp;

	switch (bpp) {
		case 1:
			return *p;
		case 2:
			return *reinterpret_cast<uint16_t *>(p);
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return p[0] << 16 | p[1] << 8 | p[2];
			else
				return p[0] | p[1] << 8 | p[2] << 16;
		case 4:
			return *reinterpret_cast<uint32_t *>(p);
		default:
			return 0; /* shouldn't happen, but avoids warnings */
	}
}

void setSurfacePixel(SDL_Surface *surface, int x, int y, uint32_t pixel) {
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to set */
	uint8_t *p = static_cast<uint8_t *>(surface->pixels) + y * surface->pitch + x * bpp;

	switch (bpp) {
		case 1:
			*p = pixel;
			break;

		case 2:
			*reinterpret_cast<uint16_t *>(p) = pixel;
			break;

		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
			} else {
				p[0] = pixel & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = (pixel >> 16) & 0xff;
			}
			break;

		case 4:
			*reinterpret_cast<uint32_t *>(p) = pixel;
			break;
	}
}

int doClipping(GPU_Rect *dst, GPU_Rect *clip, GPU_Rect *clipped) {
	if (clipped)
		clipped->x = clipped->y = 0;

	if (!dst ||
	    dst->x >= clip->x + clip->w || dst->x + dst->w <= clip->x ||
	    dst->y >= clip->y + clip->h || dst->y + dst->h <= clip->y ||
	    clip->w == 0 || clip->h == 0) // hope for no annoying ==0 float bugs, should do this properly with epsilon maybe
		return -1;

	if (dst->x < clip->x) {
		dst->w -= clip->x - dst->x;
		if (clipped)
			clipped->x = clip->x - dst->x;
		dst->x = clip->x;
	}
	if (clip->x + clip->w < dst->x + dst->w) {
		dst->w = clip->x + clip->w - dst->x;
	}

	if (dst->y < clip->y) {
		dst->h -= clip->y - dst->y;
		if (clipped)
			clipped->y = clip->y - dst->y;
		dst->y = clip->y;
	}
	if (clip->y + clip->h < dst->y + dst->h) {
		dst->h = clip->y + clip->h - dst->y;
	}
	if (clipped) {
		clipped->w = dst->w;
		clipped->h = dst->h;
	}

	return 0;
}
