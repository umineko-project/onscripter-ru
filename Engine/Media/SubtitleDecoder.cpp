/**
 *  SubtitleDecoder.cpp
 *  ONScripter-RU
 *
 *  Contains Media Engine subtitle decoder.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Media/Controller.hpp"

bool MediaProcController::SubtitleDecoder::prepare(const char *filename, int frameWidth, int frameHeight) {
	return subtitleDriver.init(frameWidth, frameHeight, filename, nullptr, codecContext);
}

void MediaProcController::SubtitleDecoder::processFrame(MediaFrame &frame) {
	if (frame.srcFormat != AV_PIX_FMT_NONE)
		subtitleDriver.blendOn(frame.planes, frame.planesCnt, frame.srcFormat, frame.linesize, frame.dataSize, frame.msTimeStamp);
	else
		subtitleDriver.blendOn(frame.surface, frame.msTimeStamp);
}
