/**
 *  Subtitle.cpp
 *  ONScripter-RU
 *
 *  Subtitle playback layer for any surfaces.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Layers/Subtitle.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"

/* Subtitle wrapper */

SubtitleLayer::SubtitleLayer(unsigned int w, unsigned int h, BaseReader **br, unsigned int scale_x, unsigned int scale_y) {
	if (scale_x <= 100)
		ratio_x = scale_x / 100.0f;
	if (scale_y <= 100)
		ratio_y = scale_y / 100.0f;

	width  = w * ratio_x;
	height = h * ratio_y;

	current_frame = gpu.createImage(w, h, 4);
	GPU_GetTarget(current_frame);
	current_frame_format = (gpu.*gpu.current_renderer->getImageFormat)(current_frame);
	reader               = br;

	/* Get shader uniform loc */
	gpu.setShaderProgram("renderSubtitles.frag");
	ntexturesHandle = gpu.getUniformLoc("ntextures");
	dstDimsHandle   = gpu.getUniformLoc("dstDims");
	texHandle       = gpu.getUniformLoc("subTex");

	for (size_t i = 0; i < SubtitleDriver::NIMGS_MAX; i++) {
		char nameBuf[32];

		std::snprintf(nameBuf, sizeof(nameBuf), "subDims[%zu]", i);
		subDimsHandles[i] = gpu.getUniformLoc(nameBuf);

		std::snprintf(nameBuf, sizeof(nameBuf), "subCoords[%zu]", i);
		subCoordsHandles[i] = gpu.getUniformLoc(nameBuf);

		std::snprintf(nameBuf, sizeof(nameBuf), "subColors[%zu]", i);
		subColorsHandles[i] = gpu.getUniformLoc(nameBuf);

		std::snprintf(nameBuf, sizeof(nameBuf), "subTexDims[%zu]", i);
		subTexDimsHandles[i] = gpu.getUniformLoc(nameBuf);
	}
	gpu.unsetShaderProgram();

	/* Prepare images */
	subImages = gpu.createImage(SubtitleDriver::IMG_W, SubtitleDriver::IMG_H, 1);
	GPU_SetBlending(subImages, false);
}

SubtitleLayer::~SubtitleLayer() {
	// Required for e.g. definereset.
	stopPlayback();

	gpu.freeImage(subImages);
}

bool SubtitleLayer::loadSubtitles(const char *filename, unsigned int rateMs) {
	stopPlayback();

	if (!subtitleDriver.init(width, height, filename, *reader, nullptr)) {
		sendToLog(LogLevel::Error, "ass.dll: Failed to load %s\n", filename);
		return false;
	}

	if (!current_frame) {
		current_frame = gpu.createImage(width, height, 4);
		GPU_GetTarget(current_frame);
	}

	decode_rate = static_cast<uint64_t>(rateMs) * 1000000;

	return startDecoding();
}

void SubtitleLayer::stopPlayback() {
	endDecoding();
	frameQueue.clear();
	subtitleDriver.deinit();
	if (current_frame)
		gpu.freeImage(current_frame);
	current_frame = nullptr;
}

void SubtitleLayer::setFont(unsigned int id) {
	subtitleDriver.setFont(id);
}

void SubtitleLayer::refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool /*centre_coordinates*/, int /*rm*/, float /*scalex*/, float /*scaley*/) {
	if (current_frame)
		gpu.copyGPUImage(current_frame, nullptr, &clip, target,
		                 width / ratio_x / 2.0 + x, height / ratio_y / 2.0 + y,
		                 1 / ratio_x, 1 / ratio_y, 0, true);
}

bool SubtitleLayer::update(bool /*old*/) {
	if (!clockProceed())
		return true;

	SDL_LockMutex(frameQueueMutex);

	//CHECKME: may it deadlock here on quit?
	while (decoded_timestamp < decode_rate || decoded_timestamp - decode_rate < display_timestamp) {
		if (frameQueue.size() == frameQueueMaxSize)
			break;
		SDL_UnlockMutex(frameQueueMutex);
		SDL_Delay(1); //TODO: replace by semaphores
		SDL_LockMutex(frameQueueMutex);
	}

	// This is possible, when there are no frames in the beginning
	if (frameQueue.empty()) {
		SDL_UnlockMutex(frameQueueMutex);
		return true;
	}

	auto it = frameQueue.begin();

	while (it != frameQueue.end()) {
		if (it->start_timestamp <= display_timestamp) {
			if (it + 1 != frameQueue.end()) {
				++it;
				continue;
			}
		} else {
			--it;
		}
		break;
	}

	assert(it != frameQueue.end());

	std::unique_ptr<uint8_t[]> needed_buffer;
	std::vector<SubtitleImage> imgs;
	bool gotFrame{false};
	if (it->start_timestamp != current_timestamp) {
		if (!it->imgs.empty()) {
			imgs = std::move(it->imgs);
		} else {
			needed_buffer = std::move(it->sw_buffer);
		}
		gotFrame          = true;
		current_timestamp = it->start_timestamp;
	}

	if (it - frameQueue.begin() > 1)
		frameQueue.erase(frameQueue.begin(), it - 1);

	SDL_UnlockMutex(frameQueueMutex);

	if (current_frame) {
		if (!imgs.empty()) {
			renderImageSet(imgs);
		} else if (needed_buffer) {
			GPU_UpdateImageBytes(current_frame, nullptr, needed_buffer.get(), width * 4);
		} else if (gotFrame) {
			gpu.clearWholeTarget(current_frame->target);
		}
	}
	return true;
}

/* Private Subtitle wrapper */

void SubtitleLayer::renderImageSet(std::vector<SubtitleImage> &imgs) {

	float x      = 0;
	float y      = 0;
	float next_y = 0;
	float subTexPos[imgs.size()][2];
	for (size_t i = 0; i < imgs.size(); i++) {
		GPU_Rect rect;
		if (x + imgs[i].w <= SubtitleDriver::IMG_W) {
			subTexPos[i][0] = static_cast<float>(x);
			subTexPos[i][1] = static_cast<float>(y);
			rect.x          = x;
			rect.y          = y;
			x += imgs[i].w;
			next_y = std::max(y + imgs[i].h, next_y);
		} else {
			subTexPos[i][0] = static_cast<float>(0);
			subTexPos[i][1] = static_cast<float>(next_y);
			rect.x          = 0;
			rect.y          = next_y;
			x               = imgs[i].w;
			y               = next_y;
			next_y += y + imgs[i].h;
		}

		rect.h = imgs[i].h;
		rect.w = imgs[i].w;

		GPU_UpdateImageBytes(subImages, &rect, imgs[i].buffer.get(), imgs[i].linesize);
	}

	gpu.setShaderProgram("renderSubtitles.frag");

	for (size_t i = 0; i < imgs.size(); i++) {
		GPU_SetUniformfv(subTexDimsHandles[i], 2, 1, subTexPos[i]);

		float subDims[]{static_cast<float>(imgs[i].w), static_cast<float>(imgs[i].h)};
		GPU_SetUniformfv(subDimsHandles[i], 2, 1, subDims);

		float subCoords[]{static_cast<float>(imgs[i].x), static_cast<float>(imgs[i].y)};
		GPU_SetUniformfv(subCoordsHandles[i], 2, 1, subCoords);

		float subColors[]{(imgs[i].color >> 24) / 255.0f,
		                  ((imgs[i].color >> 16) & 0xff) / 255.0f,
		                  ((imgs[i].color >> 8) & 0xff) / 255.0f,
		                  (255 - (imgs[i].color & 0xFF)) / 255.0f};
		GPU_SetUniformfv(subColorsHandles[i], 4, 1, subColors);
	}

	GPU_SetUniformi(ntexturesHandle, static_cast<int>(imgs.size()));
	float dstDims[]{static_cast<float>(current_frame->w), static_cast<float>(current_frame->h)};
	GPU_SetUniformfv(dstDimsHandle, 2, 1, dstDims);

	GPU_SetShaderImage(subImages, texHandle, 1);
	// This is fine and does not need gpu.render_to_self guard because we do not read/write
	// current_frame pixels simultaneously.
	GPU_SetBlending(current_frame, false);
	gpu.copyGPUImage(current_frame, nullptr, nullptr, current_frame->target);
	GPU_SetBlending(current_frame, true);

	gpu.unsetShaderProgram();
}

void SubtitleLayer::doDecoding() {
	while (true) {
		while (!should_finish.load(std::memory_order_acquire)) {
			if (async.threadShutdownRequested) {
				should_finish.store(true, std::memory_order_release);
				break;
			}

			SDL_mutexP(frameQueueMutex);
			if (frameQueue.size() >= frameQueueMaxSize) {
				SDL_mutexV(frameQueueMutex);
				SDL_Delay(1); //TODO: replace by semaphores
			} else {
				SDL_mutexV(frameQueueMutex);
				break;
			}
		}

		// Finish if requested
		if (should_finish.load(std::memory_order_acquire))
			break;

		bool frameReady{false};
		SubtitleFrame frame;
		frame.start_timestamp = decoded_timestamp;

		try {
			if (subtitleDriver.extractFrame(frame.imgs, decoded_timestamp / 1000000)) {
				frameReady = true;
			}
		} catch (ASS_Image *img) {
			sendToLog(LogLevel::Warn, "Falling back to software renderer, this will be slow\n");

			const size_t frame_size  = width * height * 4;
			auto premultiplied_frame = std::make_unique<float[]>(frame_size);

			// Use surface if needed
			if (subtitleDriver.blendBufInNeed(premultiplied_frame.get(), width, height, current_frame_format, decoded_timestamp / 1000000, img)) {
				auto byte_frame   = std::make_unique<uint8_t[]>(frame_size);
				float *float_ptr  = premultiplied_frame.get();
				uint8_t *byte_ptr = byte_frame.get();

				for (unsigned int y = 0; y < height; y++) {
					ptrdiff_t line = 4 * y * width;
					for (unsigned int x = 0; x < width; x++) {
						ptrdiff_t pos     = line + 4 * x;
						byte_ptr[pos + 0] = float_ptr[pos + 0] * 255;
						byte_ptr[pos + 1] = float_ptr[pos + 1] * 255;
						byte_ptr[pos + 2] = float_ptr[pos + 2] * 255;
						byte_ptr[pos + 3] = float_ptr[pos + 3] * 255;
					}
				}

				frame.sw_buffer = std::move(byte_frame);
				frameReady      = true;
			}
		}

		if (frameReady) {
			SDL_mutexP(frameQueueMutex);
			frameQueue.emplace_back(std::move(frame));
			SDL_mutexV(frameQueueMutex);
		}

		decoded_timestamp += decode_rate;
	}

	SDL_SemPost(threadSemaphore);
}

bool SubtitleLayer::startDecoding() {
	if (!frameQueueMutex)
		frameQueueMutex = SDL_CreateMutex();
	if (!threadSemaphore)
		threadSemaphore = SDL_CreateSemaphore(0);

	decoded_timestamp = 0;
	display_timestamp = 0;
	mediaClock.reset();
	nanosPerFrame = 1000000000 / (ons.game_fps ? ons.game_fps : DEFAULT_FPS);
	frameQueue.emplace_back(); // zero frame

	async.loadSubtitleFrames(this);

	playback = true;

	return false;
}

void SubtitleLayer::endDecoding() {
	if (threadSemaphore) {
		should_finish.store(true, std::memory_order_release);
		SDL_SemWait(threadSemaphore);
		SDL_DestroySemaphore(threadSemaphore);
		threadSemaphore = nullptr;
		should_finish.store(false, std::memory_order_seq_cst);
	}

	playback = false;

	if (frameQueueMutex)
		SDL_DestroyMutex(frameQueueMutex);
	frameQueueMutex = nullptr;
}

bool SubtitleLayer::clockProceed() {
	if (!playback || !sprite)
		return false;

	int framesToAdvance{0};

	mediaClock.tickNanos(sprite->clock.lapNanos());

	if (!mediaClock.hasCountdown()) {
		mediaClock.addCountdownNanos(nanosPerFrame);
		mediaClock.tickNanos(nanosPerFrame); // make the first frame immediately ready (off-by-1 fix)
	}
	while (mediaClock.expired()) {
		mediaClock.addCountdownNanos(nanosPerFrame);
		framesToAdvance++;
	}

	if (framesToAdvance == 0)
		return false;

	display_timestamp += nanosPerFrame * framesToAdvance;

	return true;
}
