/**
 *  SubtitleDriver.hpp
 *  ONScripter-RU
 *
 *  Contains subtitle common rendering backend.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Readers/Base.hpp"

extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <ass/ass.h>
}

#include <SDL2/SDL.h>

#include <vector>
#include <memory>

struct SubtitleImage {
	std::unique_ptr<uint8_t[]> buffer;
	int w, h;
	int linesize;
	int x, y;
	uint32_t color;
};

class SubtitleDriver {
public:
	static constexpr size_t NIMGS_MAX = 8; /* NTEXTURES in renderSubtitles.frag */
	static constexpr int IMG_W = 2048, IMG_H = 128;

	bool init(int width, int height, const char *ass_sub_file, BaseReader *reader, AVCodecContext *sub_codec_ctx);
	void process(char *data, size_t length);
	void deinit();
	void setFont(unsigned int id);
	size_t countImages(ASS_Image *img, bool &fits, size_t n = 0) {
		if (n == 0)
			fits = true;
		if (img && (img->w > IMG_W || img->h > IMG_H))
			fits = false;
		return !img ? n : countImages(img->next, fits, n + 1);
	}
	bool blendOn(SDL_Surface *surface, uint64_t timestamp);
	bool blendOn(uint8_t *planes[4], size_t planesCnt, AVPixelFormat format, int linesize[AV_NUM_DATA_POINTERS], int height, uint64_t timestamp);
	bool blendInNeed(SDL_Surface *surface, uint64_t timestamp);
	bool blendBufInNeed(float *buffer, size_t width, size_t height, int format, uint64_t timestamp, ASS_Image *img = nullptr);
	bool extractFrame(std::vector<SubtitleImage> &images, uint64_t timestamp); /* Extracts all Ass_Image`s for this timestamp */
private:
	ASS_Library *ass_library{nullptr};   // libass library (handle)
	ASS_Renderer *ass_renderer{nullptr}; // libass renderer
	ASS_Track *ass_track{nullptr};       // libass ass subtitle track (pass events here)
	unsigned int currentFontID{1};       // font id used for displaying subtitles
	std::vector<uint8_t> subtitle_buffer;
	size_t subtitle_size{0};
};
