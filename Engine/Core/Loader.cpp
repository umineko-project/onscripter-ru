/**
 *  Loader.cpp
 *  ONScripter-RU
 *
 *  Engine entry point (main).
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "External/Compatibility.hpp"
#include "Engine/Components/DiscordEvents.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Readers/Direct.hpp"
#include "Support/FileIO.hpp"
#include "Support/Unicode.hpp"
#include "Resources/Support/Version.hpp"

#include <unistd.h>
#ifdef WIN32
#include <windows.h>
extern "C" {
// Enforce discrete graphics card if available
__declspec(dllexport) unsigned long NvOptimusEnablement        = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

// We also wrap dll loading code on Windows for multiver ANGLE support
void *__real_SDL_LoadObject(const char *sofile);
void *__wrap_SDL_LoadObject(const char *sofile);
}
#endif
#include <cstdio>

ControllerCollection ctrl;

static void initFileIO() {
	if (!FileIO::initialised()) {
#ifdef LINUX
		// Stick to legacy name on Linux, because the users already have saves there
		FileIO::init("ONScripter-RU", "onscripter");
#else
		FileIO::init("ONScripter-RU", "ONScripter-RU");
#endif
	}
}

#ifdef WIN32
void *__wrap_SDL_LoadObject(const char *sofile) {
	initFileIO();

	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms724439.aspx
	// We do not manifest ourselves for win8.1, so it is safe to assume that we get 6.2
	uint16_t kernelVer     = GetVersion();
	uint8_t kernelVerMajor = kernelVer & 0xFF;
	uint8_t kernelVerMinor = kernelVer >> 8;

	sendToLog(LogLevel::Info, "Loading %s on %d.%d or higher\n",
	          sofile, kernelVerMajor, kernelVerMinor);

	// We have 10 here, but actually we should never get anything above 6.2.
	std::pair<uint8_t, uint8_t> targetVers[]{
	    {10, 0}, {6, 2}, {6, 1}, {6, 0}, {5, 1}, {0, 0}};

	for (auto targetVer : targetVers) {
		auto targetMajor = targetVer.first;
		auto targetMinor = targetVer.second;

		// Skip low kernels
		if ((kernelVerMajor == targetMajor && kernelVerMinor < targetMinor) ||
		    kernelVerMajor < targetMajor)
			continue;

		auto lookupdir = std::string(FileIO::getLaunchDir()) + std::string("dlls") + DELIMITER;
		if (targetMajor > 0)
			lookupdir += std::to_string(targetMajor) + '.' + std::to_string(targetMinor) + DELIMITER;

		//sendToLog(LogLevel::Info, "Looking up %s%s\n", lookupdir.c_str(), sofile);

		if (FileIO::accessFile(lookupdir + sofile, FileType::File)) {
			sendToLog(LogLevel::Info, "Redirected to %s%s\n", lookupdir.c_str(), sofile);

			// This is just a hack for some libraries trying to load other libraries from their dir.
			SetDllDirectoryW(decodeUTF8StringWide(lookupdir.c_str()).c_str());

			return __real_SDL_LoadObject((lookupdir + sofile).c_str());
		}
	}

	return __real_SDL_LoadObject(sofile);
}
#endif

[[noreturn]] static void performTerminate(const char *message) {
	sendToLog(LogLevel::Error, "%s\nExiting...\n", message);
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ONScripter-RU", message, nullptr);
	ctrl.quit(-1);
}

[[noreturn]] static void optionHelp() {
	FileIO::prepareConsole(150, 30);
	printf("Usage: onscripter-ru [option ...]\n");
	printf("     --cdaudio                    use CD audio if available\n");
#ifdef WIN32
	printf("     --waveout-audio              use the windows waveform audio driver (instead of Direct Sound)\n");
#endif
	printf("     --match-audiodevice-to-bgm   reset audio to match bgm specs\n");
	printf("     --nomatch-audiodevice-to-bgm don't reset audio to match bgm specs (default)\n");
	printf("     --registry file              set a registry file\n");
	printf("     --dll file                   set a dll file\n");
	printf(" -r, --root path                  set the root path to the game\n");
	printf(" -s, --save path                  set the path to use for saved games\n");
#if defined(WIN32)
	printf("     --disable-icloud             do not store saved games in iCloud for Windows\n");
	printf("     --current-user-appdata       use the current user's AppData folder instead of AllUsers' AppData\n");
	printf("     --use-console                use Windows Console for application output\n");
#elif defined(MACOSX)
	printf("     --disable-icloud             do not store saved games in iCloud\n");
	printf("     --skip-on-cmd                Cmd key behaves like Ctrl\n");
#endif
	printf("     --use-logfile                use out.txt and err.txt for application output\n");
	printf("     --use-app-icons              use the icns for the current application, if bundled/embedded\n");
	printf("     --gameid id                  set game identifier (like with game.id)\n");
	printf("     --game-script                set game script filename\n");
	printf("     --fullscreen                 start in fullscreen mode\n");
	printf("     --window                     start in window mode\n");
	printf("     --scale                      scale game to native display size when in fullscreen mode.\n");
	printf("     --window-width width         set preferred window width\n");
	printf("     --force-png-alpha            always use PNG alpha channels\n");
	printf("     --force-png-nscmask          always use NScripter-style masks\n");
	printf("     --detect-png-nscmask         detect PNG alpha images that actually use masks\n");
	printf("     --force-button-shortcut      ignore useescspc and getenter command\n");
	printf("     --automode-time time         default time at clickwaits before continuing, when in automode\n");
	printf("     --voicedelay-time time       additional delay after playing the voices, when in automode (default: 650)\n");
	printf("     --voicewait-time time        additional delay before automatic voice continuation (default: 500)\n");
	printf("     --final-voicedelay-time time additional delay after playing the final voice in a dialogue, when in automode (overrides voicedelay-time)\n");
	printf("     --nsa-offset offset          use byte offset x when reading arc*.nsa files\n");
	printf("     --allow-color-type-only      syntax option for only recognizing color type for color arguments\n");
	printf("     --enable-wheeldown-advance   advance the text on mouse wheeldown event\n");
	printf("     --set-tag-page-origin-to-1   syntax option for setting 'gettaglog' origin to 1 instead of 0\n");
	printf("     --answer-dialog-with-yes-ok  have 'yesnobox' and 'okcancelbox' give 'yes/ok' result\n");
	printf("     --audiodriver dev            set the SDL_AUDIODRIVER to dev\n");
	printf("     --audiobuffer size           set the audio buffer size in kB (default: 2)\n");
	printf("     --audioformat format         set the audio format (choose from s8, u8, s16, u16, s32, f32)\n");
	printf("     --renderer-blacklist list    comma-separated list of disabled renderers (choose from GL2, GLES2, GLES3, ANGLE2, ANGLE3)\n");
	printf("     --prefer-renderer name       try using this renderer first of all\n");
	printf("     --d3dcompiler compiler.dll   hlsl shader compiler library for angle (e.g. d3dompiler_43.dll)\n");
	printf("     --force-vsync                forces vsync (default on Windows)\n");
	printf("     --try-late-swap              tries late swap vsync mode (default on other OS)\n");
	printf("     --no-texture-reuse           forces freed textures deletion\n");
	printf("     --texture-upload style       set preferred texture uploading fallback (ramcopy or perrow, GLES2 only)\n");
	printf("     --no-glclear                 workaround for visual glitches on some specific hardware\n");
	printf("     --render-self mode           workaround for certain drivers not supporting rendering to self (auto, yes, no)\n");
	printf("     --simulate-reads             workaround for visual glitches on some specific hardware\n");
	printf("     --hwdecoder state            pass on/off to enable/disable hardware video decoder (default: on)\n");
	printf("     --hwconvert state            pass on/off to enable/disable hardware format conversion (default: on)\n");
	printf("     --breakup mode               pass new/old/newintel to enable/disable new breakup effect (default: new)\n");
	printf("     --glassbreak mode            pass new/old to enable/disable new glassbreak effect (default: new)\n");
	printf("     --texlimit size              set the maximum texture dimensions (in pixels)\n");
	printf("     --chunklimit size            set the maximum texture chunk size (in bytes)\n");
	printf("     --mouse-scrollmul mul        set mouse scroll multipler and direction\n");
	printf("     --touch-scrollmul mul        set touch scroll multipler and direction\n");
	printf("     --full-clip-limit            reduces visible fullscreen area to mitigate edge artifacts on some resolutions\n");
	printf("     --ramlimit size              set the amount of ram available on your system in megabytes\n");
	printf("     --strict                     treat warnings more like errors\n");
	printf("     --debug                      generate runtime debugging output (use multiple times to increase debug level)\n");
	printf("     --check-file-case            attempt to check file case on case-insensitive file systems\n");
	printf("     --show-fps                   display a ms/frame counter in the window title\n");
	printf("     --force-fps value            override all fps changes to this value\n");
	printf("     --cursor                     set cursor parameters: hide, show, auto are supported (default: auto)\n");
	printf("     --pad-map                    provide custom button mapping for a gamepad\n");
	printf("     --prefer-rumble              specify preferred method of gamepad rumble (sdl/libusb)\n");
	printf("     --font-overrides             provides custom font mapping interface\n");
	printf("     --font-multiplier            provides custom font scaling interface\n");
	printf("     --lang-dir                   provides language-specific game directory\n");
	printf("     --font-dir                   provides language-specific font directory\n");
	printf("     --system-offset-x            left offset to compensate for system forced offset\n");
	printf("     --system-offset-y            top offset to compensate for system forced offset\n");
	printf(" -h, --help                       show this help and exit\n");
	printf(" -v, --version                    show the version information and exit\n");
	FileIO::waitConsole();
	ctrl.quit(0);
}

[[noreturn]] static void optionVersion() {
	FileIO::prepareConsole(150, 30);
	printf("%s version %s '%s' (%d.%02d)\n", VERSION_STR1, ONS_VERSION, ONS_CODENAME, NSC_VERSION / 100, NSC_VERSION % 100);
	printf("Original written by Ogapee <ogapee@aqua.dti2.ne.jp>,\n");
	printf("English fork maintained by \"Uncle\" Mion Sonozaki <UncleMion@gmail.com>\n\n");
	printf("%s\n", VERSION_STR2);
	printf("This is free software; see the source for copying conditions.\n");
	FileIO::waitConsole();
	ctrl.quit(0);
}

static void parseOptions(int argc, char **argv, bool &hasArchivePath) {
	argv++;
	while (argc > 1) {
		if (argv[0][0] == '-') {
			if (!std::strcmp(argv[0] + 1, "h") || !std::strcmp(argv[0] + 1, "-help")) {
				optionHelp();
			} else if (!std::strcmp(argv[0] + 1, "v") || !std::strcmp(argv[0] + 1, "-version")) {
				optionVersion();
			} else if (!std::strcmp(argv[0] + 1, "-cdaudio")) {
				ons.enableCDAudio();
				ons.ons_cfg_options["cdaudio"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-waveout-audio")) {
				ons.ons_cfg_options["audiodriver"] = "winmm";
			} else if (!std::strcmp(argv[0] + 1, "-audiodriver")) {
				argc--;
				argv++;
				ons.ons_cfg_options["audiodriver"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-audiobuffer")) {
				argc--;
				argv++;
				ons.ons_cfg_options["audiobuffer"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-audioformat")) {
				argc--;
				argv++;
				ons.ons_cfg_options["audioformat"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-renderer-blacklist")) {
				argc--;
				argv++;
				ons.ons_cfg_options["renderer-blacklist"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-prefer-renderer")) {
				argc--;
				argv++;
				ons.ons_cfg_options["prefer-renderer"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-d3dcompiler")) {
				argc--;
				argv++;
				ons.ons_cfg_options["d3dcompiler"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-match-audiodevice-to-bgm")) {
				ons.setMatchBgmAudio(true);
				ons.ons_cfg_options["match-audiodevice-to-bgm"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-nomatch-audiodevice-to-bgm")) {
				ons.setMatchBgmAudio(false);
				ons.ons_cfg_options["nomatch-audiodevice-to-bgm"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-registry")) {
				argc--;
				argv++;
				ons.setRegistryFile(argv[0]);
				ons.ons_cfg_options["registry"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-dll")) {
				argc--;
				argv++;
				ons.setDLLFile(argv[0]);
				ons.ons_cfg_options["dll"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-english")) {
				ons.ons_cfg_options["english"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-japanese")) {
				ons.ons_cfg_options["japanese"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "r") || !std::strcmp(argv[0] + 1, "-root") || !std::strcmp(argv[0] + 1, "-tmp-root")) {
				// Unlike saves that could be redefined, do not allow changing root path later!
				if (!hasArchivePath) {
					bool tmp       = argv[0][2] == 't';
					hasArchivePath = true;
					argc--;
					argv++;
					auto spath = FileIO::safePath(argv[0], true);
					ons.setArchivePath(spath);
					// tmp-root is only used for relaunch purposes
					if (!tmp)
						ons.ons_cfg_options["root"] = spath;
					freearr(&spath);
				} else {
					argc--;
					argv++;
					sendToLog(LogLevel::Error, "Ignoring next attempt to redefine root path from %s to %s!\n", ons.getPath(0), argv[0]);
				}
			} else if (!std::strcmp(argv[0] + 1, "s") || !std::strcmp(argv[0] + 1, "-save")) {
				argc--;
				argv++;
				auto spath = FileIO::safePath(argv[0], true);
				ons.setSavePath(spath);
				ons.ons_cfg_options["save"] = spath;
				freearr(&spath);
			} else if (!std::strcmp(argv[0] + 1, "-current-user-appdata")) {
#ifdef WIN32
				ons.setUserAppData();
#endif
				ons.ons_cfg_options["current-user-appdata"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-use-app-icons")) {
				ons.setUseAppIcons();
				ons.ons_cfg_options["use-app-icons"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-fullscreen")) {
				ons.ons_cfg_options["fullscreen"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-window")) {
				ons.ons_cfg_options["window"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-window-width")) {
				argc--;
				argv++;
#if !defined(IOS) && !defined(DROID)
				ons.setPreferredWidth(argv[0]);
#endif
				ons.ons_cfg_options["window-width"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-gameid")) {
				argc--;
				argv++;
				ons.setGameIdentifier(argv[0]);
				ons.ons_cfg_options["gameid"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-nsa-offset")) {
				argc--;
				argv++;
				ons.setNsaOffset(argv[0]);
				ons.ons_cfg_options["nsa-offset"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-force-button-shortcut")) {
				ons.enableButtonShortCut();
				ons.ons_cfg_options["force-button-shortcut"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-automode-time")) {
				argc--;
				argv++;
				ons.setPreferredAutomodeTime(argv[0]);
				ons.ons_cfg_options["automode-time"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-voicedelay-time")) {
				argc--;
				argv++;
				ons.setVoiceDelayTime(argv[0]);
				ons.ons_cfg_options["voicedelay-time"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-voicewait-time")) {
				argc--;
				argv++;
				ons.setVoiceWaitTime(argv[0]);
				ons.ons_cfg_options["voicewait-time"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-final-voicedelay-time")) {
				argc--;
				argv++;
				ons.setFinalVoiceDelayTime(argv[0]);
				ons.ons_cfg_options["final-voicedelay-time"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-enable-wheeldown-advance")) {
				ons.enableWheelDownAdvance();
				ons.ons_cfg_options["enable-wheeldown-advance"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-debug")) {
				ons.add_debug_level();
			} else if (!std::strcmp(argv[0] + 1, "-check-file-case")) {
				FileIO::setPathCaseValidation(true);
			} else if (!std::strcmp(argv[0] + 1, "-allow-color-type-only")) {
				ons.allow_color_type_only                    = true;
				ons.ons_cfg_options["allow-color-type-only"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-set-tag-page-origin-to-1")) {
				ons.set_tag_page_origin_to_1                    = true;
				ons.ons_cfg_options["set-tag-page-origin-to-1"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-answer-dialog-with-yes-ok")) {
				ons.answer_dialog_with_yes_ok                    = true;
				ons.ons_cfg_options["answer-dialog-with-yes-ok"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-strict")) {
				ons.setStrict();
				ons.ons_cfg_options["strict"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-scale")) {
				ons.ons_cfg_options["scale"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-discord-ipc")) {
				ons.ons_cfg_options["discord-ipc"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-discord-clicks-over-music")) {
				ons.ons_cfg_options["discord-clicks-over-music"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-detect-png-nscmask")) {
				ons.setMaskType(ONScripter::PNG_MASK_AUTODETECT);
				ons.ons_cfg_options["detect-png-nscmask"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-force-png-alpha")) {
				ons.setMaskType(ONScripter::PNG_MASK_USE_ALPHA);
				ons.ons_cfg_options["force-png-alpha"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-force-png-nscmask")) {
				ons.setMaskType(ONScripter::PNG_MASK_USE_NSCRIPTER);
				ons.ons_cfg_options["force-png-nscmask"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-game-script") || !std::strcmp(argv[0] + 1, "-game_script")) {
				argc--;
				argv++;
				if (std::strlen(argv[0]) < 30 && !ons.script_is_set) {
					copystr(ons.game_script, argv[0], sizeof(ons.game_script));
					ons.script_is_set                  = true;
					ons.ons_cfg_options["game-script"] = argv[0];
				}
			} else if (!std::strcmp(argv[0] + 1, "-use-logfile")) {
				FileIO::setLogMode(FileIO::LogMode::File);
				ons.ons_cfg_options["use-logfile"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-use-console")) {
				FileIO::setLogMode(FileIO::LogMode::Console);
				ons.ons_cfg_options["use-console"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-show-fps")) {
				ons.setShowFPS();
				ons.ons_cfg_options["show-fps"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-force-fps")) {
				argc--;
				argv++;
				ons.ons_cfg_options["force-fps"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-disable-icloud")) {
				ons.ons_cfg_options["disable-icloud"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-force-vsync")) {
				ons.ons_cfg_options["force-vsync"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-try-late-swap")) {
				ons.ons_cfg_options["try-late-swap"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-skip-on-cmd")) {
				ons.ons_cfg_options["skip-on-cmd"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-no-texture-reuse")) {
				ons.ons_cfg_options["no-texture-reuse"] = "noval";
				gpu.texture_reuse                       = false;
			} else if (!std::strcmp(argv[0] + 1, "-texture-upload")) {
				argc--;
				argv++;
				ons.ons_cfg_options["texture-upload"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-no-glclear")) {
				ons.ons_cfg_options["no-glclear"] = "noval";
				gpu.use_glclear                   = false;
			} else if (!std::strcmp(argv[0] + 1, "-render-self")) {
				argc--;
				argv++;
				ons.ons_cfg_options["render-self"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-simulate-reads")) {
				ons.ons_cfg_options["simulate-reads"] = "noval";
				gpu.simulate_reads                    = true;
			} else if (!std::strcmp(argv[0] + 1, "-texlimit")) {
				argc--;
				argv++;
				ons.ons_cfg_options["texlimit"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-chunklimit")) {
				argc--;
				argv++;
				ons.ons_cfg_options["chunklimit"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-mouse-scrollmul")) {
				argc--;
				argv++;
				ons.ons_cfg_options["mouse-scrollmul"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-touch-scrollmul")) {
				argc--;
				argv++;
				ons.ons_cfg_options["touch-scrollmul"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-full-clip-limit")) {
				ons.ons_cfg_options["full-clip-limit"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-ramlimit")) {
				argc--;
				argv++;
				ons.ons_cfg_options["ramlimit"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-hwdecoder")) {
				argc--;
				argv++;
				ons.ons_cfg_options["hwdecoder"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-hwconvert")) {
				argc--;
				argv++;
				ons.ons_cfg_options["hwconvert"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-breakup")) {
				argc--;
				argv++;
				ons.ons_cfg_options["breakup"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-glassbreak")) {
				argc--;
				argv++;
				ons.ons_cfg_options["glassbreak"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-reduce-motion")) {
				argc--;
				argv++;
				ons.ons_cfg_options["reduce-motion"] = "noval";
			} else if (!std::strcmp(argv[0] + 1, "-font-overrides")) {
				argc--;
				argv++;
				ons.ons_cfg_options["font-overrides"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-font-multiplier")) {
				argc--;
				argv++;
				ons.ons_cfg_options["font-multiplier"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-lang-dir")) {
				argc--;
				argv++;
				// Note: intentionally not included in ons.ons_cfg_options,
				// as this value is part of lang.cfg vendor-provided files.
				copystr(ons.langdir_path, argv[0], sizeof(ons.langdir_path));
			} else if (!std::strcmp(argv[0] + 1, "-font-dir")) {
				argc--;
				argv++;
				// Note: intentionally not included in ons.ons_cfg_options, like lang-dir.
				copystr(ons.fontdir_path, argv[0], sizeof(ons.fontdir_path));
			} else if (!std::strcmp(argv[0] + 1, "-dialogue-style")) {
				argc--;
				argv++;
				ons.ons_cfg_options["dialogue-style"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-cursor")) {
				argc--;
				argv++;
				ons.ons_cfg_options["cursor"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-pad-map")) {
				argc--;
				argv++;
				ons.ons_cfg_options["pad-map"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-prefer-rumble")) {
				argc--;
				argv++;
				ons.ons_cfg_options["prefer-rumble"] = argv[0];
			} else if (!std::strncmp(argv[0] + 1, "-env[", 5) && *(argv[0] + 6) != '\0' && std::strchr(argv[0] + 7, ']')) {
				auto sz                   = std::strchr(argv[0] + 7, ']') - argv[0] - 6;
				auto opt                  = std::string(argv[0] + 6, sz);
				ons.user_cfg_options[opt] = argv[1];
				argc--;
				argv++;
			} else if (!std::strcmp(argv[0], "-NSDocumentRevisionsDebugMode")) {
				// Ignore macOS debugger shit.
				argc--;
				argv++;
			} else if (!std::strcmp(argv[0] + 1, "-system-offset-x")) {
				argc--;
				argv++;
				ons.ons_cfg_options["system-offset-x"] = argv[0];
			} else if (!std::strcmp(argv[0] + 1, "-system-offset-y")) {
				argc--;
				argv++;
				ons.ons_cfg_options["system-offset-y"] = argv[0];
			} else {
				char errstr[256];
				std::snprintf(errstr, sizeof(errstr), "unknown option %s", argv[0]);
				ons.errorAndCont(errstr, nullptr, "Command-Line Issue", true);
			}
		} else if (!hasArchivePath) {
			hasArchivePath = true;
			ons.setArchivePath(argv[0]);
			argc--;
			argv++;
		} else {
			optionHelp();
		}
		argc--;
		argv++;
	}
}

static bool parseOptionFile(const char *filename, bool &hasArchivePath) {
	size_t flen = 0;
	std::vector<uint8_t> fbuf;

	if (!FileIO::readFile(filename, flen, fbuf)) {
		// This should not be fatal probably because we might have other files...
		sendToLog(LogLevel::Error, "Couldn't open option file '%s'\n", filename);
		return false;
	}

	sendToLog(LogLevel::Info, "Parsing command-line options from '%s'\n", filename);

	if (fbuf.size() >= 3 && fbuf[0] == 0xEF && fbuf[1] == 0xBB && fbuf[2] == 0xBF) {
		sendToLog(LogLevel::Warn, "Unicode Byte Order Mark detected in '%s'. This should be avoided!\n", filename);
		fbuf.erase(fbuf.begin(), fbuf.begin() + 3);
	}

	std::vector<char *> arguments{nullptr};
	std::string currarg;

	bool lastend       = true;
	bool withinstr     = false;
	bool withincomment = false;
	bool leadingdashes = true;

	auto appendArgument = [&arguments, &currarg, &lastend, &withinstr, &leadingdashes]() {
		if (!lastend && !currarg.empty()) {
			lastend   = true;
			withinstr = false;
			// Arguments names should start with with two leading dashes
			auto arg = copystr(((leadingdashes ? "--" : "") + currarg).c_str());
			arguments.emplace_back(arg);
			currarg.clear();
		}
	};

	for (auto &ch : fbuf) {
		if (lastend && (ch == ';' || ch == '#'))
			withincomment = true;

		if ((!withinstr && !withincomment && (ch == '=' || ch == ' ' || ch == '\t')) ||
		    ch == '\0' || ch == '\r' || ch == '\n') {
			withincomment = false;
			appendArgument();
			// If the argument is a=b, then it should become --a b
			leadingdashes = ch != '=';
		} else if (!withincomment) {
			lastend = false;
			currarg += ch;
		}

		if (ch == '\'' || ch == '\"')
			withinstr = !withinstr;
	}

	// If we still have something
	appendArgument();

	// now parse the options
	if (arguments.size() > 1 && arguments[1])
		parseOptions(static_cast<int>(arguments.size()), arguments.data(), hasArchivePath);
	for (auto &arg : arguments)
		freearr(&arg);
	return true;
}

static bool initWithPath(const char *path, bool &hasArchivePath) {
	// Have we gotten --root previously?
	// If so, we must enforce its path regardless of the ones we try.
	// This is useful for multiple stacked configuration files and app bundles.
	if (!path || hasArchivePath)
		path = ons.getPath(0);

	char tmp_path[PATH_MAX];
	std::snprintf(tmp_path, PATH_MAX, "%s%s", path, CFG_FILE);
	if (ons.ons_cfg_path[0] == '\0') {
		if (FileIO::accessFile(tmp_path, FileType::File)) {
			std::snprintf(ons.ons_cfg_path, PATH_MAX, "%s", path);
			parseOptionFile(tmp_path, hasArchivePath);
		} else {
			std::snprintf(tmp_path, PATH_MAX, "%s%s", path, DEFAULT_CFG_FILE);
			if (FileIO::accessFile(tmp_path)) {
				parseOptionFile(tmp_path, hasArchivePath);
			}
		}
	}

	// Update --root path in case we read it from this configuration file.
	if (hasArchivePath)
		path = ons.getPath(0);

	std::snprintf(tmp_path, PATH_MAX, "%s%s", path, ons.script_is_set ? ons.game_script : DEFAULT_SCRIPT_NAME);

	if (FileIO::accessFile(tmp_path)) {
		if (path[0] != '\0')
			copystr(ons.script_path, path, PATH_MAX);
		else
			copystr(ons.script_path, FileIO::getWorkingDir(), PATH_MAX);

		// We will create the configuration file if necessary
		if (ons.ons_cfg_path[0] == '\0') {
#ifdef DROID
			// script_path may not be available for write access, starting with 4.4.2.
			// http://www.chainfire.eu/articles/113/Is_Google_blocking_apps_writing_to_SD_cards_/
			// For this reason we default to launch dir for storage options.
			copystr(ons.ons_cfg_path, FileIO::getLaunchDir(), PATH_MAX);
#else
			copystr(ons.ons_cfg_path, ons.script_path, PATH_MAX);
#endif
		}

		if (ons.script_is_set) {
			char *ext = std::strrchr(tmp_path, '.');
			if (ext && sizeof(tmp_path) - (ext - tmp_path) >= sizeof(".cfg")) {
				copystr(ext, ".cfg", sizeof(".cfg"));
				bool tmp = false;
				parseOptionFile(tmp_path, tmp);
			}
		}

		return true;
	}

	return false;
}

static void requestHighMemoryUsage() {
#ifdef IOS_LEGACY_HACKS
	// Unlocks more memory on old jailbroken devices. Prohibited presently and requires root/entitlements.
	// Tests confirm memory limit increase up to ~645 MBs instead of ~585 on iPad 4.
	// The actual value appears to be read in a weird manner, so using -1 as suggested by jetsam.
	struct memorystatus_priority_properties_t {
		int32_t priority;
		uint64_t user_data;
	};

	//int memorystatus_control(uint32_t command, int32_t pid, uint32_t flags, void *buffer, size_t buffersize);
	const int MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES    = 2;
	const int MEMORYSTATUS_CMD_SET_JETSAM_HIGH_WATER_MARK = 5;
	//const int JETSAM_PRIORITY_CRITICAL = 19;
	const int JETSAM_PRIORITY_MAX  = 21;
	const int MEMORYSTATUS_SYSCALL = 440;

	memorystatus_priority_properties_t props{0, JETSAM_PRIORITY_MAX};

	sendToLog(LogLevel::Info, "Process pid is %d, will try to increase memory limit now!\n", getpid());

	auto mem = syscall(MEMORYSTATUS_SYSCALL, MEMORYSTATUS_CMD_SET_JETSAM_HIGH_WATER_MARK, getpid(), -1, nullptr, 0);
	auto pri = syscall(MEMORYSTATUS_SYSCALL, MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES, getpid(), 0, &props, sizeof(props));

	sendToLog(LogLevel::Info, "Result is: %d %d\n", mem, pri);

	/* Looks like it is no longer allowed, perhaps, using a tool with root privileges is a better idea?
		#include <stdio.h>
		#include <stdlib.h>
		#include <unistd.h>

		typedef struct memorystatus_priority_properties {
			int32_t  priority;
			uint64_t user_data;
		} memorystatus_priority_properties_t;

		int main(int argc, char **argv) {
			if (argc != 3) {
				std::fprintf(stderr, "You must pass the pid and RAM limit\n");
				return -1;
			}

			unsigned pid = std::atoi(argv[1]);
			int ram = std::atoi(argv[2]);
			std::fprintf(stderr, "Setting memory limit of %d on %u\n", ram, pid);

			//int memorystatus_control(uint32_t command, int32_t pid, uint32_t flags, void *buffer, size_t buffersize);
			const int MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES = 2;
			const int MEMORYSTATUS_CMD_SET_JETSAM_HIGH_WATER_MARK = 5;
			//const int JETSAM_PRIORITY_CRITICAL = 19;
			const int JETSAM_PRIORITY_MAX = 21;
			const int MEMORYSTATUS_SYSCALL = 440;

			memorystatus_priority_properties_t props;
			props.priority = JETSAM_PRIORITY_MAX;
			props.user_data = 0;

			int r = syscall(MEMORYSTATUS_SYSCALL, MEMORYSTATUS_CMD_SET_JETSAM_HIGH_WATER_MARK, pid, ram, nullptr, 0);
			int p = syscall(MEMORYSTATUS_SYSCALL, MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES, pid, 0, &props, sizeof(props));
			std::fprintf(stderr, "Result is %d %d\n", r, p);
		}
	*/
#endif
}

enum {
	CRASHREPORTER_OK            = 0,
	CRASHREPORTER_NO_RUN        = 1,
	CRASHREPORTER_NO_DEBUG      = 2,
	CRASHREPORTER_NO_ALLOCGUARD = 4,
	CRASHREPORTER_NO_RUNTIME    = 8,
	CRASHREPORTER_NO_FUNCS      = 16
};

static int crashReporterError = CRASHREPORTER_NO_RUN;

CONSTRUCTOR setupCrashReporter() {

#ifdef WIN32
	// On Windows check the DEBUG environment variable to show early reports.
	if (std::getenv("DEBUG")) {
		initFileIO();
		FileIO::prepareConsole(150, 30);
	}
#endif

	crashReporterError = CRASHREPORTER_OK;

	auto memoryAllocFailure = []() {
		sendToLog(LogLevel::Error, "Memory allocation failure!\n");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ONScripter-RU", "Memory allocation failure!", nullptr);
#ifdef WIN32
		asm("ud2"); // We want Dr.Mingw catch it
#endif
		std::terminate();
	};

#ifdef WIN32
	if (!SDL_LoadObject("exchndl.dll")) {
		crashReporterError |= CRASHREPORTER_NO_DEBUG;
	}

	// On Windows we additionally try to guard C mallocs
	auto runtime = SDL_LoadObject("msvcrt");
	if (runtime) {
		using SetNewMode    = void (*)(int);
		using SetNewHandler = void (*)(void (*)());
		auto snm            = reinterpret_cast<SetNewMode>(SDL_LoadFunction(runtime, "?_set_new_mode@@YAHH@Z"));
		auto snh            = reinterpret_cast<SetNewHandler>(SDL_LoadFunction(runtime, "?_set_new_handler@@YAP6AHI@ZP6AHI@Z@Z"));
		if (snm)
			snm(1);
		if (snh)
			snh(memoryAllocFailure);
		if (!snm || !snh)
			crashReporterError |= CRASHREPORTER_NO_ALLOCGUARD | CRASHREPORTER_NO_FUNCS;
		SDL_UnloadObject(runtime);
	} else {
		crashReporterError |= CRASHREPORTER_NO_ALLOCGUARD | CRASHREPORTER_NO_RUNTIME;
	}
#else
	std::set_new_handler(memoryAllocFailure);
#endif
}

int main(int argc, char **argv) {
	initFileIO();
	initDiscord();
#ifdef DROID
	// Attempt to launch an already running ons (by tapping on the icon) right after the installation
	// will cause the library not to be loaded and reused without state initialisation.
	// We must prevent this somehow and at any cost.
	pid_t currentPid = getpid();
	static pid_t previousPid;
	FileIO::setLogMode(FileIO::LogMode::Unspecified);
	sendToLog(LogLevel::Info, "Launched with pid %d, previous pid %d\n", currentPid, previousPid);
	if (currentPid == previousPid) {
		sendToLog(LogLevel::Error, "Detected same pids, aborting!\n");
		// We cannot call the normal exit function here since we already have broken state for many reasons.
		// Additionally a new library instance is already being loaded and we are merely awaiting a kill.
		// For this reason we just quit silently and return a successful error code.
		// We should be calling standard std::quick_exit or _Exit functions but neither is exported :/.
		// Yet according to AOSP _exit appears to be _Exit alias and it actually works.
		_exit(0);
	}
	previousPid = currentPid;
#endif

	std::atexit([]() {
		ctrl.deinit();
	});

	requestHighMemoryUsage();

	bool hasArchivePath = false;
	bool works          = false;
#ifndef PUBLIC_RELEASE
	FileIO::setLogMode(FileIO::LogMode::Console); // Enable console logging in development mode
#endif
	ons.script_is_set = false;
	if (!FileIO::setArguments(ons.argc, ons.argv, argc, argv))
		performTerminate("Failed to obtain program arguments!");

	//Firstly, read command line options
	parseOptions(ons.argc, ons.argv, hasArchivePath);

	// --root has zero priority
	if (initWithPath(nullptr, hasArchivePath)) {
		works = true;
	} else {
		// Try app launch dir
		works = initWithPath(FileIO::getLaunchDir(), hasArchivePath);

		// Try app working dir
		if (!works)
			works = initWithPath(FileIO::getWorkingDir(), hasArchivePath);

		// If there is anything to try, do it
		if (!works && FileIO::getPlatformSpecificDir())
			works = initWithPath(FileIO::getPlatformSpecificDir(), hasArchivePath);
	}

	if (!works) {
#ifdef MACOSX
		performTerminate("Invalid launch directory!\nTry executing xattr -cr /path/to/onscripter-ru-osx.app");
#else
		performTerminate("Invalid launch directory!");
#endif
	}

	auto &opts = ons.ons_cfg_options;

	if (!FileIO::setStorageDir(opts.find("current-user-appdata") != opts.end()) ||
	    !FileIO::makeDir(FileIO::getStorageDir(opts.find("disable-icloud") == opts.end()), nullptr, true))
		performTerminate("Failed to access storage directory!");

	if (FileIO::getLogMode() == FileIO::LogMode::File) {
		std::string log_path(FileIO::getStorageDir());
		FileIO::fileHandleReopen(log_path + "out.txt", stdout);
		FileIO::fileHandleReopen(log_path + "err.txt", stderr);
	} else if (FileIO::getLogMode() == FileIO::LogMode::Console) {
		FileIO::prepareConsole(150, 30);
	}

	sendToLog(LogLevel::Info, "Available crash reporter features error code %d\n", crashReporterError);

	// ONScripter is based on a set of dependent controllers that are
	// initialised and deinitialised in a defined order. The deinitialisation
	// order is reverse to the initialisation order. The initialisation order
	// should roughly be as follows:
	// ONScripter (ScriptParser) :: ownInit {
	//  WindowController (for basic hints)
	//  ONScripter (ScripParser) :: initSDL {
	//   SDL [is a dependency for most of the code]
	//   SDL_gpu [is a dependency for most of the code]
	//   Window creation
	//   JoystickController
	//   GPUController
	//   GlyphAtlasController
	//  }
	//  AsyncController
	//  FontsController
	//  DialogueController
	//  TextWindowController [depends on DialogueController]
	//  DynamicPropertyController
	// }
	// Deinitialisation is done automatically by ctrl.quit(exit_code);

	if (ons.init())
		ctrl.quit(-1);
	ons.executeLabel();

	ctrl.quit(0);
}
