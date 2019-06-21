/**
 *  Breakup.hpp
 *  ONScripter-RU
 *
 *  Breakup parameters information.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <utility>
#include <cstdint>

enum {
	BREAKUP_MODE_LEFT   = 1,
	BREAKUP_MODE_LOWER  = 2,
	BREAKUP_MODE_JUMBLE = 4
};

struct BreakupCell {
	int cell_x{0}, cell_y{0};
	int dir{0};                       // old breakup only
	int radius{0};                    // old breakup only
	float xMovement{0}, yMovement{0}; // new breakup only
	int state{0};
	int disp_x{0}, disp_y{0};
	float resizeFactor{1}; // new breakup only
	int diagonal{0};
};

enum class BreakupType : int16_t {
	NONE,
	SPRITE_CANVAS,
	SPRITE_TIGHTFIT,
	SPRITESET,
	GLOBAL
};

union BreakupID {
	struct {
		BreakupType type;
		int16_t id;
	};
	uint32_t hash;
};

static_assert(sizeof(uint32_t) == sizeof(BreakupID), "BreakupID size mismatch");

namespace std {
template <>
struct hash<BreakupID> {
	size_t operator()(const BreakupID &x) const {
		return x.hash;
	}
};
template <>
struct equal_to<BreakupID> {
	bool operator()(const BreakupID &one, const BreakupID &two) const {
		return one.hash == two.hash;
	}
};
} // namespace std
