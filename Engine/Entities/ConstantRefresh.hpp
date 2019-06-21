/**
 *  ConstantRefresh.hpp
 *  ONScripter-RU.
 *
 *  Constant refresh support and its actions.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/DynamicProperty.hpp"
#include "Support/KeyState.hpp"
#include "Support/Clock.hpp"

#include <SDL2/SDL.h>

#include <unordered_set>
#include <deque>
#include <memory>
#include <iostream>

const int ONS_UPKEEP_EVENT{SDL_USEREVENT + 2};
const int ONS_EVENT_BATCH_END{SDL_USEREVENT + 3};
const int ONS_CHUNK_EVENT{SDL_USEREVENT + 4};

enum {
	REFRESH_NONE_MODE        = 0,
	REFRESH_NORMAL_MODE      = 1,
	REFRESH_SAYA_MODE        = 2,
	REFRESH_WINDOW_MODE      = 4,  //show textwindow background
	REFRESH_TEXT_MODE        = 8,  //show textwindow text
	REFRESH_CURSOR_MODE      = 16, //show textwindow cursor
	CONSTANT_REFRESH_MODE    = 32,
	REFRESH_BEFORESCENE_MODE = 64, // refresh based on ai->old_ai
	REFRESH_SOMETHING        = REFRESH_NORMAL_MODE |
	                    REFRESH_SAYA_MODE |
	                    REFRESH_WINDOW_MODE |
	                    REFRESH_TEXT_MODE |
	                    REFRESH_CURSOR_MODE
};

const std::unordered_set<int> inputEventList{
    SDL_MOUSEWHEEL,
    SDL_FINGERDOWN,
    SDL_FINGERUP,
    SDL_MULTIGESTURE,
    SDL_MOUSEBUTTONDOWN,
    SDL_MOUSEBUTTONUP,
    SDL_KEYDOWN,
    SDL_KEYUP,
    SDL_JOYHATMOTION,
    SDL_JOYBUTTONDOWN,
    SDL_JOYBUTTONUP,
    SDL_JOYAXISMOTION};

extern std::deque<std::function<void()>> postponedEventChanges;     // contains fns to make changes to global state, put to while processing each event
extern std::unordered_set<const char *> postponedEventChangeLabels; // contains unique labels to prevent multiple adding of events that should be run only once
void addToPostponedEventChanges(const std::function<void()> &f);
void addToPostponedEventChanges(const char *str, const std::function<void()> &f);

// abstract base class
class ConstantRefreshAction {
public:
	Clock clock;
	bool terminated{false};
	bool createdDuringDialogueInline{false};
	int event_mode{0};
	virtual int eventMode() {
		return event_mode;
	}
	virtual bool expired() = 0;
	virtual void run() {}
	virtual void advance(uint64_t ns) {
		clock.tickNanos(ns);
	}
	virtual void onExpired();
	virtual void terminate() {
		terminated = true;
	}
	virtual bool suspendsMainScript() {
		return !createdDuringDialogueInline;
	}
	virtual bool suspendsDialogue() {
		return createdDuringDialogueInline;
	}
	virtual std::unordered_set<int> handledEvents() {
		return std::unordered_set<int>();
	}
	virtual void initialize();
	virtual ~ConstantRefreshAction() = default;

protected:
	ConstantRefreshAction() {}
};

std::vector<std::shared_ptr<ConstantRefreshAction>> getConstantRefreshActions();
std::shared_ptr<ConstantRefreshAction> currentAction(unsigned int handler);
bool isWaitingForUserInput();
bool isWaitingForUserInterrupt();

template <class T>
class TypedConstantRefreshAction : public ConstantRefreshAction {
public:
	static T *create() {
		T *ret{new T()};
		ret->initialize();
		return ret;
	}
	static bool isCurrent(unsigned int handler) {
		auto cur = currentAction(handler);
		return !!cur && dynamic_cast<T *>(cur.get());
	}
};

template <class T>
static std::deque<std::shared_ptr<ConstantRefreshAction>> fetchConstantRefreshActions() {
	static_assert(std::is_base_of<ConstantRefreshAction, T>::value, "fetchEvents assertion failure: event type must extend ConstantRefreshAction");
	std::deque<std::shared_ptr<ConstantRefreshAction>> ret;
	for (const auto &a : getConstantRefreshActions()) {
		if (dynamic_cast<T *>(a.get())) {
			ret.push_back(a);
		}
	}
	return ret;
}

template <class T>
class AbstractWaitAction : public TypedConstantRefreshAction<T> {
public:
	int advanceProperties{0};
	std::unordered_set<int> handledEvents() override {
		return inputEventList;
	}
	bool expired() override {
		return this->clock.expired();
	}
	void onExpired() override {
		ConstantRefreshAction::onExpired();
		dynamicProperties.advance(advanceProperties); // advance the time we skipped
		dynamicProperties.apply();
	}
};

class WaitAction : public AbstractWaitAction<WaitAction> {};
class DelayAction : public AbstractWaitAction<DelayAction> {};
class WaitTimerAction : public AbstractWaitAction<WaitTimerAction> {};

class WaitVoiceAction : public TypedConstantRefreshAction<WaitVoiceAction> {
	bool countDownStarted{false};

public:
	std::unordered_set<int> handledEvents() override {
		std::unordered_set<int> set{inputEventList};
		set.insert(ONS_CHUNK_EVENT);
		return set;
	}
	uint32_t voiceDelayMs{0};
	bool expired() override;
};

class QueuedSoundAction : public TypedConstantRefreshAction<QueuedSoundAction> {
	bool countDownStarted{false};

public:
	std::unordered_set<int> handledEvents() override {
		return std::unordered_set<int>();
	}
	int32_t ch{-1};
	uint32_t soundDelayMs{0};
	// Not using std::function due to clang+armv7s alignment bug
	void (*func)(){nullptr};
	bool suspendsMainScript() override {
		return false;
	}
	bool suspendsDialogue() override {
		return false;
	}
	bool expired() override;
	void onExpired() override;
};

class ButtonWaitAction : public AbstractWaitAction<ButtonWaitAction> {
public:
	uint32_t button_timer_start;
	std::shared_ptr<void> variableInfo;
	ButtonState buttonState;
	bool del_flag{false};
	bool timer_set{false};
	bool voiced_txtbtnwait{false};
	bool final_voiced_txtbtnwait{false};
	bool expired() override {
		return timer_set && clock.expired();
	}
	void onExpired() override;
	std::unordered_set<int> handledEvents() override {
		auto ret = AbstractWaitAction::handledEvents();
		ret.insert(SDL_MOUSEMOTION);
		//FIXME: design-wise there should be some condition (WAIT_VOICE_MODE?)
		ret.insert(ONS_CHUNK_EVENT);
		return ret;
	}
};

class ButtonMonitorAction : public TypedConstantRefreshAction<ButtonMonitorAction> {
public:
	bool suspendsMainScript() override {
		return false;
	}
	bool suspendsDialogue() override {
		return false;
	}
	std::unordered_set<int> handledEvents() override {
		auto ret = inputEventList;
		ret.insert(SDL_MOUSEMOTION);
		//FIXME: design-wise there should be some condition (WAIT_VOICE_MODE?)
		ret.insert(ONS_CHUNK_EVENT);
		return ret;
	}
	bool expired() override { return false; }
	void keepAlive() { terminated = false; }
	ButtonState buttonState;
};
