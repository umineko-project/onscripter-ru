/**
 *  Animation.cpp
 *  ONScripter-RU
 *
 *  General image storage class.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Entities/Animation.hpp"
#include "Engine/Entities/ConstantRefresh.hpp"
#include "Engine/Graphics/GPU.hpp"
#include "Support/FileDefs.hpp"

#include <cmath>
#include <algorithm>

void AnimationInfo::performCopyID(const AnimationInfo &o) {
	type        = o.type;
	id          = o.id;
	exists      = o.exists;
	childImages = o.childImages;
}

void AnimationInfo::performCopyNonImageFields(const AnimationInfo &o) {
	trans_mode      = o.trans_mode;
	direct_color    = o.direct_color;
	color           = o.color;
	num_of_cells    = o.num_of_cells;
	current_cell    = o.current_cell;
	direction       = o.direction;
	duration_list   = copyarr(o.duration_list, num_of_cells);
	color_list      = copyarr(o.color_list, num_of_cells);
	loop_mode       = o.loop_mode;
	vertical_cells  = o.vertical_cells;
	is_animatable   = o.is_animatable;
	skip_whitespace = o.skip_whitespace;
	layer_no        = o.layer_no;
	file_name       = copystr(o.file_name);
	lips_name       = copystr(o.lips_name);
	mask_file_name  = copystr(o.mask_file_name);
	blending_mode   = o.blending_mode;
	copyarr(font_size_xy, o.font_size_xy);

	stale_image     = o.stale_image;
	deferredLoading = o.deferredLoading;

	orig_pos   = o.orig_pos;
	pos        = o.pos;
	scrollable = o.scrollable;

	has_z_order_override = o.has_z_order_override;
	z_order_override     = o.z_order_override;
	parentImage          = o.parentImage;

	visible      = o.visible;
	abs_flag     = o.abs_flag;
	trans        = o.trans;
	darkenHue    = o.darkenHue;
	is_big_image = o.is_big_image;
	flip         = o.flip;
	image_name   = copystr(image_name);

	// Could have used initialiser lists but screw it for the sake of code reuse
	spriteTransforms = o.spriteTransforms;
	scrollableInfo   = o.scrollableInfo;
	clock            = o.clock;
	camera           = o.camera;

	scale_x = o.scale_x;
	scale_y = o.scale_y;
	rot     = o.rot;
	copyarr(mat, o.mat);
	copyarr(corner_xy, o.corner_xy);
	bounding_rect    = o.bounding_rect;
	has_hotspot      = o.has_hotspot;
	has_scale_center = o.has_scale_center;
	hotspot          = o.hotspot;
	scale_center     = o.scale_center;
	rendering_center = o.rendering_center;

	param     = o.param;
	max_param = o.max_param;
	max_width = o.max_width;
}

AnimationInfo::AnimationInfo(const AnimationInfo &o) {
	performCopyID(o);
	performCopyNonImageFields(o);

	// Increment image refs
	image_surface = o.image_surface;
	gpu_image     = o.gpu_image;
	if (image_surface)
		image_surface->refcount++;
	if (gpu_image)
		gpu_image->refcount++;
	big_image = o.big_image;
}

AnimationInfo::~AnimationInfo() {
	reset();
}

//deepcopy everything but the images
void AnimationInfo::deepcopyNonImageFields(const AnimationInfo &o) {
	if (this == &o)
		return;

	// Clear old stuff first
	remove();

	// Copy!
	performCopyID(o);
	performCopyNonImageFields(o);
}

void AnimationInfo::deepcopy(const AnimationInfo &o) {
	if (this == &o)
		return;

	// Clear old stuff first
	remove();

	// Copy!
	performCopyNonImageFields(o);

	// Copy images
	if (o.image_surface) {
		calculateImage(o.image_surface->w, o.image_surface->h);
		copySurface(o.image_surface);
	}
	if (o.gpu_image)
		gpu_image = gpu.copyImage(o.gpu_image);
	auto bi = o.big_image.get();
	if (bi)
		big_image = std::make_shared<GPUBigImage>(*bi);
}

void AnimationInfo::reset() {
	remove();

	/* Is this the place for this? I don't think we can do it in remove,
	that's called in case of csp etc when we want to keep the old_ai around...
	but this function has no other examples of any "resource cleanup" */
	freevar(&old_ai);

	//Reset can be called outside on "reset" command
	childImages.clear();
}

void AnimationInfo::setImageName(const char *name) {
	freearr(&image_name);
	image_name = copystr(name);
}

void AnimationInfo::deleteImage() {
	if (image_surface)
		SDL_FreeSurface(image_surface);
	if (gpu_image)
		gpu.freeImage(gpu_image);

	gpu_image               = nullptr;
	image_surface           = nullptr;
	big_image               = nullptr;
	stale_image             = true;
	distinguish_from_old_ai = true;
	//We need to do it here, because updateSpritePos does not touch scrollable area and sprite reuse is pretty undefined
	scrollable.x = scrollable.y = scrollable.w = scrollable.h = 0;
}

// The difference between remove and reset is that remove is backup-preserving.
// It doesn't destroy old_ai.
void AnimationInfo::remove() {
	freearr(&image_name);
	deleteImage();
	removeNonImageFields();
}

void AnimationInfo::removeTag() {
	freearr(&duration_list);
	freearr(&color_list);
	freearr(&file_name);
	freearr(&lips_name);
	freearr(&mask_file_name);

	exists          = false;
	current_cell    = 0;
	num_of_cells    = 0;
	loop_mode       = 0;
	vertical_cells  = false;
	is_animatable   = false;
	direction       = 1;
	skip_whitespace = false;
	blending_mode   = BlendModeId::NORMAL;
	trans_mode      = TRANS_COPY;

	color           = {};
	direct_color    = {};
	font_size_xy[0] = font_size_xy[1] = -1;

	layer_no = -1;
}

void AnimationInfo::removeNonImageFields() {
	// Renamed from removeTag. The reason is:
	// This no longer removes just the fields initialized by parseTaggedString but instead all the fields of AI
	// (in order to ensure that no old data remains to screw us up later -- centralized cleanup is good)
	// except for the image name and image, which may possibly be re-used,
	// and fields like id and type, which are slot properties.

	// Just like remove(), this function is backup-preserving.

	removeTag();

	camera               = Camera();
	clock                = Clock();
	scrollableInfo       = ScrollableInfo();
	spriteTransforms     = SpriteTransforms();
	has_z_order_override = has_hotspot = has_scale_center = false;

	is_big_image = false;
	trans        = 255;
	darkenHue.r = darkenHue.g = darkenHue.b = 255;
	flip                                    = FLIP_NONE;
	deferredLoading                         = false;
	orig_pos.x = orig_pos.y = 0;
	orig_pos.w = orig_pos.h = 0;
	pos.x = pos.y = 0;
	pos.w = pos.h   = 0;
	bounding_rect.x = bounding_rect.y = 0;
	bounding_rect.w = bounding_rect.h = 0;
	visible                           = false;
	abs_flag                          = true;
	scale_x = scale_y = rot = 0;

	mat[0][0] = 1024;
	mat[0][1] = 0;
	mat[1][0] = 0;
	mat[1][1] = 1024;
}

// 0 ... restart at the end
// 1 ... stop at the end
// 2 ... reverse at the end
// 3 ... no animation
bool AnimationInfo::proceedAnimation() {
	bool is_changed = false;

	if (loop_mode != 3 && num_of_cells > 1) {
		current_cell += direction;
		is_changed = true;
	}

	if (current_cell < 0) { // loop_mode must be 2
		current_cell = 1;
		direction    = 1;
	} else if (current_cell >= num_of_cells) {
		if (loop_mode == 0) {
			current_cell = 0;
		} else if (loop_mode == 1) {
			current_cell = num_of_cells - 1;
			is_changed   = false;
		} else {
			current_cell = num_of_cells - 2;
			direction    = -1;
		}
	}

	clock.setCountdownNanos(getDurationNanos(current_cell));

	return is_changed;
}

uint64_t AnimationInfo::getDurationNanos(int i) {
	if (duration_list[i] < 0) {
		return 0; // We refresh according to CR
	}
	return 1000000 * static_cast<uint64_t>(duration_list[i]);
}

int AnimationInfo::getDuration(int i) {
	if (duration_list[i] < 0) {
		return 0; // We refresh according to CR
	}
	return duration_list[i];
}

void AnimationInfo::setCell(int cell) {
	if (cell < 0)
		cell = 0;
	else if (cell >= num_of_cells)
		cell = num_of_cells - 1;

	current_cell = cell;
}

float2 AnimationInfo::findOpaquePoint(GPU_Rect *clip) {
	//find the first opaque-enough pixel position for transbtn
	if (!image_surface)
		image_surface = GPU_CopySurfaceFromImage(gpu_image);
	int cell_width    = vertical_cells ? image_surface->w : image_surface->w / num_of_cells;
	int cell_height   = vertical_cells ? image_surface->h / num_of_cells : image_surface->h;
	GPU_Rect cliprect = {0, 0, static_cast<float>(cell_width), static_cast<float>(cell_height)};
	if (clip)
		cliprect = *clip;

	const int psize = 4;
	uint8_t *alphap = static_cast<uint8_t *>(image_surface->pixels) + 3;

	float2 ret = {0, 0};

	for (int i = cliprect.y; i < cliprect.h; ++i) {
		for (int j = cliprect.x; j < cliprect.w; ++j) {
			int alpha = *(alphap + (image_surface->w * i + j) * psize);
			if (alpha > TRANSBTN_CUTOFF) {
				ret.x = j;
				ret.y = i;
				//want to break out of the for loops
				i = cliprect.h;
				break;
			}
		}
	}
	//want to find a pixel that's opaque across all cells, if possible
	int xstart = ret.x;
	for (int i = ret.y; i < cliprect.h; ++i) {
		for (int j = xstart; j < cliprect.w; ++j) {
			bool is_opaque = true;
			for (int k = 0; k < num_of_cells; ++k) {
				int alpha = *(alphap + (image_surface->w * i + (vertical_cells ? cell_height * cell_width : cell_width) * k + j) * psize);
				if (alpha <= TRANSBTN_CUTOFF) {
					is_opaque = false;
					break;
				}
			}
			if (is_opaque) {
				ret.x = j;
				ret.y = i;
				//want to break out of the for loops
				i = cliprect.h;
				break;
			}
			xstart = cliprect.x;
		}
	}

	return ret;
}

int AnimationInfo::getPixelAlpha(int x, int y) {
	if (!image_surface)
		image_surface = GPU_CopySurfaceFromImage(gpu_image);
	const int psize       = 4;
	const int total_width = image_surface->w * psize;
	const int cell_off    = (vertical_cells ? total_width * image_surface->h : total_width) * current_cell / num_of_cells;

	return *(static_cast<uint8_t *>(image_surface->pixels) + cell_off + total_width * y + x * psize + 3);
}

void AnimationInfo::calcAffineMatrix(int script_width, int script_height) {
	auto scale_x_local = scale_x;
	auto scale_y_local = scale_y;

	if (flip & FLIP_HORIZONTALLY)
		scale_x_local = -scale_x_local;
	if (flip & FLIP_VERTICALLY)
		scale_y_local = -scale_y_local;

	/*	Here lies an approach that was never needed
	std::valarray<float> spriteRotationArray =
		 {	std::cos(-M_PI*rot/180), -std::sin(-M_PI*rot/180), 0,
			std::sin(-M_PI*rot/180),  std::cos(-M_PI*rot/180), 0,
			0, 0, 1};*/

	// calculate forward matrix
	// |mat[0][0] mat[0][1]|
	// |mat[1][0] mat[1][1]|
	int cos_i = 1024, sin_i = 0;
	if (rot != 0) {
		cos_i = static_cast<int>(1024.0 * std::cos(-M_PI * rot / 180));
		sin_i = static_cast<int>(1024.0 * std::sin(-M_PI * rot / 180));
	}
	mat[0][0] = cos_i * scale_x_local / 100;
	mat[0][1] = -sin_i * scale_y_local / 100;
	mat[1][0] = sin_i * scale_x_local / 100;
	mat[1][1] = cos_i * scale_y_local / 100;

	float scale_center_offset_x = 0, scale_center_offset_y = 0;

	if (has_scale_center) {
		scale_center_offset_x -= scale_center.x;
		scale_center_offset_y -= scale_center.y;
	}
	if (has_hotspot) {
		scale_center_offset_x -= hotspot.x - (pos.w / 2.0);
		scale_center_offset_y -= hotspot.y - (pos.h / 2.0);
	}

	// calculate bounding box
	float min_xy[2] = {0, 0}, max_xy[2] = {0, 0};
	for (int i = 0; i < 4; i++) {
		//Mion: need to make sure corners are in the right order
		//(UL,LL,LR,UR of the original image)
		float c_x = (i < 2) ? (-pos.w / 2.0) : (pos.w / 2.0);
		float c_y = ((i + 1) & 2) ? (pos.h / 2.0) : (-pos.h / 2.0);

		c_x += scale_center_offset_x;
		c_y += scale_center_offset_y;

		if (scale_x < 0)
			c_x = -c_x;
		if (scale_y < 0)
			c_y = -c_y;
		corner_xy[i][0] = (mat[0][0] * c_x + mat[0][1] * c_y) / 1024 + pos.x - scale_center_offset_x;
		corner_xy[i][1] = (mat[1][0] * c_x + mat[1][1] * c_y) / 1024 + pos.y - scale_center_offset_y;

		if (has_hotspot) {
			corner_xy[i][0] += (script_width / 2.0) - hotspot.x + (pos.w / 2.0);
			corner_xy[i][1] += (script_height)-hotspot.y + (pos.h / 2.0);
		}

		if (i == 0 || min_xy[0] > corner_xy[i][0])
			min_xy[0] = corner_xy[i][0];
		if (i == 0 || max_xy[0] < corner_xy[i][0])
			max_xy[0] = corner_xy[i][0];
		if (i == 0 || min_xy[1] > corner_xy[i][1])
			min_xy[1] = corner_xy[i][1];
		if (i == 0 || max_xy[1] < corner_xy[i][1])
			max_xy[1] = corner_xy[i][1];
	}

	bounding_rect.x = min_xy[0];
	bounding_rect.y = min_xy[1];
	bounding_rect.w = max_xy[0] - min_xy[0] + 1;
	bounding_rect.h = max_xy[1] - min_xy[1] + 1;

	//Also compute rotated center
	rendering_center.x = ((mat[0][0] * scale_center_offset_x + mat[0][1] * scale_center_offset_y) / 1024) + pos.x - scale_center_offset_x + (has_hotspot ? (script_width / 2.0) + (pos.w / 2.0) - hotspot.x : 0);
	rendering_center.y = ((mat[1][0] * scale_center_offset_x + mat[1][1] * scale_center_offset_y) / 1024) + pos.y - scale_center_offset_y + (has_hotspot ? (script_height) + (pos.h / 2.0) - hotspot.y : 0);
}

void AnimationInfo::calculateImage(int w, int h) {
	if ((!image_surface || image_surface->w != w || image_surface->h != h) &&
	    (!gpu_image || gpu_image->w != w || gpu_image->h != h) &&
	    (is_big_image && (!big_image.get() || big_image->w != w || big_image->h != h))) {
		deleteImage();
	}

	abs_flag   = true;
	orig_pos.w = w;
	orig_pos.h = h;
	pos.w      = static_cast<float>(vertical_cells ? w : w / num_of_cells);
	pos.h      = static_cast<float>(vertical_cells ? h / num_of_cells : h);
}

void AnimationInfo::copySurface(SDL_Surface *surface, GPU_Rect *src_rect, GPU_Rect *dst_rect) {
	if (!image_surface || !surface)
		return;

	GPU_Rect _dst_rect = {0, 0, static_cast<float>(image_surface->w), static_cast<float>(image_surface->h)};
	if (dst_rect)
		_dst_rect = *dst_rect;

	GPU_Rect _src_rect = {0, 0, static_cast<float>(surface->w), static_cast<float>(surface->h)};
	if (src_rect)
		_src_rect = *src_rect;

	if (_src_rect.x >= surface->w)
		return;
	if (_src_rect.y >= surface->h)
		return;

	if (_src_rect.x + _src_rect.w >= surface->w)
		_src_rect.w = surface->w - _src_rect.x;
	if (_src_rect.y + _src_rect.h >= surface->h)
		_src_rect.h = surface->h - _src_rect.y;

	if (_dst_rect.x + _src_rect.w > image_surface->w)
		_src_rect.w = image_surface->w - _dst_rect.x;
	if (_dst_rect.y + _src_rect.h > image_surface->h)
		_src_rect.h = image_surface->h - _dst_rect.y;

	int i;
	for (i = 0; i < _src_rect.h; i++)
		std::memcpy(reinterpret_cast<ONSBuf *>(static_cast<uint8_t *>(image_surface->pixels) + image_surface->pitch * (static_cast<int>(_dst_rect.y) + i)) + static_cast<int>(_dst_rect.x),
		            reinterpret_cast<ONSBuf *>(static_cast<uint8_t *>(surface->pixels) + surface->pitch * (static_cast<int>(_src_rect.y) + i)) + static_cast<int>(_src_rect.x),
		            static_cast<int>(_src_rect.w) * sizeof(ONSBuf));
}

void AnimationInfo::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	if (!image_surface)
		return;

	ONSBuf *dst_buffer = static_cast<ONSBuf *>(image_surface->pixels);
	uint8_t *alphap    = static_cast<uint8_t *>(image_surface->pixels) + 3;
	uint32_t rgb       = (r << R_SHIFT) | (g << G_SHIFT) | (b << B_SHIFT);

	int dst_margin = 0;

	for (int i = image_surface->h; i != 0; i--) {
		for (int j = image_surface->w; j != 0; j--, dst_buffer++) {
			*dst_buffer = rgb;
			*alphap     = a;
			alphap += 4;
		}
		dst_buffer += dst_margin;
	}
}

SDL_Surface *AnimationInfo::setupImageAlpha(SDL_Surface *surface,
                                            SDL_Surface *surface_m,
                                            bool has_alpha) {
	if (!surface)
		return nullptr;

	SDL_PixelFormat *fmt = surface->format;

	int w          = surface->w;
	int h          = surface->h;
	int cell_w     = vertical_cells ? w : w / num_of_cells;
	int cell_num_w = vertical_cells ? 1 : num_of_cells;
	orig_pos.w     = w;
	orig_pos.h     = h;

	uint32_t *buffer = static_cast<uint32_t *>(surface->pixels);
	uint8_t *alphap  = static_cast<uint8_t *>(surface->pixels) + 3;

	uint32_t ref_color = 0;
	if (trans_mode == TRANS_TOPLEFT) {
		ref_color = *buffer;
	} else if (trans_mode == TRANS_TOPRIGHT) {
		ref_color = *(buffer + surface->w - 1);
	} else if (trans_mode == TRANS_DIRECT) {
		ref_color = direct_color.x << fmt->Rshift |
		            direct_color.y << fmt->Gshift |
		            direct_color.z << fmt->Bshift;
	}
	ref_color &= RGBMASK;

	int i, j, c;
	if (trans_mode == TRANS_ALPHA && !has_alpha) {
		const int mask_cell_w = cell_w / 2;
		const int mask_w      = mask_cell_w * cell_num_w;
		orig_pos.w            = mask_w;
		SDL_Surface *surface2 = SDL_CreateRGBSurface(SDL_SWSURFACE, mask_w, h,
		                                             fmt->BitsPerPixel, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
		uint32_t *buffer2     = static_cast<uint32_t *>(surface2->pixels);
		alphap                = static_cast<uint8_t *>(surface2->pixels) + 3;

		for (i = h; i != 0; i--) {
			for (c = cell_num_w; c != 0; c--) {
				for (j = mask_cell_w; j != 0; j--, buffer++, alphap += 4) {
					*buffer2++ = *buffer;
					*alphap    = (*(buffer + mask_cell_w) & 0xff) ^ 0xff;
				}
				buffer += (cell_w - mask_cell_w);
			}
			buffer += surface->w - cell_w * cell_num_w;
			buffer2 += surface2->w - mask_cell_w * cell_num_w;
			alphap += (surface2->w - mask_cell_w * cell_num_w) * 4;
		}

		SDL_FreeSurface(surface);
		surface = surface2;
	} else if (trans_mode == TRANS_MASK || (trans_mode == TRANS_ALPHA && has_alpha)) {
		if (surface_m) {
			//apply mask (replacing existing alpha values, if any)
			const int mask_w  = surface_m->w;
			const int mask_wh = surface_m->w * surface_m->h;
			const int cell_w  = mask_w / cell_num_w;
			int i2            = 0;
			for (i = h; i != 0; i--) {
				uint32_t *buffer_m = static_cast<uint32_t *>(surface_m->pixels) + i2;
				for (c = cell_num_w; c != 0; c--) {
					int j2 = 0;
					for (j = cell_w; j != 0; j--, buffer++, alphap += 4) {
						uint8_t newval = (*(buffer_m + j2) & 0xff) ^ 0xff;
						if (trans_mode == TRANS_ALPHA)
							// used by spriteMaskCommand to apply a cropping mask to an alpha image
							*alphap = newval < *alphap ? newval : *alphap;
						else
							*alphap = newval;
						if (j2 >= mask_w)
							j2 = 0;
						else
							j2++;
					}
				}
				buffer += mask_w - (cell_w * cell_num_w);
				i2 += mask_w;
				if (i2 >= mask_wh)
					i2 = 0;
			}
		}
	} else if (trans_mode == TRANS_TOPLEFT ||
	           trans_mode == TRANS_TOPRIGHT ||
	           trans_mode == TRANS_DIRECT) {
		const int trans_value = RGBMASK & MEDGRAY;
		for (i = h; i != 0; i--) {
			for (j = w; j != 0; j--, buffer++, alphap += 4) {
				if ((*buffer & RGBMASK) == ref_color)
					*buffer = trans_value;
				else
					*alphap = 0xff;
			}
		}
	} else if (trans_mode == TRANS_STRING) {
		for (i = h; i != 0; i--) {
			for (j = w; j != 0; j--, buffer++, alphap += 4)
				*alphap = *buffer >> 24;
		}
	} else if (trans_mode != TRANS_ALPHA) { // TRANS_COPY
		for (i = h; i != 0; i--) {
			for (j = w; j != 0; j--, buffer++, alphap += 4)
				*alphap = 0xff;
		}
	}

	return surface;
}

void AnimationInfo::setImage(GPU_Image *image) {
	if (!image)
		return;

	gpu_image = image;
	calculateImage(image->w, image->h);
}

void AnimationInfo::setSurface(SDL_Surface *surface) {
	if (!surface)
		return;

	image_surface = surface;
	calculateImage(surface->w, surface->h);
}

void AnimationInfo::setBigImage(GPUBigImage *image) {
	if (!image)
		return;

	big_image = std::shared_ptr<GPUBigImage>(image);
	calculateImage(image->w, image->h);
}

void AnimationInfo::backupState() {
	if (old_ai)
		sendToLog(LogLevel::Error, "improper backupState call\n");
	old_ai = new AnimationInfo(*this);
}

void AnimationInfo::commitState() {
	freevar(&old_ai);
	distinguish_from_old_ai = false;
}

AnimationInfo *AnimationInfo::oldNew(int refresh_mode) {
	if (refresh_mode & REFRESH_BEFORESCENE_MODE && old_ai)
		return old_ai;
	return this;
}
