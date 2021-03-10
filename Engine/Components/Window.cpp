/**
 *  Window.cpp
 *  ONScripter-RU
 *
 *  Operating system window abstraction.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Components/Window.hpp"
#include "Engine/Graphics/GPU.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Core/ONScripter.hpp"

#include <SDL2/SDL.h>
#ifdef WIN32
#include <windows.h>
#include <SDL2/SDL_syswm.h>
#include "Resources/Support/WinRes.hpp"
#endif

#include <unordered_map>
#include <string>
#include <algorithm>

WindowController window;

int WindowController::ownInit() {
	auto system_offset_x_str = ons.ons_cfg_options.find("system-offset-x");
	if (system_offset_x_str != ons.ons_cfg_options.end())
		system_offset_x = std::stoi(system_offset_x_str->second);

	auto system_offset_y_str = ons.ons_cfg_options.find("system-offset-y");
	if (system_offset_y_str != ons.ons_cfg_options.end())
		system_offset_y = std::stoi(system_offset_y_str->second);

	if (ons.ons_cfg_options.count("scale"))
		scaled_flag = true;

	if (ons.ons_cfg_options.count("full-clip-limit"))
		fullscreen_reduce_clip = true;

	if (ons.ons_cfg_options.count("fullscreen"))
		fullscreen_mode = true;

	return 0;
}

int WindowController::ownDeinit() {

	return 0;
}

bool WindowController::showMessageBox(uint32_t flags, const char *title, const char *message, int numbuttons, const SDL_MessageBoxButtonData *buttons, int &res) {
	SDL_MessageBoxData data{};
	data.flags       = flags;
	data.window      = window;
	data.title       = title;
	data.message     = message;
	data.numbuttons  = numbuttons;
	data.buttons     = buttons;
	data.colorScheme = nullptr;
	return SDL_ShowMessageBox(&data, &res) == 0;
}

bool WindowController::showSimpleMessageBox(uint32_t flags, const char *title, const char *message) {
	return SDL_ShowSimpleMessageBox(flags, title, message, window) == 0;
}

void WindowController::setMousePosition(int x, int y) {
	SDL_WarpMouseInWindow(window, x, y);
}

void WindowController::setMinimize(bool hide) {
	if (hide)
		SDL_MinimizeWindow(window);
	else
		SDL_RestoreWindow(window);
}

void WindowController::setActiveState(bool activate) {
	SDL_GL_MakeCurrent(window, activate ? glcontext : nullptr);
}

void WindowController::setMainTarget(GPU_Target *target) {
	window    = SDL_GetWindowFromID(target->context->windowID);
	glcontext = SDL_GL_GetCurrentContext();
}

void WindowController::setTitle(const char *title) {
	SDL_SetWindowTitle(window, title);
}

void WindowController::setIcon(SDL_Surface *icon) {
	if (icon) {
		SDL_SetWindowIcon(window, icon);
		return;
	}

#ifdef WIN32
	//use the (first) Windows icon resource
	const HANDLE wicon[2]{
	    LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(ONSCRICON), IMAGE_ICON,
	              GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0),
	    LoadImage(GetModuleHandle(nullptr), MAKEINTRESOURCE(ONSCRICON), IMAGE_ICON,
	              GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0)};
	for (int i = 0; i < 2; i++) {
		if (wicon[i]) {
			SDL_SysWMinfo info;
			SDL_VERSION(&info.version);
			SDL_GetWindowWMInfo(window, &info);
			SendMessage(info.info.win.window, WM_SETICON, i == 1 ? ICON_SMALL : ICON_BIG, (LPARAM)wicon[i]);
		}
	}
#endif
}

void WindowController::translateRendering(float &x, float &y, GPU_Rect *&clip) {
	if (fullscreen_mode) {
		x += fullscript_offset_x;
		y += fullscript_offset_y;
		if (fullscreen_reduce_clip && !clip)
			clip = &fullscreen_reduced_clip;
	}
}

void WindowController::translateWindowToScriptCoords(int &x, int &y) {
	x = static_cast<int>(x * (script_width / static_cast<float>(screen_width)));
	y = static_cast<int>(y * (script_height / static_cast<float>(screen_height)));

	if (fullscreen_mode) {
		x -= fullscript_offset_x;
		y -= fullscript_offset_y;
	}
}

void WindowController::translateScriptToWindowCoords(int &x, int &y) {
	if (fullscreen_mode) {
		x = static_cast<int>(static_cast<float>(x + fullscript_offset_x) * (screen_width / static_cast<float>(script_width)));
		y = static_cast<int>(static_cast<float>(y + fullscript_offset_y) * (screen_height / static_cast<float>(script_height)));
	} else {
		x = static_cast<int>(static_cast<float>(x) * (screen_width / static_cast<float>(script_width)));
		y = static_cast<int>(static_cast<float>(y) * (screen_height / static_cast<float>(script_height)));
	}
}

void WindowController::applyDimensions(int rw, int rh, int cw, int ch, int dw) {
	script_width  = rw;
	script_height = rh;

	if (cw == 0 || ch == 0) {
		canvas_width  = script_width * 1.25;
		canvas_height = script_height * 1.25;
	} else {
		canvas_width  = cw;
		canvas_height = ch;
	}

	if (dw > 0) {
		screen_width  = dw;
		screen_height = dw * script_height / script_width;
	} else {
		screen_width  = script_width;
		screen_height = script_height;
	}

	windowed_screen_width  = screen_width;
	windowed_screen_height = screen_height;
}

void WindowController::getWindowSize(int &w, int &h) {
	w = screen_width;
	h = screen_height;
}

bool WindowController::updateDisplayData(bool getpos) {
	if (getpos)
		SDL_GetWindowPosition(window, &window_x, &window_y);

	int displays{SDL_GetNumVideoDisplays()};
	displayData.clear();
	displayData.displays.resize(displays);
	displayData.displaysByArea.resize(displays);

	GPU_Rect windowRegion{static_cast<float>(window_x), static_cast<float>(window_y), static_cast<float>(screen_width), static_cast<float>(screen_height)};

	for (int d = 0; d < displays; d++) {
		SDL_DisplayMode video_mode;
		SDL_GetDesktopDisplayMode(d, &video_mode);
		auto &display         = displayData.displays[d];
		display.id            = d;
		display.native_width  = video_mode.w;
		display.native_height = video_mode.h;
		SDL_GetDisplayBounds(d, &display.region);

		// Determine the size of the portion of the window visible on this display
		SDL_Rect &r = display.region;
		GPU_Rect displayRegion{static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h)};
		GPU_Rect visibleWindowRegion = windowRegion;
		int area                     = doClipping(&visibleWindowRegion, &displayRegion) ? 0 :
		                                                              static_cast<int>(visibleWindowRegion.w) * static_cast<int>(visibleWindowRegion.h);
		display.visibleArea = area;

		// Build the vector for later sorted-iteration (we want to try the displays in order of amount of window visible on screen)
		displayData.displaysByArea[d] = &display;
	}

	// Determine which display will be used for fullscreen.
	std::sort(displayData.displaysByArea.begin(), displayData.displaysByArea.end(), [](const Display *lhs, const Display *rhs) {
		return lhs->visibleArea >= rhs->visibleArea;
	});
	for (int d = 0; d < displays; d++) {
		Display *display = displayData.displaysByArea[d];
		if (!displayData.fullscreenDisplay) {
			if (scaled_flag) {
				// Scaling is on, so it fits no matter the display size.
				displayData.fullscreenDisplay = display;
			} else if (screen_width <= display->native_width && screen_height <= display->native_height) {
				// Only set this as a fullscreen display if it fits.
				displayData.fullscreenDisplay = display;
				fullscreen_width              = screen_width;
				fullscreen_height             = screen_height;
			}
			// The fullscreen display is the first to satisfy these conditions.
		}
		//sendToLog(LogLevel::Info, "Display %u: %u x %u (visible area %u)\n", display->id, display->native_width, display->native_height, display->visibleArea);
	}

	if (scaled_flag) {
		assert(displayData.fullscreenDisplay);
		float scr_stretch_x = displayData.fullscreenDisplay->native_width / static_cast<float>(screen_width);
		float scr_stretch_y = displayData.fullscreenDisplay->native_height / static_cast<float>(screen_height);

		// This was marked "Deprecated and should be removed" -- now it only exists in this one place. The suspicious +0.5 makes me hesitant to refactor to remove this variable.
		int screen_ratio1, screen_ratio2;

		// Constrain aspect to same as game
		if (scr_stretch_x > scr_stretch_y) {
			screen_ratio1 = displayData.fullscreenDisplay->native_height;
			screen_ratio2 = script_height;
		} else {
			screen_ratio1 = displayData.fullscreenDisplay->native_width;
			screen_ratio2 = script_width;
		}

		fullscreen_width  = std::round(static_cast<float>(script_width * screen_ratio1) / screen_ratio2);
		fullscreen_height = std::round(static_cast<float>(script_height * screen_ratio1) / screen_ratio2);
	}

	if (displayData.fullscreenDisplay) {
		fullscript_width    = script_width * displayData.fullscreenDisplay->native_width / static_cast<float>(fullscreen_width);
		fullscript_height   = script_height * displayData.fullscreenDisplay->native_height / static_cast<float>(fullscreen_height);
		fullscript_offset_x = (fullscript_width - script_width) / 2 - system_offset_x;
		fullscript_offset_y = (fullscript_height - script_height) / 2 - system_offset_y;
		// A hack for some resolutions to solve scaling issues like random stripes
		// e. g. 1366x768
		// bg white,1
		// lsp s0_1,"white1080p.png",0,0
		// print 1
		fullscreen_reduced_clip = {fullscript_offset_x + 0.5f, fullscript_offset_y + 0.5f, script_width - 1.0f, script_height - 1.0f};

		return true;
	}
	//Don't bother extra scaling when window is bigger than screen (default ONS behaviour)
	return false;
}

bool WindowController::changeMode(bool perform, bool correct, int mode) {
	// To my regret SDL & SDL_gpu fullscreen APIs are neither convenient, nor perfect.
	// This function needs some improvement I guess, because these clearWholeTargets shouldn't
	// be required on OS X, for example (though, the glitches show they are)
	// The current model is:
	// 1) Resize main window to display dimensions.
	// 2) Set up a new virtual resolution.
	// 3) Enter fullscreen mode.
	// Window positioning and mouse remaps are done in a manual manner here.

	if (!updateDisplayData() && mode > 0) {
		// Request to enter fullscreen when we are in fullscreen-banned mode. Deny it
		return false;
	}

	if (perform && mode >= 0 && static_cast<int>(fullscreen_mode) != mode) {
		// Make sure all the blits are done and the screen is empty, before we continue
		gpu.clearWholeTarget(ons.screen_target);
		GPU_Flip(ons.screen_target);
		GPU_FlushBlitBuffer();

		if (mode == 1) {
			updateDisplayData(true); // window_x and window_y have changed, so our display data must be recalculated.
			screen_width  = fullscreen_width;
			screen_height = fullscreen_height;

			SDL_SetWindowPosition(window, displayData.fullscreenDisplay->region.x, displayData.fullscreenDisplay->region.y); // Move to make it look less offscreen
			GPU_SetWindowResolution(displayData.fullscreenDisplay->native_width, displayData.fullscreenDisplay->native_height);
			gpu.setVirtualResolution(fullscript_width, fullscript_height);

			int mouse_x, mouse_y;
			SDL_GetMouseState(&mouse_x, &mouse_y);
			//Fullscreen set
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
			//We need to correct a shifted mouse
			//sendToLog(LogLevel::Info, "Going to fullscreen. Before: %u, %u\n", mouse_x, mouse_y);
			mouse_x = (mouse_x * fullscreen_width / windowed_screen_width) + ((screen_width / static_cast<float>(script_width)) * fullscript_offset_x);
			mouse_y = (mouse_y * fullscreen_height / windowed_screen_height) + ((screen_height / static_cast<float>(script_height)) * fullscript_offset_y);
			//sendToLog(LogLevel::Info, "Going to fullscreen. After: %u, %u\n", mouse_x, mouse_y);
			SDL_WarpMouseInWindow(window, mouse_x, mouse_y);
			ons.screen_target = GPU_GetContextTarget();
			fullscreen_mode   = true;
		} else {
			SDL_SetWindowFullscreen(window, 0);
			ons.screen_target = GPU_GetContextTarget();
			fullscreen_mode   = false;
		}
		// On OS X mode changes are some animation that needs to be waited for, return when it ends
		fullscreen_needs_fix = true;
	} else if (perform && mode >= 0) {
		correct = false;
	}

	if (correct) {
		// Set correct window dimensions (we are returning to windowed mode)
		if (!fullscreen_mode) {
			screen_width  = windowed_screen_width;
			screen_height = windowed_screen_height;

			int mouse_x, mouse_y;
			SDL_GetMouseState(&mouse_x, &mouse_y);
			//We need to correct a shifted mouse
			//sendToLog(LogLevel::Info, "Going to windowed. Before: %u, %u\n", mouse_x, mouse_y);
			mouse_x = ((mouse_x - (screen_width / static_cast<float>(script_width)) * fullscript_offset_x) * windowed_screen_width / fullscreen_width);
			mouse_y = ((mouse_y - (screen_height / static_cast<float>(script_height)) * fullscript_offset_y) * windowed_screen_height / fullscreen_height);
			//sendToLog(LogLevel::Info, "Going to windowed. After: %u, %u\n", mouse_x, mouse_y);

			GPU_SetWindowResolution(screen_width, screen_height);
			gpu.setVirtualResolution(script_width, script_height);

			if (fullscreen_needs_fix)
				SDL_SetWindowPosition(window, window_x, window_y);
			SDL_SetWindowSize(window, screen_width, screen_height);

			if (fullscreen_needs_fix)
				SDL_WarpMouseInWindow(window, mouse_x, mouse_y);
		}
		fullscreen_needs_fix = false;
		// mode change requires us to redraw the screen, when we are done
		gpu.clearWholeTarget(ons.screen_target);
#ifdef WIN32
		// Looks like old "don't respond to first Flip" bug is back
		GPU_Flip(ons.screen_target);
#endif
	}

	return correct;
}

bool WindowController::earlySetMode() {
	if (fullscreen_mode) {
		fullscreen_mode = false;
		return changeMode(true, true, 1);
	}

	// Unsure if true is needed, but just to make sure
	updateDisplayData(true);
	return false;
}
