/**
 *  Subtitle.hpp
 *  ONScripter-RU
 *
 *  Subtitle playback layer for any surfaces.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Layers/Layer.hpp"
#include "Engine/Media/SubtitleDriver.hpp"
#include "Engine/Readers/Base.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>

#include <atomic>
#include <deque>
#include <memory>

class SubtitleLayer : public Layer {
	float ratio_x{0.75}, ratio_y{0.75};
	// ns per frame, current decoder position + decode_rate, current display position
	uint64_t decode_rate{0}, decoded_timestamp{0}, display_timestamp{0}; // All values are in ns
	bool startDecoding();
	void endDecoding();
	bool clockProceed();
	void renderImageSet(std::vector<SubtitleImage> &imgs);

	/* glUniformLocation arrays */
	int ntexturesHandle, dstDimsHandle, texHandle;
	int subDimsHandles[SubtitleDriver::NIMGS_MAX];
	int subCoordsHandles[SubtitleDriver::NIMGS_MAX];
	int subColorsHandles[SubtitleDriver::NIMGS_MAX];

	GPU_Image *subImages{nullptr};

	struct SubtitleFrame {
		uint64_t start_timestamp;
		std::vector<SubtitleImage> imgs;
		std::unique_ptr<uint8_t[]> sw_buffer; /* Used for fallback */
	};

	const size_t frameQueueMaxSize{10};
	std::deque<SubtitleFrame> frameQueue; //TODO: replace by a ring buffer
	SDL_mutex *frameQueueMutex{nullptr};

	SDL_semaphore *threadSemaphore{nullptr};
	std::atomic<bool> should_finish{false};
	bool playback{false};

	GPU_Image *current_frame{nullptr};
	int current_frame_format{0};
	uint64_t current_timestamp{0xFFFFFFFFFFFFFFFF};

public:
	SubtitleLayer(unsigned int w, unsigned int h, BaseReader **br, unsigned int scale_x = 100, unsigned int scale_y = 100);
	~SubtitleLayer() override;
	bool loadSubtitles(const char *filename, unsigned int rateMs);
	void stopPlayback();
	void setFont(unsigned int id);

	void doDecoding();
	bool update(bool /*old*/) override;
	void refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool centre_coordinates, int /*rm*/, float /*scalex*/, float /*scaley*/) override;
	BlendModeId blendingMode(int rm) override {
		return blendingModeSupported(rm);
	}

	SubtitleDriver subtitleDriver; //-V730_NOINIT

	Clock mediaClock;
	uint64_t nanosPerFrame{0};
};
