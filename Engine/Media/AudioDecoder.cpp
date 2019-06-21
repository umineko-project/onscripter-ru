/**
 *  AudioDecoder.cpp
 *  ONScripter-RU
 *
 *  Contains Media Engine audio decoder.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Media/Controller.hpp"

bool MediaProcController::AudioDecoder::initSwrContext(const AudioSpec &audioSpec) {
	int64_t inputChannelLayout = av_get_default_channel_layout(codecContext->channels);

	if (codecContext->sample_rate != audioSpec.frequency ||
	    codecContext->channels != audioSpec.channels ||
	    codecContext->sample_fmt != audioSpec.format ||
	    inputChannelLayout != AV_CH_LAYOUT_STEREO) {
		swrContext = swr_alloc();
		av_opt_set_int(swrContext, "in_channel_layout", inputChannelLayout, 0);
		av_opt_set_int(swrContext, "out_channel_layout", audioSpec.channelLayout, 0);
		av_opt_set_int(swrContext, "in_sample_rate", codecContext->sample_rate, 0);
		av_opt_set_int(swrContext, "out_sample_rate", audioSpec.frequency, 0);
		av_opt_set_sample_fmt(swrContext, "in_sample_fmt", codecContext->sample_fmt, 0);
		av_opt_set_sample_fmt(swrContext, "out_sample_fmt", audioSpec.format, 0);

		int err = swr_init(swrContext);
		if (err < 0) {
			swr_free(&swrContext);
			return false;
		}
	}
	return true;
}

void MediaProcController::AudioDecoder::processFrame(MediaFrame &vf) {
	uint8_t *output{nullptr};
	uint32_t outputSize{0};

	if (swrContext) {
		int64_t out_samples = static_cast<int64_t>(av_rescale_rnd(swr_get_delay(swrContext, codecContext->sample_rate) + frame->nb_samples,
		                                                          media.audioSpec.frequency, codecContext->sample_rate, AV_ROUND_UP));
		//Warning: further out_samples usage may loose precision
		av_samples_alloc(&output, nullptr, media.audioSpec.channels, static_cast<int32_t>(out_samples), media.audioSpec.format, 0);
		out_samples = swr_convert(swrContext, &output, static_cast<int32_t>(out_samples),
		                          const_cast<const uint8_t **>(frame->data), frame->nb_samples);

		outputSize = av_samples_get_buffer_size(nullptr, media.audioSpec.channels,
		                                        static_cast<int32_t>(out_samples), media.audioSpec.format, 1);
	} else {
		outputSize = av_samples_get_buffer_size(nullptr, codecContext->channels,
		                                        frame->nb_samples, codecContext->sample_fmt, 1);
		output     = static_cast<uint8_t *>(av_malloc(outputSize));
		std::memcpy(output, frame->data[0], outputSize);
	}
	vf.data        = output;
	vf.dataSize    = outputSize;
	vf.dataDeleter = [](uint8_t *d) {
		av_freep(&d);
	};
	vf.frameNumber = ++debugFrameNumber;
}
