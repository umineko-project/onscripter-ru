/**
 *  ConstantRefresh.cpp
 *  ONScripter-RU.
 *
 *  Constant refresh support and its actions.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Entities/ConstantRefresh.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"

#include <algorithm>
#include <utility>

std::deque<std::function<void()>> postponedEventChanges;
std::unordered_set<const char *> postponedEventChangeLabels;

// If you are not modifying ons.registeredCRActions, you should use this function instead of
// ons.registeredCRActions to access that field, because it will ensure thread safety
// by giving you a copy instead.
// Note that the contents may not be thread-safe.
std::vector<std::shared_ptr<ConstantRefreshAction>> getConstantRefreshActions() {
	Lock lock(&ons.registeredCRActions);
	return ons.registeredCRActions;
}

// Called from event loop, so already locked.
std::shared_ptr<ConstantRefreshAction> currentAction(unsigned int handler) {
	if (handler >= getConstantRefreshActions().size())
		return nullptr;
	return getConstantRefreshActions()[handler];
}

void addToPostponedEventChanges(const std::function<void()> &f) {
	postponedEventChanges.push_back(f);
}

void addToPostponedEventChanges(const char *str, const std::function<void()> &f) {
	if (postponedEventChangeLabels.count(str))
		return;
	postponedEventChangeLabels.insert(str);
	addToPostponedEventChanges(f);
}

void ConstantRefreshAction::onExpired() {
	if (suspendsDialogue()) {
		//sendToLog(LogLevel::Info, "inline dialogue event\n");
		if (dlgCtrl.loanExecutionActive) {
			dlgCtrl.events.emplace_get().loanExecEnd = true; // otherwise, the current dialoguePart is done
		} else {
			dlgCtrl.events.emplace();
			dlgCtrl.scriptState.useDialogue();
			dlgCtrl.scriptState.disposeDialogue();
		}
	}
}

void ConstantRefreshAction::initialize() {
	createdDuringDialogueInline = dlgCtrl.executingDialogueInlineCommand;
	if (suspendsDialogue())
		dlgCtrl.waitForAction();
}

void ButtonWaitAction::onExpired() {
	AbstractWaitAction::onExpired();
	ons.btnwaitCommandHandleResult(button_timer_start, static_cast<VariableInfo *>(variableInfo.get()), buttonState, del_flag);
}

bool WaitVoiceAction::expired() {
	bool voice_ended = countDownStarted || !ons.wave_sample[0] || !Mix_Playing(0) || Mix_Paused(0);

	if (voice_ended && voiceDelayMs != 0) {
		if (!countDownStarted) {
			clock.setCountdown(voiceDelayMs);
			countDownStarted = true;
			return false;
		}
		return clock.expired();
	}

	return voice_ended;
}

bool QueuedSoundAction::expired() {
	// terminated check should not be here
	bool sound_ended = /*terminated || */ countDownStarted || !ons.wave_sample[ch] || !Mix_Playing(ch) || Mix_Paused(ch);

	if (sound_ended && soundDelayMs != 0) {
		if (!countDownStarted) {
			clock.setCountdown(soundDelayMs);
			countDownStarted = true;
			return false;
		}
		return clock.expired();
	}

	return sound_ended;
}

void QueuedSoundAction::onExpired() {
	ConstantRefreshAction::onExpired();
	if (!terminated)
		func();
}

// These functions help to detect the improper usage of non ConstantRefresh-Action-based commands
// They SHOULD NOT BE USED for anything except triggering an assert or error condition -- do not use for behavior logic!
bool isWaitingForUserInput() {
	Lock lock(&ons.registeredCRActions);
	return !fetchConstantRefreshActions<ButtonWaitAction>().empty();
}
bool isWaitingForUserInterrupt() {
	Lock lock(&ons.registeredCRActions);
	if (!fetchConstantRefreshActions<WaitAction>().empty())
		return true;
	if (!fetchConstantRefreshActions<DelayAction>().empty())
		return true;
	if (!fetchConstantRefreshActions<WaitVoiceAction>().empty())
		return true;
	return !fetchConstantRefreshActions<DialogueController::TextRenderingMonitorAction>().empty();
}
