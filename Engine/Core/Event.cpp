/**
 *  Event.cpp
 *  ONScripter-RU
 *
 *  Event handler core code.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Joystick.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Layers/Media.hpp"

#ifdef LINUX
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include <numeric>

const uint32_t MAX_TOUCH_TAP_TIMESPAN{80};
const uint32_t MAX_TOUCH_SWIPE_TIMESPAN{300};
const float TOUCH_ACTION_THRESHOLD_X = 0.1;
const float TOUCH_ACTION_THRESHOLD_Y = 0.15;

enum {
	ONS_MUSIC_EVENT,
	ONS_SEQMUSIC_EVENT,
};

static Direction getDirection(SDL_Scancode code) {
	switch (code) {
		case SDL_SCANCODE_RIGHT:
		case SDL_SCANCODE_KP_6:
			return Direction::RIGHT;
		case SDL_SCANCODE_UP:
		case SDL_SCANCODE_KP_8:
			return Direction::UP;
		case SDL_SCANCODE_DOWN:
		case SDL_SCANCODE_KP_2:
			return Direction::DOWN;
		default:
			return Direction::LEFT;
	}
}

extern bool ext_music_play_once_flag;
bool ext_music_play_once_flag = false;

/* **************************************** *
 * Callback functions
 * **************************************** */

extern "C" void musicFinishCallback();
void seqmusicCallback(int sig);
extern "C" void waveCallback(int channel);

extern "C" void musicFinishCallback() {
	SDL_Event event;
	event.type      = SDL_USEREVENT;
	event.user.code = ONS_MUSIC_EVENT;
	SDL_PushEvent(&event);
}

void seqmusicCallback(int /*sig*/) {
#ifdef LINUX
	int status;
	wait(&status);
#endif
	if (!ext_music_play_once_flag) {
		SDL_Event event;
		event.type      = SDL_USEREVENT;
		event.user.code = ONS_SEQMUSIC_EVENT;
		SDL_PushEvent(&event);
	}
}

extern "C" void waveCallback(int channel) {
	SDL_Event event;
	event.type      = ONS_CHUNK_EVENT;
	event.user.code = channel;
	SDL_PushEvent(&event);
}

void ONScripter::flushEventSub(SDL_Event &event) {
	//event related to streaming media
	if (event.user.code == ONS_MUSIC_EVENT && event.type == SDL_USEREVENT) {
		if (music_play_loop_flag ||
		    (cd_play_loop_flag && !cdaudio_flag)) {
			stopBGM(true);
			if (music_file_name)
				playSoundThreaded(music_file_name, SOUND_MUSIC, true);
			else
				playCDAudio();
		} else {
			stopBGM(false);
		}
	} else if (event.user.code == ONS_SEQMUSIC_EVENT && event.type == SDL_USEREVENT) {
		ext_music_play_once_flag = !seqmusic_play_loop_flag;
		Mix_FreeMusic(seqmusic_info);
		playSequencedMusic(seqmusic_play_loop_flag);
	} else if (event.type == ONS_CHUNK_EVENT) { // for processing btntime2 and automode correctly
		uint32_t ch = event.user.code;
		if (wave_sample[ch]) {
			if (ch >= ONS_MIX_CHANNELS || !channel_preloaded[ch]) {
				//don't free preloaded channels
				wave_sample[ch] = nullptr;
			}
			if (ch == MIX_LOOPBGM_CHANNEL0 &&
			    loop_bgm_name[1] &&
			    wave_sample[MIX_LOOPBGM_CHANNEL1])
				Mix_PlayChannel(MIX_LOOPBGM_CHANNEL1,
				                wave_sample[MIX_LOOPBGM_CHANNEL1]->chunk, -1);
			if (ch == 0 && bgmdownmode_flag)
				setCurMusicVolume(music_volume);
		}
	}
}

static std::atomic<bool> eventsArrived{false};
static SDL_SpinLock fetchedEventQueueLock{0};

void ONScripter::flushEvent() {
	std::unique_ptr<SDL_Event> event;

	while (!localEventQueue.empty() || updateEventQueue()) {
		event = std::move(localEventQueue.back());
		localEventQueue.pop_back();
		flushEventSub(*event);
	}
}

void ONScripter::handleSDLEvents() {
	updateEventQueue();

	// Process some checks before returning from runEventLoop (at least automode/voicewait related)
	auto event       = std::make_unique<SDL_Event>();
	event->type      = ONS_UPKEEP_EVENT;
	event->user.code = -1;
	localEventQueue.emplace_front(std::move(event));

	// Make sure we return from runEventLoop when we run out of events
	event            = std::make_unique<SDL_Event>();
	event->type      = ONS_EVENT_BATCH_END;
	event->user.code = -1;
	localEventQueue.emplace_front(std::move(event));

	runEventLoop();

	while (takeEventsOut(ONS_EVENT_BATCH_END))
		;
	while (takeEventsOut(ONS_UPKEEP_EVENT))
		;
}

bool ONScripter::takeEventsOut(uint32_t type) {
	auto it = localEventQueue.begin();
	bool has{false};
	while (it != localEventQueue.end()) {
		if ((*it)->type == type) {
			it  = localEventQueue.erase(it);
			has = true;
		} else
			++it;
	}

	return has;
}

bool ONScripter::updateEventQueue() {
	if (!eventsArrived.load(std::memory_order_acquire))
		return false;

	SDL_AtomicLock(&fetchedEventQueueLock);
	if (fetchedEventQueue.empty()) {
		eventsArrived.store(false, std::memory_order_release);
		SDL_AtomicUnlock(&fetchedEventQueueLock);
		return false;
	}

	localEventQueue.splice(localEventQueue.begin(), fetchedEventQueue);

	eventsArrived.store(false, std::memory_order_release);
	SDL_AtomicUnlock(&fetchedEventQueueLock);
	return true;
}

void ONScripter::fetchEventsToQueue() {
	uint32_t lastTimeStamp{0};

	auto pushEvent = [this](std::unique_ptr<SDL_Event> &event) {
		SDL_AtomicLock(&fetchedEventQueueLock);
		fetchedEventQueue.emplace_front(std::move(event));
		eventsArrived.store(true, std::memory_order_release);
		SDL_AtomicUnlock(&fetchedEventQueueLock);
	};

	auto pushFingerEvents = [this, &lastTimeStamp, pushEvent](bool force = false) {
		for (auto &fingerEvent : fingerEvents) {
			if (fingerEvent) {
				//sendToLog(LogLevel::Error, "Pushing finger event %s force %d at %d by %d, current %d\n",
				//			fingerEvent->type == SDL_FINGERUP ? "up" : "down", force,
				//			fingerEvent->common.timestamp, lastTimeStamp, SDL_GetTicks());
				if (force || fingerEvent->common.timestamp + MAX_TOUCH_TAP_TIMESPAN <
				                 (lastTimeStamp == 0 ? (lastTimeStamp = SDL_GetTicks()) : lastTimeStamp)) {
					pushEvent(fingerEvent);
				}
			}
		}
	};

	auto event = std::make_unique<SDL_Event>();
	std::unique_ptr<SDL_Event> tmp_event;

	while (SDL_WaitEventTimeout(event.get(), 1)) {
		// ignore continuous SDL_MOUSEMOTION
		while (event->type == SDL_MOUSEMOTION) {
			if (!tmp_event)
				tmp_event = std::make_unique<SDL_Event>();
			if (SDL_PeepEvents(tmp_event.get(), 1, SDL_PEEKEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 0)
				break;
			if (tmp_event->type != SDL_MOUSEMOTION)
				break;
			SDL_PeepEvents(tmp_event.get(), 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
			*event = *tmp_event;
		}

		// group finger events
		bool queueEmpty{false};
		while (event->type == SDL_FINGERDOWN || event->type == SDL_FINGERUP) {
			auto &finger = event->type == SDL_FINGERUP ? fingerEvents[1] : fingerEvents[0];
			//sendToLog(LogLevel::Error, "Found finger %s event %d from %lld (%f, %f, %f, %f, %f) current %d has %d fingers\n",
			//			event->type == SDL_FINGERUP ? "up" : "down",
			//			event->common.timestamp, event->tfinger.touchId, event->tfinger.x, event->tfinger.y,
			//			event->tfinger.dx, event->tfinger.dy, event->tfinger.pressure,
			//			finger ? finger->common.timestamp : -1, finger ? (uint32_t)finger->tfinger.fingerId : 0);
			if (finger && finger->common.timestamp + MAX_TOUCH_TAP_TIMESPAN >= event->common.timestamp) {
				finger->tfinger.fingerId++;
			} else {
				if (finger)
					pushFingerEvents(true);
				finger = std::move(event);
				event.reset(new SDL_Event);
				finger->tfinger.fingerId = 1;
			}

			if (SDL_PeepEvents(event.get(), 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) <= 0) {
				queueEmpty = true;
				break;
			}
		}

		if (!queueEmpty) {
			//sendToLog(LogLevel::Error, "Updating from %d last timestamp with %d current %d\n",
			//			event->type, event->common.timestamp, SDL_GetTicks());
			lastTimeStamp = event->common.timestamp;
			pushEvent(event);
			event.reset(new SDL_Event);
		}
	}

	pushFingerEvents();
}

void ONScripter::waitEvent(int count, bool nopPreferred) {
	//sendToLog(LogLevel::Info, "----waitEventSub(%i)\n", count);
	static unsigned int lastExitTime   = 0;
	unsigned int externalTimeThreshold = 5; // for instance

	unsigned int thisCallTime = SDL_GetTicks();
	if (!(skip_mode & SKIP_SUPERSKIP) && lastExitTime) {
		if (nopPreferred && thisCallTime - lastExitTime < externalTimeThreshold) {
			return;
		}
	}

	bool timerBreakout = count >= 0;
	//TODO: remove me, when done with constant_refresh
	static int nested_calls = 0;
	nested_calls++;

	if (nested_calls != 1) {
		errorAndExit("You are completely mad to use SDL_Events like that");
	}

	static FPSTimeGenerator *actual_fps  = nullptr;
	static FPSTimeGenerator *fps_default = new FPSTimeGenerator(DEFAULT_FPS);
	if (game_fps && !actual_fps) {
		actual_fps = new FPSTimeGenerator(game_fps);
	}
	auto fps = game_fps ? actual_fps : fps_default;

	unsigned int ticks = thisCallTime; // SDL_GetTicks()

	do {
		static unsigned int accumulatedOvershoot = 0;
		uint64_t framesOvershoot                 = 0;
		uint64_t nanosPerFrame                   = fps->nanosPerFrame();
		unsigned int timeThisFrame{fps->nextTime()};
		while (accumulatedOvershoot > timeThisFrame) {
			// must skip this frame :(
			accumulatedOvershoot -= timeThisFrame;
			timeThisFrame = fps->nextTime();
			framesOvershoot++;
		}

		advanceGameState(nanosPerFrame * (framesOvershoot + 1)); // may advance multiple frames if we are lagging
		if (allow_rendering) {
			constantRefresh();
		}
		handleSDLEvents();
		joyCtrl.handleUsbEvents();
		mainThreadDowntimeProcessing(true); // we must unfortunately call it at least once (and don't care whether it did anything, ignore return value)

		if (request_video_shutdown) {
			auto vidLayer = getLayer<MediaLayer>(video_layer);
			if (vidLayer && vidLayer->isPlaying(true)) {
				if (vidLayer->stopPlayback(MediaLayer::FinishMode::Normal)) {
					request_video_shutdown = false;
				}
			} else {
				request_video_shutdown = false;
			}
		}

		static unsigned int lastFlipTime = 0;

		if (allow_rendering && !(skip_mode & SKIP_SUPERSKIP) && !deferredLoadingEnabled) {
			if (cursor_gpu) {
				int x, y;
				SDL_GetMouseState(&x, &y);
				gpu.copyGPUImage(cursor_gpu, nullptr, nullptr, screen_target, x, y);
			}

			GPU_FlushBlitBuffer();
		}

		if (cursorAutoHide && lastCursorMove + 5000 < ticksNow) {
			cursorState(false);
		}

		while (true) {
			ticksNow = SDL_GetTicks();
			if (ticksNow - lastFlipTime >= timeThisFrame) {
				accumulatedOvershoot += (ticksNow - lastFlipTime) - timeThisFrame;
				break;
			}
			// We don't want to be precise in SSKIP mode
			if (skip_mode & SKIP_SUPERSKIP)
				break;
			// we still have time, do some downtime processing
			bool processed{mainThreadDowntimeProcessing(false)};
			// if we're way ahead of schedule (defined here as more than 5ms), let's have a little nap so we don't destroy everyone's CPU
			if (!processed && ((ticksNow - lastFlipTime) + 5 <= timeThisFrame)) {
				SDL_Delay(1);
			}
		}

		if (allow_rendering && !(skip_mode & SKIP_SUPERSKIP) && !deferredLoadingEnabled) {
			if (cursor)
				SDL_SetCursor(nullptr);
			if (screenChanged && !window.getFullscreenFix() && should_flip) {
				GPU_Flip(screen_target);
				screenChanged = false;
				gpu.clearWholeTarget(screen_target);
			} else {
				// We didn't update, assume screenChanged to be false
				screenChanged = false;
			}
		}

#ifndef DROID
		// We still must invoke this on many platforms to prevent "not responding" issues.
		// On droid it is not necessary and it additionally breaks background app handling in Android_PumpEvents
		SDL_PollEvent(nullptr);
#endif

		if (show_fps_counter && !(skip_mode & SKIP_SUPERSKIP)) {
			static std::deque<uint32_t> ticksList;
			// display fps counter in title bar averaged over 30 frames
			ticksList.push_front(ticksNow - lastFlipTime);
			if (ticksList.size() == 31)
				ticksList.pop_back();
			// calculate average
			double av = std::accumulate(ticksList.begin(), ticksList.end(), 0) / 30.0;
			// put it in a string
			size_t len        = 128 + std::strlen(wm_title_string);
			char *titlestring = new char[len];
			std::snprintf(titlestring, len, "[Renderer: %s / TPF: %.3f ms / FPS: %.3f] %s%s",
			              gpu.current_renderer->name, av, 1000.0 / av, volume_on_flag ? "" : "[Sound: Off] ", wm_title_string);
			// set the title
			window.setTitle(titlestring);
			freearr(&titlestring);
		}

		//sendToLog(LogLevel::Info,"  flipped -- aimed for %i ms, took %i ms\n", constant_refresh_interval, ticksNow - lastFlipTime);
		lastFlipTime = ticksNow;

		//printClock("(next iteration)");

		if (!endOfEventBatch) {
			// we were broken out prematurely by some condition we were waiting for, so we should return.
			if (count > 0) {
				dynamicProperties.advance(count); // advance the time we skipped
				dynamicProperties.apply();
			}
			break;
		}

		count -= (ticksNow - ticks);
		ticks = ticksNow;
		//sendToLog(LogLevel::Info,"  next iteration\n");
	} while (count > 0 || !timerBreakout); // if we are told not to break out by timer, this is an infinite loop
	//sendToLog(LogLevel::Info, "-----------------\n");
	nested_calls--;

	lastExitTime = SDL_GetTicks();

	// New process --
	// ConstantUpdate
	// ConstantRefresh
	// Process current events (if we are provided with a timer don't leave the function after flipping, instead return to step 1)
	// Wait until n ms since last flip (interleave loading). Or -- do some loading, then wait for vsync (if we decide to use that?)
	// Flip
	// Return -- here we assume that we are not going to be gone long but that isn't respected atm
	// need to check in during label execute and this fn return back if we've not been gone long
}

void ONScripter::trapHandler() {
	// End video if we are allowed to skip
	if (video_skip_mode == VideoSkip::Normal) {
		request_video_shutdown = true;
		// Script is responsible for handling trap-based exits
	} else if (video_skip_mode == VideoSkip::Trap) {
		video_skip_mode = VideoSkip::NotPlaying;
	}

	stopCursorAnimation(clickstr_state);
	setCurrentLabel(lrTrap.dest);
	lrTrap = LRTrap();
}

/* **************************************** *
 * Event handlers
 * **************************************** */
bool ONScripter::mouseMoveEvent(SDL_MouseMotionEvent &event, EventProcessingState &state) {
	controlMode = ControlMode::Mouse;

	state.buttonState.x = event.x;
	state.buttonState.y = event.y;
	window.translateWindowToScriptCoords(state.buttonState.x, state.buttonState.y);

	if (event_mode & WAIT_BUTTON_MODE) {
		mouseOverCheck(state.buttonState.x, state.buttonState.y);
		if (getmouseover_flag && hoveringButton &&
		    (hoveredButtonNumber >= getmouseover_min) &&
		    (hoveredButtonNumber <= getmouseover_max)) {
			// Both NScripter and ONScripter do not distinguish mouse over from a click.
			// This is nonsense, so we add a magic value large enough (10000) to do so.
			// Since the buttons are normally expected to be within 1~999 range and negative ones are
			// usually reserved for hardware keys, this sounds like a reasonable solution.
			state.buttonState.set(10000 + hoveredButtonNumber);
			playClickVoice();
			stopCursorAnimation(clickstr_state);
			return true;
		}
		if (btnarea_flag &&
		    (((btnarea_pos < 0) && (event.y > -btnarea_pos)) ||
		     ((btnarea_pos > 0) && (event.y < btnarea_pos)))) {
			state.buttonState.set(-4);
			playClickVoice();
			stopCursorAnimation(clickstr_state);
			return true;
		}
	}
	return false;
}

bool ONScripter::mouseButtonDecision(EventProcessingState &state, bool left, bool right, bool middle, bool up, bool down) {
	auto rclick = [&](EventProcessingState &state) {
		if ((rmode_flag && (event_mode & WAIT_TEXT_MODE)) ||
		    (event_mode & (WAIT_BUTTON_MODE | WAIT_RCLICK_MODE))) {
			state.buttonState.set(-1);
			for (auto ai : sprites(SPRITE_LSP)) {
				if (ai->scrollableInfo.isSpecialScrollable && ai->scrollableInfo.respondsToClick && ai->scrollableInfo.mouseCursorIsOverHoveredElement) {
					state.buttonState.set(-81);
					break;
				}
			}
			return true;
		}
		return false;
	};

	auto lclick = [&](EventProcessingState &state, bool down) {
		if (hoveringButton) {
			state.buttonState.set(hoveredButtonNumber);
		} else {
			state.buttonState.set(0);
			for (auto ai : sprites(SPRITE_LSP)) {
				if (ai->scrollableInfo.isSpecialScrollable && ai->scrollableInfo.respondsToClick && ai->scrollableInfo.mouseCursorIsOverHoveredElement) {
					state.buttonState.set(-80);
					break;
				}
			}
		}
		if (event_mode & WAIT_TEXTOUT_MODE && skip_enabled) {
			state.skipMode |= (SKIP_TO_WAIT | SKIP_TO_EOL);
			// script cannot detect _TO_WAIT or _TO_EOL using isskip etc -- at best TO_EOP page,
			// so from script POV this is not a change in its state, so, no eventCallbackRequired here
		}
		skip_effect = true;
		if (video_skip_mode == VideoSkip::Normal) {
			request_video_shutdown = true;
		}
		if (down)
			state.buttonState.down_flag = true;

		if (state.buttonState.valid_flag && (event_mode & WAIT_INPUT_MODE) && WaitVoiceAction::isCurrent(state.handler)) {
			currentAction(state.handler)->terminate();
		}

		return true;
	};

	auto mclick = [&](EventProcessingState &state, bool down) {
		if (!getmclick_flag)
			return false;
		state.buttonState.set(-70);
		if (down)
			state.buttonState.down_flag = true;
		return true;
	};

	return (right && up && rclick(state)) || //right-click
	       (left && lclick(state, down)) ||  //left-click
	       (middle && mclick(state, down));  //middle-click
}

bool ONScripter::checkClearAutomode(EventProcessingState &state, bool up) {
	//any mousepress clears automode, on the release
	if (up) {
		addToPostponedEventChanges([this]() { eventCallbackRequired = true; automode_flag = false; });
		if (getskipoff_flag && (event_mode & WAIT_BUTTON_MODE)) {
			state.buttonState.set(-61);
			return true;
		}
	}
	return false;
}

bool ONScripter::checkClearTrap(bool left, bool right) {
	if (lrTrap.enabled) {
		//trap that mouseclick!
		if ((right && lrTrap.right) || (left && lrTrap.left)) {

			addToPostponedEventChanges("trapHandler", [this]() { trapHandler(); });

			/* This one might have returned us during waitCommand, so it needs to signal now as well */
			if (event_mode & WAIT_WAIT_MODE) {
				for (const auto &a : fetchConstantRefreshActions<WaitAction>()) a->terminate();
			}
			if (event_mode & WAIT_DELAY_MODE) {
				for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
			}

			return true;
		}
	}
	return false;
}

bool ONScripter::checkClearSkip(EventProcessingState &state) {
	if (getskipoff_flag && (state.skipMode & SKIP_NORMAL) && (event_mode & WAIT_BUTTON_MODE)) {
		eventCallbackRequired = true;
		state.skipMode &= ~SKIP_NORMAL;
		state.buttonState.set(-60);
		return true;
	}

	if (state.skipMode & SKIP_NORMAL)
		eventCallbackRequired = true;
	state.skipMode &= ~SKIP_NORMAL;
	return false;
}

bool ONScripter::checkClearVoice() {
	if (event_mode & (WAIT_INPUT_MODE | WAIT_BUTTON_MODE)) {
		addToPostponedEventChanges("play click voice", [this] { playClickVoice(); });
		addToPostponedEventChanges([this] { stopCursorAnimation(clickstr_state); });
		if (event_mode & WAIT_DELAY_MODE) {
			for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
		}
		return true;
	}

	return false;
}

// returns true if should break out of the event loop
bool ONScripter::mousePressEvent(SDL_MouseButtonEvent &event, EventProcessingState &state) {
	if (event_mode & WAIT_BUTTON_MODE)
		last_keypress = SDL_NUM_SCANCODES;

	bool type_up    = event.type == SDL_MOUSEBUTTONUP;
	bool type_down  = event.type == SDL_MOUSEBUTTONDOWN;
	bool btn_left   = event.button == SDL_BUTTON_LEFT;
	bool btn_right  = event.button == SDL_BUTTON_RIGHT;
	bool btn_middle = event.button == SDL_BUTTON_MIDDLE;

	if (automode_flag)
		return checkClearAutomode(state, type_up);

	if (checkClearTrap(btn_left, btn_right))
		return true;

	state.buttonState.reset();
	state.buttonState.x = event.x;
	state.buttonState.y = event.y;
	window.translateWindowToScriptCoords(state.buttonState.x, state.buttonState.y);
	state.buttonState.down_flag = false;

	if (checkClearSkip(state))
		return true;

	if (!mouseButtonDecision(state, btn_left, btn_right, btn_middle, type_up, type_down))
		return false;

	return checkClearVoice();
}

bool ONScripter::touchEvent(SDL_Event &event, EventProcessingState &state) {
	if (event_mode & WAIT_BUTTON_MODE)
		last_keypress = SDL_NUM_SCANCODES;

	bool btn_left   = false;
	bool btn_right  = false;
	bool btn_middle = false;
	bool type_up    = event.type == SDL_FINGERUP || event.type == SDL_MULTIGESTURE;
	bool type_down  = event.type == SDL_FINGERDOWN;
	float event_x = 0, event_y = 0;

	auto sendKeyEvent = [this](SDL_Scancode c) {
		auto k                 = new SDL_Event;
		k->key.keysym.scancode = c;
		k->type                = SDL_KEYUP;
		localEventQueue.emplace_front(k);
	};

	if (event.type == SDL_MULTIGESTURE) {
		SDL_MultiGestureEvent &gesture = event.mgesture;

		//sendToLog(LogLevel::Error, "Multiguesture %d last %d, num %d (%f, %f)\n",
		//			event.common.timestamp, last_touchswipe_time,
		//			event.mgesture.numFingers, event.mgesture.dDist, event.mgesture.dTheta);

		// New movement
		if (last_touchswipe_time + MAX_TOUCH_SWIPE_TIMESPAN < gesture.timestamp) {
			last_touchswipe.x = gesture.x;
			last_touchswipe.y = gesture.y;
			last_touchswipe.w = last_touchswipe.h = 0;
			last_touchswipe_time                  = gesture.timestamp;
		}

		// We are applying the action, ignore the rest of the swipe
		if (last_touchswipe_time <= gesture.timestamp) {
			if (gesture.numFingers == 2) {
				SDL_MouseWheelEvent wheel{};
				wheel.type = SDL_MOUSEWHEEL;
				wheel.x    = 0;
				wheel.y    = (last_touchswipe.y - gesture.y) * touch_scroll_mul;
				return mouseScrollEvent(wheel, state);
			}
			if (gesture.numFingers == 3) {
				last_touchswipe.w = gesture.x - last_touchswipe.x; // w > 0 -> right
				last_touchswipe.h = gesture.y - last_touchswipe.y; // h > 0 -> down

				if (last_touchswipe.w > TOUCH_ACTION_THRESHOLD_X) { // right
					sendKeyEvent(ONS_SCANCODE_SKIP);
				} else if (last_touchswipe.w < -TOUCH_ACTION_THRESHOLD_X) { // left
					sendKeyEvent(SDL_SCANCODE_A);
				} else if (last_touchswipe.h > TOUCH_ACTION_THRESHOLD_Y) { // down
					sendKeyEvent(SDL_SCANCODE_TAB);
				} else if (last_touchswipe.h < -TOUCH_ACTION_THRESHOLD_Y) { // up
					sendKeyEvent(ONS_SCANCODE_MUTE);
				} else {
					return false;
				}

				// Ignore later events for some time
				last_touchswipe_time = gesture.timestamp + MAX_TOUCH_SWIPE_TIMESPAN;
			}
		}
		return false;
	}

	//sendToLog(LogLevel::Error, "Finger prevention %d %d (num %d)\n",
	//			last_touchswipe_time, event.tfinger.timestamp, event.tfinger.fingerId);

	// Prevent extra clicks right after scrolling
	if (last_touchswipe_time + MAX_TOUCH_SWIPE_TIMESPAN >= event.tfinger.timestamp)
		return false;

	// fingerId contains grouped finger amount after tapping
	if (event.tfinger.fingerId == 1)
		btn_left = true;
	else if (event.tfinger.fingerId == 2)
		btn_right = true;
	else
		btn_middle = true;

	event_x = static_cast<int>(event.tfinger.x * window.script_width);
	event_y = static_cast<int>(event.tfinger.y * window.script_height);

	if (automode_flag)
		return checkClearAutomode(state, type_up);

	if (checkClearTrap(btn_left, btn_right))
		return true;

	state.buttonState.reset();
	state.buttonState.x         = event_x;
	state.buttonState.y         = event_y;
	state.buttonState.down_flag = false;

	if (checkClearSkip(state))
		return true;

	if (!mouseButtonDecision(state, btn_left, btn_right, btn_middle, type_up, type_down))
		return false;

	return checkClearVoice();
}

bool ONScripter::mouseScrollEvent(SDL_MouseWheelEvent &event, EventProcessingState &state) {
	last_wheelscroll = event.y;

	addToPostponedEventChanges("scroll scrollables", [this]() {
		for (auto scrollElem : sprites(SPRITE_LSP | SPRITE_LSP2)) {
			if (scrollElem->scrollable.h > 0 && scrollElem->scrollableInfo.respondsToMouseOver) {
				dynamicProperties.addSpriteProperty(scrollElem, scrollElem->id, scrollElem->type == SPRITE_LSP2, false,
				                                    SPRITE_PROPERTY_SCROLLABLE_Y, mouse_scroll_mul * last_wheelscroll, 100, 1, true);
				scrollElem->scrollableInfo.snapType = AnimationInfo::ScrollSnap::NONE;
			}
		}
	});

	if (event.y > 0 &&
	    ((event_mode & WAIT_TEXT_MODE) ||
	     (usewheel_flag && (event_mode & WAIT_BUTTON_MODE)))) {
		state.buttonState.set(-2);
	} else if (event.y < 0 &&
	           ((enable_wheeldown_advance_flag && (event_mode & WAIT_TEXT_MODE)) ||
	            (usewheel_flag && (event_mode & WAIT_BUTTON_MODE)))) {
		state.buttonState.set((event_mode & WAIT_TEXT_MODE) ? 0 : -3);
	} else
		return false;

	return checkClearVoice();
}

void ONScripter::shiftHoveredButtonInDirection(int diff) {
	// If we are in this function, our buttons are valid, and a valid default is set.
	int totalButtonCount           = getTotalButtonCount();
	ONScripter::ButtonLink *button = root_button_link.next;

	// If the last known hovered button number is nowhere to be found, then we need to set the link index to the default (0 unless declared with btnhover_d).
	if (buttonNumberToLinkIndex(lastKnownHoveredButtonNumber) == -1) {
		lastKnownHoveredButtonLinkIndex = buttonNumberToLinkIndex(hoveredButtonDefaultNumber);
	}

	auto newLinkIndex = lastKnownHoveredButtonLinkIndex;
	newLinkIndex += diff;
	if (newLinkIndex < 0)
		newLinkIndex = totalButtonCount - 1;
	else if (newLinkIndex >= totalButtonCount)
		newLinkIndex = 0;

	for (int i = 0; i < newLinkIndex; ++i) {
		button = button->next;
	}

	if (button) {
		// Trigger the same code that mouseOverCheck triggers on button hover.
		controlMode = ControlMode::Arrow;
		doHoverButton(true, button->no, newLinkIndex, button);
	}
}

int ONScripter::buttonNumberToLinkIndex(int buttonNo) {
	int totalButtons = getTotalButtonCount();
	ButtonLink *button{root_button_link.next};
	for (int i = 0; i < totalButtons; i++) {
		if (!button) {
			return -1;
		}
		if (button->no == buttonNo) {
			return i;
		}
		button = button->next;
	}
	return -1;
}

int ONScripter::getTotalButtonCount() const {
	int totalButtonCount = 0;
	ButtonLink *button   = root_button_link.next;
	while (button) {
		button = button->next;
		++totalButtonCount;
	}
	return totalButtonCount;
}

// returns true if should break out of the event loop
bool ONScripter::keyDownEvent(SDL_KeyboardEvent &event, EventProcessingState &state) {
	if (event_mode & WAIT_BUTTON_MODE)
		last_keypress = event.keysym.scancode;

	int last_ctrl_status = state.keyState.ctrl;

	// keyState.ctrl assignment can't be completely deferred due to caller requiring it; must at least pass the updates to caller
	switch (event.keysym.scancode) {
#ifdef MACOSX
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI:
			if (!ons_cfg_options.count("skip-on-cmd"))
				break;
			if (event.keysym.scancode == SDL_SCANCODE_LGUI || event.keysym.scancode == SDL_SCANCODE_RGUI) {
				state.keyState.apple |= 1;
				event.keysym.scancode = SDL_SCANCODE_LCTRL;
			}
#endif
		case SDL_SCANCODE_RCTRL:
		case SDL_SCANCODE_LCTRL:
			if (event.keysym.scancode == SDL_SCANCODE_LCTRL || event.keysym.scancode == SDL_SCANCODE_RCTRL)
				if (skipIsAllowed()) {
					state.keyState.ctrl |= (event.keysym.scancode == SDL_SCANCODE_LCTRL ? 0x02 : 0x01);
					internal_slowdown_counter = 0; //maybe a slightly wrong place to do it
				}
			if (!skipIsAllowed())
				break; //Skip not allowed, exit
			if (last_ctrl_status != state.keyState.ctrl) {
				skip_effect = true; // allow short-circuiting the current effect with ctrl
				if (video_skip_mode == VideoSkip::Normal) {
					request_video_shutdown = true;
				}
			}
			//Ctrl key: do skip in text
			if (event_mode & (WAIT_INPUT_MODE | WAIT_TEXTOUT_MODE | WAIT_TEXTBTN_MODE)) {
				state.buttonState.set(0);

				if (event_mode & WAIT_WAIT_MODE) {
					for (const auto &a : fetchConstantRefreshActions<WaitAction>()) a->terminate();
				}
				if (event_mode & WAIT_DELAY_MODE) {
					for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
				}

				addToPostponedEventChanges("play click voice", [this]() { playClickVoice(); });
				stopCursorAnimation(clickstr_state);
				return true;
			}
			if (event_mode & (WAIT_SLEEP_MODE)) {
				stopCursorAnimation(clickstr_state);
				return true;
			}
			break;
		case SDL_SCANCODE_RALT:
			state.keyState.opt |= 0x01;
			break;
		case SDL_SCANCODE_LALT:
			state.keyState.opt |= 0x02;
			break;
		case SDL_SCANCODE_RSHIFT:
			state.keyState.shift |= 0x01;
			break;
		case SDL_SCANCODE_LSHIFT:
			state.keyState.shift |= 0x02;
			break;
		default:
			break;
	}

	return false;
}

void ONScripter::keyUpEvent(SDL_KeyboardEvent &event, EventProcessingState &state) {
	if (event_mode & WAIT_BUTTON_MODE)
		last_keypress = event.keysym.scancode;

	switch (event.keysym.scancode) {
#ifdef MACOSX
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI:
			if (!ons_cfg_options.count("skip-on-cmd"))
				break;
			state.keyState.apple &= ~1;
#endif
		case SDL_SCANCODE_RCTRL:
			state.keyState.ctrl &= ~0x01;
			break;
		case SDL_SCANCODE_LCTRL:
			state.keyState.ctrl &= ~0x02;
			break;
		case SDL_SCANCODE_RALT:
			state.keyState.opt &= ~0x01;
			break;
		case SDL_SCANCODE_LALT:
			state.keyState.opt &= ~0x02;
			break;
		case SDL_SCANCODE_RSHIFT:
			state.keyState.shift &= ~0x01;
			break;
		case SDL_SCANCODE_LSHIFT:
			state.keyState.shift &= ~0x02;
			break;
		default:
			break;
	}
}

// returns true if should break out of the event loop
bool ONScripter::keyPressEvent(SDL_KeyboardEvent &event, EventProcessingState &state) {
	//reset the button state
	state.buttonState.reset();
	state.buttonState.down_flag = false;

	if (automode_flag)
		return checkClearAutomode(state, event.type == SDL_KEYUP);

	if (event.type == SDL_KEYUP) {
		//'m' is for mute (toggle)
		if (((event.keysym.scancode == SDL_SCANCODE_M && state.keyState.opt) ||
		     event.keysym.scancode == ONS_SCANCODE_MUTE) &&
		    !state.keyState.ctrl) {
			addToPostponedEventChanges("setVolumeMute", [this]() {
				if (!script_mute) {
					volume_on_flag = !volume_on_flag;
					setVolumeMute(!volume_on_flag);
					sendToLog(LogLevel::Info, "turned %s volume mute\n", !volume_on_flag ? "on" : "off");
				} else {
					sendToLog(LogLevel::Info, "disallowed atm");
				}
			});
		}

		if ((event.keysym.scancode == SDL_SCANCODE_E && state.keyState.opt) ||
		    event.keysym.scancode == ONS_SCANCODE_SCREEN) {
			needs_screenshot = true;
		}
	}

	// 's', Return, Enter, or Space will clear (regular) skip mode
	// Yes, just 's' without the modifiers to make it easier.
	if ((event.type == SDL_KEYUP) &&
	    (event.keysym.scancode == SDL_SCANCODE_RETURN ||
	     event.keysym.scancode == SDL_SCANCODE_KP_ENTER ||
	     event.keysym.scancode == SDL_SCANCODE_SPACE ||
	     event.keysym.scancode == SDL_SCANCODE_S ||
	     event.keysym.scancode == ONS_SCANCODE_SKIP)) {
		if (checkClearSkip(state))
			return true;
	}

	// i to spew some debug information
	/*if (event.type == SDL_KEYUP && event.keysym.scancode == SDL_SCANCODE_i) {
		sendToLog(LogLevel::Error, "Last executed command lines:\n");
		for (auto &log : script_h.debugCommandLog)
			sendToLog(LogLevel::Error, "%s\n", log.c_str());
	}*/

	if (checkClearTrap((event.keysym.scancode == SDL_SCANCODE_RETURN ||
	                    event.keysym.scancode == SDL_SCANCODE_KP_ENTER ||
	                    event.keysym.scancode == SDL_SCANCODE_SPACE),
	                   event.keysym.scancode == SDL_SCANCODE_ESCAPE))
		return true;

	//so many ways to 'left-click' a button
	if ((event_mode & WAIT_BUTTON_MODE) &&
	    (((event.type == SDL_KEYUP || btndown_flag) &&
	      ((!getenter_flag && event.keysym.scancode == SDL_SCANCODE_RETURN) ||
	       (!getenter_flag && event.keysym.scancode == SDL_SCANCODE_KP_ENTER))) ||
	     ((spclclk_flag || !useescspc_flag) &&
	      event.keysym.scancode == SDL_SCANCODE_SPACE))) {
		if (event.keysym.scancode == SDL_SCANCODE_RETURN ||
		    event.keysym.scancode == SDL_SCANCODE_KP_ENTER ||
		    (spclclk_flag && event.keysym.scancode == SDL_SCANCODE_SPACE)) {
			state.buttonState.set(hoveringButton ? hoveredButtonNumber : 0);
			if (event.type == SDL_KEYDOWN)
				state.buttonState.down_flag = true;
		} else {
			state.buttonState.set(0);
		}
		skip_effect = true;
		if (video_skip_mode == VideoSkip::Normal) {
			request_video_shutdown = true;
		}

		if (event_mode & WAIT_DELAY_MODE) {
			for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
		}

		addToPostponedEventChanges("play click voice", [this]() { playClickVoice(); });
		stopCursorAnimation(clickstr_state);
		return true;
	}

	if (event.type == SDL_KEYDOWN)
		return false;

	if ((event_mode & (WAIT_INPUT_MODE | WAIT_BUTTON_MODE)) &&
	    (autoclick_time == 0 || (event_mode & WAIT_BUTTON_MODE))) {
		//Esc is for 'right-click' (sometimes)
		if (!useescspc_flag && event.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			state.buttonState.set(-1);
		} else if (useescspc_flag && event.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			state.buttonState.set(-10);
		} else if (!spclclk_flag && useescspc_flag && event.keysym.scancode == SDL_SCANCODE_SPACE) {
			state.buttonState.set(-11);
		}
		//'h' or left-arrow for page-up
		else if (((!getcursor_flag && event.keysym.scancode == SDL_SCANCODE_LEFT) ||
		          event.keysym.scancode == SDL_SCANCODE_H) &&
		         ((event_mode & WAIT_TEXT_MODE) ||
		          (usewheel_flag && !getcursor_flag &&
		           (event_mode & WAIT_BUTTON_MODE)))) {
			state.buttonState.set(-2);
		}
		//'l' or right-arrow for page-down
		else if (((!getcursor_flag && event.keysym.scancode == SDL_SCANCODE_RIGHT) ||
		          event.keysym.scancode == SDL_SCANCODE_L) &&
		         ((enable_wheeldown_advance_flag &&
		           (event_mode & WAIT_TEXT_MODE)) ||
		          (usewheel_flag && (event_mode & WAIT_BUTTON_MODE)))) {
			if (event_mode & WAIT_TEXT_MODE) {
				state.buttonState.set(0);
			} else {
				state.buttonState.set(-3);
			}
		}
		//'k', 'p', or up-arrow for shift to mouseover next button
		else if (((!getcursor_flag && event.keysym.scancode == SDL_SCANCODE_UP) ||
		          event.keysym.scancode == SDL_SCANCODE_K ||
		          event.keysym.scancode == SDL_SCANCODE_P) &&
		         (event_mode & WAIT_BUTTON_MODE)) {
			addToPostponedEventChanges("shiftHoveredButtonInDirection", [this]() {
				shiftHoveredButtonInDirection(1);
			});
			return false;
		}
		//'j', 'n', or down-arrow for shift to mouseover previous button
		else if (((!getcursor_flag && event.keysym.scancode == SDL_SCANCODE_DOWN) ||
		          event.keysym.scancode == SDL_SCANCODE_J ||
		          event.keysym.scancode == SDL_SCANCODE_N) &&
		         (event_mode & WAIT_BUTTON_MODE)) {
			addToPostponedEventChanges("shiftHoveredButtonInDirection", [this]() {
				shiftHoveredButtonInDirection(-1);
			});
			return false;
		} else if (getcursor_flag && (event.keysym.scancode == SDL_SCANCODE_UP || event.keysym.scancode == SDL_SCANCODE_DOWN || event.keysym.scancode == SDL_SCANCODE_LEFT || event.keysym.scancode == SDL_SCANCODE_RIGHT) &&
		           ((enable_wheeldown_advance_flag && (event_mode & WAIT_TEXT_MODE)) ||
		            (usewheel_flag && (event_mode & WAIT_BUTTON_MODE)))) {
			addToPostponedEventChanges("change scrollable hovered element", [this, event]() {
				Direction d = getDirection(event.keysym.scancode);
				for (auto sptr : sprites(SPRITE_LSP | SPRITE_LSP2)) {
					if (sptr->visible && sptr->exists && sptr->scrollableInfo.isSpecialScrollable)
						changeScrollableHoveredElement(sptr, d);
				}
			});
		} else if (getpageup_flag && (event.keysym.scancode == SDL_SCANCODE_PAGEUP)) {
			state.buttonState.set(-12);
		} else if (getpagedown_flag && (event.keysym.scancode == SDL_SCANCODE_PAGEDOWN)) {
			state.buttonState.set(-13);
		} else if ((getenter_flag && (event.keysym.scancode == SDL_SCANCODE_RETURN)) ||
		           (getenter_flag && (event.keysym.scancode == SDL_SCANCODE_KP_ENTER))) {
			state.buttonState.set(-19);
		} else if (gettab_flag && (event.keysym.scancode == SDL_SCANCODE_TAB)) {
			state.buttonState.set(-20);
		} else if (getcursor_flag && (event.keysym.scancode == SDL_SCANCODE_UP)) {
			state.buttonState.set(-40);
		} else if (getcursor_flag && (event.keysym.scancode == SDL_SCANCODE_RIGHT)) {
			state.buttonState.set(-41);
		} else if (getcursor_flag && (event.keysym.scancode == SDL_SCANCODE_DOWN)) {
			state.buttonState.set(-42);
		} else if (getcursor_flag && (event.keysym.scancode == SDL_SCANCODE_LEFT)) {
			state.buttonState.set(-43);
		} else if (getinsert_flag && (event.keysym.scancode == SDL_SCANCODE_INSERT)) {
			state.buttonState.set(-50);
		} else if (getzxc_flag && (event.keysym.scancode == SDL_SCANCODE_Z)) {
			state.buttonState.set(-51);
		} else if (getzxc_flag && (event.keysym.scancode == SDL_SCANCODE_X)) {
			state.buttonState.set(-52);
		} else if (getzxc_flag && (event.keysym.scancode == SDL_SCANCODE_C)) {
			state.buttonState.set(-53);
		} else if (getfunction_flag) {
			if (event.keysym.scancode == SDL_SCANCODE_F1)
				state.buttonState.set(-21);
			else if (event.keysym.scancode == SDL_SCANCODE_F2)
				state.buttonState.set(-22);
			else if (event.keysym.scancode == SDL_SCANCODE_F3)
				state.buttonState.set(-23);
			else if (event.keysym.scancode == SDL_SCANCODE_F4)
				state.buttonState.set(-24);
			else if (event.keysym.scancode == SDL_SCANCODE_F5)
				state.buttonState.set(-25);
			else if (event.keysym.scancode == SDL_SCANCODE_F6)
				state.buttonState.set(-26);
			else if (event.keysym.scancode == SDL_SCANCODE_F7)
				state.buttonState.set(-27);
			else if (event.keysym.scancode == SDL_SCANCODE_F8)
				state.buttonState.set(-28);
			else if (event.keysym.scancode == SDL_SCANCODE_F9)
				state.buttonState.set(-29);
			else if (event.keysym.scancode == SDL_SCANCODE_F10)
				state.buttonState.set(-30);
			else if (event.keysym.scancode == SDL_SCANCODE_F11)
				state.buttonState.set(-31);
			else if (event.keysym.scancode == SDL_SCANCODE_F12)
				state.buttonState.set(-32);
		}
		if (state.buttonState.valid_flag) {
			stopCursorAnimation(clickstr_state);
			return true;
		}
	};

	//catch 'left-button click' that fell through?
	if ((event_mode & WAIT_INPUT_MODE) && !state.keyState.pressedFlag &&
	    (autoclick_time == 0 || (event_mode & WAIT_BUTTON_MODE))) {
		//check for "button click"
		if (event.keysym.scancode == SDL_SCANCODE_RETURN ||
		    event.keysym.scancode == SDL_SCANCODE_KP_ENTER ||
		    event.keysym.scancode == SDL_SCANCODE_SPACE) {
			state.keyState.pressedFlag = true;
			skip_effect                = true;
			if (video_skip_mode == VideoSkip::Normal) {
				request_video_shutdown = true;
			}
			state.buttonState.set(0);

			if (event_mode & WAIT_DELAY_MODE) {
				for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
			}

			addToPostponedEventChanges("play click voice", [this]() { playClickVoice(); });
			stopCursorAnimation(clickstr_state);

			return true;
		}
	}

	if ((event_mode & (WAIT_INPUT_MODE | WAIT_TEXTBTN_MODE | WAIT_TEXTOUT_MODE)) &&
	    !state.keyState.pressedFlag) {
		//'s' is for skip mode
		if (((event.keysym.scancode == SDL_SCANCODE_S && state.keyState.opt) || event.keysym.scancode == ONS_SCANCODE_SKIP) &&
		    !automode_flag && !state.keyState.ctrl && skipIsAllowed()) {
			if (!(state.skipMode & SKIP_NORMAL))
				skip_effect = true; // short-circuit a current effect
			state.skipMode |= SKIP_NORMAL;
			internal_slowdown_counter = 0; //maybe a slightly wrong place to do it
			                               //if (event.keysym.scancode == SDL_SCANCODE_D) state.skipMode |= SKIP_SUPERSKIP; // rocket engines engaged
			//sendToLog(LogLevel::Info, "toggle skip to true\n");
			state.keyState.pressedFlag = true;
			if (video_skip_mode == VideoSkip::Normal) {
				request_video_shutdown = true;
			}
			state.buttonState.set(0);

			if (event_mode & WAIT_WAIT_MODE) {
				for (const auto &a : fetchConstantRefreshActions<WaitAction>()) a->terminate();
			}
			if (event_mode & WAIT_DELAY_MODE) {
				for (const auto &a : fetchConstantRefreshActions<DelayAction>()) a->terminate();
			}

			stopCursorAnimation(clickstr_state);

			return true;
		}
		//'a' is for automode
		if (event.keysym.scancode == SDL_SCANCODE_A && !state.keyState.ctrl && mode_ext_flag && !automode_flag) {
			addToPostponedEventChanges([this]() { eventCallbackRequired = true; automode_flag = true; });
			state.skipMode &= ~SKIP_NORMAL;
			sendToLog(LogLevel::Info, "change to automode\n");
			state.keyState.pressedFlag = true;
			state.buttonState.set(0);
			stopCursorAnimation(clickstr_state);

			return true;
		}
	}

#if !defined(IOS) && !defined(DROID)
	//'f' is for fullscreen toggle
	if (event.keysym.scancode == SDL_SCANCODE_F && !state.keyState.ctrl) {
		addToPostponedEventChanges("change window mode", []() {
			window.changeMode(true, false, !window.getFullscreen());
		});
	}
#endif

	//using insani's skippable wait
	if ((event_mode & WAIT_SLEEP_MODE) && (event.keysym.scancode == SDL_SCANCODE_S || event.keysym.scancode == ONS_SCANCODE_SKIP) && skipIsAllowed()) {
		state.skipMode |= SKIP_TO_WAIT;
		state.skipMode &= ~SKIP_NORMAL;
		state.keyState.pressedFlag = true;
	}
	if ((state.skipMode & SKIP_TO_WAIT) &&
	    (event.keysym.scancode == SDL_SCANCODE_RETURN ||
	     event.keysym.scancode == SDL_SCANCODE_KP_ENTER ||
	     event.keysym.scancode == SDL_SCANCODE_SPACE)) {
		state.skipMode &= ~SKIP_TO_WAIT;
		state.keyState.pressedFlag = true;
	}
	if ((event_mode & WAIT_TEXTOUT_MODE) && skipIsAllowed() &&
	    (event.keysym.scancode == SDL_SCANCODE_RETURN ||
	     event.keysym.scancode == SDL_SCANCODE_KP_ENTER ||
	     event.keysym.scancode == SDL_SCANCODE_SPACE)) {
		state.skipMode |= (SKIP_TO_WAIT | SKIP_TO_EOL);
		state.keyState.pressedFlag = true;
	}

	if ((event.keysym.scancode == SDL_SCANCODE_F1) && (version_str != nullptr)) {
		//F1 is for Help (on Windows), so show the About dialog box
		addToPostponedEventChanges("display message box", [this]() {
			window.showSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "About", version_str);
		});

		state.keyState.pressedFlag = true;
	}

	return false;
}

void ONScripter::translateKeyDownEvent(SDL_Event &event, EventProcessingState &state, bool &ret, bool ctrl_toggle) {
	if (state.skipMode & SKIP_SUPERSKIP)
		return;
	if (event.key.type == SDL_JOYBUTTONDOWN) {
		event.key.type            = SDL_KEYDOWN;
		event.key.keysym.scancode = joyCtrl.transButton(event.jbutton.button, event.jbutton.which);
		if (event.key.keysym.scancode == SDL_SCANCODE_UNKNOWN)
			return;
	}

	ret                  = keyDownEvent(event.key, state);
	bool new_ctrl_toggle = ctrl_toggle ^ (state.keyState.ctrl != 0);
	//allow skipping sleep waits with start of ctrl keydown
	ret |= (event_mode & WAIT_SLEEP_MODE) && new_ctrl_toggle;
	if (btndown_flag)
		ret |= keyPressEvent(event.key, state);
	addToPostponedEventChanges([this, state]() {
		keyState             = state.keyState;
		current_button_state = state.buttonState;
		skip_mode            = state.skipMode;
	});
	if (skip_mode != state.skipMode || keyState.ctrl != state.keyState.ctrl)
		eventCallbackRequired = true;
}

void ONScripter::translateKeyUpEvent(SDL_Event &event, EventProcessingState &state, bool &ret) {
	if (state.skipMode & SKIP_SUPERSKIP)
		return;
	if (event.key.type == SDL_JOYBUTTONUP) {
		event.key.type            = SDL_KEYUP;
		event.key.keysym.scancode = joyCtrl.transButton(event.jbutton.button, event.jbutton.which);
		if (event.key.keysym.scancode == SDL_SCANCODE_UNKNOWN)
			return;
	} else if (event.key.type == SDL_JOYHATMOTION) {
		event.key.type            = SDL_KEYUP;
		event.key.keysym.scancode = joyCtrl.transHat(event.jhat.value, event.jhat.which);
		if (event.key.keysym.scancode == SDL_SCANCODE_UNKNOWN)
			return;
	}

	keyUpEvent(event.key, state);
	ret = keyPressEvent(event.key, state);
	addToPostponedEventChanges([this, state]() {
		keyState             = state.keyState;
		current_button_state = state.buttonState;
		skip_mode            = state.skipMode;
	});
	if (skip_mode != state.skipMode || keyState.ctrl != state.keyState.ctrl)
		eventCallbackRequired = true;
}

bool ONScripter::mainThreadDowntimeProcessing(bool essentialProcessingOnly) {

	bool didSomething{false};

	// Load chunk call
	// Check loadGPUImageByChunks if you want to use this.
	/*if (imageLoader.isActive && !imageLoader.isLoaded) {
		imageLoader.loadChunk();
		didSomething = true;
	}*/

	if (allow_rendering && !essentialProcessingOnly) {
		didSomething |= gpu.handleScheduledJobs();
	}

	return didSomething;
}

void ONScripter::handleRegisteredActions(uint64_t ns) {
	Lock lock(&ons.registeredCRActions);
	auto action = registeredCRActions.begin();
	while (action != registeredCRActions.end()) {
		std::shared_ptr<ConstantRefreshAction> a = *action;
		a->advance(ns);
		if (a->terminated || a->expired()) {
			a->onExpired();
			action = registeredCRActions.erase(action);
			continue;
		} else {
			a->run();
		}
		++action;
	}
}

void ONScripter::advanceGameState(uint64_t ns) {
	handleRegisteredActions(ns);
	camera.update(static_cast<unsigned int>(ns / 1000000));

	// update animation clocks
	advanceAIclocks(ns);

	// should we make this a function?
	for (auto &ss : spritesets) {
		if (ss.second.warpAmplitude != 0) {
			ss.second.warpClock.tickNanos(ns);
			fillCanvas(true, true);
			flush(refreshMode());
		}
	}

	if (warpAmplitude != 0) {
		warpClock.tickNanos(ns);
		fillCanvas(true, true);
		flush(refreshMode());
	}

	dlgCtrl.advanceDialogueRendering(ns);

	dynamicProperties.advanceNanos(ns);
	dynamicProperties.apply();
}

void ONScripter::constantRefresh() {

	if (proceedAnimation() >= 0) {
		flush(refreshMode() |
		          (draw_cursor_flag ? REFRESH_CURSOR_MODE : 0) |
		          REFRESH_BEFORESCENE_MODE,
		      &before_dirty_rect_scene.bounding_box_script,
		      &before_dirty_rect_hud.bounding_box_script,
		      false, true);
	}

	bool effectIsOver = false;
	if (effect_current) {
		if (!effect_set) {
			bool terminateEffect = setEffect();
			if (terminateEffect)
				effect_current = nullptr;
			else {
				effect_set = true;
				if (effectskip_flag) {
					if (!skip_enabled)
						event_mode |= WAIT_INPUT_MODE;
					skip_effect = false;
				}
			}
		}
	}
	if (effect_current) {
		if (effectskip_flag && skip_effect && skip_enabled) {
			effect_counter = effect_duration;
			fillCanvas();
		}
		effectIsOver = !doEffect();
		/*sendToLog(LogLevel::Info, "effect_current: %p, effect_set: %i, effectIsOver: %i, pre_screen_render %i, constant_refresh_mode %i\n", effect_current, effect_set, effectIsOver, pre_screen_render, constant_refresh_mode);*/
	}

	GPU_Rect *hud_rect, *scene_rect;

	if (effectIsOver) {
		hud_rect = scene_rect = nullptr;
	} else if (!effect_current) {
		hud_rect   = &before_dirty_rect_hud.bounding_box_script;
		scene_rect = &before_dirty_rect_scene.bounding_box_script;
	} else {
		// ... do we actually use these rects in the case of effect_current?
		hud_rect   = &dirty_rect_hud.bounding_box_script;
		scene_rect = &dirty_rect_scene.bounding_box_script;
	}

	if (effect_current) {
		if (!pre_screen_render && !effectIsOver)
			errorAndExit("Neither pre_screen_render nor effectIsOver are set during the effect");
		// It is OK to pass refresh modes in here while effect is ongoing, because pre_screen_render should be set here, therefore, nothing new will be created
		// In fact, even REFRESH_BEFORESCENE_MODE is not needed until last_call
		flush(CONSTANT_REFRESH_MODE | REFRESH_BEFORESCENE_MODE, scene_rect, hud_rect, effect_rect_cleanup, false);
	} else if (display_mode & DISPLAY_MODE_TEXT) {
		//When we are in DISPLAY_MODE_TEXT (and normal mode) we don't clear our rects.
		//This is incorrect (due to animations/quakes) for cr. Make sure we at least have this part in CR
		addTextWindowClip(before_dirty_rect_hud);
		//Our CR mode is always resetted due to specific style of CR.
		//alphaBlendText gives proper hud_gpu to us, but we (may) update it with our cursors
		if (constant_refresh_mode != REFRESH_NONE_MODE)
			constant_refresh_mode |= (REFRESH_TEXT_MODE | REFRESH_WINDOW_MODE);
		flush(constant_refresh_mode | CONSTANT_REFRESH_MODE | REFRESH_BEFORESCENE_MODE, scene_rect, hud_rect, true, false);
	} else {
		flush(constant_refresh_mode | CONSTANT_REFRESH_MODE | REFRESH_BEFORESCENE_MODE, scene_rect, hud_rect, true, false);
	}

	if (effectIsOver) {
		effect_current = nullptr;
		event_mode &= ~(WAIT_INPUT_MODE);
	}

	constant_refresh_mode     = REFRESH_NONE_MODE;
	constant_refresh_executed = true;
}

ONScripter::EventProcessingState::EventProcessingState(unsigned int _handler) {
	keyState    = ons.keyState;
	buttonState = ons.current_button_state;
	skipMode    = ons.skip_mode;
	eventMode   = ons.event_mode;
	handler     = _handler;
}

void ONScripter::runEventLoop() {
	Lock lock(&ons.registeredCRActions);

	std::unique_ptr<SDL_Event> event;
	bool started_in_automode = automode_flag;

	while (true) {
		event = std::move(localEventQueue.back());
		localEventQueue.pop_back();

		endOfEventBatch = false;

		if (exitCode.load(std::memory_order_relaxed) != ExitType::None) {
			ons.requestQuit(exitCode);
			return; //dummy
		}

		bool ret{false};
		bool ctrl_toggle{keyState.ctrl != 0};
		bool chunk_reported_return{false};

		bool mouseMotionHandlingDone{false};
		int defaultEventMode{event_mode};

		for (unsigned int handler = 0; handler <= registeredCRActions.size(); handler++) {
			ret = false;
			if (handler == registeredCRActions.size()) {
				event_mode = defaultEventMode;
				if (isWaitingForUserInput() || isWaitingForUserInterrupt()) {
					auto got = inputEventList.find(event->type);
					if (got != inputEventList.end()) {
						// There should be more, I think
						assert(!((event_mode & WAIT_BUTTON_MODE) || ((event_mode & WAIT_INPUT_MODE) && !effect_current)));
						continue;
					}
				}
			} else {
				event_mode = registeredCRActions[handler]->eventMode();
				if (!registeredCRActions[handler]->handledEvents().count(event->type)) {
					// this event type is not handled by this handler
					continue;
				}
			}

			// Handle event with this event_mode
			{
				EventProcessingState state(handler);

				switch (event->type) {
					case SDL_MOUSEMOTION:
						if (mouseMotionHandlingDone)
							break;
						mouseMotionHandlingDone = ret = mouseMoveEvent(event->motion, state);
						addToPostponedEventChanges([this, state]() {
							current_button_state = state.buttonState;
							if (cursorAutoHide) {
								lastCursorMove = ticksNow;
								cursorState(true);
							}
						});
						break;

#if defined(IOS) || defined(DROID)
					case SDL_MULTIGESTURE:
						// Such a thing called crapdroid sends erratic move events on move attempts
						// with a distance of less than 0.00X smth. Here we try to ignore them to some level,
						// since we use gesture events to protect us from accidental r-click (double-tap) during
						/// the scrolling.
						if (std::fabs(event->mgesture.dDist) < 0.01 && std::fabs(event->mgesture.dTheta) < 0.01)
							break;
					case SDL_FINGERDOWN:
						if (event->type == SDL_FINGERDOWN && !btndown_flag)
							break;
					case SDL_FINGERUP:
						if (state.skipMode & SKIP_SUPERSKIP)
							break;
						ret = touchEvent(*event, state);
						addToPostponedEventChanges([this, state]() { current_button_state = state.buttonState; skip_mode = state.skipMode; });
						break;
#else

					case SDL_MOUSEBUTTONDOWN:
						if (!btndown_flag)
							break;
						/* fall through */
					case SDL_MOUSEBUTTONUP:
						if (state.skipMode & SKIP_SUPERSKIP)
							break;
						ret = mousePressEvent(event->button, state);
						addToPostponedEventChanges([this, state]() { current_button_state = state.buttonState; skip_mode = state.skipMode; });
						break;

					case SDL_MOUSEWHEEL:
						ret = mouseScrollEvent(event->wheel, state);
						addToPostponedEventChanges([this, state]() { current_button_state = state.buttonState; });
						break;
#endif

					case SDL_JOYBUTTONDOWN:
					case SDL_KEYDOWN:
						translateKeyDownEvent(*event, state, ret, ctrl_toggle);
						break;

					case SDL_JOYHATMOTION:
					case SDL_JOYBUTTONUP:
					case SDL_KEYUP:
						translateKeyUpEvent(*event, state, ret);
						break;

					case SDL_JOYAXISMOTION: {
#if !defined(IOS) && !defined(DROID)
						auto ke = joyCtrl.transAxis(event->jaxis);
						if (ke.key.keysym.scancode != SDL_SCANCODE_UNKNOWN) {
							if (ke.type == SDL_KEYDOWN)
								translateKeyDownEvent(ke, state, ret, ctrl_toggle);
							else
								translateKeyUpEvent(ke, state, ret);
						}
#endif
						break;
					}

					case ONS_EVENT_BATCH_END:
						endOfEventBatch = true;
						ret             = true;
						break;

					case ONS_CHUNK_EVENT:
						flushEventSub(*event);
						//sendToLog(LogLevel::Info, "ONS_CHUNK_EVENT %d: %x %d %x\n", event.user.code, wave_sample[0], automode_flag, event_mode);
						if (event->user.code != 0 || !(event_mode & WAIT_VOICE_MODE))
							break;
						event_mode &= ~WAIT_VOICE_MODE;

						chunk_reported_return = true;
						// Falls through -- will return from waitEvent (prematurely) after doing a final UPKEEP

					case ONS_UPKEEP_EVENT:
						if ((event_mode & WAIT_VOICE_MODE) && wave_sample[0] && Mix_Playing(0) && !Mix_Paused(0)) {
							break;
						}

						if (!automode_flag && started_in_automode && clickstr_state != CLICK_NONE) {
							started_in_automode = false;
							break;
						}

						if ((event_mode & (WAIT_INPUT_MODE | WAIT_BUTTON_MODE)) &&
						    (clickstr_state == CLICK_WAIT || clickstr_state == CLICK_NEWPAGE)) {
							playClickVoice();
							stopCursorAnimation(clickstr_state);
						}
						ret = chunk_reported_return;
						break; //will return right after this event in ONS_EVENT_BATCH_END, possibly breaks a call from fade event

#if defined(IOS) || defined(DROID)
					case SDL_APP_WILLENTERBACKGROUND:
						// This gets called when the user hits the home button, or gets a call.
						window.setActiveState(false);
						allow_rendering = false;
						sendToLog(LogLevel::Info, "Entering background\n");
						break;
					case SDL_APP_DIDENTERBACKGROUND:
						sendToLog(LogLevel::Info, "Entered background\n");
						break;
					case SDL_APP_WILLENTERFOREGROUND:
						sendToLog(LogLevel::Info, "Leaving background\n");
						break;
					case SDL_APP_DIDENTERFOREGROUND:
						// Your app is interactive and getting CPU again.
						window.setActiveState(true);
						allow_rendering = true;
						before_dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
						sendToLog(LogLevel::Info, "Left background\n");
						break;
					case SDL_APP_LOWMEMORY:
						sendToLog(LogLevel::Info, "Received low memory warning\n");
						break;
#endif

					case SDL_USEREVENT:
						if (event->user.code == ONS_MUSIC_EVENT ||
						    event->user.code == ONS_SEQMUSIC_EVENT)
							flushEventSub(*event);
						break;

					case SDL_WINDOWEVENT:
#ifdef MACOSX
						// OS X specific: We are done exiting fullscreen mode and the animation has finished
						if (event->window.event == SDL_WINDOWEVENT_RESTORED && window.getFullscreenFix() && !window.getFullscreen()) {
							if (window.changeMode(false, true))
								fillCanvas(true, true);
						}
						// OS X specific: We are done entering fullscreen mode and the animation has finished
						// Note: this may fail to do its work, if the latter block is not present, but we are guranteed to get
						// SDL_WINDOWEVENT_MAXIMIZED as a last event in entering fullscreen, so we need it to disable getFullscreenFix()
						else if (event->window.event == SDL_WINDOWEVENT_MAXIMIZED && window.getFullscreenFix() && window.getFullscreen()) {
							if (window.changeMode(false, true))
								fillCanvas(true, true);
							// OS X specific: We are entering/leaving fullscreen mode and window resizing is in progress
						} else if (event->window.event == SDL_WINDOWEVENT_RESIZED) { // Fired by SDL when backing scale factor changes
							addToPostponedEventChanges("backing scale factor changed", []() {
								if (window.changeMode(false, true, window.getFullscreen()))
									ons.fillCanvas(true, true);
							});
						}
#else
						// At least Windows and Linux want us to act on SDL_WINDOWEVENT_EXPOSED
						if (event->window.event == SDL_WINDOWEVENT_EXPOSED && window.getFullscreenFix()) {
							if (window.changeMode(false, true))
								fillCanvas(true, true);
						}
#endif
						// At least Linux specific: We are showing some window part that was hidden before
						else if (event->window.event == SDL_WINDOWEVENT_EXPOSED || event->window.event == SDL_WINDOWEVENT_MOVED) {
							// Now that we have commands like textoff2 we are not allowed to recklessly update hud
							before_dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
							//fillCanvas(false, true);
						}

						break;
					case SDL_QUIT:
						endCommand();
						break;
					default:
						break;
				}

				// WARNING: These may be in an improper place, particularly buttonWaitAction.
				// If you intend to respond to a click, put it in mousePressEvent, etc.
				if (handler < registeredCRActions.size()) {
					auto *bma = dynamic_cast<ButtonMonitorAction *>(registeredCRActions[handler].get());
					if (bma) {
						if (state.buttonState.valid_flag)
							bma->buttonState = state.buttonState;
					}
					auto *bwa = dynamic_cast<ButtonWaitAction *>(registeredCRActions[handler].get());
					if (bwa) {
						if (state.buttonState.valid_flag) {
							// Regardless of wait-for-voice or not, buttons should always terminate a ButtonWaitAction. (Unless it's async.)
							bwa->buttonState = state.buttonState;
							registeredCRActions[handler]->terminate();
						} else if (bwa->eventMode() & WAIT_VOICE_MODE && !(bwa->eventMode() & WAIT_TIMER_MODE) && !bwa->timer_set) {
							// This is a wait-for-voice.
							// When the voice ends, we are expected to expire the wait, or otherwise set up a timer that will expire it later.
							// (If we already went through this code once to set up a timer, then we don't need to do this, of course.)
							if (!(wave_sample[0] && Mix_Playing(0) && !Mix_Paused(0))) {
								// The voice has ended.
								// Is there an additional delay that we're supposed to wait for?
								int32_t additionalWaitTime{0};
								if (!ignore_voicedelay) {
									if (bwa->voiced_txtbtnwait && voicedelay_time)
										additionalWaitTime = voicedelay_time;
									if (bwa->final_voiced_txtbtnwait && final_voicedelay_time)
										additionalWaitTime = final_voicedelay_time;
								}
								// If there's no delay, this will expire immediately. (Same as terminate.)
								bwa->clock.setCountdown(additionalWaitTime);
								bwa->timer_set = true;
							}
						}
					}
				}
			}
		}

		// Execute all postponed changes
		for (auto &f : postponedEventChanges) f();
		postponedEventChanges.clear();
		postponedEventChangeLabels.clear();

		// Only return based on the final default handler
		if (ret)
			return;
	}
}
