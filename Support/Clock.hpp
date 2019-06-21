/**
 *  Clock.hpp
 *  ONScripter-RU
 *
 *  Contains code to control clock ticks and fps.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <algorithm>
#include <stdexcept>
#include <cmath>

class Clock {
private:
	uint64_t currentTimepoint;
	uint64_t lapTime;
	uint64_t countdownTime;

public:
	Clock()
	    : currentTimepoint(0), lapTime(0), countdownTime(0) {}
	void reset() {
		*this = Clock();
	}

	void setCountdown(unsigned int ms) {
		setCountdownNanos(static_cast<uint64_t>(ms) * 1000000);
	}
	void setCountdownNanos(uint64_t ns) {
		countdownTime = currentTimepoint + ns;
	}

	void addCountdown(unsigned int ms) {
		addCountdownNanos(static_cast<uint64_t>(ms) * 1000000);
	}
	void addCountdownNanos(uint64_t ns) {
		countdownTime += ns;
	}

	void tick(unsigned int ms) {
		tickNanos(static_cast<uint64_t>(ms) * 1000000);
	}
	void tickNanos(uint64_t ns) {
		currentTimepoint += ns;
		lapTime += ns;
	}

	unsigned int time() {
		return static_cast<unsigned int>(timeNanos() / 1000000);
	}
	uint64_t timeNanos() {
		return currentTimepoint;
	}

	unsigned int lap() {
		return static_cast<unsigned int>(lapNanos() / 1000000);
	}
	uint64_t lapNanos() {
		auto r  = lapTime;
		lapTime = 0;
		return r;
	}

	unsigned int remaining() {
		return static_cast<unsigned int>(remainingNanos() / 1000000);
	}
	uint64_t remainingNanos() {
		if (currentTimepoint > countdownTime)
			return 0;
		return countdownTime - currentTimepoint;
	}

	bool expired() {
		return remainingNanos() < 100000;
	} // expired if it's within 0.1ms of ending
	bool hasCountdown() {
		return countdownTime;
	}
};

class FPSTimeGenerator {
private:
	long double ms{0};
	float fps;
	unsigned int multiplier{0};
	unsigned int acc{0};

public:
	FPSTimeGenerator(float _fps = 0)
	    : fps(_fps) {
		if (std::floor(fps) == 0)
			throw std::runtime_error("Received 0 as FPS value. Bad idea!");
		ms = 1000.0 / static_cast<long double>(fps);
	}
	uint64_t nanosPerFrame() {
		return ms * 1000000;
	}
	unsigned int nextTime() {
		long double total = ms * ++multiplier;
		if (std::abs(std::round(total) - total) < 0.00001)
			total = std::round(total); // fix awkward rounding errors...
		unsigned int r = std::ceil(total) - acc;
		acc += r;
		return r;
	}
	void reset() {
		*this = FPSTimeGenerator(fps);
	}
};
