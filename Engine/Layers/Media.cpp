/**
 *  Media.cpp
 *  ONScripter-RU
 *
 *  Video playback layer based on ffmpeg and MediaEngine.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Layers/Media.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Graphics/GPU.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Support/FileDefs.hpp"

MediaLayer::MediaLayer(int w, int h, BaseReader **br) {
	reader = br;
	width  = w;
	height = h;
}

MediaLayer::~MediaLayer() {
	while (!stopPlayback()) {
		// This should not happen in a proper script
		sendToLog(LogLevel::Error, "You forgot to stop video playback before exiting\n");
	}
}

bool MediaLayer::loadVideo(std::string &filename, unsigned audioStream, unsigned subtitleStream) {

	// If we arrived here we are guaranteed to be not playing anything
	// However, we may still display some frame of the previous video
	//videoState &= ~VS_END_OF_FILE;
	videoState = VS_OFFLINE;

	//Secondly, try to open the video
	std::unique_ptr<char[]> video_file((*reader)->completePath(filename.c_str(), FileType::File));
	return media.loadVideo(video_file.get(), audioStream, subtitleStream);
}

bool MediaLayer::stopPlayback(FinishMode mode) {
	// Are we done?
	if (!(videoState & VS_PLAYING) && !frame_gpu[DefFrame] && !frame_gpu[NewFrame])
		return true;

	audioBridge.reset();

	if (media.finish(true)) {

		// Depending on the flags reset frame_gpu/mask_gpu
		// return false is the last frame still needs to be grabbed

		media.resetState();
		videoState &= ~VS_PLAYING;

		if (mode == FinishMode::Normal) {
			for (auto &img : {frame_gpu[DefFrame], frame_gpu[NewFrame], mask_gpu})
				if (img)
					gpu.freeImage(img);
			frame_gpu[DefFrame] = frame_gpu[NewFrame] = mask_gpu = nullptr;
		}

		return true;
	}

	return false;
}

bool MediaLayer::loadPresentation(bool alphaMasked, bool loop, std::string &sub_file) {
	/* Determine video dimensions */

	scaleRect = {0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height)};

	int frameWidth, frameHeight, channels = alphaMasked ? 4 : 3;
	media.frameSize(scaleRect, frameWidth, wFactor, frameHeight, hFactor, alphaMasked);

	videoRect.w = frameWidth;
	videoRect.h = frameHeight;

	/* Prepare GPU_Images */

	if (frame_gpu[NewFrame]) {
		sendToLog(LogLevel::Error, "Discovered uncommitted video frame, this is not allowed, attempting to recover\n");
		gpu.freeImage(frame_gpu[NewFrame]);
		frame_gpu[NewFrame] = nullptr;
	}

	if (frame_gpu[DefFrame] && (frame_gpu[DefFrame]->w != frameWidth ||
	                            frame_gpu[DefFrame]->h != frameHeight ||
	                            frame_gpu[DefFrame]->bytes_per_pixel !=
	                                static_cast<int>(channels * sizeof(uint8_t)))) {
		sendToLog(LogLevel::Error, "Transitioning from a different video type is not allowed\n");
		gpu.freeImage(frame_gpu[DefFrame]);
		frame_gpu[DefFrame] = nullptr;
	}

	// If there is an existing DefFrame, load to NewFrame
	// Otherwise load to DefFrame
	auto &frame = frame_gpu[DefFrame] ? frame_gpu[NewFrame] : frame_gpu[DefFrame];
	frame       = gpu.createImage(frameWidth, frameHeight, alphaMasked ? 4 : 3);
	GPU_GetTarget(frame);
	gpu.clearWholeTarget(frame->target);

	videoState |= VS_AWAITS_COMMIT;

	if (mask_gpu)
		gpu.freeImage(mask_gpu);
	if (alphaMasked) {
		mask_gpu = gpu.createImage(frameWidth, frameHeight, 3);
		GPU_GetTarget(mask_gpu);
		gpu.clearWholeTarget(mask_gpu->target);
	} else {
		mask_gpu = nullptr;
	}

	/* Signal readiness */
	videoState |= VS_PLAYING;

	/* Prepare media presentation */

	bool ret = media.loadPresentation(videoRect, loop);
	// We should start from displaying the first frame
	// 0 here would make it need a 1/fps delay before displaying anything
	framesToAdvance = 1;
	nanosPerFrame   = media.getNanosPerFrame();

	if (ret) {
		/* Load subtitles */
		std::unique_ptr<char[]> subtitles(sub_file != "" ? (*reader)->completePath(sub_file.c_str(), FileType::File) : nullptr);
		media.addSubtitles(subtitles.get(), frameWidth, frameHeight);

		/* Load audio */
		if (media.hasStream(MediaProcController::AudioEntry)) {
			audioBridge = std::make_unique<AudioBridge>(MIX_VIDEO_CHANNEL,
			                                            !ons.volume_on_flag ? 0 : ons.video_volume * MIX_MAX_VOLUME / 100, [](size_t &sz) {
				                                            return media.advanceAudioChunks(sz);
			                                            });
			ret         = audioBridge->prepare();
		}
	}

	return ret;
}

void MediaLayer::startProcessing() {
	media.startProcessing();
}

bool MediaLayer::ensurePlanesImgs(AVPixelFormat f, size_t n, float w, float h) {
	constexpr size_t num = sizeof(planes_gpu) / sizeof(*planes_gpu);
	if (n > num)
		return false;

	float widths[num]{w, w, w, w};
	float heights[num]{h, h, h, h};
	float formats[num]{1, 1, 1, 1};

	if (f == AV_PIX_FMT_NV12) {
		widths[1] /= 2;
		heights[1] /= 2;
		formats[1] = 2;
		assert(n == 2);
	} else if (f == AV_PIX_FMT_YUV420P) {
		widths[1] = widths[2] = widths[0] / 2;
		heights[1] = heights[2] = heights[0] / 2;
		assert(n == 3);
	}

	for (size_t i = 0; i < n; i++) {
		if (planes_gpu[i] == nullptr ||
		    planes_gpu[i]->w != widths[i] ||
		    planes_gpu[i]->h != heights[i] ||
		    planes_gpu[i]->bytes_per_pixel != formats[i]) {
			if (planes_gpu[i])
				gpu.freeImage(planes_gpu[i]);
			planes_gpu[i] = gpu.createImage(widths[i], heights[i], formats[i]);
		}
	}
	return true;
}

bool MediaLayer::update(bool old) {
	// Not much to do here, although I doubt this can happen
	if (!sprite)
		return true;
	auto sp = old ? sprite->oldNew(REFRESH_BEFORESCENE_MODE) : sprite;

	// Reset clock if:
	// - EOF reached;
	// - not playing;
	// - we are uncommitted
	// Also reset the clock to avoid going forward before staring playback
	if ((videoState & (VS_END_OF_FILE | VS_AWAITS_COMMIT)) || !(videoState & VS_PLAYING)) {
		sp->clock.reset();
		// Do not update unless we have not
		if (!(videoState & VS_AWAITS_COMMIT))
			return true;
	}

	//sendToLog(LogLevel::Info, "MediaLayer::update. Total: %i, Lap: %i, ", mediaClock.time(), mediaClock.lap());
	uint32_t toAdd{0};

	// Do not update until audio plays if it is enabled
	if (media.hasStream(MediaProcController::AudioEntry) && audioBridge && !audioBridge->update(toAdd))
		return true;

	if (toAdd != 0) {
		sp->clock.reset();
		//sendToLog(LogLevel::Info, "toAdd %d\n", toAdd);
	}

	auto objectClockLap = sp->clock.lapNanos();

	//sendToLog(LogLevel::Info, "mediaClock: %llu, yesobjectClock: %llu, objectClockLap: %llu\n", mediaClock.timeNanos(), object->clock.timeNanos(), objectClockLap);
	mediaClock.tickNanos(objectClockLap);
	if (toAdd != 0)
		mediaClock.tick(toAdd);
	if (!mediaClock.hasCountdown())
		mediaClock.addCountdownNanos(nanosPerFrame);
	while (mediaClock.expired()) {
		mediaClock.addCountdownNanos(nanosPerFrame);
		framesToAdvance++;
	}

	//sendToLog(LogLevel::Info, "framesToAdvance: %i\n", framesToAdvance);

	if (framesToAdvance > 0) {
		bool endOfFile      = false;
		auto thisVideoFrame = media.advanceVideoFrames(framesToAdvance, endOfFile);
		if (endOfFile)
			videoState |= VS_END_OF_FILE;

		if (thisVideoFrame) {
			// This is not a mistake, frame update logic does not depend on old
			// old is only relevant in sprite verification
			auto frame = frame_gpu[NewFrame] ? frame_gpu[NewFrame] : frame_gpu[DefFrame];
			//sendToLog(LogLevel::Info, "[Frame %lld] Fmt: %d(nv12<%d>,yuv420p<%d>), planes: %d<%d, %d, %d>, gpu out: %dx%d\n",
			//			thisVideoFrame->frameNumber, thisVideoFrame->srcFormat, AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P,
			//			thisVideoFrame->planesCnt, thisVideoFrame->linesize[0], thisVideoFrame->linesize[1], thisVideoFrame->linesize[2],
			//			frame->w, frame->h);
			if (thisVideoFrame->srcFormat == AV_PIX_FMT_NV12 || thisVideoFrame->srcFormat == AV_PIX_FMT_YUV420P) {
				ensurePlanesImgs(thisVideoFrame->srcFormat, thisVideoFrame->planesCnt, videoRect.w, mask_gpu ? videoRect.h * 2 : videoRect.h);
				if (thisVideoFrame->srcFormat == AV_PIX_FMT_NV12) {
					gpu.convertNV12ToRGB(frame, planes_gpu, videoRect, thisVideoFrame->planes, thisVideoFrame->linesize, mask_gpu);
				} else {
					gpu.convertYUVToRGB(frame, planes_gpu, videoRect, thisVideoFrame->planes, thisVideoFrame->linesize, mask_gpu);
				}
			} else /*if (thisVideoFrame->srcFormat == AV_PIX_FMT_NONE)*/ { /* Converted by sws earlier */
				if (mask_gpu) {
					GPU_Rect maskRect = videoRect;
					maskRect.y += maskRect.h;
					gpu.mergeAlpha(frame, &videoRect, mask_gpu, &maskRect, thisVideoFrame->surface);
				} else {
					gpu.updateImage(frame, nullptr, thisVideoFrame->surface, nullptr, false);
				}
			}

			//sendToLog(LogLevel::Info, "Updated frame number %d\n", thisFrame->frameNumber);

			// Now we are done; give back the surface for later use
			media.giveImageBack(thisVideoFrame->surface);
		}
	}
	return true;
}

void MediaLayer::refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool centre_coordinates, int rm, float scalex, float scaley) {
	auto frame = (!(rm & REFRESH_BEFORESCENE_MODE) && frame_gpu[NewFrame]) ? frame_gpu[NewFrame] : frame_gpu[DefFrame];

	// I think this should never happen actually
	if (!frame || clip.w == 0 || clip.h == 0) {
		return;
	}

	if (!centre_coordinates) {
		x += (frame->w * wFactor) / 2.0;
		y += (frame->h * hFactor) / 2.0;
	}

	scalex = scalex != 0 ? wFactor * scalex : wFactor;
	scaley = scaley != 0 ? hFactor * scaley : hFactor;

	//sendToLog(LogLevel::Info, "frame_gpu->h %u h_factor %f y %f y+(f*h/2.0) %f\n", frame_gpu->h, h_factor, y, y+((frame_gpu->h*h_factor)/2.0));

	gpu.copyGPUImage(frame, nullptr, &clip, target, x, y, scalex, scaley, 0, true);

	if (audioBridge)
		audioBridge->startPlayback();

	if ((videoState & VS_END_OF_FILE) && (videoState & VS_PLAYING)) {
		while (!stopPlayback(FinishMode::LeaveCurrent)) {
			// This should not happen since the decoders are stopped by this time
			sendToLog(LogLevel::Error, "Failed to stop video playback at once, something is wrong\n");
		}
	}
}

void MediaLayer::commit() {
	if (frame_gpu[NewFrame]) {
		gpu.freeImage(frame_gpu[DefFrame]); // Guaranteed to be not null
		frame_gpu[DefFrame] = frame_gpu[NewFrame];
		frame_gpu[NewFrame] = nullptr;
	}
	videoState &= ~VS_AWAITS_COMMIT;
}

bool MediaLayer::isPlaying(bool checkStatic) {
	return (videoState & VS_PLAYING) || (checkStatic && frame_gpu[DefFrame]);
}
