/**
 *  Spriteset.hpp
 *  ONScripter-RU
 *
 *  Sprite set entity support.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Graphics/GPU.hpp"

class SpritesetInfo {
	bool enable{false};
	bool uncommitted{false}, nextEnableState{false};

public:
	int id{0};
	GPU_Rect pos{0, 0, 0, 0};
	int maskSpriteNumber{-1};
	int trans{255};
	int blur{0};
	int breakupFactor{0};
	int pixelateFactor{0};
	int breakupDirectionFlagset{0};
	Clock warpClock;
	float warpSpeed{0};
	float warpWaveLength{1000};
	float warpAmplitude{0};
	int flip{FLIP_NONE};
	bool has_scale_center{false};
	float scale_center_x{0}, scale_center_y{0};
	float scale_x{100}, scale_y{100};
	float rot{0};
	GPUTransformableCanvasImage im, imAfterscene;
	bool isNullTransform() {
		return pos.x == 0 && pos.y == 0 && maskSpriteNumber == -1 &&
		       trans >= 255 && blur == 0 && breakupFactor == 0 && pixelateFactor == 0 &&
		       !warpAmplitude && warpWaveLength == 1000 &&
		       rot == 0 && scale_x == 100 && scale_y == 100;
	}
	bool isEnabled(bool beforescene = false) {
		return ((beforescene || !uncommitted) ? enable : nextEnableState);
	}
	void setEnable(bool state) {
		uncommitted     = true;
		nextEnableState = state;
	}
	bool isUncommitted() {
		return uncommitted;
	}
	void commit() {
		// Do not call this fn directly outside commitSpriteset() or the images will not be correctly cleaned
		if (!uncommitted)
			return;
		enable      = nextEnableState;
		uncommitted = false;
		if (!enable) {
			*this = SpritesetInfo();
		}
	}
};
