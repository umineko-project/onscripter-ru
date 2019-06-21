/**
 *  ObjectFall.hpp
 *  ONScripter-RU
 *
 *  "snow.dll" analogue with improved dencity and performance.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Layers/Layer.hpp"

#include <SDL2/SDL.h>

#include <vector>
#include <cmath>
#include <cstdint>

const uint32_t baseDropWidth  = 2;
const uint32_t baseDropHeight = 110;
const SDL_Color baseDropColour{90, 90, 90, 160};

/* Would be better to have a proper vector math library ^_^ */
template <typename T>
class MathVector {
public:
	T x{0};
	T y{0};
	MathVector translate(T _x, T _y) {
		return MathVector<T>(x + _x, y + _y);
	}
	MathVector rotate(T a) {
		return rotate(std::sin(a), std::cos(a));
	}
	MathVector rotate(T sin, T cos) {
		T nX = (x * cos) - (y * sin);
		T nY = (x * sin) + (y * cos);
		return MathVector<T>(nX, nY);
	}
	MathVector operator-() {
		return MathVector<T>(-x, -y);
	}
	MathVector operator+(const MathVector<T> &v) {
		return translate(v.x, v.y);
	}
	MathVector operator-(const MathVector<T> &v) {
		return translate(-v.x, -v.y);
	}
	MathVector operator*(T n) {
		return MathVector<T>(x * n, y * n);
	}
	MathVector(T _x = 0, T _y = 0) {
		x = _x;
		y = _y;
	}
};

class ObjectFallLayer : public Layer {
public:
	ObjectFallLayer(uint32_t w, uint32_t h);
	~ObjectFallLayer() override;
	ObjectFallLayer(const ObjectFallLayer &) = delete;
	ObjectFallLayer &operator=(const ObjectFallLayer &) = delete;
	void setDims(uint32_t w, uint32_t h);                            // prop 21
	void setSpeed(uint32_t speed = 0);                               // prop 21, speed: 0 = calc
	void setCustomSpeed(uint32_t speed);                             // real prop 21
	void setAmplifiers(float s, float w, float h, float r, float m); // A way to fine-tune the rain
	void setAmount(uint32_t dropNum);                                // prop 20
	void setWind(int32_t factor);                                    // prop 22
	void setBaseDrop(GPU_Image *newBaseDrop);                        // if we need a custom drop
	void setBaseDrop(SDL_Color &colour, uint32_t w, uint32_t h);     // if we need a custom coloured drop
	void setPause(bool state);
	void setBlend(BlendModeId mode);
	void coverScreen();
	bool update(bool old) override;
	void refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool centre_coordinates, int rm, float scalex = 1.0, float scaley = 1.0) override;
	BlendModeId blendingMode(int /*rm*/) override {
		return blendMode;
	}
	void commit() override;
	std::unordered_map<std::string, DynamicPropertyInterface> properties() override;

private:
	static size_t constexpr CurrentScene = 0;
	static size_t constexpr FormerScene  = 1;
	bool paused[2]{false, false};
	GPU_Image *baseDrop{nullptr};   // drop image used as a base
	uint32_t dropW{baseDropWidth};  // actual drop width set by script
	uint32_t dropH{baseDropHeight}; // actual drop height set by script
	uint32_t dropSpeed{70};         // drop speed in px/frame (in layer refresh rate)
	uint32_t dropAmount{300};       // drop amount on the screen
	// These values affect the constants
	float speedAmplifier{1}, widthAmplifier{1}, heightAmplifier{1}, randomAmplifier{0}, windAmplifier{1};

	// These two do not have a script interface at the moment.
	uint32_t overlapForcePercentage{20}; // how often a drop should be followed by another one in the same position (set to 0 to disable)
	uint32_t overlapForceProximity{10};  // how close to position that same-positioned drop to the previous drop

	std::deque<uint32_t> dropSpawnOrder; // Randomized order of positions along the i axis (sky axis) to create the drops
	double currentJiggle{0};             // a randomization factor that slightly changes rain positions to prevent repetitiveness

	// Script factor belongs to {-1000,1000}, which roughly equals to -73~73 degrees, let's use 75 as a guess
	const float transFactor{75 / 1000.0};
	struct {
		double sin{0}, cos{0};                      // cache wind sin/cos for speedup
		MathVector<float> top, left, bottom, right; // Holds transformed bounds of the ij coordinate system
		MathVector<float> originalTop;              // Holds the original xy position of the vertex which rain falls from (or of either vertex on that edge)
		int32_t factor{0};                          // wind factor in degrees (-180 to 180), 0 is down, + is counter-clockwise
	} transforms[3];                                //-V730_NOINIT

	BlendModeId blendMode{BlendModeId::ADD};

	struct Drop {
		double i{0}, j{0}, jMax{0}, w{0}, h{0}, angle{0}, sin{0}, cos{0}, r{0};
		MathVector<float> originalTop, top;
		MathVector<float> pos() {
			return MathVector<float>(i, j);
		}
	};

	std::vector<Drop> drops;
	cmp::optional<std::vector<Drop>> old_drops;
};
