/**
 *  Dialogue.hpp
 *  ONScripter-RU
 *
 *  Class controller (component) instance management.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <vector>
#include <typeinfo>

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

class BaseController;
class ControllerCollection {
	std::vector<BaseController *> controllers;

public:
	void deinit(); // You must setup an atexit wrapper calling this function

	void add(BaseController *c) {
		controllers.insert(controllers.begin(), c);
	}

	[[noreturn]] void quit(int c, const char *message = nullptr, ...) {
		if (message) {
			va_list args;
			va_start(args, message);
			std::vfprintf(stderr, message, args);
			va_end(args);
			std::fprintf(stderr, "\n");
		}
		std::exit(c);
	}
};

extern ControllerCollection ctrl;

class BaseController {
	friend class ControllerCollection;
	int counter{0};
	const std::type_info &type;
	bool is_initialised{false};
	bool is_deinitialising{false};

protected:
	// These should never be called directly
	virtual int ownDeinit() = 0;
	virtual int ownInit()   = 0;

public:
	int init() {
		counter++;
		if (counter != 1)
			std::fprintf(stderr, "[Error] BaseController is initialising %s for a %d time\n", type.name(), counter);
		ctrl.add(this);
		int r = ownInit();
		if (r == 0)
			is_initialised = true;
		return r;
	}

	int deinit() {
		if (counter != 1)
			std::fprintf(stderr, "[Error] BaseController is deinitialising %s initialised %d times\n", type.name(), counter);
		counter--;
		is_initialised    = false;
		is_deinitialising = true;
		return ownDeinit();
	}

	// This returns false when asked from ownDeinit()
	bool initialised() {
		return is_initialised;
	}

	// Was hoping to avoid this but unfortunately not.
	// Starts to return true from deinit() call.
	bool deinitialising() {
		return is_deinitialising;
	}

	template <typename T>
	BaseController(T * /*inst*/)
	    : type(typeid(T)) {
		//std::fprintf(stderr, "[Info] BaseController: %s construction complete, new construction options.\n", type.name());
	}
	virtual ~BaseController() {
		if (counter != 0)
			std::fprintf(stderr, "[Error] BaseController is destructing not deinitialised %s initialised %d times\n", type.name(), counter);
		is_initialised = false;
	}

	BaseController(const BaseController &) = delete;
	BaseController &operator=(const BaseController &) = delete;
};

inline void ControllerCollection::deinit() {
	for (auto c : controllers)
		c->deinit();
	controllers.clear();
}
