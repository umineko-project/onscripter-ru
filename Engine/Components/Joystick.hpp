/**
 *  Joystick.hpp
 *  ONScripter-RU
 *
 *  SDL based gamepad mapping.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Components/Base.hpp"

#include <SDL2/SDL.h>

#if !defined(USE_LIBUSB) && (defined(LINUX) || defined(WIN32) || defined(MACOSX))
#define USE_LIBUSB 1
#include <libusb-1.0/libusb.h>
#endif

#include <array>
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

#include <cstdint>

class NativeController {
protected:
	// Used for bRequest (HID)
	static constexpr int RequestSetReport = 0x9;
	// Used for wValue (higher byte - report type, lower byte - report id)
	static constexpr int ReportTypeInput   = 0x1;
	static constexpr int ReportTypeOutput  = 0x2;
	static constexpr int ReportTypeFeature = 0x3;

public:
	virtual bool init()   = 0;
	virtual void deinit() = 0;
	virtual bool rumble(float /*strength*/, int /*length*/) {
		return false;
	}
	virtual void ping() {}
	virtual ~NativeController() = default;
};

#ifdef USE_LIBUSB

class SteamController : public NativeController {
	static constexpr float PeriodRatio{495483.0};
	libusb_device_handle *handle{nullptr};
	int interface_num{0};

public:
	bool init() override;
	void deinit() override;
	bool rumble(float strength, int length) override;
	static void LIBUSB_CALL completeRumble(libusb_transfer *transfer);
};

class DS3Controller : public NativeController {
	libusb_device_handle *handle{nullptr};
	int interface_num{0};
	uint32_t last_length{0};
	uint32_t completion_time{0};
	static constexpr size_t RumbleLengthL{3};
	static constexpr size_t RumblePowerL{4};
	static constexpr size_t RumbleLengthR{1};
	static constexpr size_t RumblePowerR{2};
	bool configure();

public:
	bool init() override;
	void deinit() override;
	bool rumble(float strength, int length) override;
	static void LIBUSB_CALL completeRumble(libusb_transfer *transfer);
	void ping() override;
};

class DS4Controller : public NativeController {
	libusb_device_handle *handle{nullptr};
	int interface_num{0};
	uint32_t last_length{0};
	uint32_t completion_time{0};
	static constexpr size_t RumblePowerSmall{4};
	static constexpr size_t RumblePowerLarge{5};
	static constexpr size_t LedRed{6};
	static constexpr size_t LedGreen{7};
	static constexpr size_t LedBlue{8};

public:
	bool init() override;
	void deinit() override;
	bool rumble(float strength, int length) override;
	static void LIBUSB_CALL completeRumble(libusb_transfer *transfer);
	void ping() override;
};

#else
struct libusb_context;
#endif

static constexpr auto ONS_SCANCODE_MUTE   = static_cast<SDL_Scancode>(SDL_NUM_SCANCODES + 1);
static constexpr auto ONS_SCANCODE_SKIP   = static_cast<SDL_Scancode>(SDL_NUM_SCANCODES + 2);
static constexpr auto ONS_SCANCODE_SCREEN = static_cast<SDL_Scancode>(SDL_NUM_SCANCODES + 3);

class JoystickController : public BaseController {
	enum class RumbleMethod {
		SDL,
		LIBUSB
	};

	struct Info {
		SDL_Joystick *handler{};
		std::array<uint8_t, 16> guid{};
		int prevAxis{-1};
	};

	libusb_context *usbContext{nullptr};
	bool usingCustomMapping{false};
	std::unordered_map<SDL_JoystickID, SDL_Haptic *> haptic;
	std::unordered_map<SDL_JoystickID, Info> joystick;
	std::unordered_map<uint8_t, SDL_Scancode> customMapping;
	std::vector<std::unique_ptr<NativeController>> nativeControllers;
	RumbleMethod preferredRumbleMethod{RumbleMethod::SDL};

	bool rumbleSDL(float strength, int length);
	bool rumbleLibusb(float strength, int length);

protected:
	int ownInit() override;
	int ownDeinit() override;

public:
	JoystickController()
	    : BaseController(this) {}
	void provideCustomMapping(const char *mapping);
	void setPreferredRumbleMethod(const std::string &str) {
		if (str == "libusb")
			preferredRumbleMethod = RumbleMethod::LIBUSB;
		else
			preferredRumbleMethod = RumbleMethod::SDL;
	}
	bool rumble(float strength, int length);
	SDL_Scancode transButton(uint8_t button, SDL_JoystickID id);
	SDL_Scancode transHat(uint8_t button, SDL_JoystickID id);
	SDL_Event transAxis(SDL_JoyAxisEvent &axisEvent);
	libusb_context *getUsbContext();
	void handleUsbEvents();
};

extern JoystickController joyCtrl;
