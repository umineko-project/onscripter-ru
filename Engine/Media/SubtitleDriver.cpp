/**
 *  SubtitleDriver.cpp
 *  ONScripter-RU
 *
 *  Contains subtitle common rendering backend.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Media/SubtitleDriver.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Support/FileIO.hpp"

#include <SDL2/SDL.h>

/* libass debug log function */
static void ass_msg_callback(int level, const char *fmt, va_list va, void * /*data*/) {
#ifdef PUBLIC_RELEASE
	if (level > 5)
		return;
#endif
	char message[2048];
	std::vsnprintf(message, sizeof(message), fmt, va);

	if (level <= 2)
		sendToLog(LogLevel::Error, "libass [%d]: %s\n", level, message);
	else if (level <= 4)
		sendToLog(LogLevel::Warn, "libass [%d]: %s\n", level, message);
	else
		sendToLog(LogLevel::Info, "libass [%d]: %s\n", level, message);
}

/* A basic software ASS_Image blitter */
static void ass_sw_blend(SDL_Surface *frame, ASS_Image *img) {
	//int cnt = 0;
	while (img) {
		uint8_t opacity = 255 - (img->color & 0xFF);
		uint8_t r       = (img->color >> 24);
		uint8_t g       = (img->color >> 16) & 0xFF;
		uint8_t b       = (img->color >> 8) & 0xFF;

		uint8_t *src;
		uint8_t *dst;

		int32_t Bpp = frame->format->BytesPerPixel;

		src = img->bitmap;
		dst = static_cast<uint8_t *>(frame->pixels) + img->dst_y * frame->pitch + img->dst_x * Bpp;
		for (int32_t y = 0; y < img->h; ++y) {
			for (int32_t x = 0; x < img->w; ++x) {
				uint32_t k = (static_cast<uint32_t>(src[x])) * opacity / 255;
				// possible endianness problems
				if (Bpp == 3) {
					dst[x * Bpp]     = (k * b + (255 - k) * dst[x * Bpp]) / 255;
					dst[x * Bpp + 1] = (k * g + (255 - k) * dst[x * Bpp + 1]) / 255;
					dst[x * Bpp + 2] = (k * r + (255 - k) * dst[x * Bpp + 2]) / 255;
				} else if (Bpp == 4) {
					if (k != 0) {
						dst[x * Bpp]     = b;
						dst[x * Bpp + 1] = g;
						dst[x * Bpp + 2] = r;
						dst[x * Bpp + 3] = k;
					}
				}
			}
			src += img->stride;
			dst += frame->pitch;
		}
		//++cnt;
		img = img->next;
	}
	//sendToLog(LogLevel::Info, "%d images blended\n", cnt);
}

static inline void rgbo2yuva(uint32_t rgbo, uint8_t &y, uint8_t &u, uint8_t &v, uint8_t &a) {
	uint8_t r = (rgbo >> 24);
	uint8_t g = (rgbo >> 16) & 0xFF;
	uint8_t b = (rgbo >> 8) & 0xFF;

	y = 0.182586 * r + 0.614230 * g + 0.062008 * b + 16;
	u = -0.100645 * r - 0.338570 * g + 0.439215 * b + 128;
	v = 0.439215 * r - 0.398941 * g - 0.040273 * b + 128;
	a = 255 - (rgbo & 0xFF);
}

static void yblend(uint8_t *src, uint8_t srcY, uint8_t alpha, ASS_Image *img, uint8_t *dstY_p, int stride) {
	uint8_t *dstY = dstY_p + img->dst_y * stride + img->dst_x;

	for (int32_t y = 0; y < img->h; y++) {
		for (int32_t x = 0; x < img->w; x++) {
			uint32_t srcA = src[x] * alpha * 129;
			dstY[x]       = (srcA * srcY + (255 * 255 * 129 - srcA) * dstY[x]) >> 23;
		}
		dstY += stride;
		src += img->stride;
	}
}

static void uvblend(uint8_t *src, uint8_t srcU, uint8_t srcV, uint8_t alpha, ASS_Image *img, uint8_t *dstUV_p, int stride) {
	constexpr uintptr_t even_mask = UINTPTR_MAX - 1;

	uint8_t *dstUV = dstUV_p + (((img->dst_y + (img->dst_y & 1)) * stride) >> 1) + img->dst_x;
	uint8_t *dstU  = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(dstUV) & even_mask);
	uint8_t *dstV  = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(dstUV) | 1);

	for (int32_t y = 0; y < img->h; y += 2) {
		for (int32_t x = 0; x < img->w; x += 2) {
			uint32_t srcA = src[x] * alpha * 129;
			dstU[x]       = (srcA * srcU + (255 * 255 * 129 - srcA) * dstU[x]) >> 23;
			dstV[x]       = (srcA * srcV + (255 * 255 * 129 - srcA) * dstV[x]) >> 23;
		}
		dstU += stride;
		dstV += stride;
		src += img->stride * 2;
	}
}

static void uvblend(uint8_t *src, uint8_t srcU, uint8_t srcV, uint8_t alpha, ASS_Image *img, uint8_t *dstU_p, uint8_t *dstV_p, int stride) {
	uint8_t *dstU = dstU_p + (((img->dst_y + (img->dst_y & 1)) * stride + img->dst_x) >> 1);
	uint8_t *dstV = dstV_p + (((img->dst_y + (img->dst_y & 1)) * stride + img->dst_x) >> 1);

	for (int32_t y = 0; y < img->h; y += 2) {
		for (int32_t x = 0; x < img->w; x += 2) {
			uint32_t srcA = src[x] * alpha * 129;
			dstU[x >> 1]  = (srcA * srcU + (255 * 255 * 129 - srcA) * dstU[x >> 1]) >> 23;
			dstV[x >> 1]  = (srcA * srcV + (255 * 255 * 129 - srcA) * dstV[x >> 1]) >> 23;
		}
		dstU += stride;
		dstV += stride;
		src += img->stride * 2;
	}
}

static void ass_yuv_blend(uint8_t *planes[4], size_t planesCnt, AVPixelFormat format, int linesize[AV_NUM_DATA_POINTERS], ASS_Image *img) {
	assert(format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_NV12);
	if (format == AV_PIX_FMT_YUV420P) {
		assert(planesCnt == 3);
	} else if (format == AV_PIX_FMT_NV12) {
		assert(planesCnt == 2);
	}

	while (img) {
		uint8_t srcY, srcU, srcV, alpha;
		rgbo2yuva(img->color, srcY, srcU, srcV, alpha);

		yblend(img->bitmap, srcY, alpha, img, planes[0], linesize[0]);
		if (format == AV_PIX_FMT_YUV420P || planesCnt == 3)
			uvblend(img->bitmap, srcU, srcV, alpha, img, planes[1], planes[2], linesize[1]);
		else
			uvblend(img->bitmap, srcU, srcV, alpha, img, planes[1], linesize[1]);

		img = img->next;
	}
}

static void ass_pregpu_blend(float *frame, size_t linesize, int format, ASS_Image *img) {
	//int cnt = 0;

	off_t r_off = 2;
	off_t g_off = 1;
	off_t b_off = 0;
	off_t a_off = 3;

	if (format == gpu.current_renderer->formatRGBA)
		std::swap(r_off, b_off);
	else if (format != gpu.current_renderer->formatBGRA)
		ons.errorAndExit("Unsupported texture foramt");

	while (img) {
		float opacity     = (255 - ((img->color) & 0xFF)) / 65025.0f;
		float r           = (img->color >> 24) / 255.0f;
		float g           = ((img->color >> 16) & 0xFF) / 255.0f;
		float b           = ((img->color >> 8) & 0xFF) / 255.0f;
		const int32_t Bpp = 4;

		uint8_t *src;
		float *dst;

		src = img->bitmap;
		dst = frame + img->dst_y * linesize * Bpp + img->dst_x * Bpp;
		for (int32_t y = 0; y < img->h; ++y) {
			for (int32_t x = 0; x < img->w; ++x) {
				if (!src[x])
					continue;

				// GPU_FUNC_ONE, GPU_FUNC_ONE_MINUS_SRC_ALPHA

				float a_ = src[x] * opacity;
				float r_ = r * a_;
				float g_ = g * a_;
				float b_ = b * a_;

				dst[x * Bpp + r_off] *= 1 - a_;
				dst[x * Bpp + b_off] *= 1 - a_;
				dst[x * Bpp + g_off] *= 1 - a_;
				dst[x * Bpp + a_off] *= 1 - a_;

				dst[x * Bpp + r_off] += r_;
				dst[x * Bpp + b_off] += b_;
				dst[x * Bpp + g_off] += g_;
				dst[x * Bpp + a_off] += a_;
			}
			src += img->stride;
			dst += linesize * Bpp;
		}
		//++cnt;
		img = img->next;
	}
	//sendToLog(LogLevel::Info, "%d images blended\n", cnt);
}

/* libass interface */

bool SubtitleDriver::init(int width, int height, const char *ass_sub_file, BaseReader *reader, AVCodecContext *sub_codec_ctx) {
	ass_library = ass_library_init();
	if (!ass_library) {
		sendToLog(LogLevel::Error, "ass_library_init failed!\n");
	} else {
		// Uncomment and implement in case of issues
		ass_set_message_cb(ass_library, ass_msg_callback, nullptr);
		ass_renderer = ass_renderer_init(ass_library);
		if (!ass_renderer) {
			ass_library_done(ass_library);
			ass_library = nullptr;
			sendToLog(LogLevel::Error, "ass_renderer_init failed!\n");
		} else {
            ass_set_fonts_dir(ass_library, ons.getFontDir());
			ass_set_frame_size(ass_renderer, width, height);
			ass_set_fonts(ass_renderer, ons.getFontPath(currentFontID, true), "Sans", ASS_FONTPROVIDER_NONE, nullptr, 0);
			bool subs_ok{false};
			if (ass_sub_file && ass_sub_file[0] != '\0') {
				char *ass_file = reader ? reader->completePath(ass_sub_file, FileType::File) : nullptr;
				if (FileIO::readFile(ass_file ? ass_file : ass_sub_file, subtitle_size, subtitle_buffer)) {
					ass_track = ass_read_memory(ass_library, reinterpret_cast<char *>(subtitle_buffer.data()),
					                            subtitle_size, nullptr);
					subs_ok   = true;
				}

				freearr(&ass_file);
			} else if (subs_ok || sub_codec_ctx) { // Use internal subtitles
				ass_track = ass_read_memory(ass_library, reinterpret_cast<char *>(sub_codec_ctx->extradata),
				                            sub_codec_ctx->extradata_size, nullptr);
				subs_ok   = true;
			}

			if (!ass_track) {
				ass_library_done(ass_library);
				ass_library = nullptr;
				ass_renderer_done(ass_renderer);
				ass_renderer = nullptr;
				sendToLog(LogLevel::Error, "ass_read_memory failed!\n");
				return false;
			}

			return subs_ok;
		}
	}

	return false;
}

void SubtitleDriver::process(char *data, size_t length) {
	Lock lock(ass_track);
	ass_process_data(ass_track, data, static_cast<int>(length));
}

bool SubtitleDriver::blendOn(SDL_Surface *surface, uint64_t timestamp) {
	ASS_Image *img{nullptr};

	{
		Lock lock(ass_track);
		img = ass_render_frame(ass_renderer, ass_track, timestamp, nullptr);
	}

	if (img && img->w > 0 && img->h > 0) {
		ass_sw_blend(surface, img);
		return true;
	}

	return false;
}

bool SubtitleDriver::blendOn(uint8_t *planes[4], size_t planesCnt, AVPixelFormat format, int linesize[AV_NUM_DATA_POINTERS], int /*height*/, uint64_t timestamp) {
	ASS_Image *img{nullptr};

	{
		Lock lock(ass_track);
		img = ass_render_frame(ass_renderer, ass_track, timestamp, nullptr);
	}

	if (img && img->w > 0 && img->h > 0) {
		ass_yuv_blend(planes, planesCnt, format, linesize, img);
		return true;
	}

	return false;
}

bool SubtitleDriver::blendInNeed(SDL_Surface *surface, uint64_t timestamp) {
	ASS_Image *img{nullptr};
	int changed{0};

	{
		Lock lock(ass_track);
		img = ass_render_frame(ass_renderer, ass_track, timestamp, &changed);
	}

	if (changed && img && img->w > 0 && img->h > 0) {
		ass_sw_blend(surface, img);
		return true;
	}

	return changed;
}

bool SubtitleDriver::blendBufInNeed(float *buffer, size_t width, size_t /*height*/, int format, uint64_t timestamp, ASS_Image *img) {
	int changed{0};

	if (!img) {
		Lock lock(ass_track);
		img = ass_render_frame(ass_renderer, ass_track, timestamp, &changed);
	} else {
		changed = 1;
	}

	if (changed && img && img->w > 0 && img->h > 0) {
		ass_pregpu_blend(buffer, width, format, img);
		return true;
	}

	return changed >= 1;
}

bool SubtitleDriver::extractFrame(std::vector<SubtitleImage> &images, uint64_t timestamp) {
	ASS_Image *img{nullptr};
	int changed{0};

	{
		Lock lock(ass_track);
		img = ass_render_frame(ass_renderer, ass_track, timestamp, &changed);
	}

	bool fits;
	size_t num = countImages(img, fits);

	if (!fits || num > NIMGS_MAX)
		throw img;

	images.reserve(num);

	while (img) {
		SubtitleImage subImg;

		subImg.w        = img->w;
		subImg.h        = img->h;
		subImg.linesize = img->stride;
		subImg.x        = img->dst_x;
		subImg.y        = img->dst_y;
		subImg.color    = img->color;

		subImg.buffer = std::make_unique<uint8_t[]>(img->h * img->stride);
		std::memcpy(subImg.buffer.get(), img->bitmap, img->h * img->stride);

		images.emplace_back(std::move(subImg));

		img = img->next;
	}

	return changed;
}

void SubtitleDriver::deinit() {
	if (ass_track)
		ass_free_track(ass_track);
	ass_track = nullptr;

	if (ass_renderer)
		ass_renderer_done(ass_renderer);
	ass_renderer = nullptr;

	if (ass_library)
		ass_library_done(ass_library);
	ass_library = nullptr;

	subtitle_buffer.clear();
	subtitle_size = 0;
}

void SubtitleDriver::setFont(unsigned int id) {
	currentFontID = id;
}
