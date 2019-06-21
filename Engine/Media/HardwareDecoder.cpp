/**
 *  HardwareDecoder.cpp
 *  ONScripter-RU
 *
 *  Contains Media Engine hardware decoding support.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Media/Controller.hpp"

#if defined(IOS) || defined(MACOSX)

extern "C" {
#include <VideoToolbox/VideoToolbox.h>
#include <libavcodec/videotoolbox.h>
#include <libavutil/imgutils.h>
}

namespace HardwareDecoderVT {

static void reg() {}

static AVPixelFormat init(AVCodecContext *context, const AVPixelFormat *format) {
	if (MediaProcController::HardwareDecoderIFace::hasFormat(format, AV_PIX_FMT_VIDEOTOOLBOX) && av_videotoolbox_default_init(context) >= 0) {
		sendToLog(LogLevel::Info, "Successfully initialised VT decoder\n");
		return AV_PIX_FMT_VIDEOTOOLBOX;
	}

	return MediaProcController::HardwareDecoderIFace::defaultFormat(format);
}

static AVCodec *findDecoder(AVCodecContext *) {
	return nullptr;
}

static void deinit(AVCodecContext *context) {
	if (context->pix_fmt == AV_PIX_FMT_VIDEOTOOLBOX)
		av_videotoolbox_default_free(context);
}

static AVFrame *process(AVFrame *dFrame, AVFrame *&tempFrame) {
	if (dFrame->format != AV_PIX_FMT_VIDEOTOOLBOX)
		return dFrame;

	auto pixbuf         = reinterpret_cast<CVPixelBufferRef>(dFrame->data[3]);
	OSType pixel_format = CVPixelBufferGetPixelFormatType(pixbuf);

	if (tempFrame) {
		av_frame_unref(tempFrame);
	} else {
		tempFrame = av_frame_alloc();
	}

	switch (pixel_format) {
		case kCVPixelFormatType_420YpCbCr8Planar:
			tempFrame->format = AV_PIX_FMT_YUV420P;
			break;
		case kCVPixelFormatType_422YpCbCr8:
			tempFrame->format = AV_PIX_FMT_UYVY422;
			break;
		case kCVPixelFormatType_422YpCbCr8_yuvs:
			tempFrame->format = AV_PIX_FMT_YUYV422;
			break;
		case kCVPixelFormatType_32BGRA:
			tempFrame->format = AV_PIX_FMT_BGRA;
			break;
		case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
			tempFrame->format = AV_PIX_FMT_NV12;
			break;
		default:
			sendToLog(LogLevel::Error, "Can't decode video frame with VT decoder\n");
			return nullptr;
	}

	tempFrame->width  = dFrame->width;
	tempFrame->height = dFrame->height;
	int ret           = av_frame_get_buffer(tempFrame, 32);
	if (ret < 0) {
		return nullptr;
	}

	CVReturn err = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
	if (err != kCVReturnSuccess) {
		sendToLog(LogLevel::Error, "Error locking the pixel buffer\n");
		return nullptr;
	}

	size_t planes{0};
	uint8_t *data[4]{};
	int linesize[4]{};

	if (CVPixelBufferIsPlanar(pixbuf)) {
		planes = CVPixelBufferGetPlaneCount(pixbuf);
		for (size_t i = 0; i < planes; i++) {
			data[i]     = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixbuf, i));
			linesize[i] = static_cast<int>(CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i));
		}
	} else {
		data[0]     = static_cast<uint8_t *>(CVPixelBufferGetBaseAddress(pixbuf));
		linesize[0] = static_cast<int>(CVPixelBufferGetBytesPerRow(pixbuf));
	}

	av_image_copy(tempFrame->data, tempFrame->linesize,
	              const_cast<const uint8_t **>(data), linesize, static_cast<AVPixelFormat>(tempFrame->format),
	              dFrame->width, dFrame->height);

	ret = av_frame_copy_props(tempFrame, dFrame);
	CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
	if (ret < 0) {
		return nullptr;
	}

	av_frame_unref(dFrame);
	av_frame_move_ref(dFrame, tempFrame);

	return dFrame;
}
} // namespace HardwareDecoderVT

#elif defined(DROID)

namespace HardwareDecoderMC {
#include <jni.h>

extern "C" {
#include <libavcodec/mediacodec.h>
#include <libavcodec/jni.h>
}

static JavaVM *getJavaVM() {
	static JavaVM *vm{nullptr};

	if (vm)
		return vm;

	auto env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
	if (env)
		env->GetJavaVM(&vm);
	else
		sendToLog(LogLevel::Error, "Failed to get JNIEnv\n");

	return vm;
}

static void reg() {
	auto vm = getJavaVM();

	if (vm) {
		int err = av_jni_set_java_vm(vm, nullptr);
		if (err)
			sendToLog(LogLevel::Error, "Failed to set java vm for hw accelerated decoding\n");
	} else {
		sendToLog(LogLevel::Error, "No java vm available for hw accelerated decoding\n");
	}
}

static AVPixelFormat init(AVCodecContext *context, const AVPixelFormat *format) {
	if (MediaProcController::HardwareDecoderIFace::hasFormat(format, AV_PIX_FMT_MEDIACODEC)) {
		auto hwctx = context->hwaccel_context ? context->hwaccel_context : av_mediacodec_alloc_context();
		if (hwctx) {
			context->hwaccel_context = hwctx;
			sendToLog(LogLevel::Info, "Successfully initialised MC decoder\n");
			return AV_PIX_FMT_MEDIACODEC;
		} else {
			sendToLog(LogLevel::Error, "Failed to allocate MC decoder context\n");
		}
	}

	return MediaProcController::HardwareDecoderIFace::defaultFormat(format);
}

static AVCodec *findDecoder(AVCodecContext *context) {
	switch (context->codec_id) {
		case AV_CODEC_ID_H264:
			return avcodec_find_decoder_by_name("h264_mediacodec");
		case AV_CODEC_ID_HEVC:
			return avcodec_find_decoder_by_name("hevc_mediacodec");
		case AV_CODEC_ID_MPEG4:
			return avcodec_find_decoder_by_name("mpeg4_mediacodec");
		case AV_CODEC_ID_VP8:
			return avcodec_find_decoder_by_name("vp8_mediacodec");
		case AV_CODEC_ID_VP9:
			return avcodec_find_decoder_by_name("vp9_mediacodec");
		default:
			return nullptr;
	}
}

static void deinit(AVCodecContext *context) {
	if (context->hwaccel_context)
		av_mediacodec_default_free(context);
}

static AVFrame *process(AVFrame *dFrame, AVFrame *&tempFrame) {
	(void)tempFrame;
	return dFrame;
}
} // namespace HardwareDecoderMC
#endif

const std::unordered_set<int> MediaProcController::HardwareDecoderIFace::hardwareAcceleratedFormats {
#if defined(LINUX)
	AV_PIX_FMT_VDPAU_H264,
	AV_PIX_FMT_VDPAU_MPEG1,
	AV_PIX_FMT_VDPAU_MPEG2,
	AV_PIX_FMT_VDPAU_WMV3,
	AV_PIX_FMT_VDPAU_VC1,
	AV_PIX_FMT_VDPAU,
	AV_PIX_FMT_VAAPI_MOCO,
	AV_PIX_FMT_VAAPI_IDCT,
	AV_PIX_FMT_VAAPI,
#elif defined(WIN32)
	AV_PIX_FMT_DXVA2_VLD,
	AV_PIX_FMT_D3D11VA_VLD,
#elif defined(IOS) || defined(MACOSX)
#ifdef AV_PIX_FMT_VDA_VLD
	// VDA support is disabled in our builds, and ffmpeg 4.x has it removed.
	// Let code compile without it at the very least.
	AV_PIX_FMT_VDA_VLD,
#endif
	AV_PIX_FMT_VIDEOTOOLBOX
#elif defined(DROID)
	AV_PIX_FMT_MEDIACODEC
#endif
};

const std::unordered_set<int> MediaProcController::HardwareDecoderIFace::hwConvertedFormats{
    AV_PIX_FMT_NV12,
    AV_PIX_FMT_YUV420P};

void MediaProcController::HardwareDecoderIFace::reg() {
#if defined(IOS) || defined(MACOSX)
	HardwareDecoderVT::reg();
#elif defined(DROID)
	HardwareDecoderMC::reg();
#endif
}

AVPixelFormat MediaProcController::HardwareDecoderIFace::init(AVCodecContext *context, const AVPixelFormat *format) {
#if defined(IOS) || defined(MACOSX)
	return HardwareDecoderVT::init(context, format);
#elif defined(DROID)
	return HardwareDecoderMC::init(context, format);
#else
	(void)context;
	return defaultFormat(format);
#endif
}

AVCodec *MediaProcController::HardwareDecoderIFace::findDecoder(AVCodecContext *context) {
#if defined(IOS) || defined(MACOSX)
	return HardwareDecoderVT::findDecoder(context);
#elif defined(DROID)
	return HardwareDecoderMC::findDecoder(context);
#else
	(void)context;
	return nullptr;
#endif
}

void MediaProcController::HardwareDecoderIFace::deinit(AVCodecContext *context) {
	if (!context)
		return;

#if defined(IOS) || defined(MACOSX)
	HardwareDecoderVT::deinit(context);
#elif defined(DROID)
	return HardwareDecoderMC::deinit(context);
#endif
}

AVFrame *MediaProcController::HardwareDecoderIFace::process(AVFrame *hwFrame, AVFrame *&tmpFrame) {
#if defined(IOS) || defined(MACOSX)
	return HardwareDecoderVT::process(hwFrame, tmpFrame);
#elif defined(DROID)
	return HardwareDecoderMC::process(hwFrame, tmpFrame);
#else
	(void)tmpFrame;
	return hwFrame;
#endif
}
