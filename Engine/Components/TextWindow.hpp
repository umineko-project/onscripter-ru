/**
 *  TextWindow.hpp
 *  ONScripter-RU
 *
 *  Textbox window compositor.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"

#include <SDL2/SDL_gpu.h>

#include <vector>

struct BlitData {
	GPU_Rect src;
	GPU_Rect dst;
};

struct Sides {
	float top{0}, right{0}, bottom{0}, left{0};
	Sides(float t, float r, float b, float l)
	    : top(t), right(r), bottom(b), left(l) {}
};

class TextWindowController : public BaseController {
public:
	int ownInit() override;
	int ownDeinit() override;

	TextWindowController()
	    : BaseController(this) {}

	bool usingDynamicTextWindow{false};

	void setWindow(const GPU_Rect &w) {
		originalWindowSize = w;
	}

	GPU_Rect mainRegionDimensions{0, 0, 0, 0}; // the area in the texture map occupied by the main region
	float mainRegionExtensionCol{0};

	GPU_Rect noNameRegionDimensions{0, 0, 0, 0}; // the area in the texture map occupied by the no-name region
	float noNameRegionExtensionCol{0};

	GPU_Rect nameRegionDimensions{0, 0, 0, 0}; // the area in the texture map occuped by the name region
	float nameRegionExtensionCol{0};

	float nameBoxExtensionCol{0};
	float nameBoxExtensionRow{0};
	float nameBoxDividerCol{0};

	Sides mainRegionPadding{0, 0, 0, 0}, nameBoxPadding{0, 0, 0, 0};

	std::vector<BlitData> getRegions();

	GPU_Rect getPrintableNameBoxRegion();
	GPU_Rect getExtendedWindow();
	void updateTextboxExtension(bool smoothly = false);
	int extension{0};

private:
	float previousGoalExtension{0};
	float getRequiredAdditionalHeight(const GPU_Rect &window);

	GPU_Rect originalWindowSize{0, 0, 0, 0}; // sentence_font_info.pos, essentially

	std::vector<BlitData> getBottomRegion(const GPU_Rect &window);
	std::vector<BlitData> getTopRegion(const GPU_Rect &window);
	std::vector<BlitData> getNameRegion(const GPU_Rect &window);
	std::vector<BlitData> getNoNameRegion(const GPU_Rect &window);
	GPU_Rect getNameBoxRegion(const GPU_Rect &window);
	GPU_Rect getPrintableNameBoxRegion(const GPU_Rect &window);
	GPU_Rect getExtendedWindow(GPU_Rect window);
	float getTopOfBottom(const GPU_Rect &window) {
		return window.y + window.h - mainRegionDimensions.h - mainRegionPadding.bottom;
	}
};

extern TextWindowController wndCtrl;
