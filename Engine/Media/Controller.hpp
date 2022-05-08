/**
 *  Controller.hpp
 *  ONScripter-RU
 *
 *  Contains A/V controller interface.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"
#include "Engine/Readers/Base.hpp"
#include "Engine/Media/SubtitleDriver.hpp"
#include "Engine/Graphics/Pool.hpp"
#include "Support/Clock.hpp"

extern "C" {
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_gpu.h>

#include <array>
#include <deque>
#include <memory>
#include <unordered_set>

class MediaDemux;

class MediaProcController : public BaseController {
	friend class MediaDemux;

public:
	struct MediaFrame {
		bool has() {
			return surface || data || planesCnt > 0;
		}
		SDL_Surface *surface{nullptr};
		uint8_t *data{nullptr};
		uint8_t *planes[4]{};
		size_t planesCnt{0};
		int linesize[AV_NUM_DATA_POINTERS]{};
		uint32_t dataSize{0};
		bool isLastFrame{false};
		int64_t frameNumber{0};
		uint64_t msTimeStamp{0};
		AVPixelFormat srcFormat{AV_PIX_FMT_NONE};
		static inline void defaultDeleter(uint8_t arr[]) {
			delete[] arr;
		}
		std::function<void(uint8_t[])> dataDeleter{defaultDeleter};

		MediaFrame()                   = default;
		MediaFrame(const MediaFrame &) = delete;
		MediaFrame &operator=(const MediaFrame &) = delete;
		~MediaFrame();
	};

	struct HardwareDecoderIFace {
		static const std::unordered_set<int> hardwareAcceleratedFormats;
		static const std::unordered_set<int> hwConvertedFormats;

		static AVPixelFormat defaultFormat(const AVPixelFormat *format) {
			for (size_t i = 0; format[i] != AV_PIX_FMT_NONE; i++) {
				if (hardwareAcceleratedFormats.find(format[i]) == hardwareAcceleratedFormats.end())
					return format[i];
			}
			return AV_PIX_FMT_NONE;
		}
		static bool hasFormat(const AVPixelFormat *format, AVPixelFormat check) {
			for (size_t i = 0; format[i] != AV_PIX_FMT_NONE; i++) {
				if (format[i] == check)
					return true;
			}
			return false;
		}
		static bool isFormatHWConverted(AVPixelFormat format) {
			return (hwConvertedFormats.find(format) != hwConvertedFormats.end());
		}
		static void reg();
		static AVPixelFormat init(AVCodecContext *context, const AVPixelFormat *format);
		static AVCodec *findDecoder(AVCodecContext *context);
		static void deinit(AVCodecContext *context);
		static AVFrame *process(AVFrame *hwFrame, AVFrame *&tmpFrame);
	};

	enum MediaEntries {
		InvalidEntry = -1,
		VideoEntry   = 0,
		AudioEntry   = 1,
		SubsEntry    = 2
	};

protected:
	struct AudioSpec {
		int init(const SDL_AudioSpec &spec);
		AVSampleFormat format{AV_SAMPLE_FMT_S16};
		int64_t channelLayout{AV_CH_LAYOUT_STEREO};
		int8_t channels{2};
		int32_t frequency{48000};
	};

	class MediaDemux {
	public:
		static constexpr int InvalidStream = InvalidEntry;

		MediaDemux()                   = default;
		MediaDemux(const MediaDemux &) = delete;
		MediaDemux &operator=(const MediaDemux &) = delete;
		~MediaDemux() {
			for (auto semArr : {&packetQueueSemSpaces, &packetQueueSemData})
				for (auto sem : *semArr)
					if (sem)
						SDL_DestroySemaphore(sem);
		}

		bool prepare(int videoStream = InvalidStream, int audioStream = InvalidStream, int subtitleStream = InvalidStream);
		bool resetPacketQueue();

		AVPacket *obtainPacket(MediaEntries index, bool &cacheReadStarted);
		static void freePacket(AVPacket *packet) {
			av_packet_free(&packet);
		}
		void demultiplexStreams(long double videoTimeBase);

		bool packetQueueSpacesAvailable(MediaEntries entry) {
			return SDL_SemValue(packetQueueSemSpaces[entry]) == 0;
		}

		void resetSpacesSem() {
			while (SDL_SemValue(packetQueueSemSpaces[VideoEntry]) != VideoPacketBufferSize) SDL_SemPost(packetQueueSemSpaces[VideoEntry]);
			while (SDL_SemValue(packetQueueSemSpaces[AudioEntry]) != AudioPacketBufferSize) SDL_SemPost(packetQueueSemSpaces[AudioEntry]);
		}

		void resetDataSem() {
			while (SDL_SemValue(packetQueueSemData[VideoEntry]) != 0) SDL_SemWait(packetQueueSemData[VideoEntry]);
			while (SDL_SemValue(packetQueueSemData[AudioEntry]) != 0) SDL_SemWait(packetQueueSemData[AudioEntry]);
		}

		int waitForData(MediaEntries entry, uint32_t ms) {
			return SDL_SemWaitTimeout(packetQueueSemData[entry], ms);
		}

		std::atomic<bool> shouldFinish{false};
		std::atomic<bool> demuxComplete{false};

	private:
		int streamIds[3]{InvalidEntry, InvalidEntry, InvalidEntry};
		std::deque<AVPacket *> packetQueue[2];                    // ff packet queues (video, audio)
		SDL_semaphore *packetQueueSemSpaces[2]{nullptr, nullptr}; // semaphores used to control packetQueue[x] free space (AVFrames)
		SDL_semaphore *packetQueueSemData[2]{nullptr, nullptr};   // semaphores used to inform about the data in packetQueue[x] (AVFrames)

		void pushPacket(MediaEntries id, AVPacket *packet, int read_result, bool &demultiplexingComplete, long double videoTimeBase);
	};

	class Decoder {
		friend class MediaProcController;
		friend class MediaDemux;

	protected:
		enum class CacheRead {
			None,
			Started,
			Finished
		};

		int64_t debugFrameNumber{-1};

		AVCodecContext *codecContext{nullptr}; // ff codec context
		AVCodec *codec{nullptr};               // ff codec
		AVFrame *frame{nullptr};               // ff frame

		int stream;

		Decoder(AVCodecContext *context, AVCodec *codec, int stream)
		    : codecContext(context), codec(codec), stream(stream) {}

		int decodeFrameFromPacket(bool &frameFinished, AVPacket *packet);

	private:
		static AVCodec *findCodec(AVCodecContext *);

	public:
		std::atomic<bool> shouldFinish{false};

		void decodeFrame(MediaEntries index); // remove a loop from here?

		virtual void processFrame(MediaFrame &vf) = 0;

		virtual ~Decoder() {
			if (frame) {
				av_frame_free(&frame);
			}

			if (codecContext) {
				avcodec_close(codecContext);
				//codecContext = nullptr;
				//codec = nullptr;
			}
		}

		template <class T>
		static std::unique_ptr<Decoder> create(AVCodecContext *context = nullptr, int stream = -1) {
			AVCodec *codec = context ? findCodec(context) : nullptr;

			if (!context || codec)
				return std::make_unique<T>(context, codec, stream);

			return {};
		}
	};

	class AudioDecoder : public Decoder {
		friend class MediaProcController;
		SwrContext *swrContext{nullptr};

	public:
		AudioDecoder(AVCodecContext *context, AVCodec *codec, int stream)
		    : Decoder(context, codec, stream) {
			frame = av_frame_alloc();
			av_frame_unref(frame);
		}
		~AudioDecoder() override {
			if (swrContext) {
				swr_free(&swrContext);
			}
		}
		void processFrame(MediaFrame &vf) override;

	protected:
		bool initSwrContext(const AudioSpec &audioSpec);
	};

	class VideoDecoder : public Decoder {
		friend class MediaProcController;
		AVPixelFormat imageConvertSourceFormat{AV_PIX_FMT_NONE}; // updated on creation
		SwsContext *imageConvertContext{nullptr};                // ff sws context (initial scaling)
		AVFrame *tempFrame{nullptr};

	protected:
		bool initSwsContext(int dstW, int dstH, const AVPixelFormat *format = nullptr, bool forHardware = true);
		void deinitSwsContext();
		bool initTiming(int64_t duration);

	public:
		void processFrame(MediaFrame &vf) override;
		VideoDecoder(AVCodecContext *context, AVCodec *codec, int stream)
		    : Decoder(context, codec, stream) {
			frame = av_frame_alloc();
			av_frame_unref(frame);
		}
		~VideoDecoder() override {
			deinitSwsContext();

			if (tempFrame) {
				av_frame_free(&tempFrame);
			}

			HardwareDecoderIFace::deinit(codecContext);
		}

		// Framerate dection helper
		long double getVideoFramerate(bool &isVFR, bool &isCorrupted);
		uint64_t nanosPerFrame{0};
	};

	class SubtitleDecoder : public Decoder {
		friend class MediaProcController;

	protected:
		SubtitleDriver subtitleDriver; // subtitle data

		bool prepare(const char *filename, int frameWidth, int frameHeight);
		void processFrame(MediaFrame &frame) override;

	public:
		SubtitleDecoder(AVCodecContext *context, AVCodec *codec, int stream)
		    : Decoder(context, codec, stream) {}
		~SubtitleDecoder() override {
			subtitleDriver.deinit();
		}
		void processData(char *data, size_t length) {
			subtitleDriver.process(data, length);
		}
	};

	int ownInit() override;
	int ownDeinit() override;
	void processSubsData(char *data, size_t length);
	void applySubtitles(MediaFrame &frame);

private:
	// These limits are same for 'ready' frames
	// These limits may not cover possible nullptrs, since they have minimal size
#if defined(DROID) || defined(IOS)
	static constexpr size_t VideoPacketBufferSize = 12;
#else
	static constexpr size_t VideoPacketBufferSize = 25;
#endif
	static constexpr size_t AudioPacketBufferSize = VideoPacketBufferSize * 2;

	static int lockManager(void **mutex, AVLockOp op);
	static void logLine(void *inst, int level, const char *fmt, va_list args);

	std::unique_ptr<Decoder> findDecoder(AVMediaType type, unsigned streamNumber = 1, AVCodecID restrictCodecId = AV_CODEC_ID_NONE);

	AudioSpec audioSpec;
	std::unique_ptr<TempImagePool> imagePool{nullptr}; // image pool of SDL_Surfaces for video frames

	AVFormatContext *formatContext{nullptr}; // ff format context

	SDL_semaphore *frameQueueSem[2]{nullptr, nullptr}; // semaphores used to control async..results queue size (VideoFrames)
	SDL_mutex *frameQueuemutex[2]{nullptr, nullptr};   // mutexes used to control frame decoding execution
	SDL_semaphore *initVideoTimecodesLock{nullptr};    // a semaphore to control the state of video timecode grabbing
	SDL_mutex *subtitleMutex{nullptr};                 // subtitle decoder mutex

	std::atomic<int> decoderWorkerCount{0}; // equals number of unfinished workers (i.e. 0 when all are done)

	std::array<long double, VideoPacketBufferSize> initVideoTimecodes; // video timecodes grabbed from the beginning of the file
	std::deque<AVPacket *> packetQueue[2];                             // ff packet queues (video, audio)

	std::unique_ptr<Decoder> decoders[3];
	std::unique_ptr<MediaDemux> demux;

	bool alphaMasked{false};       // alpha in the bottom
	bool loopVideo{false};         // loop the video file
	bool hardwareDecoding{true};   // try hardware video decoding if supported
	bool hardwareConversion{true}; // try hardware frame conversion if supported

	void resetDemuxer();
	void resetDecoders();
	void resetFrameQueues(int vidStart = 0, int vidEnd = 0);

public:
	bool loadVideo(const char *filename, unsigned audioStream, unsigned subtitleStream);
	bool loadPresentation(const GPU_Rect &rect, bool loop);
	void frameSize(const SDL_Rect &rect, int &width, float &wFactor, int &height, float &hFactor, bool alpha);
	bool addSubtitles(const char *filename, int frameWidth, int frameHeight);
	void startProcessing();
	void resetState();
	void demultiplexStreams();
	void decodeFrames(MediaEntries entry);
	std::unique_ptr<MediaFrame> advanceVideoFrames(int &framesToAdvance, bool &endOfFile);
	cmp::unique_ptr_del<uint8_t[]> advanceAudioChunks(size_t &buffSz);
	void getVideoTimecodes(size_t &counter, AVPacket *packet, long double videoTimeBase);
	bool finish(bool needLastFrame);

	uint64_t getNanosPerFrame() {
		if (hasStream(VideoEntry)) {
			return static_cast<VideoDecoder &>(*decoders[VideoEntry]).nanosPerFrame;
		}
		return 0;
	}

	bool hasStream(MediaEntries entry) {
		return decoders[entry] != nullptr;
	}

	void giveImageBack(SDL_Surface *surface) {
		if (imagePool) //-V614
			imagePool->giveImage(surface);
		else
			throw std::runtime_error("No pool provided to return cached surface");
	}

	void setHardwareDecoding(bool enableDecoding, bool enableConversion) {
		hardwareDecoding   = enableDecoding;
		hardwareConversion = enableConversion;
	}

	MediaProcController()
	    : BaseController(this) {}
};

extern MediaProcController media;
