/**
 *  Async.hpp
 *  ONScripter-RU
 *
 *  Asynchronuous execution management and threading support.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"

#include <SDL2/SDL_thread.h>

#include <memory>
#include <deque>
#include <atomic>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

class AsyncInstruction;
class AsyncInstructionQueue;

void defaultThreadEnd(AsyncInstructionQueue *qPtr);

class AsyncInstructionQueue {
public:
	std::deque<std::unique_ptr<AsyncInstruction>> q;
	std::deque<void *> results;
	SDL_SpinLock lock{0}, loopLock{0}, resultsLock{0};

	SDL_sem *instructionsWaiting{nullptr}, *resultsWaiting{nullptr};
	SDL_Thread *thread{nullptr};
	const char *name{nullptr};
	bool quitOnEmpty{true};
	bool hasQueue{true};

	int (*threadLoopFunction)(void *){nullptr}; //-V730_NOINIT
	void (*threadStopFunction)(AsyncInstructionQueue *){defaultThreadEnd};

	void init();
	AsyncInstructionQueue(const char *threadname, bool quits = true, bool queued = true)
	    : name(threadname), quitOnEmpty(quits), hasQueue(queued) {}
};

class AsyncController;

// Instructions
class AsyncInstruction {
public:
	AsyncController *ac;
	virtual AsyncInstructionQueue *getInstructionQueue() = 0;
	AsyncInstruction(AsyncController *_ac)
	    : ac(_ac) {}
	virtual void execute()      = 0;
	virtual ~AsyncInstruction() = default;
}; // abstract class
// Subclasses of AsyncInstruction must define an override execute method,
// which is what happens when they are popped off the queue and processed in another thread

class LoadImageCacheInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	int id;
	std::string filename;
	bool allow_rgb;
	void execute() override;
	LoadImageCacheInstruction(AsyncController *_ac, int _id, std::string _filename, bool _allow_rgb)
	    : AsyncInstruction(_ac), id(_id), filename(std::move(_filename)), allow_rgb(_allow_rgb) {}
};

class LoadSoundCacheInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	int id;
	std::string filename;
	void execute() override;
	LoadSoundCacheInstruction(AsyncController *_ac, int _id, std::string _filename)
	    : AsyncInstruction(_ac), id(_id), filename(std::move(_filename)) {}
};

class AnimationInfo;
class LoadImageInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	AnimationInfo *aiPtr;
	void execute() override;
	LoadImageInstruction(AsyncController *_ac, AnimationInfo *_ai)
	    : AsyncInstruction(_ac), aiPtr(_ai) {}
};

class LoadPacketArraysInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	// Should this be a pointer to VideoLayer? Is loadVideoFramesInstruction video layer specific?
	// At any rate we need SOME data structure containing "AVPacket packet" etc ... we need somewhere to store data as we decode.
	// Should that be more generic than VideoLayer or is that where we're doing all video from now on?
	void execute() override;
	LoadPacketArraysInstruction(AsyncController *_ac)
	    : AsyncInstruction(_ac) {}
};

class LoadVideoFramesInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	// Should this be a pointer to VideoLayer? Is loadVideoFramesInstruction video layer specific?
	// At any rate we need SOME data structure containing "AVPacket packet" etc ... we need somewhere to store data as we decode.
	// Should that be more generic than VideoLayer or is that where we're doing all video from now on?
	void execute() override;
	LoadVideoFramesInstruction(AsyncController *_ac)
	    : AsyncInstruction(_ac) {}
};

class LoadAudioFramesInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	// Should this be a pointer to VideoLayer? Is loadVideoFramesInstruction video layer specific?
	// At any rate we need SOME data structure containing "AVPacket packet" etc ... we need somewhere to store data as we decode.
	// Should that be more generic than VideoLayer or is that where we're doing all video from now on?
	void execute() override;
	LoadAudioFramesInstruction(AsyncController *_ac)
	    : AsyncInstruction(_ac) {}
};

class SubtitleLayer;
class LoadSubtitleFramesInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	SubtitleLayer *sl;
	// Should this be a pointer to VideoLayer? Is loadVideoFramesInstruction video layer specific?
	// At any rate we need SOME data structure containing "AVPacket packet" etc ... we need somewhere to store data as we decode.
	// Should that be more generic than VideoLayer or is that where we're doing all video from now on?
	void execute() override;
	LoadSubtitleFramesInstruction(AsyncController *_ac, SubtitleLayer *_sl)
	    : AsyncInstruction(_ac), sl(_sl) {}
};

class PlaySoundInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	const char *filename;
	int format;
	bool loop_flag;
	int channel;
	void execute() override;
	PlaySoundInstruction(AsyncController *_ac, const char *_filename, int _format, bool _loop_flag, int _channel)
	    : AsyncInstruction(_ac), filename(_filename), format(_format), loop_flag(_loop_flag), channel(_channel) {}
};

class EventQueueInstruction : public AsyncInstruction {
public:
	AsyncInstructionQueue *getInstructionQueue() override;
	void execute() override;
	EventQueueInstruction(AsyncController *_ac)
	    : AsyncInstruction(_ac) {}
};

class VirtualMutexes {
public:
	void setMutex(void *ptr);
	void unsetMutex(void *ptr);
	void init();

	// bit of a misplaced method to help us debug weird thread issues by coordinating the locations of two separate running threads.
	// (only here 'cause i wanted to use the access_mutex)
	// call as debugJoin(debug1, debug2) from one place and debugJoin(debug2, debug1) from another (reversed argument order).
	// Pick any two numbers but they must be the same and unique to each pair of places you want to join.
	void debugJoin(int debug1, int debug2);

private:
	std::unordered_map<void *, SDL_mutex *> mutexes;
	std::unordered_map<int, SDL_sem *> semaphores;
	SDL_SpinLock access_mutex{0};
};

int imageCacheThreadLoop(void *arg);
int soundCacheThreadLoop(void *arg);
int loadImageThreadLoop(void *arg);
int loadPacketArraysThreadLoop(void *arg);
int loadVideoFramesThreadLoop(void *arg);
int loadAudioFramesThreadLoop(void *arg);
int loadSubtitleFramesThreadLoop(void *arg);
int playSoundThreadLoop(void *arg);
int eventQueueThreadLoop(void *arg);

class AsyncController : public BaseController {
protected:
	int ownInit() override;
	int ownDeinit() override;

public:
	AsyncInstructionQueue imageCacheQueue, soundCacheQueue,
	    loadImageQueue, loadPacketArraysQueue, loadFramesQueue[3],
	    playSoundQueue, eventQueueQueue;
	std::vector<AsyncInstructionQueue *> queueCollection;
	VirtualMutexes mutexes; //-V730_NOINIT
	bool threadShutdownRequested{false};

	class ThreadTerminate : public std::exception {};

	void endThreads();
	int asyncLoop(AsyncInstructionQueue &queue);
	void cacheImage(int id, const std::string &filename, bool allow_rgb);
	void cacheSound(int id, const std::string &filename);
	void loadImage(AnimationInfo *ai);
	void loadPacketArrays();
	void loadVideoFrames();
	void loadAudioFrames();
	void loadSubtitleFrames(SubtitleLayer *sl);
	void playSound(const char *filename, int format, bool loop_flag, int channel);
	void startEventQueue();

	void queue(std::unique_ptr<AsyncInstruction> inst);

	AsyncController();
};

extern AsyncController async;

class Lock {
public:
	void *ptr;
	Lock(void *_ptr)
	    : ptr(_ptr) {
		if (async.initialised())
			async.mutexes.setMutex(ptr);
	}
	~Lock() {
		if (async.initialised())
			async.mutexes.unsetMutex(ptr);
	}
};
