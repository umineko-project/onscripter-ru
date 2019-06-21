/**
 *  BaseReader.hpp
 *  ONScripter-RU
 *
 *  Base class for effect layers.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/DynamicProperty.hpp"

#include <string>
#include <unordered_map>

struct Pt {
	int x;
	int y; /*int type;*/
	int cell;
};

class AnimationInfo;
class BaseReader;

class Layer {
protected:
	BlendModeId blendingModeSupported(int rm);
	void drawLayerToGPUTarget(GPU_Target *target, AnimationInfo *anim, GPU_Rect &clip, float x, float y);

public:
	BaseReader **reader{nullptr};
	AnimationInfo *sprite_info{nullptr}, *sprite{nullptr};
	uint32_t width{0}, height{0};

	Layer(uint32_t w = 0, uint32_t h = 0)
	    : width(w), height(h) {}
	virtual ~Layer() = default;

	void setSpriteInfo(AnimationInfo *sinfo, AnimationInfo *anim) {
		sprite_info = sinfo;
		sprite      = anim;
	}

	// Refresh the internal frame, old == true equals to old_ai call from estimateNextDuration, did update?
	virtual bool update(bool old) = 0;
	// Draw the internal frame to target, rm stands for refresh_mode, transform stands for no blending (copy for transformation)
	virtual void refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool centre_coordinates, int rm, float scalex = 1.0, float scaley = 1.0) = 0;
	// Commit the internal state
	virtual void commit() {}
	// Standard way of intercommunication: message, return code
	virtual char *message(const char * /*unused*/, int & /*unused*/) {
		return nullptr;
	}
	// Blending mode used for rendering
	virtual BlendModeId blendingMode(int /*rm*/) {
		return BlendModeId::NORMAL;
	}
	// Layer-specific properties
	virtual std::unordered_map<std::string, DynamicPropertyInterface> properties() {
		return {};
	}
};
