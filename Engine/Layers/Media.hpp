/**
 *  Media.hpp
 *  ONScripter-RU
 *
 *  Video playback layer based on ffmpeg and MediaEngine.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Layers/Layer.hpp"
#include "Engine/Media/Controller.hpp"
#include "Engine/Graphics/GPU.hpp"
#include "Support/AudioBridge.hpp"
#include "Support/Clock.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>

#include <array>
#include <algorithm>
#include <string>
#include <atomic>
#include <memory>
#include <cstdint>
#include <cassert>

class MediaLayer : public Layer {
public:
	MediaLayer(int w, int h, BaseReader **br);
	~MediaLayer() override;
	bool loadVideo(std::string &filename, unsigned audioStream = 1, unsigned subtitleStream = 0);
	bool loadPresentation(bool alphaMasked, bool loop, std::string &sub_file);
	void startProcessing();
	bool update(bool /*old*/) override;
	void refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool centre_coordinates, int rm, float scalex = 1.0, float scaley = 1.0) override;
	BlendModeId blendingMode(int rm) override {
		return blendingModeSupported(rm);
	}
	void commit() override;
	bool isPlaying(bool checkStatic);

	enum class FinishMode {
		Normal,       /* Kill all frames */
		LeaveCurrent, /* Leave current frame */
		LeaveLast     /* Leave last frame */
	};

	bool stopPlayback(FinishMode mode = FinishMode::Normal);

private:
	// Frame representation
	float wFactor{1}, hFactor{1};   // required multipliers to match the scaled area
	SDL_Rect scaleRect{0, 0, 0, 0}; // scaled video frame size
	GPU_Rect videoRect{0, 0, 0, 0}; // calculated source video frame size
	uint64_t nanosPerFrame{0};

	std::unique_ptr<AudioBridge> audioBridge;
	// Video variables
	static constexpr size_t DefFrame{0};
	static constexpr size_t NewFrame{1};
	GPU_Image *frame_gpu[2]{nullptr, nullptr}; // main frame
	GPU_Image *mask_gpu{nullptr};              // separated mask (temporary)
	GPU_Image *planes_gpu[4]{nullptr};         // storage for planar color buffers

	enum VideoState {
		VS_OFFLINE       = 0,
		VS_AWAITS_COMMIT = 1, // prevents update(), we set to true after loading before committing
		VS_END_OF_FILE   = 2, // prevents update(), we set to true in update() after using a frame with isLastFrame==true
		VS_PLAYING       = 4  // set to true in loadPresentation and to false when stopPlayback succeeds
	};

	bool ensurePlanesImgs(AVPixelFormat f, size_t n, float w, float h); // ensure that first n planes from planes_gpu are
	                                                                    // available with dimensions of rect

	int videoState{VS_OFFLINE};

	int framesToAdvance{0};
	Clock mediaClock;
};
