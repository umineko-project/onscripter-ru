/**
 *  DynamicProperty.hpp
 *  ONScripter-RU
 *
 *  Dynamic transition component support (e.g. animations).
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"
#include "Engine/Entities/Animation.hpp"
#include "Engine/Entities/Spriteset.hpp"
#include "Support/Clock.hpp"

#include <deque>
#include <vector>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace std {
template <>
struct hash<pair<void *, int>> {
	size_t operator()(const std::pair<void *, int> &x) const {
		return reinterpret_cast<uintptr_t>(x.first) ^ x.second;
	}
};
template <>
struct hash<pair<AnimationInfo *, int>> {
	size_t operator()(const std::pair<AnimationInfo *, int> &x) const {
		return x.first->id ^ x.second;
	}
};
template <>
struct hash<pair<int, int>> {
	size_t operator()(const std::pair<int, int> &x) const {
		return x.first ^ x.second;
	}
};
} // namespace std

enum {
	MOTION_EQUATION_LINEAR                = 0,
	MOTION_EQUATION_SLOWDOWN              = 1,
	MOTION_EQUATION_SPEEDUP               = 2,
	MOTION_EQUATION_SMOOTH                = 3, // there are more than this in ps3, in fact...
	MOTION_EQUATION_CONSTANT_ROTATE_SPEED = 4,
	MOTION_EQUATION_COSINE_WAVE           = 5
};

enum {
	SPRITE_PROPERTY_NONE               = 0,
	SPRITE_PROPERTY_X_POSITION         = 1,
	SPRITE_PROPERTY_Y_POSITION         = 2,
	SPRITE_PROPERTY_ALPHA_MULTIPLIER   = 3,
	SPRITE_PROPERTY_RED_MULTIPLIER     = 4,
	SPRITE_PROPERTY_GREEN_MULTIPLIER   = 5,
	SPRITE_PROPERTY_BLUE_MULTIPLIER    = 6,
	SPRITE_PROPERTY_SCALE_X            = 7,
	SPRITE_PROPERTY_SCALE_Y            = 8,
	SPRITE_PROPERTY_ROTATION_ANGLE     = 9,
	SPRITE_PROPERTY_BLUR               = 10,
	SPRITE_PROPERTY_BREAKUP_DIRECTION  = 11,
	SPRITE_PROPERTY_BREAKUP            = 12,
	SPRITE_PROPERTY_QUAKE_X_MULTIPLIER = 13,
	SPRITE_PROPERTY_QUAKE_X_AMPLITUDE  = 14,
	SPRITE_PROPERTY_QUAKE_X_CYCLE_TIME = 15,
	SPRITE_PROPERTY_QUAKE_Y_MULTIPLIER = 16,
	SPRITE_PROPERTY_QUAKE_Y_AMPLITUDE  = 17,
	SPRITE_PROPERTY_QUAKE_Y_CYCLE_TIME = 18,
	SPRITE_PROPERTY_WARP_SPEED         = 19,
	SPRITE_PROPERTY_WARP_WAVELENGTH    = 20,
	SPRITE_PROPERTY_WARP_AMPLITUDE     = 21,
	SPRITE_PROPERTY_SCROLLABLE_H       = 22,
	SPRITE_PROPERTY_SCROLLABLE_W       = 23,
	SPRITE_PROPERTY_SCROLLABLE_Y       = 24,
	SPRITE_PROPERTY_SCROLLABLE_X       = 25,
	SPRITE_PROPERTY_FLIP_MODE          = 26,
	SPRITE_PROPERTY_Z_ORDER            = 27
};
// keep this in sync with the above enum.
const std::vector<const char *> dynamicSpritePropertyNames{"none", "xpos", "ypos", "alpha", "darken_r", "darken_g", "darken_b", "scalex", "scaley", "rot", "blur", "breakupdir",
                                                           "breakup", "quakexmul", "quakexamp", "quakexcycle", "quakeymul", "quakeyamp", "quakeycycle", "warp_spd", "warp_wave",
                                                           "warp_amp", "scroll_h", "scroll_w", "scroll_y", "scroll_x", "flip"};

enum {
	GLOBAL_PROPERTY_NONE               = 0,
	GLOBAL_PROPERTY_QUAKE_X_MULTIPLIER = 1, // multiplies the amplitude
	GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE  = 2, // max amplitude in px.
	GLOBAL_PROPERTY_QUAKE_X_CYCLE_TIME = 3, // provides the length of HALF a cycle in ms.
	GLOBAL_PROPERTY_QUAKE_Y_MULTIPLIER = 4,
	GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE  = 5,
	GLOBAL_PROPERTY_QUAKE_Y_CYCLE_TIME = 6,
	GLOBAL_PROPERTY_ONION_ALPHA        = 7,
	GLOBAL_PROPERTY_ONION_SCALE        = 8,
	GLOBAL_PROPERTY_TEXTBOX_EXTENSION  = 9,
	GLOBAL_PROPERTY_BLUR               = 10,
	GLOBAL_PROPERTY_CAMERA_X           = 11,
	GLOBAL_PROPERTY_CAMERA_Y           = 12,
	GLOBAL_PROPERTY_CAMERA_CENTRE_X    = 13, // only 960 is used, which is middle, unimplemented
	GLOBAL_PROPERTY_CAMERA_CENTRE_Y    = 14, // only 540 is used, which is middle, unimplemented
	GLOBAL_PROPERTY_WARP_SPEED         = 15,
	GLOBAL_PROPERTY_WARP_WAVELENGTH    = 16,
	GLOBAL_PROPERTY_WARP_AMPLITUDE     = 17,
	GLOBAL_PROPERTY_BGM_CHANNEL_VOLUME = 127,
	GLOBAL_PROPERTY_MIX_CHANNEL_VOLUME = 128 // bit of a hack
};
const std::vector<const char *> dynamicGlobalPropertyNames{"none", "quakexmul", "quakexamp", "quakexcycle", "quakeymul", "quakeyamp", "quakeycycle", "onionalpha", "onionscale", "extension", "blur",
                                                           "xpos", "ypos", "centrex", "centrey", "warp_spd", "warp_wave", "warp_amp"};

enum {
	SPRITESET_PROPERTY_NONE              = 0,
	SPRITESET_PROPERTY_X_POSITION        = 1,
	SPRITESET_PROPERTY_Y_POSITION        = 2,
	SPRITESET_PROPERTY_ALPHA             = 3,
	SPRITESET_PROPERTY_BLUR              = 4,
	SPRITESET_PROPERTY_BREAKUP_DIRECTION = 5,
	SPRITESET_PROPERTY_BREAKUP           = 6,
	SPRITESET_PROPERTY_PIXELATE          = 7,
	SPRITESET_PROPERTY_WARP_SPEED        = 8,
	SPRITESET_PROPERTY_WARP_WAVELENGTH   = 9,
	SPRITESET_PROPERTY_WARP_AMPLITUDE    = 10,
	SPRITESET_PROPERTY_CENTRE_X          = 11,
	SPRITESET_PROPERTY_CENTRE_Y          = 12,
	SPRITESET_PROPERTY_SCALE_X           = 13,
	SPRITESET_PROPERTY_SCALE_Y           = 14,
	SPRITESET_PROPERTY_ROTATION_ANGLE    = 15,
	SPRITESET_PROPERTY_FLIP_MODE         = 16
};
const std::vector<const char *> dynamicSpritesetPropertyNames{"none", "xpos", "ypos", "alpha", "blur", "breakupdir", "breakup", "pixelate", "warp_spd", "warp_wave", "warp_amp",
                                                              "centrex", "centrey", "scalex", "scaley", "rot", "flip"};

struct DynamicPropertyInterface {
	double (*getValue)(void *);
	void (*setValue)(void *, double);
};

class DynamicPropertyController : public BaseController {
private:
	class DynamicProperty {
	protected:
		DynamicPropertyController *controller;
		static constexpr double alpha_f{15753.0 / 10000.0};

	private:
		int start_value{0};
		int end_value;
		bool is_abs;
		double getInterpolatedValue();
		// subclasses must define how the property is read and written.
		// This class takes care of everything else.
		virtual double getValue()           = 0;
		virtual void setValue(double value) = 0;

	public:
		Clock clock;
		unsigned int duration;
		int motion_equation;
		bool endless {false};

		void begin();
		void apply();
		double getRemainingValue();
		int getRemainingDuration();

		DynamicProperty(DynamicPropertyController *_controller, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR, bool _is_abs = true)
		    : controller(_controller), end_value(_value), is_abs(_is_abs), duration(_duration), motion_equation(_motion_equation) {}
		DynamicProperty(const DynamicProperty &) = default;
		DynamicProperty &operator=(const DynamicProperty &) = default;
		virtual ~DynamicProperty()                          = default;
	};

	class DynamicCustomProperty : public DynamicProperty {
	private:
		void *ptr{nullptr};
		int property;

		double getValue() override {
			return controller->registeredProperties[property].getValue(ptr);
		}
		void setValue(double value) override {
			controller->registeredProperties[property].setValue(ptr, value);
		}

	public:
		DynamicCustomProperty(DynamicPropertyController *_controller, void *_ptr, bool _is_abs, int _property, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR)
		    : DynamicProperty(_controller, _value, _duration, _motion_equation, _is_abs), ptr(_ptr), property(_property) {}
	};

	class DynamicSpriteProperty : public DynamicProperty {
	private:
		double getValue() override;
		void setValue(double value) override;

	public:
		AnimationInfo *ai;
		int sprite_number;
		bool is_lsp2;
		int property;
		bool for_distinguished_new_ai;
		DynamicSpriteProperty(DynamicPropertyController *_controller, AnimationInfo *_ai, int _sprite_number, bool _is_lsp2, bool _is_abs, int _property, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR)
		    : DynamicProperty(_controller, _value, _duration, _motion_equation, _is_abs), ai(_ai), sprite_number(_sprite_number), is_lsp2(_is_lsp2), property(_property), for_distinguished_new_ai(false) {}
	};

	class DynamicGlobalProperty : public DynamicProperty {
	private:
		double getValue() override;
		void setValue(double value) override;

	public:
		int property;
		DynamicGlobalProperty(DynamicPropertyController *_controller, bool _is_abs, int _property, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR)
		    : DynamicProperty(_controller, _value, _duration, _motion_equation, _is_abs), property(_property) {}
	};

	class DynamicSpritesetProperty : public DynamicProperty {
	private:
		double getValue() override;
		void setValue(double value) override;

	public:
		//SpritesetInfo *set;
		int spriteset_number;
		int property;
		DynamicSpritesetProperty(DynamicPropertyController *_controller, int _spriteset_number, bool _is_abs, int _property, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR)
		    : DynamicProperty(_controller, _value, _duration, _motion_equation, _is_abs), spriteset_number(_spriteset_number), property(_property) {}
	};

	std::unordered_map<std::pair<void *, int>, std::deque<DynamicCustomProperty>> customProperties;
	std::unordered_map<std::pair<AnimationInfo *, int>, std::deque<DynamicSpriteProperty>> spriteProperties;
	std::unordered_map<int, std::deque<DynamicGlobalProperty>> globalProperties;
	std::unordered_map<std::pair<int, int>, std::deque<DynamicSpritesetProperty>> spritesetProperties;

	template <class T>
	int getMaxRemainingDuration(std::deque<T> &props);
	template <class T>
	void waitOnPropertyGeneric(std::deque<T> &props, int event_mode_addons);

	std::vector<DynamicPropertyInterface> registeredProperties;
	std::unordered_map<std::string, int> registeredPropertiesMap;

public:
	/* API methods ^___^ */
	// Registers property implementation for later usage and returns its id, could be called multiple times
	int registerProperty(const std::string &name, DynamicPropertyInterface &&iface) {
		auto it = registeredPropertiesMap.find(name);
		if (it != registeredPropertiesMap.end()) {
			registeredProperties[it->second] = iface;
			return it->second;
		}
		// This int is to conform to the legacy API :/
		auto idx = static_cast<int>(registeredProperties.size());
		registeredProperties.emplace_back(iface);
		return registeredPropertiesMap[name] = idx;
	}
	// Returns previously registered property id
	int getRegisteredProperty(const std::string &name) {
		auto it = registeredPropertiesMap.find(name);
		if (it != registeredPropertiesMap.end())
			return it->second;
		throw std::invalid_argument("Invalid property name: " + name);
	}
	// Registers a custom property change for immediate asynchronous execution via constant refresh.
	void addCustomProperty(void *_ptr, bool _is_abs, int _property, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR, bool _is_override = false);
	// Same for sprite properties. Changes to the same property on the same sprite will queue.
	// i.e. (lsp50,xpos,500ms)+(lsp50,ypos,700ms) = 700ms total execution time (longer of the two)
	//      (lsp50,xpos,500ms)+(lsp50,xpos,300ms) = 800ms total execution time (second will not execute until the first is over)
	void addSpriteProperty(AnimationInfo *_ai, int _sprite_number, bool _is_lsp2, bool _is_abs, int _property, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR, bool _is_override = false);
	// Same for global properties. (Changes to the same property will queue.)
	void addGlobalProperty(bool _is_abs, int _property, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR, bool _is_override = false);
	// Same for spriteset properties. (Changes to the same property on the same spriteset will queue.)
	void addSpritesetProperty(int _spriteset_number, bool _is_abs, int _property, int _value = 0, int _duration = 0, int _motion_equation = MOTION_EQUATION_LINEAR);

	// Returns control to the SDL event queue until all registered changes for the passed property-pointer pair have completed.
	void waitOnCustomProperty(void *ptr, int property, int event_mode_addons = 0);
	// Same for sprite properties.
	void waitOnSpriteProperty(AnimationInfo *ai, int property, int event_mode_addons = 0);
	// Same for global properties.
	void waitOnGlobalProperty(int property, int event_mode_addons = 0);
	// Same for spriteset properties.
	void waitOnSpritesetProperty(int spriteset_number, int property, int event_mode_addons = 0);

	// Terminates all sprite property changes for this sprite (e.g. sprite is replaced by another and committed)
	void terminateSpriteProperties(AnimationInfo *ai);

	// Terminates all set property changes for this set (e.g. set is disabled and committed)
	void terminateSpritesetProperties(SpritesetInfo *si);

	// Advances the internal clock of all registered dynamic sprite properties by N ms.
	void advance(int ms);
	// same for nanoseconds
	void advanceNanos(uint64_t ns);

	// Changes the value based on that property's current clock and removes it from the queue if the property-change is over
	void apply();
	// Resets everything, for cleanup when ONS resets
	void reset();

	DynamicPropertyController()
	    : BaseController(this) {}

protected:
	int ownInit() override {
		return 0;
	}

	int ownDeinit() override {
		reset();
		return 0;
	}
};

extern DynamicPropertyController dynamicProperties;
