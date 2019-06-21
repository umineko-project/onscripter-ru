/**
 *  AudioBridge.hpp
 *  ONScripter-RU
 *
 *  SDL_Mixer external audio handler interaction.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <memory>
#include <atomic>
#include <functional>
#include <utility>
#include <cstddef>
#include <cstdint>

class AudioBridge {
public:
	AudioBridge(uint32_t channelNumber, uint32_t channelVolume, std::function<cmp::unique_ptr_del<uint8_t[]>(size_t &)> retrieval, size_t rawBufferSize = 2048)
	    : channelNumber(channelNumber), channelVolume(channelVolume), retrieval(std::move(retrieval)), rawBufferSize(rawBufferSize) {}
	~AudioBridge() {
		if (started.load(std::memory_order_relaxed))
			Mix_HaltChannel(channelNumber);
		if (rawChunk)
			Mix_FreeChunk(rawChunk);
	}
	AudioBridge(const AudioBridge &) = delete;
	AudioBridge &operator=(const AudioBridge &) = delete;

	bool prepare();
	bool startPlayback();
	bool update(uint32_t &toAdd);

private:
	static void fillBuffers(int channel, void *stream, int len, void *udata);

	uint32_t channelNumber{0};
	uint32_t channelVolume{0};
	std::function<cmp::unique_ptr_del<uint8_t[]>(size_t &)> retrieval; // Passed function returning audio chunks from the external source

	cmp::unique_ptr_del<uint8_t[]> curBuffer; // Current frame buffer
	size_t curBufferSize{0};                  // Internal buffer size
	size_t curBufferPos{0};                   // Internal buffer position

	std::unique_ptr<uint8_t[]> rawBuffer; // Playing buffer
	size_t rawBufferSize{0};              // Playing buffer size
	Mix_Chunk *rawChunk{nullptr};         // Playing chunk

	std::atomic<bool> startedToPlay{false}; // A signal that SDL_mixer started to play the sound
	std::atomic<bool> started{false};       // A signal that we called SDL_mixer to play the sound
	uint32_t startedTime{0};                // A timestamp when audio started to play (SDL_GetTicks())
};
