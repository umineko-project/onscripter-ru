/**
 *  KeyState.hpp
 *  ONScripter-RU
 *
 *  Contains unified key/button structures.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <cstdint>

struct KeyState {
	bool pressedFlag{false};
	int8_t ctrl{0};
	int8_t opt{0};
	int8_t shift{0};
#ifdef MACOSX
	int8_t apple{0};
#endif
};

struct ButtonState {
	int x{0}, y{0}, button{0};
	bool down_flag{false}, valid_flag{false};
	void reset() { //Mion - clear the button state
		button     = 0;
		valid_flag = false;
	}
	void set(int val) { //Mion - set button & valid_flag
		button     = val;
		valid_flag = true;
	}
};
