/**
 *  Async.cpp
 *  ONScripter-RU
 *
 *  Asynchronuous execution management and threading support.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Components/Async.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Core/Parser.hpp"
#include "Engine/Media/Controller.hpp"
#include "Engine/Layers/Subtitle.hpp"
#include "Support/FileDefs.hpp"

AsyncController async;

// Must be called during ONS initialization to once-only initialize mutexes etc
int AsyncController::ownInit() {
	/* Create all mutexes */
	mutexes.init();

	for (AsyncInstructionQueue *qPtr : queueCollection) {
		qPtr->init();
	}

	startEventQueue();

	return 0;
}

int AsyncController::ownDeinit() {
	endThreads();
	return 0;
}

AsyncController::AsyncController()
    : BaseController(this),
      imageCacheQueue("imageCacheQueue"),
      soundCacheQueue("soundCacheQueue"),
      loadImageQueue("loadImageQueue", false /*don't quit!*/),
      loadPacketArraysQueue("loadPacketArraysQueue", false),
      loadFramesQueue{{"loadVideoFramesQueue", false},
                      {"loadAudioFramesQueue", false},
                      {"loadSubtitleFramesQueue", false}},
      playSoundQueue("playSoundQueue", false),
      eventQueueQueue("eventQueueQueue", false, false /*needs no instructions*/) {
	imageCacheQueue.threadLoopFunction                                  = imageCacheThreadLoop;
	soundCacheQueue.threadLoopFunction                                  = soundCacheThreadLoop;
	loadImageQueue.threadLoopFunction                                   = loadImageThreadLoop;
	loadPacketArraysQueue.threadLoopFunction                            = loadPacketArraysThreadLoop;
	loadFramesQueue[MediaProcController::VideoEntry].threadLoopFunction = loadVideoFramesThreadLoop;
	loadFramesQueue[MediaProcController::AudioEntry].threadLoopFunction = loadAudioFramesThreadLoop;
	loadFramesQueue[MediaProcController::SubsEntry].threadLoopFunction  = loadSubtitleFramesThreadLoop;
	playSoundQueue.threadLoopFunction                                   = playSoundThreadLoop;
	eventQueueQueue.threadLoopFunction                                  = eventQueueThreadLoop;

	queueCollection.push_back(&imageCacheQueue);
	queueCollection.push_back(&soundCacheQueue);
	queueCollection.push_back(&loadImageQueue);
	queueCollection.push_back(&loadFramesQueue[MediaProcController::VideoEntry]);
	queueCollection.push_back(&loadFramesQueue[MediaProcController::AudioEntry]);
	queueCollection.push_back(&loadFramesQueue[MediaProcController::SubsEntry]);
	queueCollection.push_back(&loadPacketArraysQueue);
	queueCollection.push_back(&playSoundQueue);
	queueCollection.push_back(&eventQueueQueue);
}

void AsyncController::endThreads() {
	threadShutdownRequested = true;

	for (AsyncInstructionQueue *qPtr : queueCollection) {
		sendToLog(LogLevel::Info, "[Info] AsyncController is going to kill %s-based thread\n", qPtr->name);
		qPtr->threadStopFunction(qPtr);
	}

	threadShutdownRequested = false;
}

void AsyncController::queue(std::unique_ptr<AsyncInstruction> inst) {
	AsyncInstructionQueue *instQueue = inst->getInstructionQueue();
	// Runs thread if it is not already running
	SDL_AtomicLock(&instQueue->lock);
	instQueue->q.push_back(std::move(inst));
	if (!instQueue->quitOnEmpty)
		SDL_SemPost(instQueue->instructionsWaiting);
	if (!instQueue->thread) {
		instQueue->thread = SDL_CreateThread(instQueue->threadLoopFunction,
		                                     instQueue->name,
		                                     instQueue->q.back()->ac);
	}
	SDL_AtomicUnlock(&instQueue->lock);
}

// Main genericized async loop function
int AsyncController::asyncLoop(AsyncInstructionQueue &queue) {
	SDL_AtomicLock(&queue.loopLock);
	while (true) {
		if (threadShutdownRequested) {
			SDL_AtomicLock(&queue.lock);
			queue.thread = nullptr;
			SDL_AtomicUnlock(&queue.lock);
			break;
		}

		if (!queue.quitOnEmpty && queue.hasQueue) {
			SDL_SemWait(queue.instructionsWaiting);
		}

		if (threadShutdownRequested) {
			SDL_AtomicLock(&queue.lock);
			queue.thread = nullptr;
			SDL_AtomicUnlock(&queue.lock);
			break;
		}

		SDL_AtomicLock(&queue.lock);
		if (!queue.q.empty()) {
			std::unique_ptr<AsyncInstruction> inst;
			AsyncInstruction *ptr;

			if (queue.hasQueue) {
				inst = std::move(queue.q.front());
				queue.q.pop_front();
				ptr = inst.get();
			} else {
				ptr = queue.q.front().get();
			}

			SDL_AtomicUnlock(&queue.lock);

			//WARNING: It is assumed that queue is not accessed at this step
			try {
				ptr->execute(); // Do the actual work
			} catch (ThreadTerminate &) {
				SDL_AtomicLock(&queue.lock);
				SDL_SemPost(queue.resultsWaiting);
				queue.thread = nullptr;
				SDL_AtomicUnlock(&queue.lock);
				break;
			}

			SDL_AtomicLock(&queue.lock);
			if (!queue.quitOnEmpty)
				SDL_SemPost(queue.resultsWaiting);
			if (threadShutdownRequested || (queue.q.empty() && queue.quitOnEmpty)) {
				queue.thread = nullptr;
				SDL_AtomicUnlock(&queue.lock);
				break;
			}
		}
		SDL_AtomicUnlock(&queue.lock);
	}
	SDL_AtomicUnlock(&queue.loopLock);
	return 0;
}

/* ---------------- Async Instruction Queue  ----------------- */

void AsyncInstructionQueue::init() {
	instructionsWaiting = SDL_CreateSemaphore(0);
	resultsWaiting      = SDL_CreateSemaphore(0);
}

void defaultThreadEnd(AsyncInstructionQueue *qPtr) {
	// It might be suspended on a semaphore waiting for an instruction. If so, wake it up so it can exit.
	if (!qPtr->quitOnEmpty)
		SDL_SemPost(qPtr->instructionsWaiting);
	// Wait for the loop mutex to be given back (i.e. for the thread to exit)
	SDL_AtomicLock(&qPtr->loopLock);
	// Tidy up the queue state (remove all outstanding instructions and results)
	SDL_AtomicLock(&qPtr->lock);
	// Empty instructions queue
	if (qPtr->thread) {
		while (!qPtr->q.empty()) {
			SDL_SemWait(qPtr->instructionsWaiting); // this should never suspend
			qPtr->q.pop_front();
		}
	} else {
		// The thread is gone, just reset the leftovers.
		qPtr->q.clear();
		while (SDL_SemValue(qPtr->instructionsWaiting))
			SDL_SemWait(qPtr->instructionsWaiting);
	}
	// Empty results queue (semaphore -- we don't know anything about where the actual results are and will have to hope something else clears them up...)
	// WARNING : This is unsafe if there is anything waiting on the results queue, but we should not call endThreads when we are waiting on a result anyway, I think
	// (these are mutually exclusive actions by the main thread -- either we're in playSound etc or we are quitting)
	SDL_DestroySemaphore(qPtr->resultsWaiting);
	qPtr->resultsWaiting = SDL_CreateSemaphore(0); // recreate it at 0
	SDL_AtomicUnlock(&qPtr->lock);
	// Return the loop mutex (we don't need it)
	SDL_AtomicUnlock(&qPtr->loopLock);
}

/* ---------------- Virtual Mutexes ----------------- */

void VirtualMutexes::init() {
	//currently empty
}

void VirtualMutexes::setMutex(void *ptr) {
	SDL_mutex *m = nullptr;
	SDL_AtomicLock(&access_mutex);
	if (!ptr) {
		SDL_AtomicUnlock(&access_mutex);
		throw std::runtime_error("Resource is dead");
	}

	if (mutexes.count(ptr) > 0) {
		m = mutexes[ptr];
	} else {
		m = (mutexes[ptr] = SDL_CreateMutex());
	}
	SDL_AtomicUnlock(&access_mutex);
	SDL_mutexP(m);
}

void VirtualMutexes::unsetMutex(void *ptr) {
	SDL_mutex *m = nullptr;
	SDL_AtomicLock(&access_mutex);
	if (mutexes.count(ptr) > 0) {
		m = mutexes[ptr];
	} else {
		SDL_AtomicUnlock(&access_mutex);
		throw std::runtime_error("Amen, uncreated mutex was released into heavens");
	}
	SDL_AtomicUnlock(&access_mutex);
	SDL_mutexV(m);
}

void VirtualMutexes::debugJoin(int debug1, int debug2) {
	SDL_sem *s1 = nullptr;
	SDL_sem *s2 = nullptr;
	SDL_AtomicLock(&access_mutex);
	if (semaphores.count(debug1) > 0) {
		s1 = semaphores[debug1];
		s2 = semaphores[debug2];
	} else {
		s1 = (semaphores[debug1] = SDL_CreateSemaphore(0));
		s2 = (semaphores[debug2] = SDL_CreateSemaphore(0));
	}
	SDL_AtomicUnlock(&access_mutex);
	SDL_SemPost(s2);                     //<- important part
	int r = SDL_SemWaitTimeout(s1, 100); //<- important part
	if (r == SDL_MUTEX_TIMEDOUT) {
		SDL_SemTryWait(s2); // take it away again, but don't block if the other thread just consumed it in a case of really bad timing
	}
}

/* ---------------- Load image cache instruction ----------------- */

void LoadImageCacheInstruction::execute() {
	ons.loadImageIntoCache(id, filename, allow_rgb);
}

AsyncInstructionQueue *LoadImageCacheInstruction::getInstructionQueue() {
	return &ac->imageCacheQueue;
}

void AsyncController::cacheImage(int id, const std::string &filename, bool allow_rgb) {
	std::string stringForNewThread(filename.data(), filename.length()); // avoids possible COW issues
	queue(std::make_unique<LoadImageCacheInstruction>(this, id, stringForNewThread, allow_rgb));
}

int imageCacheThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->imageCacheQueue);
}

/* ---------------- Load sound cache instruction ----------------- */

void LoadSoundCacheInstruction::execute() {
	ons.loadSoundIntoCache(id, filename, true);
}

AsyncInstructionQueue *LoadSoundCacheInstruction::getInstructionQueue() {
	return &ac->soundCacheQueue;
}

void AsyncController::cacheSound(int id, const std::string &filename) {
	std::string stringForNewThread(filename.data(), filename.length()); // avoids possible COW issues
	queue(std::make_unique<LoadSoundCacheInstruction>(this, id, stringForNewThread));
}

int soundCacheThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->soundCacheQueue);
}

/* ----------------- Load image instruction ----------------- */

void LoadImageInstruction::execute() {
	ons.buildAIImage(aiPtr);
}

AsyncInstructionQueue *LoadImageInstruction::getInstructionQueue() {
	return &ac->loadImageQueue;
}

void AsyncController::loadImage(AnimationInfo *ai) {
	queue(std::make_unique<LoadImageInstruction>(this, ai));
}

int loadImageThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->loadImageQueue);
}

/* -------------- Load packet arrays instruction -------------- */

void LoadPacketArraysInstruction::execute() {
	//async.loadPacketArraysQueue.resultsLock is set inside
	media.demultiplexStreams();
}

AsyncInstructionQueue *LoadPacketArraysInstruction::getInstructionQueue() {
	return &ac->loadPacketArraysQueue;
}

void AsyncController::loadPacketArrays() {
	queue(std::make_unique<LoadPacketArraysInstruction>(this));
}

int loadPacketArraysThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->loadPacketArraysQueue);
}

/* -------------- Load video frame instruction -------------- */

void LoadVideoFramesInstruction::execute() {
	//async.loadVideoFramesQueue.resultsLock is set inside
	media.decodeFrames(MediaProcController::VideoEntry);
}

AsyncInstructionQueue *LoadVideoFramesInstruction::getInstructionQueue() {
	return &ac->loadFramesQueue[MediaProcController::VideoEntry];
}

void AsyncController::loadVideoFrames() {
	queue(std::make_unique<LoadVideoFramesInstruction>(this));
}

int loadVideoFramesThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->loadFramesQueue[MediaProcController::VideoEntry]);
}

/* -------------- Load audio frame instruction -------------- */

void LoadAudioFramesInstruction::execute() {
	//async.loadAudioFramesQueue.resultsLock is set inside
	media.decodeFrames(MediaProcController::AudioEntry);
}

AsyncInstructionQueue *LoadAudioFramesInstruction::getInstructionQueue() {
	return &ac->loadFramesQueue[MediaProcController::AudioEntry];
}

void AsyncController::loadAudioFrames() {
	queue(std::make_unique<LoadAudioFramesInstruction>(this));
}

int loadAudioFramesThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->loadFramesQueue[MediaProcController::AudioEntry]);
}

/* -------------- Load subtitle frame instruction -------------- */

void LoadSubtitleFramesInstruction::execute() {
	//async.loadAudioFramesQueue.resultsLock is unused
	sl->doDecoding();
}

AsyncInstructionQueue *LoadSubtitleFramesInstruction::getInstructionQueue() {
	return &ac->loadFramesQueue[MediaProcController::SubsEntry];
}

void AsyncController::loadSubtitleFrames(SubtitleLayer *sl) {
	queue(std::make_unique<LoadSubtitleFramesInstruction>(this, sl));
}

int loadSubtitleFramesThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->loadFramesQueue[MediaProcController::SubsEntry]);
}

/* -------------- Play sound instruction -------------- */

void PlaySoundInstruction::execute() {
	auto r = static_cast<uintptr_t>(ons.playSound(filename, format, loop_flag, channel));
	SDL_AtomicLock(&ac->playSoundQueue.resultsLock);
	ac->playSoundQueue.results.push_back(reinterpret_cast<void *>(r));
	SDL_AtomicUnlock(&ac->playSoundQueue.resultsLock);
}

AsyncInstructionQueue *PlaySoundInstruction::getInstructionQueue() {
	return &ac->playSoundQueue;
}

void AsyncController::playSound(const char *filename, int format, bool loop_flag, int channel) {
	queue(std::make_unique<PlaySoundInstruction>(this, filename, format, loop_flag, channel));
}

int playSoundThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->playSoundQueue);
}

/* -------------- Event Queue instruction -------------- */

void EventQueueInstruction::execute() {
	ons.fetchEventsToQueue();
}

AsyncInstructionQueue *EventQueueInstruction::getInstructionQueue() {
	return &ac->eventQueueQueue;
}

void AsyncController::startEventQueue() {
	queue(std::make_unique<EventQueueInstruction>(this));
}

int eventQueueThreadLoop(void *arg) {
	AsyncController *ac = static_cast<AsyncController *>(arg);
	return ac->asyncLoop(ac->eventQueueQueue);
}
