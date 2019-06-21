/**
 *  CocoaWrapper.hpp
 *  ONScripter-RU
 *
 *  Implements cocoa-specific interfaces.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

struct CocoaAction {
	enum class CocoaActionType {
		FUNCTION,
		SELECTOR
	};
	CocoaActionType type;
	CocoaAction(void (*act)())
	    : type(CocoaActionType::FUNCTION), act(act) {}
	CocoaAction(const char *sel)
	    : type(CocoaActionType::SELECTOR), sel(sel) {}

	void (*act)()   = nullptr;
	const char *sel = nullptr;
};

long allocateMenuEntry(const char *name, long atIdx = -1);
long deallocateMenuEntry(long idx);
long deallocateMenuItem(long baseIdx, long itemIdx);
long enableFullscreen(void (*act)(), long setBaseIdx = -1);

template <typename T>
long allocateMenuItem(T baseMenu, const char *name, CocoaAction act, const char *key = nullptr, long atIdx = -1);

template <>
long allocateMenuItem(const char *baseMenu, const char *name, CocoaAction act, const char *key, long atIdx);

void nativeCursorState(bool show);
