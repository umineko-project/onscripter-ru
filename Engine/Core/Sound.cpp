/**
 *  Sound.cpp
 *  ONScripter-RU
 *
 *  Methods for playing sound.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Readers/Base.hpp"
#include "Support/FileIO.hpp"

#ifdef LINUX
#include <signal.h>
#endif

#if defined(INTEGER_OGG_VORBIS)
#include <tremor/ivorbisfile.h>
#else
#include <vorbis/vorbisfile.h>
#endif

#include <cstdio>

struct WAVE_HEADER {
	char chunk_riff[4];
	char riff_length[4];
	// format chunk
	char fmt_id[8];
	char fmt_size[4];
	char data_fmt[2];
	char channels[2];
	char frequency[4];
	char byte_size[4];
	char sample_byte_size[2];
	char sample_bit_size[2];
};

struct WAVE_DATA_HEADER {
	// data chunk
	char chunk_id[4];
	char data_length[4];
};

static void setupWaveHeader(uint8_t *buffer, int channels, int bits,
                            unsigned long rate, size_t data_length,
                            size_t extra_bytes = 0, uint8_t *extra_ptr = nullptr);

extern bool ext_music_play_once_flag;

extern "C" {
extern void musicFinishCallback();
}
extern void seqmusicCallback(int sig);

#define TMP_MUSIC_FILE "tmp.mus"

//AVI header format
#define IS_AVI_HDR(buf)                        \
	(((buf)[0] == 'R') && ((buf)[1] == 'I') && \
	 ((buf)[2] == 'F') && ((buf)[3] == 'F') && \
	 ((buf)[8] == 'A') && ((buf)[9] == 'V') && \
	 ((buf)[10] == 'I'))

//ID3v2 tag header format
#define HAS_ID3V2_TAG(buf)                                          \
	(((buf)[0] == 'I') && ((buf)[1] == 'D') && ((buf)[2] == '3') && \
	 ((buf)[3] != 0xFF) && ((buf)[4] != 0xFF) && !((buf)[5] & 0x1F))

struct OVInfo {
	uint8_t *buf;
	ogg_int64_t length;
	ogg_int64_t pos;
	OggVorbis_File ovf;
};

static size_t oc_read_func(void *ptr, size_t size, size_t nmemb, void *datasource) {
	OVInfo *ogg_vorbis_info = static_cast<OVInfo *>(datasource);

	size_t len = size * nmemb;
	if (static_cast<size_t>(ogg_vorbis_info->pos) + len > static_cast<size_t>(ogg_vorbis_info->length))
		len = static_cast<size_t>(ogg_vorbis_info->length - ogg_vorbis_info->pos);
	std::memcpy(ptr, ogg_vorbis_info->buf + ogg_vorbis_info->pos, len);
	ogg_vorbis_info->pos += len;

	return len;
}

static int oc_seek_func(void *datasource, ogg_int64_t offset, int whence) {
	OVInfo *ogg_vorbis_info = static_cast<OVInfo *>(datasource);

	ogg_int64_t pos = 0;
	if (whence == 0)
		pos = offset;
	else if (whence == 1)
		pos = ogg_vorbis_info->pos + offset;
	else if (whence == 2)
		pos = ogg_vorbis_info->length + offset;

	if (pos < 0 || pos > ogg_vorbis_info->length)
		return -1;

	ogg_vorbis_info->pos = pos;

	return 0;
}

static long oc_tell_func(void *datasource) {
	OVInfo *ogg_vorbis_info = static_cast<OVInfo *>(datasource);

	return static_cast<long>(ogg_vorbis_info->pos);
}

void ONScripter::loadSoundIntoCache(int id, const std::string &filename_str, bool async) {

	int ret{0};
	if (async) {
		ret = playSound(filename_str.c_str(), SOUND_PRELOAD | SOUND_CHUNK, false, MIX_CACHE_CHANNEL_ASYNC);
	} else {
		ret = playSoundThreaded(filename_str.c_str(), SOUND_PRELOAD | SOUND_CHUNK, false, MIX_CACHE_CHANNEL_BLOCK);
	}

	if (ret == SOUND_NONE) {
		if (pending_cache_chunk[async])
			pending_cache_chunk[async] = nullptr;
		sendToLog(LogLevel::Error, "Failed to cache sound %s in slot %d with async %d\n", filename_str.c_str(), id, async);
		return;
	}

	assert(pending_cache_chunk[async]);
	{
		Lock lock(&soundCache);
		soundCache.add(id, filename_str, pending_cache_chunk[async]);
	}
	pending_cache_chunk[async] = nullptr;

	//sendToLog(LogLevel::Info, "Cached sound %s in slot %d with async %d\n", filename_str.c_str(), id, async);
}

int ONScripter::trySoundCache(const char *filename, int format, bool loop_flag, int channel) {
	if (format & SOUND_CHUNK) {
		std::shared_ptr<Wrapped_Mix_Chunk> r{nullptr};
		{
			Lock lock(&soundCache);
			r = soundCache.get(filename);
		}
		// I hope shared_ptr is thread safe...
		if (r && r->chunk) {
			if (channel == MIX_CACHE_CHANNEL_ASYNC || channel == MIX_CACHE_CHANNEL_BLOCK) {
				bool async{channel == MIX_CACHE_CHANNEL_ASYNC};
				assert(!pending_cache_chunk[async]);
				pending_cache_chunk[async] = r;
			} else if (playWave(r, format, loop_flag, channel) != 0) {
				errorAndExit("Something mad was found in sound cache");
			}
			return SOUND_CHUNK;
		}
	}
	return SOUND_NONE;
}

int ONScripter::playSoundThreaded(const char *filename, int format, bool loop_flag, int channel, bool waitevent) {
	// note that this function is blocking -- it will not return until playSound is done
	// this function should be used instead of playSound pretty much all of the time,
	// to run playSound in a thread so that our main loop can continue without being horribly stalled

	//stop lipsEvent from being called, otherwise we will get lips broken due to loadLips calls in playSound -> playWave
	skipLipsAction = true;

	int cacheRet = trySoundCache(filename, format, loop_flag, channel);
	if (cacheRet) {
		skipLipsAction = false;
		return cacheRet;
	}

	// set up the normal playSound call
	async.playSound(filename, format, loop_flag, channel);

	preventExit(true);
	while (SDL_SemWaitTimeout(async.playSoundQueue.resultsWaiting, 1) != 0) {
		if (waitevent) {
			event_mode = IDLE_EVENT_MODE;
			Lock lock(&playSoundThreadedLock);
			waitEvent(0);
		}
	}
	preventExit(false);

	// pop the result of playSound and return it
	SDL_AtomicLock(&async.playSoundQueue.resultsLock);
	auto r = reinterpret_cast<uintptr_t>(async.playSoundQueue.results.front());
	async.playSoundQueue.results.pop_front();
	SDL_AtomicUnlock(&async.playSoundQueue.resultsLock);

	//enable lipsEvent
	skipLipsAction = false;

	return static_cast<int>(r);
}

int ONScripter::playSound(const char *filename, int format, bool loop_flag, int channel) {
	// This function modifies the following global variables:
	// music_info, script_h.errbuf, music_buffer, music_buffer_length, wave_sample, kv.second.lipsData.speechStart, audio_format, audio_open_flag

	if (!audio_open_flag)
		return SOUND_NONE;

	//Mion: account for mode_wave_demo setting
	//(i.e. if not set, then don't play non-bgm wave/ogg during skip mode)
	if (!mode_wave_demo_flag &&
	    // --- ^ --------------------------------------
	    // r.o. except for define section, should be ok
	    // --------------------------------------------
	    ((skip_mode & SKIP_NORMAL) || keyState.ctrl)) {
		// --- ^ ------------------------- ^ ---------------------------------------------------------
		// these globals are unmutexed, which is not ideal, but we think there will be no major issues
		// -------------------------------------------------------------------------------------------
		if ((format & SOUND_CHUNK) &&
		    ((channel < ONS_MIX_CHANNELS) || (channel == MIX_WAVE_CHANNEL)))
			return SOUND_NONE;
	}

	int cacheRet = trySoundCache(filename, format, loop_flag, channel);
	if (cacheRet)
		return cacheRet;

	size_t length{0};
	uint8_t *buffer{nullptr};
	{
		Lock lock(&music_file_name);
		// ------------- ^ ---------------------------------------------------------------------------
		// ! locked using a different lock to image, make sure all readers can access separate files !
		// at this moment only DirectReader is reliable
		// -------------------------------------------------------------------------------------------
		if (!script_h.reader->getFile(filename, length, &buffer))
			return SOUND_NONE;
	}

	if (length == 0)
		return SOUND_NONE;

	if ((channel == MIX_CACHE_CHANNEL_BLOCK || channel == MIX_CACHE_CHANNEL_ASYNC) &&
	    !(format & SOUND_CHUNK && format & SOUND_PRELOAD)) {
		freearr(&buffer);
		errorAndExit("Invalid sound cache call");
		return SOUND_NONE; //dummy
	}

	if ((format & SOUND_CHUNK) && !buffer[0] && !buffer[1] && !buffer[2] && !buffer[3]) {
		// "chunk" sound files would have a 4+ byte magic number,
		// so this could be a WAV with a bad (encrypted?) header;
		// will recreate the header from a ".fmt" file if one exists
		// assumes the first 128 bytes are bad (encrypted)
		// _and_ that the file contains uncompressed PCM data
		char *fmtname = new char[std::strlen(filename) + std::strlen(".fmt") + 1];
		std::sprintf(fmtname, "%s.fmt", filename);

		size_t fmtlen{0};
		uint8_t *fmtbuffer{nullptr};
		{
			Lock lock(&music_file_name);
			script_h.reader->getFile(fmtname, fmtlen, &fmtbuffer);
		}

		if (fmtlen >= 8) {
			// a file called filename + ".fmt" exists, of appropriate size;
			// read fmt info
			int channels, bits;
			unsigned long rate = 0, data_length = 0;

			channels = fmtbuffer[0];
			for (int i = 5; i > 1; i--) {
				rate = (rate << 8) + fmtbuffer[i];
			}
			bits = fmtbuffer[6];
			if (fmtlen >= 12) {
				// read the data_length
				for (int i = 11; i > 7; i--) {
					data_length = (data_length << 8) + fmtbuffer[i];
				}
			} else {
				// no data_length provided, fake it from the buffer length
				data_length = length - sizeof(WAVE_HEADER) - sizeof(WAVE_DATA_HEADER);
			}
			uint8_t fill = 0;
			if (bits == 8)
				fill = 128;
			for (size_t i = 0; (i < 128 && i < length); i++) {
				//clear the first 128 bytes (encryption noise)
				buffer[i] = fill;
			}
			if (fmtlen > 12) {
				setupWaveHeader(buffer, channels, bits, rate, data_length, fmtlen - 12, fmtbuffer + 12);
			} else {
				setupWaveHeader(buffer, channels, bits, rate, data_length);
			}
			// -------- ^^ ---------------------------------------------
			// local vars only, should be ok
			// ---------------------------------------------------------
			if ((bits == 8) && (fmtlen < 12)) {
				//hack: clear likely "pad bytes" at the end of the buffer
				//      (only on 8-bit samples when the fmt file doesn't
				//      include the data length)
				int i = 1;
				while (i < 5 && buffer[length - i] == 0) {
					buffer[length - i] = fill;
					i++;
				}
			}
		}
		freearr(&fmtbuffer);
		freearr(&fmtname);
	}

	if (format & SOUND_MUSIC) {
		int id3v2_size = 0;
		if (HAS_ID3V2_TAG(buffer)) {
			//found an ID3v2 tag, skipping since SMPEG doesn't
			for (int i = 0; i < 4; i++) {
				if (buffer[6 + i] & 0x80) { //err music_buffer brb, sorry {think of it}. See above ^
					id3v2_size = 0;
					break;
				}
				id3v2_size <<= 7;
				id3v2_size += buffer[6 + i];
			}
			if (id3v2_size > 0) {
				id3v2_size += 10;
				sendToLog(LogLevel::Info, "found ID3v2 tag in file '%s', size %d bytes\n", filename, id3v2_size);
			}
		}
		uint8_t *m_buf   = buffer + id3v2_size;
		const long m_len = length - id3v2_size;

		Mix_Music *music_info_local = Mix_LoadMUS_RW(SDL_RWFromMem(m_buf, static_cast<int>(m_len)), 0);

		if (music_info_local) {
			if (match_bgm_audio_flag) {
				// -------- ^ --------------------------------------------
				// r.o. except for parseOptions, should be fine
				// -------------------------------------------------------
				//check how well the music matches the current mixer spec
				Mix_MusicType mtype  = Mix_GetMusicType(music_info_local);
				SDL_AudioSpec wanted = audio_format;
				bool change_spec     = false;

				if (mtype == MUS_MP3) {
					SMPEG *mp3_chk = SMPEG_new_rwops(SDL_RWFromMem(m_buf, static_cast<int>(m_len)), nullptr, 0, 0);
					SMPEG_wantedSpec(mp3_chk, &wanted);
					SMPEG_delete(mp3_chk);
					if ((wanted.freq != audio_format.freq) ||
					    (wanted.format != audio_format.format))
						change_spec = true;
				}

				if (mtype == MUS_OGG) {
					OVInfo *ovi = new OVInfo();
					ovi->buf    = m_buf;
					ovi->length = m_len;
					ovi->pos    = 0;
					//annoying having to set callbacks just to check the specs...
					ov_callbacks oc;
					oc.read_func  = oc_read_func;
					oc.seek_func  = oc_seek_func;
					oc.close_func = nullptr;
					oc.tell_func  = oc_tell_func;
					if (ov_test_callbacks(ovi, &ovi->ovf, nullptr, 0, oc) >= 0) {
						vorbis_info *vi = ov_info(&ovi->ovf, -1);
						if (vi) {
							wanted.channels = vi->channels;
							wanted.freq     = static_cast<int>(vi->rate);
							wanted.format   = AUDIO_S16;
						}
						ov_clear(&ovi->ovf);
					}
					delete ovi;
				}

				if (mtype == MUS_WAV) {
					WAVE_HEADER *wav_hdr = reinterpret_cast<WAVE_HEADER *>(m_buf);
					wanted.freq          = wav_hdr->frequency[3];
					wanted.freq          = (wanted.freq << 8) + wav_hdr->frequency[2];
					wanted.freq          = (wanted.freq << 8) + wav_hdr->frequency[1];
					wanted.freq          = (wanted.freq << 8) + wav_hdr->frequency[0];
				}

				if (!change_spec && (wanted.freq != audio_format.freq)) {
					change_spec = true;
					//don't change for an ogg/wav w/frequency factor 2 or 4,
					//since SDL can convert it fine
					if ((wanted.freq * 2 == audio_format.freq) ||
					    (wanted.freq * 4 == audio_format.freq) ||
					    (audio_format.freq * 2 == wanted.freq) ||
					    (audio_format.freq * 4 == wanted.freq))
						change_spec = false;
				}
				if (change_spec) {
					//audio spec doesn't match well enough, reset the mixer
					//(and also free & reload the music_info)
					Mix_FreeMusic(music_info_local);
					Mix_CloseAudio();
					// resetting the mixer will stop all current sounds,
					// and a new spec can mess with preloaded chunks --
					// need to check for preloads, and either free them
					// or change their audiocvt settings FIXME
					//(at least until we get some decent audioconverters...)
					openAudio(wanted);
					if (!audio_open_flag) {
						// didn't work, use the old settings
						openAudio(default_audio_format);
					}
					// ^ openAudio() writes to globals, and audio_open_flag is also global, but, nothing in event uses either
					// and we won't be processing commands or anything as long as we are in this method

					// -----------------------------------------------------------------------------------
					// !!!! ^^^^^^ old problematic comment spotted ^^^^^ !!!!
					// -----------------------------------------------------------------------------------

					music_info_local = Mix_LoadMUS_RW(SDL_RWFromMem(buffer + id3v2_size, static_cast<int>(length - id3v2_size)), 0);
				}
			}

			setMusicVolume(music_volume, volume_on_flag);
			Mix_HookMusicFinished(musicFinishCallback);
			if (Mix_PlayMusic(music_info_local, music_play_loop_flag ? -1 : 0) == 0) {
				Lock lock(&playSoundThreadedLock);
				assert(!music_buffer);
				music_info          = music_info_local;
				music_buffer        = buffer;
				music_buffer_length = length;
				return SOUND_MUSIC;
			}
		} else {
			char errBuf[MAX_ERRBUF_LEN];
			std::snprintf(errBuf, MAX_ERRBUF_LEN, "error playing music '%s': %s\n", filename, Mix_GetError());
			freearr(&buffer);
			errorAndExit(errBuf);
			return SOUND_NONE; //dummy
		}
		Lock lock(&playSoundThreadedLock);
		music_info = music_info_local;
	}

	if (format & SOUND_CHUNK) {
		Mix_Chunk *chunk = Mix_LoadWAV_RW(SDL_RWFromMem(buffer, static_cast<int>(length)), 1);
		if (!chunk) {
			char errBuf[MAX_ERRBUF_LEN];
			std::snprintf(errBuf, MAX_ERRBUF_LEN, "error playing sound '%s': %s\n", filename, Mix_GetError());
			freearr(&buffer);
			errorAndExit(errBuf);
			return SOUND_NONE; //dummy
		} else if (channel == MIX_CACHE_CHANNEL_BLOCK || channel == MIX_CACHE_CHANNEL_ASYNC) {
			// We are here to cache our Mix_Chunk and nothing else
			bool async{channel == MIX_CACHE_CHANNEL_ASYNC};
			assert(!pending_cache_chunk[async]);
			pending_cache_chunk[async] = std::make_shared<Wrapped_Mix_Chunk>(chunk);
			freearr(&buffer);
			return SOUND_CHUNK; //doesn't matter what to return
		} else {
			// may deadlock here onexit
			Lock lock(&playSoundThreadedLock);
			if (playWave(std::make_shared<Wrapped_Mix_Chunk>(chunk), format, loop_flag, channel) == 0) {
				freearr(&buffer);
				return SOUND_CHUNK;
			}
		}
	}

	if (format & SOUND_SEQMUSIC) {
		FILE *fp;
		{
			Lock lock(&music_file_name);
			fp = FileIO::openFile(TMP_MUSIC_FILE, "wb", script_h.getSavePath(TMP_MUSIC_FILE));
		}
		if (fp == nullptr) {
			char errBuf[MAX_ERRBUF_LEN];
			std::snprintf(errBuf, MAX_ERRBUF_LEN,
			              "can't open temporary music file %s", TMP_MUSIC_FILE);
			errorAndExit(errBuf);
		} else {
			{
				Lock lock(&music_file_name);
				if (std::fwrite(buffer, 1, length, fp) != length) {
					char errBuf[MAX_ERRBUF_LEN];
					std::snprintf(errBuf, MAX_ERRBUF_LEN,
					              "can't write to temporary music file %s",
					              TMP_MUSIC_FILE);
					errorAndExit(errBuf);
				}
				std::fclose(fp);
			}
			Lock lock(&playSoundThreadedLock);
			ext_music_play_once_flag = !loop_flag;
			if (playSequencedMusic(loop_flag) == 0) {
				freearr(&buffer);
				return SOUND_SEQMUSIC;
			}
		}
	}

	freearr(&buffer);
	return SOUND_OTHER;
}

void ONScripter::playCDAudio() {
	if (!audio_open_flag)
		return;

	//Search the "cd" subfolder
	//for a file named "track01.mp3" or similar, depending on the
	//track number; check for mp3, ogg and wav files
	char filename[256];
	std::sprintf(filename, R"(cd\track%2.2d.mp3)", current_cd_track);
	int ret = playSoundThreaded(filename, SOUND_MUSIC, cd_play_loop_flag);
	if (ret == SOUND_MUSIC)
		return;

	std::sprintf(filename, R"(cd\track%2.2d.ogg)", current_cd_track);
	ret = playSoundThreaded(filename, SOUND_MUSIC, cd_play_loop_flag);
	if (ret == SOUND_MUSIC)
		return;

	std::sprintf(filename, R"(cd\track%2.2d.wav)", current_cd_track);
	playSoundThreaded(filename, SOUND_MUSIC, cd_play_loop_flag, MIX_BGM_CHANNEL);
}

int ONScripter::playWave(const std::shared_ptr<Wrapped_Mix_Chunk> &chunk, int format, bool loop_flag, int channel) {
	Mix_Pause(channel);
	wave_sample[channel] = chunk;

	if (!chunk)
		return -1;

	if (channel < ONS_MIX_CHANNELS)
		setVolume(channel, channelvolumes[channel], volume_on_flag);
	else if (channel == MIX_BGM_CHANNEL)
		setVolume(MIX_BGM_CHANNEL, music_volume, volume_on_flag);
	else
		setVolume(channel, se_volume, volume_on_flag);

	if (lipsChannels[channel].has())
		loadLips(channel);

	if (!(format & SOUND_PRELOAD)) {
		if (lipsChannels[channel].has()) {
			LipsAnimationAction *lipsAction = LipsAnimationAction::create();
			lipsAction->channel             = channel;
			{
				Lock lock(&ons.registeredCRActions);
				registeredCRActions.emplace_back(lipsAction);
			}
			Mix_PlayChannel(channel, wave_sample[channel]->chunk, loop_flag ? -1 : 0);
			lipsChannels[channel].get().lipsData.speechStart = SDL_GetTicks();
		} else {
			Mix_PlayChannel(channel, wave_sample[channel]->chunk, loop_flag ? -1 : 0);
		}
	}

	return 0;
}

int ONScripter::playSequencedMusic(bool loop_flag) {
	Mix_SetMusicCMD(seqmusic_cmd);

	char seqmusic_filename[256];
	std::sprintf(seqmusic_filename, "%s%s", script_h.save_path, TMP_MUSIC_FILE);
	{
		Lock lock(&music_file_name); //general sound i/o lock
		seqmusic_info = Mix_LoadMUS(seqmusic_filename);
	}
	if (seqmusic_info == nullptr) {
		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "error in sequenced music file %s", seqmusic_filename);
		errorAndCont(script_h.errbuf, Mix_GetError());
		return -1;
	}

	int seqmusic_looping = loop_flag ? -1 : 0;

#ifdef LINUX
	signal(SIGCHLD, seqmusicCallback);
	if (seqmusic_cmd)
		seqmusic_looping = 0;
#endif
	setMusicVolume(music_volume, volume_on_flag);
	Mix_PlayMusic(seqmusic_info, seqmusic_looping);
	current_cd_track = -2;

	return 0;
}

int ONScripter::playingMusic() {
	return (audio_open_flag && (Mix_GetMusicHookData() || Mix_Playing(MIX_BGM_CHANNEL) == 1 || Mix_PlayingMusic() == 1)) ? 1 : 0;
}

int ONScripter::setCurMusicVolume(int volume) {
	if (!audio_open_flag)
		return 0;

	if (bgmdownmode_flag && music_info && wave_sample[0] && Mix_Playing(0))
		volume /= 2;
	if (Mix_Playing(MIX_BGM_CHANNEL) == 1) // wave/ogg (unstreamed)
		setVolume(MIX_BGM_CHANNEL, volume, volume_on_flag);
	else if (Mix_PlayingMusic() == 1) // mp3,ogg,midi,wave
		//FIXME: Can anybody tell me why this does not update music_volume?
		setMusicVolume(volume, volume_on_flag);

	return 0;
}

int ONScripter::setVolumeMute(bool do_mute) {
	if (!audio_open_flag)
		return 0;

	std::string title = wm_title_string;
	if (do_mute)
		title.insert(0, "[Sound: Off] ");
	window.setTitle(title.c_str());

	int music_vol = music_volume;
	if (bgmdownmode_flag && music_info && wave_sample[0] && Mix_Playing(0))
		music_vol /= 2;

	if (Mix_Playing(MIX_BGM_CHANNEL) == 1) // wave
		setVolume(MIX_BGM_CHANNEL, music_vol, !do_mute);
	else if (Mix_PlayingMusic() == 1) // mp3,ogg,midi
		setMusicVolume(music_vol, !do_mute);

	for (uint32_t i = 1; i < ONS_MIX_CHANNELS; i++)
		setVolume(i, channelvolumes[i], !do_mute);

	setVolume(MIX_LOOPBGM_CHANNEL0, se_volume, !do_mute);
	setVolume(MIX_LOOPBGM_CHANNEL1, se_volume, !do_mute);
	setVolume(MIX_VIDEO_CHANNEL, video_volume, !do_mute);

	return 0;
}

void ONScripter::stopBGM(bool continue_flag) {
	if (wave_sample[MIX_BGM_CHANNEL]) {
		Mix_Pause(MIX_BGM_CHANNEL);
		wave_sample[MIX_BGM_CHANNEL] = nullptr;
	}

	if (music_info) {
		ext_music_play_once_flag = true;
		// Mix_HaltMusic in SDL2_mixer calls musicFinishCallback in the end, we want to avoid that
		// SDL12 backported that change in http://hg.libsdl.org/SDL_mixer/rev/a4e9c53d9c30
		// We use a prior rev atm but I add surrounding calls for both versions just in case.
		Mix_HookMusicFinished(nullptr);
		Mix_HaltMusic();
		Mix_HookMusicFinished(musicFinishCallback);
		Lock lock(&playSoundThreadedLock);
		Mix_FreeMusic(music_info);
		music_info = nullptr;
	}

	if (seqmusic_info) {
		ext_music_play_once_flag = true;
		// Mix_HaltMusic in SDL2_mixer calls musicFinishCallback in the end, we want to avoid that
		// SDL12 backported that change in http://hg.libsdl.org/SDL_mixer/rev/a4e9c53d9c30
		// We use a prior rev atm but I add surrounding calls for both versions just in case.
		Mix_HookMusicFinished(nullptr);
		Mix_HaltMusic();
		Mix_HookMusicFinished(musicFinishCallback);
		Mix_FreeMusic(seqmusic_info);
		seqmusic_info = nullptr;
	}

	if (!continue_flag) {
		script_h.setStr(&music_file_name, nullptr);
		music_play_loop_flag = false;
		if (initialised()) {
			Lock lock(&playSoundThreadedLock);
			if (music_buffer) {
				delete[] music_buffer;
				music_buffer = nullptr;
			}
		}

		script_h.setStr(&seqmusic_file_name, nullptr);
		seqmusic_play_loop_flag = false;

		current_cd_track = -1;
	}
}

void ONScripter::stopDWAVE(int channel) {
	if (!audio_open_flag)
		return;

	//avoid stopping dwave outside array
	if (channel < 0)
		channel = 0;
	else if (channel >= ONS_MIX_CHANNELS)
		channel = ONS_MIX_CHANNELS - 1;

	if (wave_sample[channel]) {
		Mix_Pause(channel);
		if (!channel_preloaded[channel]) {
			//don't free preloaded channels
			wave_sample[channel]       = nullptr;
			channel_preloaded[channel] = false;
		}
	}
	if ((channel == 0) && bgmdownmode_flag)
		setCurMusicVolume(music_volume);
}

void ONScripter::stopAllDWAVE() {
	if (!audio_open_flag)
		return;

	for (int ch = 0; ch < ONS_MIX_CHANNELS; ch++) {
		if (wave_sample[ch]) {
			Mix_Pause(ch);
			if (!channel_preloaded[ch]) {
				wave_sample[ch] = nullptr;
			}
		}
	}
	// just in case the bgm was turned down for the voice channel,
	// set the bgm volume back to normal
	if (bgmdownmode_flag)
		setCurMusicVolume(music_volume);
}

void ONScripter::playClickVoice() {
	if (clickstr_state == CLICK_NEWPAGE) {
		if (clickvoice_file_name[CLICKVOICE_NEWPAGE])
			playSoundThreaded(clickvoice_file_name[CLICKVOICE_NEWPAGE],
			                  SOUND_CHUNK, false, MIX_WAVE_CHANNEL);
	} else if (clickstr_state == CLICK_WAIT) {
		if (clickvoice_file_name[CLICKVOICE_NORMAL])
			playSoundThreaded(clickvoice_file_name[CLICKVOICE_NORMAL],
			                  SOUND_CHUNK, false, MIX_WAVE_CHANNEL);
	}
}

void ONScripter::startLvPlayback() {
	script_h.logState.currVoiceSet++;

	auto &data = script_h.logState.dialogueData[script_h.logState.currVoiceDialogueLabelIndex];

	if (data.voices.size() <= static_cast<size_t>(script_h.logState.currVoiceSet)) {
		stopLvPlayback();
		return;
	}

	auto &set   = data.voices[script_h.logState.currVoiceSet];
	auto vol    = data.volume * script_h.logState.currVoiceVolume / 100;
	int last_ch = -1;
	for (auto &voice : set) {
		stopDWAVE(voice.first);
		channel_preloaded[voice.first] = true;
		playSoundThreaded(voice.second.c_str(), SOUND_CHUNK | SOUND_PRELOAD, false, voice.first, false);
		setVolume(voice.first, vol, volume_on_flag);
		last_ch = voice.first;
	}

	auto action          = QueuedSoundAction::create();
	action->ch           = last_ch;
	action->func         = []() { ons.startLvPlayback(); };
	action->soundDelayMs = ignore_voicedelay ? 0 : voicedelay_time;

	{
		Lock lock(&ons.registeredCRActions);
		registeredCRActions.emplace_back(action);
	}

	for (auto &voice : set) {
		if (wave_sample[voice.first] == nullptr)
			errorAndExit("Cannot play a not loaded channel");
		Mix_PlayChannel(voice.first, wave_sample[voice.first]->chunk, 0);
	}
}

void ONScripter::stopLvPlayback() {
	script_h.logState.currVoiceSet    = -1;
	script_h.logState.currVoiceVolume = 100;
	for (auto &act : getConstantRefreshActions()) {
		if (dynamic_cast<QueuedSoundAction *>(act.get())) {
			act->terminate();
		}
	}

	for (auto &set : script_h.logState.dialogueData[script_h.logState.currVoiceDialogueLabelIndex].voices) {
		for (auto &voice : set) {
			stopDWAVE(voice.first);
		}
	}
	script_h.logState.currVoiceDialogueLabelIndex = -1;
}

void setupWaveHeader(uint8_t *buffer, int channels, int bits,
                     unsigned long rate, size_t data_length,
                     size_t extra_bytes, uint8_t *extra_ptr) {
	WAVE_HEADER header;
	WAVE_DATA_HEADER data_header;
	std::memcpy(header.chunk_riff, "RIFF", 4);
	unsigned long riff_length = sizeof(WAVE_HEADER) + sizeof(WAVE_DATA_HEADER) +
	                            data_length + extra_bytes - 8;
	header.riff_length[0] = riff_length & 0xff;
	header.riff_length[1] = (riff_length >> 8) & 0xff;
	header.riff_length[2] = (riff_length >> 16) & 0xff;
	header.riff_length[3] = (riff_length >> 24) & 0xff;
	std::memcpy(header.fmt_id, "WAVEfmt ", 8);
	header.fmt_size[0] = 0x10 + extra_bytes;
	header.fmt_size[1] = header.fmt_size[2] = header.fmt_size[3] = 0;
	header.data_fmt[0]                                           = 1;
	header.data_fmt[1]                                           = 0; // PCM format
	header.channels[0]                                           = channels;
	header.channels[1]                                           = 0;
	header.frequency[0]                                          = rate & 0xff;
	header.frequency[1]                                          = (rate >> 8) & 0xff;
	header.frequency[2]                                          = (rate >> 16) & 0xff;
	header.frequency[3]                                          = (rate >> 24) & 0xff;

	int sample_byte_size       = channels * bits / 8;
	unsigned long byte_size    = sample_byte_size * rate;
	header.byte_size[0]        = byte_size & 0xff;
	header.byte_size[1]        = (byte_size >> 8) & 0xff;
	header.byte_size[2]        = (byte_size >> 16) & 0xff;
	header.byte_size[3]        = (byte_size >> 24) & 0xff;
	header.sample_byte_size[0] = sample_byte_size;
	header.sample_byte_size[1] = 0;
	header.sample_bit_size[0]  = bits;
	header.sample_bit_size[1]  = 0;

	std::memcpy(data_header.chunk_id, "data", 4);
	data_header.data_length[0] = static_cast<char>(data_length & 0xff);
	data_header.data_length[1] = static_cast<char>((data_length >> 8) & 0xff);
	data_header.data_length[2] = static_cast<char>((data_length >> 16) & 0xff);
	data_header.data_length[3] = static_cast<char>((data_length >> 24) & 0xff);

	std::memcpy(buffer, &header, sizeof(header));
	if (extra_bytes > 0) {
		if (extra_ptr != nullptr)
			std::memcpy(buffer + sizeof(header), extra_ptr, extra_bytes);
		else
			std::memset(buffer + sizeof(header), 0, extra_bytes);
	}
	std::memcpy(buffer + sizeof(header) + extra_bytes, &data_header, sizeof(data_header));
}
