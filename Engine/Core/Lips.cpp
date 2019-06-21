/**
 *  Lips.cpp
 *  ONScripter-RU
 *
 *  Provides lipsync implementation.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"

bool ONScripter::LipsAnimationAction::expired() {
	if (ons.skipLipsAction)
		return false; // can't expire if we're told not even to do anything
	// Deal with non-playing channels.

	return !ons.wave_sample[channel] || !Mix_Playing(channel) || Mix_Paused(channel);
}

void ONScripter::LipsAnimationAction::setCellForCharacter(const std::string &characterName, int cellNumber) {
	for (AnimationInfo *ai : ons.sprites(SPRITE_LSP | SPRITE_LSP2, true)) {
		if (ai->exists && ai->gpu_image && ai->visible && ai->lips_name) {
			if (characterName == ai->lips_name && ai->current_cell != cellNumber) {
				ai->setCell(cellNumber);
				ons.dirtySpriteRect(ai->id, ai->type == SPRITE_LSP2);
			}
		}
		if (ai->old_ai)
			ai = ai->old_ai;
		if (ai->exists && ai->gpu_image && ai->visible && ai->lips_name) {
			if (characterName == ai->lips_name && ai->current_cell != cellNumber) {
				ai->setCell(cellNumber);
				ons.dirtySpriteRect(ai->id, ai->type == SPRITE_LSP2, true);
			}
		}
	}
	ons.flush(ons.refreshMode());
}

void ONScripter::LipsAnimationAction::draw() {
	// do this even if expired on last call. onExpire
	for (auto &charName : ons.lipsChannels[channel].get().characterNames)
		setCellForCharacter(charName, 0);
}

void ONScripter::LipsAnimationAction::onExpired() {
	ConstantRefreshAction::onExpired();
	draw();
}

void ONScripter::LipsAnimationAction::run() {
	if (ons.skipLipsAction)
		return;

	draw();
	int now = SDL_GetTicks();

	// Deal with playing channels.
	if (!ons.wave_sample[channel] || !Mix_Playing(channel) || Mix_Paused(channel))
		return;
	Lips &lipdata = ons.lipsChannels[channel].get().lipsData;

	int index = static_cast<int>((0.0 + now - lipdata.speechStart) / MS_PER_CHUNK);
	if (index < 0 || index >= lipdata.seqSize)
		return;

	for (auto &charName : ons.lipsChannels[channel].get().characterNames) {
		setCellForCharacter(charName, lipdata.seq[index]);
	}
}

double ONScripter::readChunk(int channel, uint32_t no) {
	switch (audio_format.format) {
		case AUDIO_S8:
			return std::abs(*reinterpret_cast<int8_t *>(wave_sample[channel]->chunk->abuf + no));
		case AUDIO_U8:
			return wave_sample[channel]->chunk->abuf[no];
		case AUDIO_S16:
			return std::abs(*reinterpret_cast<int16_t *>(wave_sample[channel]->chunk->abuf + no));
		case AUDIO_U16:
			return *reinterpret_cast<uint16_t *>(wave_sample[channel]->chunk->abuf + no);
		case AUDIO_S32:
			return std::abs(*reinterpret_cast<int32_t *>(wave_sample[channel]->chunk->abuf + no));
		case AUDIO_F32:
		default:
			return std::fabs(*reinterpret_cast<float *>(wave_sample[channel]->chunk->abuf + no));
	}
}

void ONScripter::getChunkParams(uint32_t &chunk_size, double &max_value) {
	switch (audio_format.format) {
		case AUDIO_S8:
			chunk_size = sizeof(int8_t) * audio_format.channels;
			max_value  = std::numeric_limits<int8_t>::max();
			break;
		case AUDIO_U8:
			chunk_size = sizeof(uint8_t) * audio_format.channels;
			max_value  = std::numeric_limits<uint8_t>::max();
			break;
		case AUDIO_S16:
			chunk_size = sizeof(int16_t) * audio_format.channels;
			max_value  = std::numeric_limits<int16_t>::max();
			break;
		case AUDIO_U16:
			chunk_size = sizeof(uint16_t) * audio_format.channels;
			max_value  = std::numeric_limits<uint16_t>::max();
			break;
		case AUDIO_S32:
			chunk_size = sizeof(int32_t) * audio_format.channels;
			max_value  = std::numeric_limits<int32_t>::max();
			break;
		case AUDIO_F32:
		default:
			chunk_size = sizeof(float) * audio_format.channels;
			max_value  = 1.0;
			break;
	}
}

void ONScripter::loadLips(int channel) {
	uint32_t chunk_size;
	double max_value;
	getChunkParams(chunk_size, max_value);

	uint32_t buf_len = wave_sample[channel]->chunk->alen;
	if (buf_len > LIPS_AUDIO_RATE * MAX_SOUND_LENGTH * chunk_size) {
		sendToLog(LogLevel::Error, "The file is too big!\n");
		return;
	}

	uint32_t i{0};
	int vc{0}, count{0};
	double peak{0};
	Lips &lipdata = lipsChannels[channel].get().lipsData;

	do {
		auto v = readChunk(channel, i);
		if (v > peak)
			peak = v;

		count++;
		i += chunk_size;

		if (count >= SAMPLES_PER_CHUNK || i >= buf_len) {
			peak /= max_value;

			lipdata.seq[vc] = 2;
			if (peak < speechLevels[1])
				lipdata.seq[vc] = 1;
			if (peak < speechLevels[0])
				lipdata.seq[vc] = 0;

			if (vc == 0 && lipdata.seq[vc] == 2)
				lipdata.seq[vc] = 1;
			if (vc > 0) {
				if ((lipdata.seq[vc] == 2 && lipdata.seq[vc - 1] == 0) ||
				    (lipdata.seq[vc] == 0 && lipdata.seq[vc - 1] == 2)) {
					lipdata.seq[vc] = 1;
				}
			}
			if (i >= buf_len && lipdata.seq[vc] == 2)
				lipdata.seq[vc] = 1;

			vc++;

			count -= SAMPLES_PER_CHUNK;
			peak = 0;
		}
	} while (i < buf_len);

	lipdata.seqSize = vc;
}
