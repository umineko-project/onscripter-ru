/**
 *  Joystick.cpp
 *  ONScripter-RU
 *
 *  SDL based gamepad mapping.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Components/Joystick.hpp"
#include "Engine/Core/ONScripter.hpp"

#include <SDL2/SDL.h>

#include <array>
#include <unordered_map>
#include <sstream>
#include <cstdint>

JoystickController joyCtrl;

enum JoyId {
	DualShock3,
	GenericUnknown,
	FuSaGamePad,
	FuSaGamePadLinux,
	DualShock4,
	DualShock4Xinput,
	Rumblepad2,
	Rumblepad2Xinput,
	GenericXinput,
	GenericXinputNouveau,
	TotalControllers
};

const std::array<uint8_t, 16> JOYGUID[TotalControllers]{
    {{0x4C, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCB, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x03, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xCB, 0x01, 0x00, 0x00, 0x10, 0x01, 0x00, 0x00}},
    {{0x4C, 0x05, 0xC4, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44}},
    {{0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x6D, 0x04, 0x19, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44}},
    {{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x78, 0x69, 0x6E, 0x70, 0x75, 0x74, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {{0x03, 0x00, 0x00, 0x00, 0x5E, 0x04, 0x00, 0x00, 0x8E, 0x02, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01}}};

// Base mapping for a generic DS3+ gamepad
const std::array<SDL_Scancode, 22> KEYMAP{
    {
        SDL_SCANCODE_H,       /* TRIANGLE */
        SDL_SCANCODE_RETURN,  /* CIRCLE   */
        SDL_SCANCODE_ESCAPE,  /* CROSS    */
        SDL_SCANCODE_Z,       /* SQUARE   */
        SDL_SCANCODE_A,       /* L1       */
        ONS_SCANCODE_SKIP,    /* R1       */
        SDL_SCANCODE_DOWN,    /* DOWN     */
        SDL_SCANCODE_LEFT,    /* LEFT     */
        SDL_SCANCODE_UP,      /* UP       */
        SDL_SCANCODE_RIGHT,   /* RIGHT    */
        ONS_SCANCODE_MUTE,    /* SELECT   */
        SDL_SCANCODE_TAB,     /* START    */
        ONS_SCANCODE_SCREEN,  /* HOME     */
        SDL_SCANCODE_UNKNOWN, /* L2       */
        SDL_SCANCODE_RCTRL,   /* R2       */
        SDL_SCANCODE_UNKNOWN, /* L3       */
        SDL_SCANCODE_UNKNOWN, /* R3       */
        SDL_SCANCODE_UNKNOWN, /* EXTRA    */
        /* HAT MAPPING */
        SDL_SCANCODE_DOWN, /* DOWN     */
        SDL_SCANCODE_LEFT, /* LEFT     */
        SDL_SCANCODE_UP,   /* UP       */
        SDL_SCANCODE_RIGHT /* RIGHT    */
    }};

const std::array<SDL_Scancode, 4> axisMap{
    {
        KEYMAP[7], /* AL-LEFT  */
        KEYMAP[9], /* AL-RIGHT */
        KEYMAP[8], /* AL-UP    */
        KEYMAP[6]  /* AL-DOWN  */
    }};

const std::pair<std::array<uint8_t, 16>, std::unordered_map<uint8_t, SDL_Scancode>> joyMap[]{
    {// PLAYSTATION(R)3 Controller (OS X, Windows MotioninJoy DirectInput)
     JOYGUID[JoyId::DualShock3],
     {
         {12, KEYMAP[0]},  /* TRIANGLE */
         {13, KEYMAP[1]},  /* CIRCLE   */
         {14, KEYMAP[2]},  /* CROSS    */
         {15, KEYMAP[3]},  /* SQUARE   */
         {10, KEYMAP[4]},  /* L1       */
         {11, KEYMAP[5]},  /* R1       */
         {6, KEYMAP[6]},   /* DOWN     */
         {7, KEYMAP[7]},   /* LEFT     */
         {4, KEYMAP[8]},   /* UP       */
         {5, KEYMAP[9]},   /* RIGHT    */
         {0, KEYMAP[10]},  /* SELECT   */
         {3, KEYMAP[11]},  /* START    */
         {16, KEYMAP[12]}, /* HOME     */
         {8, KEYMAP[13]},  /* L2       */
         {9, KEYMAP[14]},  /* R2       */
         {1, KEYMAP[15]},  /* L3       */
         {2, KEYMAP[16]},  /* R3       */
     }},
    {// A generic mapping for improper (emulated) Xinput controllers, may work with various devices
     JOYGUID[JoyId::GenericUnknown],
     {
         {13, KEYMAP[0]},  /* TRIANGLE */
         {11, KEYMAP[1]},  /* CIRCLE   */
         {10, KEYMAP[2]},  /* CROSS    */
         {12, KEYMAP[3]},  /* SQUARE   */
         {8, KEYMAP[4]},   /* L1       */
         {9, KEYMAP[5]},   /* R1       */
         {1, KEYMAP[6]},   /* DOWN     */
         {2, KEYMAP[7]},   /* LEFT     */
         {0, KEYMAP[8]},   /* UP       */
         {3, KEYMAP[9]},   /* RIGHT    */
         {5, KEYMAP[10]},  /* SELECT   */
         {4, KEYMAP[11]},  /* START    */
         {14, KEYMAP[12]}, /* HOME     */
         //{X,	KEYMAP[13]}, /* L2       */
         //{X,	KEYMAP[14]}, /* R2       */
         {6, KEYMAP[15]}, /* L3       */
         {7, KEYMAP[16]}, /* R3       */
     }},
    {// FuSa GamePad (OS X, Windows)
     JOYGUID[JoyId::FuSaGamePad],
     {
         {3, KEYMAP[0]}, /* TRIANGLE */
         {1, KEYMAP[1]}, /* CIRCLE   */
         {0, KEYMAP[2]}, /* CROSS    */
         {2, KEYMAP[3]}, /* SQUARE   */
         {4, KEYMAP[4]}, /* LTRIGGER */
         {5, KEYMAP[5]}, /* RTRIGGER */
         //{X,	KEYMAP[6]},  /* DOWN     */
         //{X,	KEYMAP[7]},  /* LEFT     */
         //{X,	KEYMAP[8]},  /* UP       */
         //{X,	KEYMAP[9]},  /* RIGHT    */
         {6, KEYMAP[10]},  /* SELECT   */
         {7, KEYMAP[11]},  /* START    */
         {8, KEYMAP[12]},  /* HOME     */
         {9, KEYMAP[13]},  /* - (L2)   */
         {10, KEYMAP[14]}, /* + (R2)   */
         {11, KEYMAP[15]}, /* SCREEN (L3) */
     }},
    {// FuSa GamePad (Ubuntu)
     JOYGUID[JoyId::FuSaGamePadLinux],
     {
         {3, KEYMAP[0]}, /* TRIANGLE */
         {1, KEYMAP[1]}, /* CIRCLE   */
         {0, KEYMAP[2]}, /* CROSS    */
         {2, KEYMAP[3]}, /* SQUARE   */
         {4, KEYMAP[4]}, /* LTRIGGER */
         {5, KEYMAP[5]}, /* RTRIGGER */
         //{X,	KEYMAP[6]},  /* DOWN     */
         //{X,	KEYMAP[7]},  /* LEFT     */
         //{X,	KEYMAP[8]},  /* UP       */
         //{X,	KEYMAP[9]},  /* RIGHT    */
         {6, KEYMAP[10]},  /* SELECT   */
         {7, KEYMAP[11]},  /* START    */
         {8, KEYMAP[12]},  /* HOME     */
         {9, KEYMAP[13]},  /* - (L2)   */
         {10, KEYMAP[14]}, /* + (R2)   */
         {11, KEYMAP[15]}, /* SCREEN (L3) */
     }},
    {// PLAYSTATION(R)4 Controller (Windows Native DirectInput)
     JOYGUID[JoyId::DualShock4],
     {
         {3, KEYMAP[0]}, /* TRIANGLE */
         {2, KEYMAP[1]}, /* CIRCLE   */
         {1, KEYMAP[2]}, /* CROSS    */
         {0, KEYMAP[3]}, /* SQUARE   */
         {4, KEYMAP[4]}, /* L1       */
         {5, KEYMAP[5]}, /* R1       */
         //{X,	KEYMAP[6]},  /* DOWN     */
         //{X,	KEYMAP[7]},  /* LEFT     */
         //{X,	KEYMAP[8]},  /* UP       */
         //{X,	KEYMAP[9]},  /* RIGHT    */
         {9, KEYMAP[10]},  /* OPTIONS (SELECT) */
         {8, KEYMAP[11]},  /* SHARE (START) */
         {12, KEYMAP[12]}, /* HOME     */
         {6, KEYMAP[13]},  /* L2       */
         {7, KEYMAP[14]},  /* R2       */
         {10, KEYMAP[15]}, /* L3       */
         {11, KEYMAP[16]}, /* R3       */
         {13, KEYMAP[17]}, /* SENSOR   */
     }},
    {// PLAYSTATION(R)4 Controller (Windows Xinput DS4Tool)
     // That one also has all zero id, but it adds two joysticks at once, we will try detecting it and hack the id
     JOYGUID[JoyId::DualShock4Xinput],
     {
         {13, KEYMAP[0]},  /* TRIANGLE */
         {11, KEYMAP[1]},  /* CIRCLE   */
         {10, KEYMAP[2]},  /* CROSS    */
         {12, KEYMAP[3]},  /* SQUARE   */
         {8, KEYMAP[4]},   /* L1       */
         {9, KEYMAP[5]},   /* R1       */
         {1, KEYMAP[6]},   /* DOWN     */
         {2, KEYMAP[7]},   /* LEFT     */
         {0, KEYMAP[8]},   /* UP       */
         {3, KEYMAP[9]},   /* RIGHT    */
         {4, KEYMAP[10]},  /* OPTIONS (SELECT) */
         {5, KEYMAP[11]},  /* SHARE (START) */
         {14, KEYMAP[12]}, /* HOME     */
         //{X,	KEYMAP[13]}, /* L2       */
         //{X,	KEYMAP[14]}, /* R2       */
         {6, KEYMAP[15]}, /* L3       */
         {7, KEYMAP[16]}, /* R3       */
     }},
    {// Logitech Rumblepad 2 (Windows DirectInput)
     JOYGUID[JoyId::Rumblepad2],
     {
         {3, KEYMAP[0]}, /* TRIANGLE */
         {2, KEYMAP[1]}, /* CIRCLE   */
         {1, KEYMAP[2]}, /* CROSS    */
         {0, KEYMAP[3]}, /* SQUARE   */
         {4, KEYMAP[4]}, /* L1       */
         {5, KEYMAP[5]}, /* R1       */
         //{X,	KEYMAP[6]},  /* DOWN     */
         //{X,	KEYMAP[7]},  /* LEFT     */
         //{X,	KEYMAP[8]},  /* UP       */
         //{X,	KEYMAP[9]},  /* RIGHT    */
         {8, KEYMAP[10]}, /* SELECT   */
         {9, KEYMAP[11]}, /* START    */
         //{X,	KEYMAP[12]}, /* HOME     */
         {6, KEYMAP[13]},  /* L2       */
         {7, KEYMAP[14]},  /* R2       */
         {10, KEYMAP[15]}, /* L3       */
         {11, KEYMAP[16]}, /* R3       */
     }},
    {// Logitech Rumblepad 2 (Windows Xinput Emulator)
     // That one also has all zero id, but it adds two joysticks at once, we will try detecting it and hack the id
     JOYGUID[JoyId::Rumblepad2Xinput],
     {
         {13, KEYMAP[0]}, /* TRIANGLE */
         {11, KEYMAP[1]}, /* CIRCLE   */
         {10, KEYMAP[2]}, /* CROSS    */
         {12, KEYMAP[3]}, /* SQUARE   */
         {8, KEYMAP[4]},  /* L1       */
         {9, KEYMAP[5]},  /* R1       */
         {1, KEYMAP[6]},  /* DOWN     */
         {3, KEYMAP[7]},  /* LEFT     */
         {0, KEYMAP[8]},  /* UP       */
         {2, KEYMAP[9]},  /* RIGHT    */
         {5, KEYMAP[10]}, /* SELECT   */
         {4, KEYMAP[11]}, /* START    */
         //{X,	KEYMAP[12]}, /* HOME     */
         {14, KEYMAP[13]},
         /* L2       */ // doesn't work
         {15, KEYMAP[14]},
         /* R2       */   // doesn't work
         {6, KEYMAP[15]}, /* L3       */
         {7, KEYMAP[16]}, /* R3       */
     }},
    {// Generic Xinput driver
     // PLAYSTATION(R)3 Controller (Windows MotioninJoy Xinput)
     // Steam Controller (Xinput)
     JOYGUID[JoyId::GenericXinput],
     {
         {3, KEYMAP[0]}, /* TRIANGLE */
         {1, KEYMAP[1]}, /* CIRCLE   */
         {0, KEYMAP[2]}, /* CROSS    */
         {2, KEYMAP[3]}, /* SQUARE   */
         {4, KEYMAP[4]}, /* L1       */
         {5, KEYMAP[5]}, /* R1       */
         //{X,	KEYMAP[6]},  /* DOWN     */
         //{X,	KEYMAP[7]},  /* LEFT     */
         //{X,	KEYMAP[8]},  /* UP       */
         //{X,	KEYMAP[9]},  /* RIGHT    */
         {6, KEYMAP[10]},  /* SELECT   */
         {7, KEYMAP[11]},  /* START    */
         {10, KEYMAP[12]}, /* HOME     */
         //{X,	KEYMAP[13]}, /* L2       */
         //{X,	KEYMAP[14]}, /* R2       */
         {8, KEYMAP[15]}, /* L3       */
         {9, KEYMAP[16]}, /* R3       */
     }},
    {JOYGUID[JoyId::GenericXinputNouveau],
     {
         {3, KEYMAP[0]}, /* TRIANGLE */
         {1, KEYMAP[1]}, /* CIRCLE   */
         {0, KEYMAP[2]}, /* CROSS    */
         {2, KEYMAP[3]}, /* SQUARE   */
         {4, KEYMAP[4]}, /* L1       */
         {5, KEYMAP[5]}, /* R1       */
         //{X,	KEYMAP[6]},  /* DOWN     */
         //{X,	KEYMAP[7]},  /* LEFT     */
         //{X,	KEYMAP[8]},  /* UP       */
         //{X,	KEYMAP[9]},  /* RIGHT    */
         {6, KEYMAP[10]},  /* SELECT   */
         {7, KEYMAP[11]},  /* START    */
         {10, KEYMAP[12]}, /* HOME     */
         //{X,	KEYMAP[13]}, /* L2       */
         //{X,	KEYMAP[14]}, /* R2       */
         {8, KEYMAP[15]}, /* L3       */
         {9, KEYMAP[16]}, /* R3       */
     }}};

#ifdef USE_LIBUSB

// Thanks to https://gitlab.com/Pilatomic/SteamControllerSinger

bool SteamController::init() {
	auto ctx = joyCtrl.getUsbContext();
	if (!ctx)
		return false;

	if ((handle = libusb_open_device_with_vid_pid(ctx, 0x28DE, 0x1102))) {
		sendToLog(LogLevel::Info, "SteamController::Initialising wired Steam Controller\n");
		interface_num = 2;
	} else if ((handle = libusb_open_device_with_vid_pid(ctx, 0x28DE, 0x1142))) {
		sendToLog(LogLevel::Info, "SteamController::Initialising Steam Dongle, will use first Steam Controller\n");
		interface_num = 1;
	} else {
		// Unfortunately nothing
		return false;
	}

	// On Linux, automatically detach and reattach kernel module
	libusb_set_auto_detach_kernel_driver(handle, 1);

	// Claim the USB interface controlling the haptic actuators
	auto r = libusb_claim_interface(handle, interface_num);
	if (r < 0) {
		sendToLog(LogLevel::Info, "SteamController::Interface claim error %d\n", r);
		// OS X may not allow this but could still be fine
		//libusb_close(handle);
		//handle = nullptr;
	}

	return true;
}

void SteamController::deinit() {
	// Releasing access to Steam Controller
	auto r = libusb_release_interface(handle, interface_num);
	if (r < 0) {
		sendToLog(LogLevel::Info, "SteamController::Interface release error %d\n", r);
		return;
	}
	libusb_close(handle);
	handle = nullptr;
}

bool SteamController::rumble(float strength, int length) {
	uint8_t dataBlob[64]{
	    0x8f,
	    0x07,
	    0x00, //Trackpad select : 0x01 = left, 0x00 = right
	    0xff, //LSB Pulse High Duration
	    0xff, //MSB Pulse High Duration
	    0xff, //LSB Pulse Low Duration
	    0xff, //MSB Pulse Low Duration
	    0xff, //LSB Pulse repeat count
	    0x04, //MSB Pulse repeat count
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	auto submit = [&dataBlob, this]() {
		auto packetData = new uint8_t[LIBUSB_CONTROL_SETUP_SIZE + sizeof(dataBlob)];
		libusb_fill_control_setup(packetData, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
		                          RequestSetReport, (ReportTypeOutput << 8) | 1, 2, sizeof(dataBlob));
		std::memcpy(&packetData[LIBUSB_CONTROL_SETUP_SIZE], dataBlob, sizeof(dataBlob));

		auto transfer = libusb_alloc_transfer(0);
		libusb_fill_control_transfer(transfer, handle, packetData, completeRumble,
		                             static_cast<void *>(this), 1000);

		int r = libusb_submit_transfer(transfer);
		if (r < 0)
			sendToLog(LogLevel::Info, "SteamController::Command error %d\n", r);

		return r >= 0;
	};

	uint16_t supported[]{10, 20, 30, 40, 50, 60, 70, 80, 90};
	uint16_t power = strength * 100;
	power          = *std::min_element(std::begin(supported), std::end(supported), [&power](const uint16_t &a, const uint16_t &b) {
		return std::abs(power - a) < std::abs(power - b);
	});

	double period          = 1.0 / power;
	uint16_t periodCommand = period * PeriodRatio;
	uint16_t repeatCount   = length / period / 1000.0;

	dataBlob[2] = 0; // channel 0
	dataBlob[3] = periodCommand % 0xff;
	dataBlob[4] = periodCommand / 0xff;
	dataBlob[5] = periodCommand % 0xff;
	dataBlob[6] = periodCommand / 0xff;
	dataBlob[7] = repeatCount % 0xff;
	dataBlob[8] = repeatCount / 0xff;
	bool ret    = submit();

	dataBlob[2] = 1; // channel 1
	ret |= submit();

	return ret;
}

void SteamController::completeRumble(libusb_transfer *transfer) {
	freearr(&transfer->buffer);
	libusb_free_transfer(transfer);
}

// Thanks to Sixaxis_wii

bool DS3Controller::init() {
	auto ctx = joyCtrl.getUsbContext();
	if (!ctx)
		return false;

	if ((handle = libusb_open_device_with_vid_pid(ctx, 0x054C, 0x0268))) {
		sendToLog(LogLevel::Info, "DS3Controller::Initialising DualShock 3\n");
		interface_num = 0;
	} else {
		// Unfortunately nothing
		return false;
	}

	// On Linux, automatically detach and reattach kernel module
	libusb_set_auto_detach_kernel_driver(handle, 1);

	// Claim the USB interface controlling the haptic actuators

	auto r = libusb_claim_interface(handle, interface_num);
	if (r < 0) {
		sendToLog(LogLevel::Info, "DS3Controller::Interface claim error %d\n", r);
		//libusb_close(handle);
		//handle = nullptr;
	}

	// Configure (detaches the controller from PS3)
	configure();

	// Disable the LEDs
	rumble(0, 0);

	return true;
}

void DS3Controller::deinit() {
	// Releasing access to DualShock 3 Controller
	auto r = libusb_release_interface(handle, interface_num);
	if (r < 0) {
		sendToLog(LogLevel::Info, "DS3Controller::Interface release error %d\n", r);
		return;
	}
	libusb_close(handle);
	handle = nullptr;
}

bool DS3Controller::configure() {
	uint8_t dataBlob[]{
	    0x42, 0x0C, 0x00, 0x00};

	auto packetData = new uint8_t[LIBUSB_CONTROL_SETUP_SIZE + sizeof(dataBlob)];
	libusb_fill_control_setup(packetData, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
	                          RequestSetReport, (ReportTypeFeature << 8) | 0xF4, 0x0, sizeof(dataBlob));
	std::memcpy(&packetData[LIBUSB_CONTROL_SETUP_SIZE], dataBlob, sizeof(dataBlob));

	auto transfer = libusb_alloc_transfer(0);
	libusb_fill_control_transfer(transfer, handle, packetData, completeRumble,
	                             static_cast<void *>(this), 1000);

	int r = libusb_submit_transfer(transfer);
	if (r < 0)
		sendToLog(LogLevel::Info, "DS3Controller::Command configure error %d\n", r);

	return r >= 0;
}

bool DS3Controller::rumble(float strength, int length) {
	uint8_t dataBlob[]{
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00, // rumble values [0x00, right-timeout, right-force, left-timeout, left-force]
	    0x00,
	    0x00, // Gyro
	    0x00,
	    0x00,
	    0x00, // 0x02=LED1 .. 0x10=LED4
	    /*
		 * the total time the led is active (0xff means forever)
		 * |     duty_length: how long a cycle is in deciseconds:
		 * |     |                              (0 means "blink very fast")
		 * |     |     ??? (Maybe a phase shift or duty_length multiplier?)
		 * |     |     |     % of duty_length led is off (0xff means 100%)
		 * |     |     |     |     % of duty_length led is on (0xff is 100%)
		 * |     |     |     |     |
		 * 0xff, 0x27, 0x10, 0x00, 0x32,
		 */
	    0xff,
	    0x27,
	    0x10,
	    0x00,
	    0x32, // LED 4
	    0xff,
	    0x27,
	    0x10,
	    0x00,
	    0x32, // LED 3
	    0xff,
	    0x27,
	    0x10,
	    0x00,
	    0x32, // LED 2
	    0xff,
	    0x27,
	    0x10,
	    0x00,
	    0x32, // LED 1
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    // Necessary for Fake DS3
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	};

	last_length     = length;
	completion_time = 0;

	dataBlob[RumbleLengthL] = dataBlob[RumbleLengthR] = cmp::clamp(length / 20, 0, 255);
	dataBlob[RumblePowerL]                            = strength * 255;
	dataBlob[RumblePowerR]                            = strength > 0;

	auto packetData = new uint8_t[LIBUSB_CONTROL_SETUP_SIZE + sizeof(dataBlob)];
	libusb_fill_control_setup(packetData, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
	                          RequestSetReport, (ReportTypeOutput << 8) | 1, 0x0, sizeof(dataBlob));
	std::memcpy(&packetData[LIBUSB_CONTROL_SETUP_SIZE], dataBlob, sizeof(dataBlob));

	auto transfer = libusb_alloc_transfer(0);
	libusb_fill_control_transfer(transfer, handle, packetData, completeRumble,
	                             static_cast<void *>(this), 1000);

	int r = libusb_submit_transfer(transfer);
	if (r < 0)
		sendToLog(LogLevel::Info, "DS3Controller::Command rumble error %d\n", r);

	return r >= 0;
}

void DS3Controller::completeRumble(libusb_transfer *transfer) {
	auto self = static_cast<DS3Controller *>(transfer->user_data);
	if (self->last_length)
		self->completion_time = SDL_GetTicks() + self->last_length;
	freearr(&transfer->buffer);
	libusb_free_transfer(transfer);
}

void DS3Controller::ping() {
	if (completion_time > 0 && SDL_GetTicks() > completion_time)
		rumble(0, 0);
}

// Thanks to ScpToolkit

bool DS4Controller::init() {
	auto ctx = joyCtrl.getUsbContext();
	if (!ctx)
		return false;

	if ((handle = libusb_open_device_with_vid_pid(ctx, 0x054C, 0x05C4))) {
		sendToLog(LogLevel::Info, "DS4Controller::Initialising DualShock 4\n");
		interface_num = 0;
	} else {
		// Unfortunately nothing
		return false;
	}

	// On Linux, automatically detach and reattach kernel module
	libusb_set_auto_detach_kernel_driver(handle, 1);

	// Claim the USB interface controlling the haptic actuators

	auto r = libusb_claim_interface(handle, interface_num);
	if (r < 0) {
		sendToLog(LogLevel::Info, "DS4Controller::Interface claim error %d\n", r);
		//libusb_close(handle);
		//handle = nullptr;
	}

	// Disable the LEDs
	rumble(0, 0);

	return true;
}

void DS4Controller::deinit() {
	// Releasing access to DualShock 4 Controller
	auto r = libusb_release_interface(handle, interface_num);
	if (r < 0) {
		sendToLog(LogLevel::Info, "DS4Controller::Interface release error %d\n", r);
		return;
	}
	libusb_close(handle);
	handle = nullptr;
}

bool DS4Controller::rumble(float strength, int length) {
	uint8_t dataBlob[]{
	    0x05,
	    0xFF, 0x00, 0x00, 0x00, 0x00,
	    0xFF, 0xFF, 0xFF, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00};

	last_length     = length;
	completion_time = 0;

	dataBlob[LedRed] = dataBlob[LedGreen] = dataBlob[LedBlue] = 0;
	dataBlob[RumblePowerLarge]                                = strength * 255;
	dataBlob[RumblePowerSmall]                                = strength * 255;

	auto packetData = new uint8_t[LIBUSB_CONTROL_SETUP_SIZE + sizeof(dataBlob)];
	libusb_fill_control_setup(packetData, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
	                          RequestSetReport, (ReportTypeOutput << 8) | 1, 0x0, sizeof(dataBlob));
	std::memcpy(&packetData[LIBUSB_CONTROL_SETUP_SIZE], dataBlob, sizeof(dataBlob));

	auto transfer = libusb_alloc_transfer(0);
	libusb_fill_control_transfer(transfer, handle, packetData, completeRumble,
	                             static_cast<void *>(this), 1000);

	int r = libusb_submit_transfer(transfer);
	if (r < 0)
		sendToLog(LogLevel::Info, "DS4Controller::Command error %d\n", r);

	return r >= 0;
}

void DS4Controller::completeRumble(libusb_transfer *transfer) {
	auto self = static_cast<DS4Controller *>(transfer->user_data);
	if (self->last_length)
		self->completion_time = SDL_GetTicks() + self->last_length;
	freearr(&transfer->buffer);
	libusb_free_transfer(transfer);
}

void DS4Controller::ping() {
	if (completion_time > 0 && SDL_GetTicks() > completion_time)
		rumble(0, 0);
}

#endif

int JoystickController::ownInit() {

#ifdef USE_LIBUSB
	// Check if we have a Steam Controller and add it
	std::unique_ptr<NativeController> sc = std::make_unique<SteamController>();
	if (sc->init())
		nativeControllers.push_back(std::move(sc));

	// Do the same for DualShock 3
	sc = std::make_unique<DS3Controller>();
	if (sc->init())
		nativeControllers.push_back(std::move(sc));

	// Do the same for DualShock 4
	sc = std::make_unique<DS4Controller>();
	if (sc->init())
		nativeControllers.push_back(std::move(sc));
#endif

	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) == 0) {
		bool undefinedFound{false};
		for (int i = 0; i < SDL_NumJoysticks(); i++) {
			SDL_Joystick *joy = SDL_JoystickOpen(i);
			if (joy) {
				SDL_JoystickID id = SDL_JoystickInstanceID(joy);
				if (id >= 0) {
					std::array<uint8_t, 16> guid;
					std::memcpy(&guid[0], SDL_JoystickGetGUID(joy).data, 16);

					if (std::all_of(guid.begin(), guid.end(), [](uint8_t j) { return j == 0; })) {
						undefinedFound = true;
					}

					sendToLog(LogLevel::Info, "Initialising joystick(%d -> %d): %s\n", i, id, SDL_JoystickNameForIndex(i));

					joystick[id] = Info{joy, guid};

					SDL_Haptic *hapt = SDL_HapticOpenFromJoystick(joy);

					if (hapt) {
						haptic[id] = hapt;
						sendToLog(LogLevel::Info, "Haptic status: maybe supported\n");
					} else {
						sendToLog(LogLevel::Info, "Haptic status: unsupported\n");
					}

					sendToLog(LogLevel::Info, "Gamepad GUID:");
					for (auto &b : guid)
						sendToLog(LogLevel::Info, " 0x%.2X%s", b, &b == &guid[15] ? "" : ",");
					sendToLog(LogLevel::Info, "\n");
				}
			}
		}
		if (undefinedFound) {
			sendToLog(LogLevel::Info, "Warning: some gamepad had a null id, it may work improperly\n");
#ifdef WIN32
			// Time to write some hacks for Windows
			uint8_t firstByte{0x00};
			for (auto &elem : joystick) {
				if (elem.second.guid == JOYGUID[JoyId::Rumblepad2]) {
					firstByte = 0x01;
					sendToLog(LogLevel::Info, "Warning: Manually disabling DirectInput for Logitech Cordless RumblePad 2\n");
					if (SDL_JoystickGetAttached(elem.second.handler)) {
						SDL_JoystickClose(elem.second.handler);
					}
					joystick.erase(elem.first);
					break;
				} else if (elem.second.guid == JOYGUID[JoyId::DualShock4]) {
					firstByte = 0x02;
					sendToLog(LogLevel::Info, "Warning: Manually disabling DirectInput for Sony DualShock 4\n");
					if (SDL_JoystickGetAttached(elem.second.handler)) {
						SDL_JoystickClose(elem.second.handler);
					}
					joystick.erase(elem.first);
					break;
				}
			}

			if (firstByte != 0x00) {
				for (auto &elem : joystick) {
					if (std::all_of(elem.second.guid.begin(), elem.second.guid.end(), [](uint8_t i) { return i == 0; })) {
						elem.second.guid[0] = firstByte;
					}
				}
			}
#endif
		}
	}

	return 0;
}

int JoystickController::ownDeinit() {
	for (auto &ctrl : nativeControllers)
		ctrl->deinit();

	for (auto &hapt : haptic)
		if (hapt.second)
			SDL_HapticClose(hapt.second);

	for (auto &joy : joystick)
		if (SDL_JoystickGetAttached(joy.second.handler))
			SDL_JoystickClose(joy.second.handler);

#ifdef USE_LIBUSB
	if (usbContext) {
		libusb_exit(usbContext);
		usbContext = nullptr;
	}
#endif

	return 0;
}

void JoystickController::provideCustomMapping(const char *mapping) {
	std::stringstream userMapping(mapping);

	for (size_t i = 0; !userMapping.eof() && i < KEYMAP.size(); i++) {
		if (userMapping.peek() == ',')
			userMapping.get();

		int c = userMapping.peek();
		if ((c < '0' || c > '9') && c != '-') {
			ons.errorAndExit("Invalid gamepad mapping provided");
			return; //dummy
		}

		userMapping >> c;
		if (c >= 0)
			customMapping[c] = KEYMAP[i];
	}

	usingCustomMapping = true;
}

SDL_Scancode JoystickController::transButton(uint8_t button, SDL_JoystickID id) {
	auto &info = joystick[id];
	if (info.handler == nullptr) {
		if (ons.debug_level > 0)
			sendToLog(LogLevel::Info, "This joystick was not used\n");
		return SDL_SCANCODE_UNKNOWN;
	}

	if (ons.debug_level > 0)
		sendToLog(LogLevel::Info, "Gamepad event, button: %d\n", button);

	if (usingCustomMapping) {
		auto r = customMapping.find(button);
		return r != customMapping.end() ? r->second : SDL_SCANCODE_UNKNOWN;
	}

	for (auto &transPair : joyMap) {
		if (transPair.first == info.guid) {
			auto r = transPair.second.find(button);
			return r != transPair.second.end() ? r->second : SDL_SCANCODE_UNKNOWN;
		}
	}

	if (ons.debug_level > 0)
		sendToLog(LogLevel::Info, "No mapping found\n");

	// Use DualShock 3 mapping if nothing was found
	auto r = joyMap[0].second.find(button);
	if (r != joyMap[0].second.end())
		return r->second;

	return SDL_SCANCODE_UNKNOWN;
}

SDL_Scancode JoystickController::transHat(uint8_t button, SDL_JoystickID id) {
	if (joystick[id].handler == nullptr) {
		if (ons.debug_level > 0)
			sendToLog(LogLevel::Info, "This joystick was not used\n");
		return SDL_SCANCODE_UNKNOWN;
	}

	int k = -1;

	switch (button) {
		case SDL_HAT_LEFTDOWN:
		case SDL_HAT_RIGHTDOWN:
		case SDL_HAT_DOWN:
			k = 18;
			break;
		case SDL_HAT_LEFT:
			k = 19;
			break;
		case SDL_HAT_LEFTUP:
		case SDL_HAT_RIGHTUP:
		case SDL_HAT_UP:
			k = 20;
			break;
		case SDL_HAT_RIGHT:
			k = 21;
			break;
	}

	if (ons.debug_level > 0)
		sendToLog(LogLevel::Info, "Gamepad event, hat move: %d\n", k);

	if (k < 0) {
		return SDL_SCANCODE_UNKNOWN;
	}

	if (usingCustomMapping) {
		auto r = customMapping.find(k);
		if (r != customMapping.end())
			return r->second;
	}

	return KEYMAP[k];
}

SDL_Event JoystickController::transAxis(SDL_JoyAxisEvent &axisEvent) {
	SDL_Event eventbase{};
	SDL_KeyboardEvent &event = eventbase.key;

	event.keysym.scancode = SDL_SCANCODE_UNKNOWN;

	auto &info = joystick[axisEvent.which];
	if (info.handler == nullptr) {
		if (ons.debug_level > 0)
			sendToLog(LogLevel::Info, "This joystick was not used\n");
		return eventbase;
	}

	int axis = -1;
	if (info.guid == JOYGUID[JoyId::GenericXinputNouveau] || info.guid == JOYGUID[JoyId::GenericXinput]) {
		//if (axisEvent.axis == 2 || axisEvent.axis == 5)
		//	sendToLog(LogLevel::Info, "Received axis: %d value: %d\n", axisEvent.axis, axisEvent.value);

		if (axisEvent.axis == 2 || axisEvent.axis == 5) {
			auto map = axisEvent.axis == 2 ? KEYMAP[13] : KEYMAP[14];
			if (axisEvent.value > 0) {
				if (info.prevAxis != -1 && axisEvent.axis != info.prevAxis) {
					// Unpress the button
					map                   = info.prevAxis == 2 ? KEYMAP[13] : KEYMAP[14];
					event.type            = SDL_KEYUP;
					event.keysym.scancode = map;
					info.prevAxis         = -1;
				} else {
					event.type            = SDL_KEYDOWN;
					event.keysym.scancode = map;
					info.prevAxis         = axisEvent.axis;
				}
			} else if (axisEvent.value <= 0 && axisEvent.axis == info.prevAxis) {
				event.type            = SDL_KEYUP;
				event.keysym.scancode = map;
				info.prevAxis         = -1;
			}
		}
	} else {
		// rerofumi: Jan.15.2007
		// DS3 pad has 0x1b axis (with analog button)
		if (axisEvent.axis < 2)
			axis = ((3200 > axisEvent.value) && (axisEvent.value > -3200) ? -1 :
			                                                                (axisEvent.axis * 2 + (axisEvent.value > 0 ? 1 : 0)));

		if (axis != info.prevAxis) {
			if (axis == -1) {
				event.type            = SDL_KEYUP;
				event.keysym.scancode = axisMap[info.prevAxis];
			} else {
				event.type            = SDL_KEYDOWN;
				event.keysym.scancode = axisMap[axis];
			}

			info.prevAxis = axis;
		}
	}

	return eventbase;
}

bool JoystickController::rumble(float strength, int length) {
	bool success{false};
	if (preferredRumbleMethod == RumbleMethod::SDL) {
		success = rumbleSDL(strength, length);
		if (!success)
			success = rumbleLibusb(strength, length);
	} else {
		success = rumbleLibusb(strength, length);
		if (!success)
			success = rumbleSDL(strength, length);
	}
	return success;
}

bool JoystickController::rumbleSDL(float strength, int length) {
	bool success{false};
	for (auto &hapt : joyCtrl.haptic) {
		if (hapt.second) {
			if (SDL_HapticRumbleInit(hapt.second)) {
				if (ons.debug_level > 0)
					sendToLog(LogLevel::Error, "ERROR: SDL_HapticRumbleInit(haptic) failed with %s\n", SDL_GetError());
				return false;
			}

			if (SDL_HapticRumblePlay(hapt.second, strength, length)) {
				if (ons.debug_level > 0)
					sendToLog(LogLevel::Error, "ERROR: SDL_HapticRumblePlay(haptic, strength=%f, length=%d) failed with %s\n", strength, length, SDL_GetError());
				return false;
			}
			success = true;
			// Even after getting a success, we can end up returning false if a later gamepad fails haptic init ... is this desired?
		}
	}
	return success;
}

bool JoystickController::rumbleLibusb(float strength, int length) {
	bool success{false};
	for (auto &ctrl : nativeControllers) {
		if (ctrl->rumble(strength, length)) {
			success = true;
		}
	}
	return success;
}

libusb_context *JoystickController::getUsbContext() {
#ifdef USE_LIBUSB
	if (!usbContext) {
		int r = libusb_init(&usbContext);
		if (r < 0)
			sendToLog(LogLevel::Error, "ERROR: libusb_init failed with %d\n", r);
	}
#endif

	return usbContext;
}

void JoystickController::handleUsbEvents() {
#ifdef USE_LIBUSB
	if (usbContext) {
		struct timeval tv {};
		libusb_handle_events_timeout_completed(usbContext, &tv, nullptr);
		for (auto &ctrl : nativeControllers) {
			ctrl->ping();
		}
	}
#endif
}
