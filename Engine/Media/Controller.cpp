/**
 *  Controller.cpp
 *  ONScripter-RU
 *
 *  Contains A/V controller interface.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Media/Controller.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"

#include <SDL2/SDL.h>

#include <stdexcept>
#include <cassert>

MediaProcController media;

int MediaProcController::ownInit() {
	av_log_set_level(AV_LOG_QUIET);
	av_log_set_callback(logLine);
	HardwareDecoderIFace::reg();
	audioSpec = AudioSpec();
	int error     = audioSpec.init(ons.audio_format);

	return error;
}

int MediaProcController::ownDeinit() {
	resetState();
	return 0;
}

MediaProcController::MediaFrame::~MediaFrame() {
	if (surface) {
		if (media.imagePool) {
			media.imagePool->giveImage(surface);
		} else {
			SDL_FreeSurface(surface);
		}
	}

	dataDeleter(data);
	for (auto &p : planes) freearr(&p);

	planesCnt = 0;
	srcFormat = AV_PIX_FMT_NONE;
}

int MediaProcController::AudioSpec::init(const SDL_AudioSpec &spec) {
	// Grab the format
	switch (spec.format) {
		case AUDIO_U8:
			format = AV_SAMPLE_FMT_U8;
			break;
		case AUDIO_S16:
			format = AV_SAMPLE_FMT_S16;
			break;
		case AUDIO_S32:
			format = AV_SAMPLE_FMT_S32;
			break;
		case AUDIO_F32:
			format = AV_SAMPLE_FMT_FLT;
			break;
		default:
			sendToLog(LogLevel::Error, "Unsupported output audio format\n");
			return -1;
	}

	// Grab the channels
	channelLayout = av_get_default_channel_layout(spec.channels);
	channels      = spec.channels;

	// Grab the frequency
	frequency = spec.freq;

	return 0;
}

int MediaProcController::lockManager(void **mutex, int op) {
	switch (op) {
		case 0:
			*mutex = SDL_CreateMutex();
			if (!*mutex)
				return 1;
			return 0;
		case 1:
			return SDL_LockMutex(static_cast<SDL_mutex *>(*mutex)) != 0;
		case 2:
			return SDL_UnlockMutex(static_cast<SDL_mutex *>(*mutex)) != 0;
		case 3:
			SDL_DestroyMutex(static_cast<SDL_mutex *>(*mutex));
			return 0;
	}
	return 1;
}

void MediaProcController::logLine(void *inst, int level, const char *fmt, va_list args) {
	// We seem to get called regardless of log level
	if (level > AV_LOG_ERROR)
		return;

	char msg[1024];
	std::vsnprintf(msg, sizeof(msg), fmt, args);

	LogLevel iolevel;

	switch (level) {
		case AV_LOG_PANIC:
		case AV_LOG_FATAL:
		case AV_LOG_ERROR:
			iolevel = LogLevel::Error;
			break;
		case AV_LOG_WARNING:
			iolevel = LogLevel::Warn;
			break;
		default:
			iolevel = LogLevel::Info;
			break;
	}

	sendToLog(iolevel, "[ff %d/0x%x] %s", level, inst, msg);
}

std::unique_ptr<MediaProcController::Decoder> MediaProcController::findDecoder(AVMediaType type, unsigned streamNumber, AVCodecID restrictCodecId) {
	unsigned stream = 0;

	while (streamNumber && stream < formatContext->nb_streams) {
		if (formatContext->streams[stream]->codecpar->codec_type == type &&
		    (restrictCodecId == AV_CODEC_ID_NONE || restrictCodecId == formatContext->streams[stream]->codecpar->codec_id)) {
			if (streamNumber == 1) {
				auto codecContext = avcodec_alloc_context3(nullptr);
				if (!codecContext || avcodec_parameters_to_context(codecContext, formatContext->streams[stream]->codecpar) < 0)
					throw std::runtime_error("Failed to create AVCodecContext");
				switch (type) {
					case AVMEDIA_TYPE_VIDEO:
						// Starting with 6f69f7a8bf6a0d013985578df2ef42ee6b1c7994 ffmpeg no longer sets decoding thread count to auto.
						// Specify it ourselves.
						codecContext->thread_count = 0;
						return Decoder::create<VideoDecoder>(codecContext, stream);
					case AVMEDIA_TYPE_AUDIO:
						return Decoder::create<AudioDecoder>(codecContext, stream);
					case AVMEDIA_TYPE_SUBTITLE:
						return Decoder::create<SubtitleDecoder>(codecContext, stream);
					default:
						throw std::runtime_error("Unsupported AVMediaType");
				}
			} else {
				streamNumber--;
			}
		}
		stream++;
	}

	return {};
}

bool MediaProcController::loadVideo(const char *filename, unsigned audioStream, unsigned subtitleStream) {
	if (!filename) {
		return false;
	}

	int err = avformat_open_input(&formatContext, filename, nullptr, nullptr);

	if (err < 0) {
		//errorAndCont("ffmpeg: Unable to open input file %s.",video_file);
		formatContext = nullptr;
		return false;
	}

	err = avformat_find_stream_info(formatContext, nullptr);
	if (err < 0) {
		//errorAndCont("ffmpeg: Unable to find stream info.");
		resetState();
		return false;
	}

	decoders[VideoEntry] = findDecoder(AVMEDIA_TYPE_VIDEO);
	decoders[AudioEntry] = findDecoder(AVMEDIA_TYPE_AUDIO, audioStream);
	decoders[SubsEntry]  = findDecoder(AVMEDIA_TYPE_SUBTITLE, subtitleStream, AV_CODEC_ID_SSA);
	if (!hasStream(VideoEntry)) {
		sendToLog(LogLevel::Error, "WAHHAHAHAHHAHAHH (erika noises) exiting");
		resetState();
		return false;
	}
	frameQueueSem[VideoEntry] = SDL_CreateSemaphore(VideoPacketBufferSize);
	frameQueueSem[AudioEntry] = SDL_CreateSemaphore(AudioPacketBufferSize);

	frameQueuemutex[VideoEntry] = SDL_CreateMutex();
	frameQueuemutex[AudioEntry] = SDL_CreateMutex();
	sendToLog(LogLevel::Info, "WAHHAHAHAHHAHAHH (erika noises) (she is happy)");

	subtitleMutex = SDL_CreateMutex();
	return !hasStream(AudioEntry) || static_cast<AudioDecoder *>(decoders[AudioEntry].get())->initSwrContext(audioSpec);
}

bool MediaProcController::loadPresentation(const GPU_Rect &rect, bool loop) {
	loopVideo = loop;

	int nworkers = 0;
	for (auto i : {VideoEntry, AudioEntry})
		if (decoders[i])
			nworkers++;
	decoderWorkerCount.store(nworkers, std::memory_order_relaxed);

	demux = std::make_unique<MediaDemux>();
	demux->prepare(decoders[VideoEntry] ? decoders[VideoEntry]->stream : MediaDemux::InvalidStream,
	               decoders[AudioEntry] ? decoders[AudioEntry]->stream : MediaDemux::InvalidStream,
	               decoders[SubsEntry] ? decoders[SubsEntry]->stream : MediaDemux::InvalidStream);

	/* Prepare SW scale */
	auto vdec = static_cast<VideoDecoder *>(decoders[VideoEntry].get());
	if (vdec->initSwsContext(rect.w, alphaMasked ? rect.h * 2 : rect.h, nullptr, false)) {

		/* Allocate surfaces */
		imagePool         = std::make_unique<TempImagePool>();
		imagePool->size.x = rect.w;
		imagePool->size.y = alphaMasked ? rect.h * 2 : rect.h;
		imagePool->addImages(VideoPacketBufferSize);

		initVideoTimecodesLock = SDL_CreateSemaphore(0);
		async.loadPacketArrays();

		/* Get timing info */
		vdec->initTiming(formatContext->duration);

		return true;
	}

	return false;
}

bool MediaProcController::addSubtitles(const char *filename, int frameWidth, int frameHeight) {
	// No subtitles are supported in case of alpha-masked video
	if (alphaMasked) {
		if (hasStream(SubsEntry)) {
			decoders[SubsEntry].reset();
		} else if (!filename) {
			return true;
		}

		sendToLog(LogLevel::Error, "Cannot use subtitles on alphamasked videos\n");
		return false;
	}

	if (filename && decoders[SubsEntry]) {
		sendToLog(LogLevel::Error, "Subtitles had already been loaded\n");
		filename = nullptr;
	} else if (filename) {
		decoders[SubsEntry] = Decoder::create<SubtitleDecoder>();
	} else {
		return true;
	}

	return static_cast<SubtitleDecoder *>(decoders[SubsEntry].get())->prepare(filename, frameWidth, frameHeight);
}

void MediaProcController::frameSize(const SDL_Rect &rect, int &width, float &wFactor, int &height, float &hFactor, bool alpha) {
	alphaMasked = alpha;

	auto context = decoders[VideoEntry]->codecContext;

	if (context->width < rect.w) {
		width   = context->width;
		wFactor = rect.w / static_cast<float>(context->width);
	} else {
		width   = rect.w;
		wFactor = 1;
	}

	if (context->height < rect.h) {
		if (!alphaMasked) {
			height  = context->height;
			hFactor = rect.h / static_cast<float>(context->height);
		} else if (context->height / 2 < rect.h) {
			height  = context->height / 2;
			hFactor = 2.0f * rect.h / context->height;
		}
	} else {
		height  = rect.h;
		hFactor = 1;
	}

	//sendToLog(LogLevel::Info, "Frame size is %dx%d, out frame size is %dx%d\n", context->width, context->height, width, height);
}

void MediaProcController::startProcessing() {
	async.loadVideoFrames();
	if (hasStream(AudioEntry))
		async.loadAudioFrames();
}

bool MediaProcController::finish(bool needLastFrame) {
	//TODO: implement skip to last frame

	int value = decoderWorkerCount.load(std::memory_order_acquire);

	if (value > 0) { // Decoders are not done
		for (auto &decoder : decoders) {
			if (decoder)
				decoder->shouldFinish.store(true, std::memory_order_relaxed);
		}
		std::atomic_thread_fence(std::memory_order_release);

		// Let them finish
		SDL_Delay(1);

		value = decoderWorkerCount.load(std::memory_order_acquire);
	}

	// Decoders are done?
	if (value == 0) {
		// Signal demux
		if (demux)
			demux->shouldFinish.store(true, std::memory_order_release);

		resetDecoders();

		if (needLastFrame) {
			resetFrameQueues(0, 1);
		} else {
			resetFrameQueues(1, 0);
		}

		// Demux is done?
		if (demux && demux->demuxComplete.load(std::memory_order_acquire)) {
			resetDemuxer();
		}

		if (!demux) {
			return true;
		}
	}

	return false;
}

void MediaProcController::resetDecoders() {
	// Destroy decoders
	SDL_mutexP(subtitleMutex);
	for (auto &decoder : decoders) {
		if (decoder)
			decoder.reset();
	}
	SDL_mutexV(subtitleMutex);

	for (auto &mutex : frameQueuemutex) {
		if (mutex) {
			SDL_DestroyMutex(mutex);
			mutex = nullptr;
		}
	}

	for (auto semArr : {&frameQueueSem}) {
		for (auto &sem : *semArr) {
			if (sem) {
				SDL_DestroySemaphore(sem);
				sem = nullptr;
			}
		}
	}
}

void MediaProcController::resetDemuxer() {
	if (demux) {
		demux->resetPacketQueue();
		demux.reset();
	}
}

void MediaProcController::resetFrameQueues(int vidStart, int vidEnd) {
	// Cleanup the queues
	auto &vidQueue = async.loadFramesQueue[VideoEntry].results;
	if (!vidQueue.empty()) {
		std::for_each(std::begin(vidQueue) + vidStart, std::end(vidQueue) - vidEnd, [](void *&elem) {
			delete static_cast<MediaFrame *>(elem);
		});
		vidQueue.erase(std::begin(vidQueue) + vidStart, std::end(vidQueue) - vidEnd); /* Leave last available frame */
	}

	for (auto &elem : async.loadFramesQueue[AudioEntry].results) {
		delete static_cast<MediaFrame *>(elem);
	}
	async.loadFramesQueue[AudioEntry].results.clear();

	// Note that SubsEntry queue is not ours
}

void MediaProcController::resetState() {
	resetDecoders();
	resetFrameQueues();
	resetDemuxer();

	if (subtitleMutex) {
		SDL_DestroyMutex(subtitleMutex);
		subtitleMutex = nullptr;
	}

	// This is probably no longer needed
	/*if (demux) demux->resetSpacesSem();
	while (SDL_SemValue(frameQueueSem[VideoEntry]) != VideoPacketBufferSize) SDL_SemPost(frameQueueSem[VideoEntry]);
	while (SDL_SemValue(frameQueueSem[AudioEntry]) != AudioPacketBufferSize) SDL_SemPost(frameQueueSem[AudioEntry]);
	if (demux) demux->resetDataSem();*/

	imagePool.reset();

	if (formatContext)
		avformat_close_input(&formatContext);
}

void MediaProcController::decodeFrames(MediaEntries entry) {
	if (hasStream(entry)) {
		decoders[entry]->decodeFrame(entry);
		media.decoderWorkerCount.fetch_sub(1, std::memory_order_release);
	}
}

void MediaProcController::demultiplexStreams() {
	demux->demultiplexStreams(av_q2d(formatContext->streams[VideoEntry]->time_base));
	demux->demuxComplete.store(true, std::memory_order_release);
}

void MediaProcController::getVideoTimecodes(size_t &counter, AVPacket *packet, long double videoTimeBase) {
	static int64_t initialValue = 0;
	/* For future framerate detection */
	// pts values are normally disordered, this may cause some issues, like isVFR = 1 or isCorrupted = 1
	if (counter < VideoPacketBufferSize && !(packet->flags & AV_PKT_FLAG_CORRUPT)) {
		if (packet->stream_index == decoders[VideoEntry]->stream) {
			if (packet->pts == AV_NOPTS_VALUE) {
				if (counter == 0)
					initialValue = 0;
				initVideoTimecodes[counter] = 0;
			} else {
				if (counter == 0)
					initialValue = packet->pts;
				initVideoTimecodes[counter] = videoTimeBase * (packet->pts - initialValue);
			}
			counter++;
		} else if (hasStream(AudioEntry) && packet->stream_index == decoders[AudioEntry]->stream &&
		           demux->packetQueueSpacesAvailable(AudioEntry)) {
			for (; counter < VideoPacketBufferSize; counter++) initVideoTimecodes[counter] = 0;
		}
		if (counter == VideoPacketBufferSize && initVideoTimecodesLock) {
			SDL_SemPost(initVideoTimecodesLock);
		}
	}
}

void MediaProcController::processSubsData(char *data, size_t length) {
	SDL_mutexP(subtitleMutex);
	if (hasStream(SubsEntry)) {
		static_cast<SubtitleDecoder *>(media.decoders[SubsEntry].get())->processData(data, length);
	}
	SDL_mutexV(subtitleMutex);
}

void MediaProcController::applySubtitles(MediaFrame &frame) {
	SDL_mutexP(subtitleMutex);
	if (hasStream(SubsEntry)) {
		static_cast<SubtitleDecoder *>(decoders[SubsEntry].get())->processFrame(frame);
	}
	SDL_mutexV(subtitleMutex);
}

// Decoder stuff

int MediaProcController::Decoder::decodeFrameFromPacket(bool &frameFinished, AVPacket *packet) {
	frameFinished = false;

	// According to ffmpeg sources we must be dropping the last packet instead of reading it.
	// Otherwise we will get into EOF loop.
	if (media.loopVideo && packet->data == nullptr && packet->size == 0) {
		avcodec_flush_buffers(codecContext);
		return 0;
	}

	int err = avcodec_send_packet(codecContext, packet);
	if (err < 0 && err != AVERROR_EOF)
		return 0;
	err = avcodec_receive_frame(codecContext, frame);
	if (err < 0 && err != AVERROR(EAGAIN) && err != AVERROR_EOF)
		return 0;

	frameFinished = true;
	return frame->pkt_size;
}

void MediaProcController::Decoder::decodeFrame(MediaEntries index) {

	CacheRead crMode{CacheRead::None};
	cmp::unique_ptr_del<AVPacket> packet(nullptr, MediaDemux::freePacket);

	do {
		if (packet == nullptr && crMode == CacheRead::None) {
			while (media.demux->waitForData(index, 10)) {
				if (async.threadShutdownRequested || shouldFinish.load(std::memory_order_acquire))
					return;
			}
		}

		if (index != VideoEntry && shouldFinish.load(std::memory_order_acquire)) /* Required to finish */
			return;

		SDL_mutexP(media.frameQueuemutex[index]);
		SDL_AtomicLock(&async.loadFramesQueue[index].resultsLock);
		// RETURN if the queue has items and the last item is either nullptr or the last frame
		bool mustReturn = !async.loadFramesQueue[index].results.empty() && !async.loadFramesQueue[index].results.back();

		if (index == VideoEntry && shouldFinish.load(std::memory_order_acquire)) /* If we requested VideoDecoder to exit, do so if we have at least one frame */
			mustReturn |= async.loadFramesQueue[index].results.size() > 1;

		SDL_AtomicUnlock(&async.loadFramesQueue[index].resultsLock);
		{
			if (async.threadShutdownRequested || mustReturn) {
				SDL_mutexV(media.frameQueuemutex[index]);
				break;
			}
		}

		while (packet == nullptr && crMode == CacheRead::None) {
			// Obtain a packet
			bool cacheReadStarted = false;
			packet                = cmp::unique_ptr_del<AVPacket>(media.demux->obtainPacket(index, cacheReadStarted), MediaDemux::freePacket);
			if (cacheReadStarted)
				crMode = CacheRead::Started; /* WARN: it's done after packet queue lock is unlocked */

			// If that failed, wait for some time
			if (packet == nullptr) {
				SDL_Delay(3);
			}
		}

		//sendToLog(LogLevel::Info, "packet->dts %lld packet->pts %lld\n",packet->dts, packet->pts);

		auto vf = std::make_unique<MediaFrame>();
		AVPacket temp_packet{};
		av_packet_ref(&temp_packet, packet.get());

		bool frame_finished{false};

		do {
			int decode_size = decodeFrameFromPacket(frame_finished, packet.get());

			if (decode_size < 0 || (crMode != CacheRead::None && !frame_finished)) {
				// This is a useless packet, it was either buffered or read corrupted
				if (crMode == CacheRead::Finished && packet->size == 0) {
					// flushing is done, time to exit
					vf.reset();
				} else if (crMode != CacheRead::None) {
					// prepare for a flush in case of a 'last' packet
					packet->size = 0;
					packet->data = nullptr;
					crMode       = CacheRead::Finished;
				} else {
					// ffplay doesn't seem to call av_free_packet in this case, just destruct it
					packet.reset();
				}
				break;
			}

			// avcodec_decode_video2 could have succeeded or failed (mainly in the end of a flush)
			// nevertheless one packet may technically have more than a one frame, we ignore this case
			if (frame_finished) {
				processFrame(*vf);

				if (crMode != CacheRead::None) {
					// prepare for a flush this is a 'last' packet
					packet->size = 0;
					packet->data = nullptr;
					crMode       = CacheRead::Finished;
				}

				if (packet->size != 0) {
					packet->data += decode_size;
					packet->size -= decode_size;
				}

				break;
			}

			av_frame_unref(frame);

			// We don't modify our packet while flushing, which sets packet->size to 0
			if (packet->size != 0) {
				packet->data += decode_size;
				packet->size -= decode_size;
			}

		} while (packet->size > 0 && crMode == CacheRead::None);

		// Clean the packet while not flushing and after flushing
		if ((crMode == CacheRead::None || !vf) && packet) {
			av_packet_unref(&temp_packet);
			packet.reset();
		}

		SDL_mutexV(media.frameQueuemutex[index]);

		if (!vf || vf->has()) {
			bool exiting = false;
			while (SDL_SemWaitTimeout(media.frameQueueSem[index], 10)) {
				if (async.threadShutdownRequested || shouldFinish.load(std::memory_order_acquire)) {
					exiting = true;
					break;
				}
			}
			if (exiting)
				break;

			if (vf && vf->has() && index == VideoEntry) {
				media.applySubtitles(*vf);
			}

			SDL_AtomicLock(&async.loadFramesQueue[index].resultsLock);
			async.loadFramesQueue[index].results.push_back(vf.release());
			SDL_AtomicUnlock(&async.loadFramesQueue[index].resultsLock);
		}

	} while (true);
}

AVCodec *MediaProcController::Decoder::findCodec(AVCodecContext *context) {
	AVCodec *codec = nullptr;

	// Setup hw acceleration
	if (context->codec_type == AVMEDIA_TYPE_VIDEO && media.hardwareDecoding) {
		context->get_format = HardwareDecoderIFace::init;

		// Some decoders might need explicit open
		codec = HardwareDecoderIFace::findDecoder(context);
		if (codec) {
			int readWidth  = context->width;
			int readHeight = context->height;

			int err = avcodec_open2(context, codec, nullptr);
			if (!err) {
				// This is a workaround for certain hw accelerated decoders e.g. droid's MediaCodec.
				// It seems that the context is initialised with some default value (320x240) and later gets changed to the real dims.
				// Fortunately we should already have the real dimensions.
				if (context->codec_type == AVMEDIA_TYPE_VIDEO && (readWidth != context->width || readHeight != context->height)) {
					sendToLog(LogLevel::Warn, "Fixing up dimensions to %dx%d from %dx%d\n", readWidth, readHeight, context->width, context->height);
					context->width  = readWidth;
					context->height = readHeight;
				}
			} else {
				sendToLog(LogLevel::Error, "Unable to open explicit hw decoder %d\n", err);
				codec = nullptr;
			}
		}
	}

	// Fallback to implicit hardware or software decoder
	if (!codec) {
		auto codec = avcodec_find_decoder(context->codec_id);

		int err = avcodec_open2(context, codec, nullptr);
		if (err < 0) {
			sendToLog(LogLevel::Error, "Unable to open decoder %d\n", err);
			avcodec_close(context);
			return nullptr;
		}
	}

	return codec;
}

std::unique_ptr<MediaProcController::MediaFrame> MediaProcController::advanceVideoFrames(int &framesToAdvance, bool &endOfFile) {
	std::unique_ptr<MediaFrame> frame = nullptr;

	AsyncInstructionQueue &vidQueue = async.loadFramesQueue[VideoEntry];
	bool canSkipThisFrame           = false;

	while (framesToAdvance) {
		SDL_AtomicLock(&vidQueue.resultsLock);
		if (vidQueue.results.empty()) {
			// There is nothing to render unfortunately, exit
			SDL_AtomicUnlock(&vidQueue.resultsLock);
			return nullptr;
		}
		if (vidQueue.results.size() == 1 &&
		    vidQueue.results.front() == nullptr) {
			endOfFile = true;
			SDL_AtomicUnlock(&vidQueue.resultsLock);
			return nullptr;
		}

		frame = std::unique_ptr<MediaFrame>(static_cast<MediaFrame *>(vidQueue.results.front()));
		vidQueue.results.pop_front();
		canSkipThisFrame = !vidQueue.results.empty() &&
		                   !(vidQueue.results.size() == 1 &&
		                     vidQueue.results.front() == nullptr);
		SDL_AtomicUnlock(&vidQueue.resultsLock);
		SDL_SemPost(frameQueueSem[VideoEntry]);

		framesToAdvance--;

		if (framesToAdvance == 0 || !canSkipThisFrame) {
			break;
		}
	}

	assert(frame->surface != nullptr);

	return frame;
}

cmp::unique_ptr_del<uint8_t[]> MediaProcController::advanceAudioChunks(size_t &buffSz) {
	MediaFrame *frame{nullptr};

	AsyncInstructionQueue &audQueue = async.loadFramesQueue[AudioEntry];
	SDL_AtomicLock(&audQueue.resultsLock);
	if (audQueue.results.empty()) {
		SDL_AtomicUnlock(&audQueue.resultsLock);
	} else if (audQueue.results.size() == 1 &&
	           audQueue.results.front() == nullptr) {
		SDL_AtomicUnlock(&audQueue.resultsLock);
	} else {
		frame = static_cast<MediaFrame *>(audQueue.results.front());
		audQueue.results.pop_front();
		SDL_AtomicUnlock(&audQueue.resultsLock);
		SDL_SemPost(media.frameQueueSem[AudioEntry]);
	}

	if (frame) {
		buffSz = frame->dataSize;
		cmp::unique_ptr_del<uint8_t[]> ret(frame->data, frame->dataDeleter);
		frame->data = nullptr;
		delete frame;
		return ret;
	}

	return {};
}
