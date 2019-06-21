/**
 *  PNG.cpp
 *  ONScripter-RU
 *
 *  Contains thread safe libpng wrapper, most of the code borrowed from SDL_image.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Graphics/PNG.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Graphics/GPU.hpp"

PNGLoader::PNGLoader() {
	this->png_create_info_struct  = ::png_create_info_struct;
	this->png_create_read_struct  = ::png_create_read_struct;
	this->png_destroy_read_struct = ::png_destroy_read_struct;
	this->png_get_IHDR            = ::png_get_IHDR;
	this->png_get_channels        = ::png_get_channels;
	this->png_get_io_ptr          = ::png_get_io_ptr;
	this->png_get_PLTE            = ::png_get_PLTE;
	this->png_get_tRNS            = ::png_get_tRNS;
	this->png_get_valid           = ::png_get_valid;
	this->png_read_image          = ::png_read_image;
	this->png_read_info           = ::png_read_info;
	this->png_read_update_info    = ::png_read_update_info;
	this->png_set_expand          = ::png_set_expand;
	this->png_set_gray_to_rgb     = ::png_set_gray_to_rgb;
	this->png_set_packing         = ::png_set_packing;
	this->png_set_read_fn         = ::png_set_read_fn;
	this->png_set_strip_16        = ::png_set_strip_16;
	this->png_sig_cmp             = ::png_sig_cmp;
	this->png_set_longjmp_fn      = ::png_set_longjmp_fn;
}

/* See if an image is contained in a data source */
int PNGLoader::isPng(SDL_RWops *src) {
	Sint64 start;
	int is_PNG;
	uint8_t magic[4];

	if (!src) {
		return 0;
	}

	start  = SDL_RWtell(src);
	is_PNG = 0;
	if (SDL_RWread(src, magic, 1, sizeof(magic)) == sizeof(magic)) {
		if (magic[0] == 0x89 &&
		    magic[1] == 'P' &&
		    magic[2] == 'N' &&
		    magic[3] == 'G') {
			is_PNG = 1;
		}
	}
	SDL_RWseek(src, start, RW_SEEK_SET);
	return (is_PNG);
}

void PNGLoader::png_read_data(png_structp ctx, png_bytep area, png_size_t size) {
	SDL_RWops *src;

	src = static_cast<SDL_RWops *>(::png_get_io_ptr(ctx));
	SDL_RWread(src, area, size, 1);
}

SDL_Surface *PNGLoader::loadPng(SDL_RWops *src) {
	Sint64 start;
	const char *error;
	SDL_Surface *volatile surface;
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type, num_channels;
	uint32_t Rmask;
	uint32_t Gmask;
	uint32_t Bmask;
	uint32_t Amask;
	SDL_Palette *palette;
	png_bytep *volatile row_pointers;
	int row, i;
	int ckey = -1;
	png_color_16 *transv;

	if (!src) {
		/* The error message has been set in SDL_RWFromFile */
		return nullptr;
	}
	start = SDL_RWtell(src);

	/* Initialize the data we will clean up when we're done */
	error        = nullptr;
	info_ptr     = nullptr;
	row_pointers = nullptr;
	surface      = nullptr;

	/* Create the PNG loading context structure */
	png_ptr = this->png_create_read_struct(PNG_LIBPNG_VER_STRING,
	                                       nullptr, nullptr, nullptr);
	if (png_ptr == nullptr) {
		error = "Couldn't allocate memory for PNG file or incompatible PNG dll";
		goto done;
	}

	/* Allocate/initialize the memory for image information.  REQUIRED. */
	info_ptr = this->png_create_info_struct(png_ptr);
	if (info_ptr == nullptr) {
		error = "Couldn't create image information for PNG file";
		goto done;
	}

	/* Set error handling if you are using setjmp/longjmp method (this is
     * the normal method of doing things with libpng).  REQUIRED unless you
     * set up your own error handlers in png_create_read_struct() earlier.
     */
	if (setjmp(*this->png_set_longjmp_fn(png_ptr, longjmp, sizeof(jmp_buf)))) {
		error = "Error reading the PNG file.";
		goto done;
	}

	/* Set up the input control */
	this->png_set_read_fn(png_ptr, src, this->png_read_data);

	/* Read PNG header info */
	this->png_read_info(png_ptr, info_ptr);
	this->png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
	                   &color_type, &interlace_type, nullptr, nullptr);

	/* tell libpng to strip 16 bit/color files down to 8 bits/color */
	this->png_set_strip_16(png_ptr);

	/* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
     * byte into separate bytes (useful for paletted and grayscale images).
     */
	this->png_set_packing(png_ptr);

	/* scale greyscale values to the range 0..255 */
	if (color_type == PNG_COLOR_TYPE_GRAY)
		this->png_set_expand(png_ptr);

	/* For images with a single "transparent colour", set colour key;
	 if more than one index has transparency, or if partially transparent
	 entries exist, use full alpha channel */
	if (this->png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		int num_trans;
		uint8_t *trans;
		this->png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &transv);
		if (color_type == PNG_COLOR_TYPE_PALETTE) {
			/* Check if all tRNS entries are opaque except one */
			int j, t = -1;
			for (j = 0; j < num_trans; j++) {
				if (trans[j] == 0) {
					if (t >= 0) {
						break;
					}
					t = j;
				} else if (trans[j] != 255) {
					break;
				}
			}
			if (j == num_trans) {
				/* exactly one transparent index */
				ckey = t;
			} else {
				/* more than one transparent index, or translucency */
				this->png_set_expand(png_ptr);
			}
		} else {
			ckey = 0; /* actual value will be set later */
		}
	}

	if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		this->png_set_gray_to_rgb(png_ptr);

	this->png_read_update_info(png_ptr, info_ptr);

	this->png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
	                   &color_type, &interlace_type, nullptr, nullptr);

	/* Allocate the SDL surface to hold the image */
	Rmask = Gmask = Bmask = Amask = 0;
	num_channels                  = this->png_get_channels(png_ptr, info_ptr);
	if (num_channels >= 3) {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		Rmask = 0x000000FF;
		Gmask = 0x0000FF00;
		Bmask = 0x00FF0000;
		Amask = (num_channels == 4) ? 0xFF000000 : 0;
#else
		int s = (num_channels == 4) ? 0 : 8;
		Rmask = 0xFF000000 >> s;
		Gmask = 0x00FF0000 >> s;
		Bmask = 0x0000FF00 >> s;
		Amask = 0x000000FF >> s;
#endif
	}

	gpu.scheduleLoadImage(width, height);

	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height,
	                               bit_depth * num_channels, Rmask, Gmask, Bmask, Amask);
	if (surface == nullptr) {
		error = SDL_GetError();
		goto done;
	}

	if (ckey != -1) {
		if (color_type != PNG_COLOR_TYPE_PALETTE) {
			//FIXME: Should these be truncated or shifted down?
			ckey = SDL_MapRGB(surface->format,
			                  static_cast<uint8_t>(transv->red),
			                  static_cast<uint8_t>(transv->green),
			                  static_cast<uint8_t>(transv->blue));
		}
		SDL_SetColorKey(surface, SDL_TRUE, ckey);
	}

	/* Create the array of pointers to image data */
	row_pointers = static_cast<png_bytep *>(SDL_malloc(sizeof(png_bytep) * height));
	if (!row_pointers) {
		error = "Out of memory";
		goto done;
	}
	for (row = 0; row < static_cast<int>(height); row++) {
		row_pointers[row] = static_cast<png_bytep>(surface->pixels) + row * surface->pitch;
	}

	/* Read the entire image in one go */
	this->png_read_image(png_ptr, row_pointers);

	/* and we're done!  (png_read_end() can be omitted if no processing of
     * post-IDAT text/time/etc. is desired)
     * In some cases it can't read PNG's created by some popular programs (ACDSEE),
     * we do not want to process comments, so we omit png_read_end
	 
	 this->png_read_end(png_ptr, info_ptr);
	 */

	/* Load the palette, if any */
	palette = surface->format->palette;
	if (palette) {
		int png_num_palette;
		png_colorp png_palette;
		this->png_get_PLTE(png_ptr, info_ptr, &png_palette, &png_num_palette);
		if (color_type == PNG_COLOR_TYPE_GRAY) {
			palette->ncolors = 256;
			for (i = 0; i < 256; i++) {
				palette->colors[i].r = i;
				palette->colors[i].g = i;
				palette->colors[i].b = i;
			}
		} else if (png_num_palette > 0) {
			palette->ncolors = png_num_palette;
			for (i = 0; i < png_num_palette; ++i) {
				palette->colors[i].b = png_palette[i].blue;
				palette->colors[i].g = png_palette[i].green;
				palette->colors[i].r = png_palette[i].red;
			}
		}
	}

done: /* Clean up and return */
	if (png_ptr) {
		this->png_destroy_read_struct(&png_ptr,
		                              info_ptr ? &info_ptr : nullptr,
		                              nullptr);
	}
	if (row_pointers) {
		SDL_free(row_pointers);
	}
	if (error) {
		SDL_RWseek(src, start, RW_SEEK_SET);
		if (surface) {
			SDL_FreeSurface(surface);
			surface = nullptr;
		}
		SDL_SetError("%s", error);
	}
	return (surface);
}
