/**
 *  Window.hpp
 *  ONScripter-RU
 *
 *  Operating system window abstraction.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"

#include <SDL2/SDL_gpu.h>

#include <vector>

class WindowController : public BaseController {
#if defined(IOS) || defined(DROID)
	static constexpr bool DefaultScaled     = true;
	static constexpr bool DefaultFullscreen = true;
#else
	static constexpr bool DefaultScaled     = false;
	static constexpr bool DefaultFullscreen = false;
#endif

	struct Display {
		// Display number
		int id{0};
		// Native screen (display) resolution (actual pixels)
		int native_width{0}, native_height{0};
		// Region the display occupies
		SDL_Rect region{0, 0, 0, 0};
		// Area of the window on this display (used by changeWindowMode)
		int visibleArea{0};
	};

	struct DisplayData {
		std::vector<Display> displays;
		std::vector<Display *> displaysByArea;
		Display *fullscreenDisplay{nullptr};
		void clear() {
			displays.clear();
			displaysByArea.clear();
			fullscreenDisplay = nullptr;
		}
	} displayData;

	// Current OpenGL context of the window.
	SDL_GLContext glcontext{nullptr};
	// Current window.
	SDL_Window *window{nullptr};
	// Current window position.
	int window_x{0}, window_y{0};
	// Actual size of the window when in windowed mode. (Constant.)
	int windowed_screen_width{0}, windowed_screen_height{0};
	// Actual size of the window when in fullscreen mode. (May vary if multiple monitors are in use.)
	int fullscreen_width{0}, fullscreen_height{0};
	// Actual fullscreen rendering area. (Fixes graphical glitches on the edges of some drivers/resolutions.)
	GPU_Rect fullscreen_reduced_clip{};
	bool fullscreen_reduce_clip{false};
	// Fullscreen offsets (for image centering) (in the script_width coordinate system)
	int fullscript_offset_x{0}, fullscript_offset_y{0};
	// Offset to compensate for system-forced offset, x->left, y->top
	int system_offset_x{0}, system_offset_y{0};
	// Native screen resolution (in the script_width coordinate system)
	int fullscript_width{0}, fullscript_height{0};
	// Actual size of the window. (Current.)
	// This may vary if "scale" is in use.
	int screen_width{0}, screen_height{0};
	// Scaled mode (fullscreen stretching).
	bool scaled_flag{DefaultScaled};
	// Currently in fullscreen mode.
	bool fullscreen_mode{DefaultFullscreen};
	// Currently in fullscreen transition state.
	bool fullscreen_needs_fix{false};

public:
	// Resolution the script runs at.
	int script_width{0}, script_height{0};
	// Size of the canvas (onto which scenes are painted and which we can move the camera around)
	int canvas_width{0}, canvas_height{0};

	int ownInit() override;
	int ownDeinit() override;

	bool showMessageBox(uint32_t flags, const char *title, const char *message, int numbuttons, const SDL_MessageBoxButtonData *buttons, int &res);
	bool showSimpleMessageBox(uint32_t flags, const char *title, const char *message);

	void setMousePosition(int x, int y);
	void setMinimize(bool hide);
	void setActiveState(bool activate);
	void setMainTarget(GPU_Target *target);
	void setTitle(const char *title);
	void setIcon(SDL_Surface *icon = nullptr);

	void translateRendering(float &x, float &y, GPU_Rect *&clip);
	void translateWindowToScriptCoords(int &x, int &y);
	void translateScriptToWindowCoords(int &x, int &y);

	void applyDimensions(int rw, int rh, int cw, int ch, int dw);
	void getWindowSize(int &w, int &h);
	bool updateDisplayData(bool getpos = false);
	// Returns true when correction requires dirty rect refresh (and repaint).
	bool changeMode(bool perform, bool correct, int mode = -1);
	bool earlySetMode();
	//FIXME: There are legacy interfaces, which need to be gone sooner or later.
	bool getFullscreen() {
		return fullscreen_mode;
	}
	bool getFullscreenFix() {
		return fullscreen_needs_fix;
	}

	WindowController()
	    : BaseController(this) {}
};

extern WindowController window;
