/**
 *  VideoDecoder.cpp
 *  ONScripter-RU
 *
 *  Contains Media Engine video decoder.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Media/Controller.hpp"

extern "C" {
#include <libavutil/pixdesc.h>
}

#include <iostream>

//"Well known" framerate
//Should be noticed that this function works approximately, because the timeCodes received are taken before decoding
long double MediaProcController::VideoDecoder::getVideoFramerate(bool &isVFR, bool &isCorrupted) {

	long double fromTimeBase = av_q2d(codecContext->time_base) * codecContext->ticks_per_frame;
	if (fromTimeBase <= 0)
		fromTimeBase = 0;
	else
		fromTimeBase = 1 / fromTimeBase;

	//long double fromLengthWithPackets = formatContext->streams[streams[VideoEntry]]->nb_frames*av_q2d(formatContext->streams[streams[VideoEntry]]->time_base);
	//if (fromLengthWithPackets <= 0) fromLengthWithPackets = 0;
	//else fromLengthWithPackets = 1/fromLengthWithPackets;

	long double fromAvgFramerate = av_q2d(media.formatContext->streams[stream]->avg_frame_rate);
	if (fromAvgFramerate < 0)
		fromAvgFramerate = 0;

	long double fromRframerate = av_q2d(media.formatContext->streams[stream]->r_frame_rate);
	if (fromRframerate < 0)
		fromRframerate = 0;

	SDL_SemWait(media.initVideoTimecodesLock);
	SDL_DestroySemaphore(media.initVideoTimecodesLock);
	media.initVideoTimecodesLock = nullptr;

	// This will still let us detect VFR, but will deal with B-frames
	std::sort(media.initVideoTimecodes.begin(), media.initVideoTimecodes.end());
	media.initVideoTimecodes[VideoPacketBufferSize - 1] = 0; // remove the last one, in case that's a B-Frame

	isCorrupted = false;
	long double previous{0}, sum{0};
	int vfrCount{0}, cfrCount{0}, count{0}, previousCount{0}, countSkipped{0};

	long double minVFR, maxVFR;
	// Needs correct coefficients unique per file, they not only depend from fps but from other factors
	if (fromTimeBase > 31) {
		minVFR = 0.995;
		maxVFR = 1.005;
	} else {
		minVFR = 0.95;
		maxVFR = 1.05;
	}

	for (unsigned int i = 0; i < VideoPacketBufferSize; i++) {
		if (media.initVideoTimecodes[i] < 0) {
			if (media.initVideoTimecodes[i] < -1)
				isCorrupted = true;
			media.initVideoTimecodes[i] = 0;
			countSkipped++;
		} else if (media.initVideoTimecodes[i] > 0) {
			int currCount = i - countSkipped;
			if (previous > 0 && currCount > 0) {
				if (media.initVideoTimecodes[i] / currCount * minVFR > previous / previousCount ||
				    media.initVideoTimecodes[i] / currCount * maxVFR < previous / previousCount) {
					vfrCount++;
				} else {
					cfrCount++;
				}
			}
			count += currCount;
			sum += media.initVideoTimecodes[i];
			previousCount = currCount;
			previous      = media.initVideoTimecodes[i];
		}
	}

	bool isRounded{true};
#ifdef DROID
	// Workarounds clang-3.8 crash https://llvm.org/bugs/show_bug.cgi?id=26803
	double fromPacketQueue = 1 / (sum / count);
#else
	long double fromPacketQueue = 1 / (sum / count);
#endif
	// bit rough, thanks to pts; dts is more reliable... but it may not exist ><
	isVFR = vfrCount > 0;

	if (fromPacketQueue > 4.990 && fromPacketQueue <= 5.010)
		fromPacketQueue = 5.000;
	else if (fromPacketQueue > 9.990 && fromPacketQueue <= 10.010)
		fromPacketQueue = 10.000;
	else if (fromPacketQueue > 11.990 && fromPacketQueue <= 12.010)
		fromPacketQueue = 12.000;
	else if (fromPacketQueue > 14.990 && fromPacketQueue <= 15.010)
		fromPacketQueue = 15.000;
	else if (fromPacketQueue > 23.952 && fromPacketQueue <= 23.988)
		fromPacketQueue = 24000.0 / 1001.0;
	else if (fromPacketQueue > 23.988 && fromPacketQueue <= 24.024)
		fromPacketQueue = 24.000;
	else if (fromPacketQueue > 24.975 && fromPacketQueue <= 25.025)
		fromPacketQueue = 25.000;
	else if (fromPacketQueue > 29.940 && fromPacketQueue <= 29.985)
		fromPacketQueue = 30000.0 / 1001.0;
	else if (fromPacketQueue > 29.970 && fromPacketQueue <= 30.030)
		fromPacketQueue = 30.000;
	else if (fromPacketQueue > 23.952 * 2 && fromPacketQueue <= 23.988 * 2)
		fromPacketQueue = (24000.0 / 1001.0) * 2;
	else if (fromPacketQueue > 23.988 * 2 && fromPacketQueue <= 24.024 * 2)
		fromPacketQueue = 48.000;
	else if (fromPacketQueue > 24.975 * 2 && fromPacketQueue <= 25.025 * 2)
		fromPacketQueue = 50.000;
	else if (fromPacketQueue > 29.940 * 2 && fromPacketQueue <= 29.985 * 2)
		fromPacketQueue = 60000.0 / 1001.0;
	else if (fromPacketQueue > 29.970 * 2 && fromPacketQueue <= 30.030 * 2)
		fromPacketQueue = 60.000;
	else
		isRounded = false;

	long double bestFramerate{0};

	if (!isVFR && isRounded) {
		bestFramerate = fromPacketQueue;
	} else if (fromTimeBase != 0) {
		bestFramerate = fromTimeBase;
	} else if (fromRframerate != 0) {
		bestFramerate = fromRframerate;
	} else if (fromAvgFramerate != 0) {
		bestFramerate = fromAvgFramerate;
		//} else if (fromLengthWithPackets != 0) {
		//	bestFramerate = fromLengthWithPackets;
	} else {
		bestFramerate = fromPacketQueue;
	}

	sendToLog(LogLevel::Info, "Detected framerate is %f, vfr %d, corrupted %d\n", static_cast<double>(bestFramerate), isVFR, isCorrupted);

	return bestFramerate;
}

bool MediaProcController::VideoDecoder::initSwsContext(int dstW, int dstH, const AVPixelFormat *format, bool forHardware) {
	deinitSwsContext();

	if (!format)
		format = &codecContext->pix_fmt;

	// Initially allow trying hardware converter
	if (!forHardware && imageConvertSourceFormat == AV_PIX_FMT_NONE && HardwareDecoderIFace::isFormatHWConverted(*format)) {
		return true;
	}

	imageConvertSourceFormat = *format;

	imageConvertContext = sws_getCachedContext(nullptr,
	                                           codecContext->width,
	                                           codecContext->height,
	                                           imageConvertSourceFormat,
	                                           dstW, dstH,
	                                           AV_PIX_FMT_RGB24, SWS_BICUBIC,
	                                           nullptr, nullptr, nullptr);

	if (!imageConvertContext) {
		return false;
	}

	int *inv_table, *table, srcRange, dstRange, brightness, contrast, saturation;
	// Due to "science cannot answer this question" reasons, ffmpeg enforces SMPTE 170M tables
	// at yuv2rgb conversion (mpeg & SMPTE 170M -> mpeg & SMPTE 170M). Even theoretically this
	// cannot work, so we try to fix it enforcing ITU-R Rec. 709 as a src matrix.
	if (!sws_getColorspaceDetails(imageConvertContext,
	                              &inv_table, &srcRange, &table, &dstRange,
	                              &brightness, &contrast, &saturation)) {
		sws_setColorspaceDetails(imageConvertContext,
		                         sws_getCoefficients(SWS_CS_ITU709), 0,
		                         sws_getCoefficients(SWS_CS_ITU709), 0,
		                         brightness, contrast, saturation);
	}

	return true;
}

void MediaProcController::VideoDecoder::deinitSwsContext() {
	if (imageConvertContext) {
		sws_freeContext(imageConvertContext);
		//imageConvertContext = nullptr;
	}
}

bool MediaProcController::VideoDecoder::initTiming(int64_t /*duration*/) {
	bool isVFR, isCorrupted;
	auto framerate = getVideoFramerate(isVFR, isCorrupted);

	if (isVFR || isCorrupted)
		sendToLog(LogLevel::Warn, "Warning, at this moment it is not reliably possible to play VFR and corrupted videos\n");

	nanosPerFrame = static_cast<uint64_t>(1 / framerate * 1000000000);

	return !isVFR && !isCorrupted;
}

void MediaProcController::VideoDecoder::processFrame(MediaFrame &vf) {
	if (HardwareDecoderIFace::process(frame, tempFrame)) {
		SDL_Surface *workingSurface = media.imagePool->getImage();

		uint8_t *data[]{static_cast<uint8_t *>(workingSurface->pixels)};
		int linesize[]{workingSurface->pitch};

		if (imageConvertSourceFormat != frame->format) {
			/* Check if we can use shader later on for conversion */
			if (media.hardwareConversion && HardwareDecoderIFace::isFormatHWConverted(static_cast<AVPixelFormat>(frame->format))) {

				for (size_t i = 0; frame->data[i]; i++) {
					AVBufferRef *buf = av_frame_get_plane_buffer(frame, static_cast<int>(i));
					vf.planes[i]     = new uint8_t[buf->size];
					std::memcpy(vf.planes[i], buf->data, buf->size);
					vf.planesCnt++;
				}

				vf.srcFormat = static_cast<AVPixelFormat>(frame->format);
				vf.dataSize  = codecContext->height;
				std::memcpy(vf.linesize, frame->linesize, AV_NUM_DATA_POINTERS * sizeof(int));
			}

			/* If we can't, fall back to sws */
			else {
				initSwsContext(workingSurface->w, workingSurface->h, reinterpret_cast<AVPixelFormat *>(&frame->format));
				vf.srcFormat = AV_PIX_FMT_NONE;
			}
		}

		if (vf.srcFormat == AV_PIX_FMT_NONE)
			sws_scale(imageConvertContext,
			          frame->data,
			          frame->linesize,
			          0, codecContext->height,
			          data, linesize);

		vf.surface     = workingSurface;
		vf.frameNumber = ++debugFrameNumber;
		vf.msTimeStamp = std::round(debugFrameNumber * nanosPerFrame / 1000000); //pts*/
	} else {
		throw std::runtime_error("Failed to decode a frame");
	}
}
