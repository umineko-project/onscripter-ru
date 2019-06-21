/**
 *  Camera.hpp
 *  ONScripter-RU
 *
 *  Camera view interface for object movement.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Support/Clock.hpp"

#include <SDL2/SDL_gpu.h>

#include <cmath>

class CameraMove {
	double amplitude{0}; //use getter/setter (so that we can make sure cameramoves intelligently initialize/reset themselves after use...)
public:
	enum class Type {
		X,
		Y
	};
	Type moveType;
	Clock clock;
	int multiplier{1};
	unsigned int cycleTime{200}; // ms *half* cycle time
	CameraMove(Type type = Type::X)
	    : moveType(type) {}
	float updateMove(unsigned int advance) {
		if (getAmplitude() == 0)
			return 0;

		clock.tick(advance);

		// new non-dampened equation (has no duration)
		return multiplier * getAmplitude() * std::sin(M_PI * clock.time() / static_cast<float>(cycleTime));
	}
	double getAmplitude() {
		return amplitude;
	}
	void setAmplitude(double v) {
		if (v == 0) {
			*this = CameraMove(moveType);
		} else {
			amplitude = v;
		}
	}
};

class Camera {
public:
	float2 pos{0, 0};
	float2 offset_pos{0, 0};
	GPU_Rect center_pos{0, 0, 0, 0};
	bool has_moved{false};
	CameraMove x_move{CameraMove::Type::X}, y_move{CameraMove::Type::X};
	bool isMoving() {
		return x_move.getAmplitude() || y_move.getAmplitude();
	}
	void update(unsigned int advance) {
		auto newPos{offset_pos};
		newPos.x += x_move.updateMove(advance);
		newPos.y += y_move.updateMove(advance);

		if (pos.x != newPos.x || pos.y != newPos.y) {
			has_moved = true;
			pos       = newPos;
		}
	}
	void resetMove() {
		x_move.setAmplitude(0);
		y_move.setAmplitude(0);
		pos.x = 0;
		pos.y = 0;
	}
};
