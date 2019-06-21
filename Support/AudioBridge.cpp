/**
 *  AudioBridge.cpp
 *  ONScripter-RU
 *
 *  SDL_Mixer external audio handler interaction.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Support/AudioBridge.hpp"
#include "Support/FileDefs.hpp"

#include <iostream>
#include <algorithm>
#include <cstring>
#include <cassert>

void AudioBridge::fillBuffers(int /*channel*/, void * /*stream*/, int /*len*/, void *udata) {
	AudioBridge *ab = static_cast<AudioBridge *>(udata);

	size_t rawPos{0};
	uint8_t *raw = ab->rawBuffer.get();

	do {
		// Obtain new frame if needed
		if (!ab->curBuffer) {
			ab->curBufferPos = 0;
			ab->curBuffer    = ab->retrieval(ab->curBufferSize);
			//	sendToLog(LogLevel::Info, "frame: 0x%x frameSz: %ld\n", ab->curBuffer.get(), ab->curBufferSize);
		}

		if (ab->curBuffer) {
			size_t leftOverRaw = ab->rawBufferSize - rawPos;
			size_t leftOverCur = ab->curBufferSize - ab->curBufferPos;

			size_t leftover = std::min(leftOverCur, leftOverRaw);

			std::memcpy(raw, ab->curBuffer.get() + ab->curBufferPos, leftover);
			ab->curBufferPos += leftover;
			rawPos += leftover;

			if (ab->curBufferSize == ab->curBufferPos) {
				ab->curBuffer.reset();
			}

		} else { // No frames left
			std::memset(raw, 0, ab->rawBufferSize - rawPos);
			rawPos = ab->rawBufferSize;
		}

		raw += rawPos;

	} while (rawPos != ab->rawBufferSize);

	if (!ab->startedToPlay.load(std::memory_order_acquire)) {
		ab->startedTime = SDL_GetTicks();
		ab->startedToPlay.store(true, std::memory_order_release);
	}
}

bool AudioBridge::update(uint32_t &toAdd) {
	if (!startedToPlay.load(std::memory_order_acquire))
		return false;
	if (startedTime != 0) {
		toAdd       = SDL_GetTicks() - startedTime;
		startedTime = 0;
	}
	return true;
}

bool AudioBridge::startPlayback() {
	if (started.load(std::memory_order_relaxed))
		return true;

	// Call that externally to avoid issues
	if (!Mix_Playing(channelNumber)) {
		Mix_Volume(channelNumber, channelVolume);
		Mix_PlayChannel(channelNumber, rawChunk, -1);
		started.store(true, std::memory_order_relaxed);
		return true;
	}

	return false;
}

bool AudioBridge::prepare() {
	bool ret = true;

	rawBuffer = std::make_unique<uint8_t[]>(rawBufferSize);
	std::memset(rawBuffer.get(), 0, rawBufferSize);
	rawChunk = Mix_QuickLoad_RAW(rawBuffer.get(), static_cast<uint32_t>(rawBufferSize));

	if (!rawChunk) {
		sendToLog(LogLevel::Error, "Failed to prepare an audio stream %d\n", Mix_GetError());
		ret = false;
	} else {
		if (!Mix_RegisterEffect(channelNumber, fillBuffers, nullptr, static_cast<void *>(this))) {
			sendToLog(LogLevel::Error, "Failed to prepare audio update function: %d\n", Mix_GetError());
			ret = false;
		}
	}

	// A delay is very big anyway, a chance it happens before we start to display anything... is too low
	//Mix_PlayChannel(audio_channel_number, audio_raw_chunk, -1);
	//Mix_Pause(audio_channel_number);

	return ret;
}
