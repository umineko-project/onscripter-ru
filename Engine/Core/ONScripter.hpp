/**
 *  ONScripter.hpp
 *  ONScripter-RU
 *
 *  Engine core.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Core/Parser.hpp"
#include "Engine/Components/Dialogue.hpp"
#include "Engine/Components/DynamicProperty.hpp"
#include "Engine/Components/GlyphAtlas.hpp"
#include "Engine/Components/TextWindow.hpp"
#include "Engine/Entities/ConstantRefresh.hpp"
#include "Engine/Entities/Animation.hpp"
#include "Engine/Entities/Spriteset.hpp"
#include "Engine/Entities/Breakup.hpp"
#include "Engine/Entities/StringTree.hpp"
#include "Support/KeyState.hpp"
#include "Support/DirtyRect.hpp"
#include "Support/Camera.hpp"
#include "Support/Cache.hpp"

#ifndef USE_STD_REGEX
#include "External/slre.h"
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_gpu.h>
#include <smpeg2/smpeg.h>
#include <unistd.h>

#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <deque>
#include <set>
#include <iostream>
#include <vector>
#include <list>
#include <memory>
#include <utility>
#include <atomic>
#include <array>
#ifdef USE_STD_REGEX
#include <regex>
#endif

#include <cmath>
#include <cstring>

const int MAX_SPRITE_NUM = 1000;
const int MAX_TEXT_TREES = 50;
const int MAX_PARAM_NUM  = 100;
const int MAX_EFFECT_NUM = 256;

const int DEFAULT_VOLUME     = 100;
const int DEFAULT_AUDIO_RATE = 48000;
const int DEFAULT_AUDIOBUF   = 2048;
const int DEFAULT_FPS        = 30;

const int ONS_MIX_CHANNELS = 50;

enum {
	MIX_WAVE_CHANNEL = ONS_MIX_CHANNELS,
	MIX_BGM_CHANNEL,
	MIX_LOOPBGM_CHANNEL0,
	MIX_LOOPBGM_CHANNEL1,
	MIX_VIDEO_CHANNEL,
	ONS_MIX_EXTRA_CHANNELS, // allocation end
	MIX_CACHE_CHANNEL_BLOCK,
	MIX_CACHE_CHANNEL_ASYNC
};

const int LIPS_AUDIO_RATE        = 48000;
const int CHUNKS_PER_SECOND      = 20;
const int MAX_SOUND_LENGTH       = 180; // in seconds
const int SAMPLES_PER_CHUNK      = DEFAULT_AUDIO_RATE / CHUNKS_PER_SECOND;
const double MS_PER_CHUNK        = (1.0 / CHUNKS_PER_SECOND * 1000);
const int BREAKUP_CELLSEPARATION = 16;
const int BREAKUP_CELLWIDTH      = 24;
const int BREAKUP_CELLFORMS      = 16;

enum class Direction {
	LEFT,
	RIGHT,
	UP,
	DOWN
};

enum class ScriptLanguage {
	English,
	Japanese
};

class ONScripter : public ScriptParser {
public:
	// So that ONSLabel methods can be called from async threads
	friend class LoadImageCacheInstruction;
	friend class LoadImageInstruction;
	friend class PlaySoundInstruction;
	friend class LoadSoundCacheInstruction;
	// Because I'm lazy and tired ><
	friend class GPUController;
	friend class DynamicPropertyController;
	friend class DialogueController;
	friend class ScriptState;
	friend class StringTree;

	using ONSBuf                        = AnimationInfo::ONSBuf;
	static size_t constexpr AfterScene  = 0;
	static size_t constexpr BeforeScene = 1;

	ONScripter();

	bool scriptExecutionPermitted();
	void executeLabel();
	void runScript();

	// ----------------------------------------
	// start-up options
	void enableCDAudio();
	void setMatchBgmAudio(bool flag);
	void setRegistryFile(const char *filename);
	void setDLLFile(const char *filename);
#ifdef WIN32
	void setUserAppData();
#endif
	void setUseAppIcons();
	void setPreferredWidth(const char *widthstr);
	void enableButtonShortCut();
	void setPreferredAutomodeTime(const char *timestr);
	void setVoiceDelayTime(const char *timestr);
	void setVoiceWaitTime(const char *timestr);
	void setFinalVoiceDelayTime(const char *timestr);
	void enableWheelDownAdvance();
	bool script_is_set;
	char game_script[256];
	std::vector<std::string> script_list;
	char script_path[PATH_MAX]{};
	char ons_cfg_path[PATH_MAX]{};
	char langdir_path[PATH_MAX]{};
	char fontdir_path[PATH_MAX]{};
	ScriptLanguage script_language{ScriptLanguage::English};
	//That's a feature duplication
	//TODO: make this an <enum_opt_id, val> map with a const
	//<enum_opt_id, pair<name, desc> reference map
	std::unordered_map<std::string, std::string> ons_cfg_options;  // normal cfg options
	std::unordered_map<std::string, std::string> user_cfg_options; // env[param]=value options
	char **argv;
	int argc;

	void setShowFPS();
	void setStrict() {
		script_h.strict_warnings = true;
	}
	void setGameIdentifier(const char *gameid);
	enum {
		PNG_MASK_AUTODETECT    = 0,
		PNG_MASK_USE_ALPHA     = 1,
		PNG_MASK_USE_NSCRIPTER = 2
	};
	void setMaskType(int mask_type) {
		png_mask_type = mask_type;
	}

	std::list<std::unique_ptr<SDL_Event>> localEventQueue, fetchedEventQueue;
	bool takeEventsOut(uint32_t type);
	bool updateEventQueue();
	void fetchEventsToQueue();

	void runEventLoop();

	void reset();         // used if definereset
	void resetSub();      // used if reset
	void resetFlags();    // for resetting (non-pointer) definereset variables
	void resetFlagsSub(); // for resetting (non-pointer) reset variables

	//Mion: routines for error handling & cleanup
	bool doErrorBox(const char *title, const char *errstr, bool is_simple = false, bool is_warning = false);
	void openDebugFolders();

public:
	/* ---------------------------------------- */
	/* Commands */

	int zOrderOverrideCommand();
	int zOrderOverridePreserveCommand();
	int wheelvalueCommand();
	int waitOnDialogueCommand();
	int waitlipsCommand();
	int waitvoiceCommand();
	int waitvideoCommand();
	int vvSetLogCommand();
	int videovolCommand();
	int verifyFilesCommand();
	int useTextGradientsForSpritesCommand();
	int useTextGradientsCommand();
	int treeSetCommand();
	int treeClearCommand();
	int treeExecuteCommand();
	int treeGetCommand();
	int textDisplaySpeedCommand();
	int textFadeDurationCommand();
	int textAtlasCommand();
	int superSkipCommand();
	int superSkipUnsetCommand();
	int subtitleStopCommand();
	int subtitleLoadCommand();
	int subtitleFontCommand();
	int stopwatchCommand();
	int spritesetPropertyWaitCommand();
	int spritesetPropertyCommand();
	int spritesetPosCommand();
	int spritesetMaskCommand();
	int spritesetEnableCommand();
	int spritesetBlurCommand();
	int spritesetAlphaCommand();
	int spritePropertyWaitCommand();
	int spritePropertyCommand();
	int snapLogCommand();
	int smartquotesCommand();
	int skipUnreadCommand();
	int skipModeCommand();
	int setVoiceWaitMulCommand();
	int setwindowDynamicNamePaddingCommand();
	int setwindowDynamicPaddingCommand();
	int setwindowDynamicDimensionsCommand();
	int setwindowDynamicMainRegionCommand();
	int setwindowDynamicNoNameRegionCommand();
	int setwindowDynamicNameRegionCommand();
	int setwindowDynamicCommand();
	int setwindow4Command();
	int setScaleCenterCommand();
	int setLogCommand();
	int setHotspotCommand();
	int setFpsCommand();
	int scrollableDisplayCommand();
	int scrollableGetHoveredElementCommand();
	int scrollableConfigCommand();
	int scrollableSpriteCommand();
	int scrollableScrollCommand();
	int scrollExceedsCommand();
	int scriptMuteCommand();
	int screenFlipCommand();
	[[noreturn]] int saveresetCommand();
	int rumbleCommand();
	[[noreturn]] int relaunchCommand();
	int regexDefineCommand();
	int quakeApiCommand();
	int quakeendCommand();
	int profilestopCommand();
	int profilestartCommand();
	int presetdefineCommand();
	int pastLogCommand();
	int pastLabelCommand();
	int operateConfigCommand();
	int nearestJumpableLogEntryIndexCommand();
	int nosmartquotesCommand();
	int moreramCommand();
	int makeChoiceCommand();
	int markRangeReadCommand();
	int markReadCommand();
	int markAllReadCommand();
	int mainGotoCommand();
	int mainLabelCommand();
	int mixChannelPropertyWaitCommand();
	int mixChannelPropertyCommand();
	int lvStopCommand();
	int lvSetLogCommand();
	int lvPlayCommand();
	int lookaheadCommand();
	int loadregCommand();
	int loadfromregCommand();
	int loadCacheCommand();
	int lipsSpriteCommand();
	int lipsLimitsCommand();
	int lipsChannelCommand();
	int jautomodeCommand();
	int jnautomodeCommand();
	int jskipSuperCommand();
	int jnskipSuperCommand();
	int jskipCommand();
	int jnskipCommand();
	int ignoreVoiceDelayCommand();
	int hyphenCarryCommand();
	int gotoCommand();
	int globalPropertyWaitCommand();
	int globalPropertyCommand();
	int getvideovolCommand();
	int getramCommand();
	int getScriptPathCommand();
	int getScriptNumCommand();
	int getRendererNameCommand();
	int getRendererNumCommand();
	int getChoiceVectorSizeCommand();
	int getLogDataCommand();
	int getUniqueLogEntryIndexCommand();
	int fallCommand();
	int errorCommand();
	int enableTransitionsCommand();
	int enableCustomCursors();
	int dropCacheCommand();
	int conditionDialogueCommand();
	int disposeDialogueCommand();
	int displayScreenshotCommand();
	int dialogueSetVoiceWaitCommand();
	int dialogueContinueCommand();
	int dialogueNameCommand();
	int dialogueCommand();
	int dialogueAddEndsCommand();
	int debugStrCommand();
	int customCursorCommand();
	int countSymbolsCommand();
	int colorModCommand();
	int clearCacheCommand();
	int clearLogCommand();
	int choicesToStringCommand();
	int choicesFromStringCommand();
	int childImageDetachCommand();
	int childImageCommand();
	int changeFontCommand();
	int cacheSlotTypeCommand();
	int borderPaddingCommand();
	int blurCommand();
	int blendModeCommand();
	int bgmPropertyWaitCommand();
	int bgmPropertyCommand();
	int btnhover_dCommand();
	int backupDisableCommand();
	int asyncLoadCacheCommand();
	int apiCompatCommand();
	int aliasFontCommand();
	int acceptChoiceVectorSizeCommand();
	int acceptChoiceNextIndexCommand();
	int acceptChoiceCommand();
	int atomicCommand();

	int yesnoboxCommand();
	int wavestopCommand();
	int waveCommand();
	int waittimerCommand();
	int waitCommand();
	int vspCommand();
	int voicevolCommand();
	int vCommand();
	int trapCommand();
	int transbtnCommand();
	int textspeeddefaultCommand();
	int textspeedCommand();
	int textshowCommand();
	int textonCommand();
	int textoffCommand();
	int texthideCommand();
	int textexbtnCommand();
	int textclearCommand();
	int textbtnstartCommand();
	int textbtnoffCommand();
	int texecCommand();
	int tateyokoCommand();
	int talCommand();
	int tablegotoCommand();
	int systemcallCommand();
	int strspCommand();
	int stopCommand();
	int sp_rgb_gradationCommand();
	int spstrCommand();
	int spreloadCommand();
	int splitCommand();
	int spclclkCommand();
	int spbtnCommand();
	int skipoffCommand();
	int shellCommand();
	int sevolCommand();
	int setwindow3Command();
	int setwindow2Command();
	int setwindowCommand();
	int seteffectspeedCommand();
	int setcursorCommand();
	int selectCommand();
	int savetimeCommand();
	int saveonCommand();
	int saveoffCommand();
	int savegameCommand();
	int savefileexistCommand();
	int savescreenshotCommand();
	int rndCommand();
	int rmodeCommand();
	int resettimerCommand();
	int resetCommand();
	int repaintCommand();
	int puttextCommand();
	int prnumclearCommand();
	int prnumCommand();
	int printCommand();
	int playCommand();
	int ofscopyCommand();
	int negaCommand();
	int mvCommand();
	int mspCommand();
	int mp3volCommand();
	int mp3stopCommand();
	int mp3fadeoutCommand();
	int mp3fadeinCommand();
	int mp3Command();
	int movieCommand();
	int movemousecursorCommand();
	int mousemodeCommand();
	int monocroCommand();
	int minimizewindowCommand();
	int mesboxCommand();
	int menu_windowCommand();
	int menu_waveonCommand();
	int menu_waveoffCommand();
	int menu_fullCommand();
	int menu_click_pageCommand();
	int menu_click_defCommand();
	int menu_automodeCommand();
	int lsp2Command();
	int lspCommand();
	int loopbgmstopCommand();
	int loopbgmCommand();
	int lookbackflushCommand();
	int lookbackbuttonCommand();
	int logspCommand();
	int locateCommand();
	int locatePxCommand();
	int loadgameCommand();
	int linkcolorCommand();
	int ldCommand();
	int languageCommand();
	int jumpfCommand();
	int jumpbCommand();
	int ispageCommand();
	int isfullCommand();
	int isskipCommand();
	int isdownCommand();
	int inputCommand();
	int indentCommand();
	int humanorderCommand();
	int getzxcCommand();
	int getvoicevolCommand();
	int getversionCommand();
	int gettimerCommand();
	int gettextbtnstrCommand();
	int gettextCommand();
	int gettaglogCommand();
	int gettagCommand();
	int gettabCommand();
	int getspsizeCommand();
	int getspmodeCommand();
	int getskipoffCommand();
	int getsevolCommand();
	int getscreenshotCommand();
	int getsavestrCommand();
	int getretCommand();
	int getregCommand();
	int getpageupCommand();
	int getpageCommand();
	int getmp3volCommand();
	int getmouseposCommand();
	int getmouseoverCommand();
	int getmclickCommand();
	int getlogCommand();
	int getinsertCommand();
	int getfunctionCommand();
	int getenterCommand();
	int getcursorposCommand();
	int getcursorCommand();
	int getcselstrCommand();
	int getcselnumCommand();
	int gameCommand();
	int flushoutCommand();
	int fileexistCommand();
	int exec_dllCommand();
	int exbtnCommand();
	int erasetextwindowCommand();
	int erasetextbtnCommand();
	int endCommand();
	int effectskipCommand();
	int dwavestopCommand();
	int dwaveCommand();
	int dvCommand();
	int drawtextCommand();
	int drawsp3Command();
	int drawsp2Command();
	int drawspCommand();
	int drawfillCommand();
	int drawendCommand();
	int drawclearCommand();
	int drawbg2Command();
	int drawbgCommand();
	int drawCommand();
	int deletescreenshotCommand();
	int delayCommand();
	int defineresetCommand();
	int cspCommand();
	int cselgotoCommand();
	int cselbtnCommand();
	int clickCommand();
	int clCommand();
	int chvolCommand();
	int checkpageCommand();
	int checkkeyCommand();
	int cellCommand();
	int captionCommand();
	int btnwaitCommand();
	int btntimeCommand();
	int btndownCommand();
	int btndefCommand();
	int btnareaCommand();
	int btnasyncCommand();
	int btnCommand();
	int brCommand();
	int bltCommand();
	int bgmdownmodeCommand();
	int bgcopyCommand();
	int bgCommand();
	int barclearCommand();
	int barCommand();
	int aviCommand();
	int automode_timeCommand();
	int autoclickCommand();
	int allsp2resumeCommand();
	int allspresumeCommand();
	int allsp2hideCommand();
	int allsphideCommand();
	int amspCommand();
	int insertmenuCommand();
	int resetmenuCommand();
	int layermessageCommand();

	void jumpToTilde(bool setGlobalFields) {
		// FIXME: setGlobalFields=false appears to be an optimization, but it is broken.
		// Easy to reproduce by putting jskip_s ~ before a *dXXXX label. The log will contain dialogue dups.
		setGlobalFields = true;
		const char *buf = script_h.getNext();
		while (*buf != '\0' && *buf != '~') buf++;
		if (*buf == '~')
			buf++;
		script_h.setCurrent(buf);
		if (setGlobalFields) {
			current_label_info = script_h.getLabelByAddress(buf);
			current_line       = script_h.getLineByAddress(buf, current_label_info);
		}
	}

	uint32_t validSprite(uint32_t no) {
		if (no < MAX_SPRITE_NUM)
			return no;
		errorAndExit("An invalid sprite number was read!");
		return 0; //dummy
	}

	uint32_t validTree(uint32_t no) {
		if (no < MAX_TEXT_TREES)
			return no;
		errorAndExit("An invalid tree number was read!");
		return 0; //dummy
	}

	uint32_t validVolume(uint32_t vol) {
		if (vol <= DEFAULT_VOLUME)
			return vol;
		errorAndExit("An invalid volume level was read!");
		return 0; //dummy
	}

	uint32_t validChannel(uint32_t ch) {
		// This is how it was originally, but I think we must be more restrictive
		//if (ch >= ONS_MIX_CHANNELS) ch = ONS_MIX_CHANNELS-1;
		//return ch;
		if (ch < ONS_MIX_CHANNELS)
			return ch;
		errorAndExit("An invalid channel was read!");
		return 0; //dummy
	}

	void btnwaitCommandHandleResult(uint32_t button_timer_start, VariableInfo *resultVar, ButtonState buttonState, bool del_flag);

	/* ---------------------------------------- */
	/* Event related variables */
	std::vector<std::shared_ptr<ConstantRefreshAction>> registeredCRActions;
	std::unique_ptr<SDL_Event> fingerEvents[2];

protected:
	int ownInit() override;
	int ownDeinit() override;

	enum {
		NOT_EDIT_MODE            = 0,
		EDIT_SELECT_MODE         = 1,
		EDIT_VOLUME_MODE         = 2,
		EDIT_VARIABLE_INDEX_MODE = 3,
		EDIT_VARIABLE_NUM_MODE   = 4,
		EDIT_MP3_VOLUME_MODE     = 5,
		EDIT_VOICE_VOLUME_MODE   = 6,
		EDIT_SE_VOLUME_MODE      = 7
	};

	KeyState keyState;

	bool bgmdownmode_flag{false};
	bool skipLipsAction{false};
	bool endOfEventBatch{false};
	uint32_t lastCursorMove{0};
	bool cursorAutoHide{true};
	void cursorState(bool show);

#if defined(MACOSX)
	int mouse_scroll_mul{-10};
#elif defined(IOS)
	int mouse_scroll_mul{-1};
#else
	int mouse_scroll_mul{-100};
#endif
	int touch_scroll_mul{-300};

	struct EventProcessingState {
		KeyState keyState;
		ButtonState buttonState;
		int skipMode;
		int eventMode;
		unsigned int handler;
		EventProcessingState(unsigned int _handler);
	};
	bool keyDownEvent(SDL_KeyboardEvent &event, EventProcessingState &state);
	void keyUpEvent(SDL_KeyboardEvent &event, EventProcessingState &state);
	bool keyPressEvent(SDL_KeyboardEvent &event, EventProcessingState &state);
	void translateKeyDownEvent(SDL_Event &event, EventProcessingState &state, bool &ret, bool ctrl_toggle);
	void translateKeyUpEvent(SDL_Event &event, EventProcessingState &state, bool &ret);
	bool mouseButtonDecision(EventProcessingState &state, bool left, bool right, bool middle, bool up, bool down);
	bool checkClearAutomode(EventProcessingState &state, bool up);
	bool checkClearTrap(bool left, bool right);
	bool checkClearSkip(EventProcessingState &state);
	bool checkClearVoice();
	bool mousePressEvent(SDL_MouseButtonEvent &event, EventProcessingState &state);
	bool touchEvent(SDL_Event &event, EventProcessingState &state);
	bool mouseScrollEvent(SDL_MouseWheelEvent &event, EventProcessingState &state);
	bool mouseMoveEvent(SDL_MouseMotionEvent &event, EventProcessingState &state);
	void flushEventSub(SDL_Event &event);
	void flushEvent();
	void handleSDLEvents();
	void waitEvent(int count, bool nopPreferred = false);
	void trapHandler();
	void initSDL();
	void reopenAudioOnMismatch(const SDL_AudioSpec &match);
	void openAudio(const SDL_AudioSpec &spec);
	double readChunk(int channel, uint32_t no);
	void getChunkParams(uint32_t &chunk_size, double &max_value);
	void loadLips(int channel = 0);

	void handleRegisteredActions(uint64_t ns);
	bool mainThreadDowntimeProcessing(bool essentialProcessingOnly);
	void advanceGameState(uint64_t ns);
	void constantRefresh();

private:
	enum {
		DISPLAY_MODE_NORMAL  = 0,
		DISPLAY_MODE_TEXT    = 1,
		DISPLAY_MODE_UPDATED = 2
	};
	enum {
		IDLE_EVENT_MODE   = 0,
		WAIT_RCLICK_MODE  = 1,  // for lrclick
		WAIT_BUTTON_MODE  = 2,  // For select, btnwait and rmenu.
		WAIT_INPUT_MODE   = 4,  // can be skipped by a click
		WAIT_TEXTOUT_MODE = 8,  // can be skipped by a click
		WAIT_SLEEP_MODE   = 16, // cannot be skipped by ctrl but not click
		WAIT_TIMER_MODE   = 32,
		WAIT_TEXTBTN_MODE = 64,
		WAIT_VOICE_MODE   = 128,
		WAIT_TEXT_MODE    = 256, // clickwait, newpage, select
		// Special WAITS for ConstantRefresh actions
		WAIT_DELAY_MODE     = 512 | WAIT_TIMER_MODE | WAIT_INPUT_MODE,
		WAIT_WAIT_MODE      = 1024 | WAIT_TIMER_MODE | WAIT_SLEEP_MODE,
		WAIT_WAIT2_MODE     = 2048 | WAIT_TIMER_MODE,
		WAIT_WAITTIMER_MODE = 4096 | WAIT_TIMER_MODE
	};
	enum {
		ALPHA_BLEND_CONST          = 1,
		ALPHA_BLEND_MULTIPLE       = 2,
		ALPHA_BLEND_FADE_MASK      = 3,
		ALPHA_BLEND_CROSSFADE_MASK = 4
	};

	// ----------------------------------------
	// start-up options
	bool cdaudio_flag{false};
	int audiobuffer_size{DEFAULT_AUDIOBUF};
	bool match_bgm_audio_flag{false};
	char *registry_file{nullptr};
	char *dll_file{nullptr};
	char *getret_str{nullptr};
	int getret_int{0};
	bool enable_wheeldown_advance_flag{false};
	bool enable_custom_cursors{false};
	bool show_fps_counter{false};

	void UpdateAnimPosXY(AnimationInfo *animp) {
		animp->pos.x = animp->orig_pos.x;
		animp->pos.y = animp->orig_pos.y;
	}
	void UpdateAnimPosWH(AnimationInfo *animp) {
		animp->pos.w = animp->orig_pos.w;
		animp->pos.h = animp->orig_pos.h;
	}

	// ----------------------------------------
	// Global definitions
	uint32_t internal_timer;
	uint32_t internal_slowdown_counter{0};
	uint32_t ticksNow{0}; //TODO: rewrite more code using this
	bool automode_flag;
	bool preferred_automode_time_set{false};
	int32_t preferred_automode_time{1000};
	int32_t automode_time{1000};
	// Starting with chiru the waits between clicks are set to 650 ms.
	int32_t voicedelay_time{650};
	// Chiru additionally introduced a 500 ms delay for | commands.
	int32_t voicewait_time{500};
	// For rondo we additionally introduced a 0.4 multiplier, which fits most of the scenes.
	float voicewait_multiplier{1.0};
	int32_t final_voicedelay_time{0};
	bool ignore_voicedelay{false};
	int32_t autoclick_time;
	bool reduce_motion{false};

	bool btnasync_active{false};
	bool btnasync_draw_required{false};
	bool atomic_flag{false};

	bool saveon_flag;
	bool internal_saveon_flag; // to saveoff at the head of text

	bool monocro_flag[2];
	SDL_Color monocro_color[2];
	int nega_mode[2];
	int blur_mode[2];

	class LRTrap {
	public:
		bool left{false};
		bool right{false};
		bool enabled{true};
		char *dest{nullptr};
	} lrTrap;

	char *wm_title_string{nullptr};

#ifdef WIN32
	bool current_user_appdata{false};
#endif
	bool use_app_icons{false};
	int ram_limit{0};

	bool btntime2_flag;
	int32_t btntime_value;
	uint32_t btnwait_time;
	bool btndown_flag;
	bool transbtn_flag;

	SDL_Scancode last_keypress;
	int last_wheelscroll{0};
	GPU_Rect last_touchswipe{};
	uint32_t last_touchswipe_time{0};

public:
	enum class ExitType {
		None,
		Normal,
		Error
	};

	void requestQuit(ExitType code);

private:
	void cleanImages();
	void cleanLabel();

	/* ---------------------------------------- */
	/* Script related variables */

	int refresh_window_text_mode;
	int display_mode;
	bool did_leavetext;

	int event_mode;

	enum class ControlMode {
		Mouse,
		Arrow
	} controlMode{ControlMode::Mouse};

	uint32_t pixel_format_enum_32bpp{0};
	uint32_t pixel_format_enum_24bpp{0};

	GPU_Image *accumulation_gpu{nullptr};
	GPU_Image *hud_gpu{nullptr};
	GPU_Image *pre_screen_gpu{nullptr};

	GPU_Image *screenshot_gpu{nullptr}, *draw_gpu{nullptr}, *draw_screen_gpu{nullptr};
	GPU_Image *tmp_image{nullptr};

	// Pooled canvas images, need to be canvas sized for quake during effects
	GPU_Image *combined_effect_src_gpu{nullptr}, *combined_effect_dst_gpu{nullptr};
	GPU_Image *effect_src_gpu{nullptr}, *hud_effect_src_gpu{nullptr};
	GPU_Image *effect_dst_gpu{nullptr}, *hud_effect_dst_gpu{nullptr};
	GPU_Image *onion_alpha_gpu{nullptr};

public:
#ifdef IOS
	// Currently we have a weird issue on iOS, iPad 4 only, where text rendered from text_gpu looks differently
	// when text_gpu is not equal to canvas size.
	//FIXME: this should be addressed properly
	static constexpr bool canvasTextWindow{true};
#else
	static constexpr bool canvasTextWindow{false};
#endif
	GPU_Image *text_gpu{nullptr};   // Contains rendered text after dlgCtrl is deactivated
	GPU_Image *window_gpu{nullptr}; // Contains old text window if dlgCtrl is deactivated and wndCtrl is on
	GPU_Image *cursor_gpu{nullptr};

	GPU_Target *screen_target{nullptr};

private:
	SDL_Cursor *cursor{nullptr};

	bool needs_screenshot{false};
	bool display_draw{false};
	bool allow_rendering{true};

	GPUImageChunkLoader imageLoader;

	/* ---------------------------------------- */
	/* Button related variables */
	AnimationInfo btndef_info;

	ButtonState current_button_state, last_mouse_state;

	struct ButtonLink {
		enum BUTTON_TYPE {
			NORMAL_BUTTON     = 0,
			SPRITE_BUTTON     = 1,
			EX_SPRITE_BUTTON  = 2,
			LOOKBACK_BUTTON   = 3,
			TMP_SPRITE_BUTTON = 4,
			TEXT_BUTTON       = 5
		};

		ButtonLink *next{nullptr};
		ButtonLink *same{nullptr}; //Mion: to link buttons that act in concert
		BUTTON_TYPE button_type{NORMAL_BUTTON};
		int no{0};
		int sprite_no{0};
		char *exbtn_ctl{nullptr};
		bool show_flag{false}; // 0...show nothing, 1... show anim
		GPU_Rect select_rect{};
		GPU_Rect image_rect{};
		AnimationInfo *anim{};

		~ButtonLink() {
			if ((button_type == NORMAL_BUTTON ||
			     button_type == TMP_SPRITE_BUTTON ||
			     button_type == TEXT_BUTTON) &&
			    anim)
				delete anim;
			anim = nullptr;
			freearr(&exbtn_ctl);
			next = nullptr;
			same = nullptr;
		}
		void insert(ButtonLink *button) {
			button->next = this->next;
			this->next   = button;
		}
		void connect(ButtonLink *button) {
			button->same = this->same;
			this->same   = button;
		}
		void removeSprite(int spno) {
			ButtonLink *p = this;
			while (p->next) {
				if ((p->next->sprite_no == spno) &&
				    ((p->next->button_type == SPRITE_BUTTON) ||
				     (p->next->button_type == EX_SPRITE_BUTTON))) {
					ButtonLink *p2 = p->next;
					p->next        = p->next->next;
					delete p2;
				} else {
					p = p->next;
				}
			}
		}
	} root_button_link, *previouslyHoveredButtonLink{nullptr},
	    exbtn_d_button_link, text_button_link;
	bool is_exbtn_enabled;

	// Indicates whether a button is *currently* being hovered over.
	bool hoveringButton{false};
	// Indicates the button number of the currently hovered button. Illegal to access if !hoveringButton.
	int hoveredButtonNumber{-1};
	// Indicates the previously hovered button's link index.
	int lastKnownHoveredButtonLinkIndex{-1};
	// Indicates the previously hovered button's number.
	int lastKnownHoveredButtonNumber{-1};
	// Indicates the default button number to use for ControlMode::Arrow.
	int hoveredButtonDefaultNumber{0};

	/* ---------------------------------------- */
	/* Mion: textbtn related variables */
	struct TextButtonInfoLink {
		TextButtonInfoLink *next{nullptr};
		const char *text{nullptr}; //actual "text" of the button
		char *prtext{nullptr};     // button text as printed (w/linebreaks)
		ButtonLink *button{nullptr};
		int xy[2]{-1, -1};
		int no{-1};
		~TextButtonInfoLink() {
			delete[] text;
			delete[] prtext;
		}
		void insert(TextButtonInfoLink *info) {
			info->next = this->next;
			this->next = info;
		}
	} text_button_info;
	int txtbtn_start_num;
	int next_txtbtn_num;
	bool in_txtbtn;
	bool txtbtn_show;
	bool txtbtn_visible;
	uchar3 linkcolor[2];

	bool getzxc_flag;
	bool gettab_flag;
	bool getpageup_flag;
	bool getpagedown_flag;
	bool getinsert_flag;
	bool getfunction_flag;
	bool getenter_flag;
	bool getcursor_flag;
	bool spclclk_flag;
	bool getmclick_flag;
	bool getskipoff_flag;
	bool getmouseover_flag;
	int getmouseover_min, getmouseover_max;
	bool btnarea_flag;
	int btnarea_pos;

	void resetSentenceFont();
	void deleteButtonLink();
	void processTextButtonInfo();
	void deleteTextButtonInfo();
	void refreshButtonHoverState(bool forced = false);
	void refreshSprite(int sprite_no, bool active_flag, int cell_no, GPU_Rect *check_src_rect, GPU_Rect *check_dst_rect);

	void decodeExbtnControl(const char *ctl_str, GPU_Rect *check_src_rect = nullptr, GPU_Rect *check_dst_rect = nullptr);

	void disableGetButtonFlag();
	int getNumberFromBuffer(const char **buf);

	/* ---------------------------------------- */
	/* General image-related variables */
	int png_mask_type;

	/* ---------------------------------------- */
	/* Background related variables */
	AnimationInfo bg_info;

	/* ---------------------------------------- */
	/* Tachi-e related variables */
	/* 0 ... left, 1 ... center, 2 ... right */
	AnimationInfo tachi_info[3];
	int human_order[3];

	/* ---------------------------------------- */
	/* Sprite related variables */
	AnimationInfo *sprite_info{nullptr};
	AnimationInfo *sprite2_info{nullptr};
	bool all_sprite_hide_flag;
	bool all_sprite2_hide_flag;
	bool preserve{false};

	struct cmpById {
		bool operator()(const AnimationInfo *a, const AnimationInfo *b) const {
			return a->id > b->id || (a->id == b->id && a->type == SPRITE_LSP);
		}
	};
	std::unordered_map<int, std::set<AnimationInfo *, cmpById>> spriteZLevels;

	std::map<int, SpritesetInfo> spritesets;
	std::set<AnimationInfo *> nontransitioningSprites;

	void insertIfExists(AnimationInfo &ai, bool w_old, std::set<AnimationInfo *> &ret) {
		if (ai.exists || (w_old && ai.old_ai && ai.old_ai->exists))
			ret.insert(&ai);
	}

	std::set<AnimationInfo *> sprites(int which = SPRITE_ALL, bool w_old = false) {
		std::set<AnimationInfo *> ret;
		int i;
		if (which & SPRITE_LSP)
			for (i = 0; i < MAX_SPRITE_NUM; i++) insertIfExists(sprite_info[i], w_old, ret);
		if (which & SPRITE_LSP2)
			for (i = 0; i < MAX_SPRITE_NUM; i++) insertIfExists(sprite2_info[i], w_old, ret);
		if (which & SPRITE_CURSOR)
			for (i = 0; i < 2; i++) insertIfExists(cursor_info[i], w_old, ret);
		if (which & SPRITE_TACHI)
			for (i = 0; i < 3; i++) insertIfExists(tachi_info[i], w_old, ret);
		if (which & SPRITE_SENTENCE_FONT)
			insertIfExists(sentence_font_info, w_old, ret);
		//FIXME: Not sure what to do about this bunch of extra stuff
		if (which & SPRITE_BG)
			insertIfExists(bg_info, w_old, ret);
		//FIXME: Give these their own pointer-existence maps or turn them into such, this iteration defeats the purpose
		if (which & SPRITE_BAR)
			for (i = 0; i < MAX_PARAM_NUM; i++)
				if (bar_info[i])
					ret.insert(bar_info[i]);
		if (which & SPRITE_PRNUM)
			for (i = 0; i < MAX_PARAM_NUM; i++)
				if (prnum_info[i])
					ret.insert(prnum_info[i]);

		// the worst one in ons
		ButtonLink *p_button_link = root_button_link.next;
		while (which & SPRITE_BUTTONS && p_button_link) {
			ButtonLink *cur_button_link = p_button_link;
			while (cur_button_link) {
				auto &spritePtr = cur_button_link->anim;
				if (spritePtr) {
					ret.insert(spritePtr);
				}
				cur_button_link = cur_button_link->same;
			}
			p_button_link = p_button_link->next;
		}
		return ret;
	}

	/* ----------------------------------------- */
	/* Camera related variables */

	Camera camera;
	GPU_Rect full_script_clip;

	enum class VideoSkip {
		NotPlaying,
		Normal,
		Trap
	};

	VideoSkip video_skip_mode{VideoSkip::NotPlaying};
	bool request_video_shutdown{false};

	/* ----------------------------------------- */
	/* Lips related variables */
public:
	class LipsAnimationAction : public TypedConstantRefreshAction<LipsAnimationAction> {
	public:
		int channel{0};
		void run() override;
		void draw();
		bool expired() override;
		void onExpired() override;
		bool suspendsMainScript() override {
			return false;
		}
		bool suspendsDialogue() override {
			return false;
		}

	private:
		void setCellForCharacter(const std::string &characterName, int cellNumber);
	};

private:
	struct Lips {
		char seq[CHUNKS_PER_SECOND * MAX_SOUND_LENGTH];
		int seqSize;
		int speechStart;
	};
	struct LipsChannel {
		std::vector<std::string> characterNames;
		Lips lipsData;
	};
	double speechLevels[2]{0.3, 0.7};

	std::vector<cmp::optional<LipsChannel>> lipsChannels; // used instead of map for threading guarantees of preallocated vector
	                                                      // Various solutions were imagined:
	                                                      // 1: vector of LipsChannel, and we check .characterNames.size() to see if it is set for the purpose of loadLips()
	                                                      // 2: 'exists' field to LipsChannel
	                                                      // 3: vector of smart pointer to LipsChannel
	                                                      // 4: This solution.
	                                                      // for 1, I am pretty sure that checking size() would be unsafe while setting characterNames.
	                                                      // for 2, this seemed like a class-specific pointless reimplementation of cmp::optional.
	                                                      // for 3, this seemed like again what cmp::optional does except with more explicit object creation?
	                                                      //         Since we don't intend for the object to go anywhere but here, cmp::optional seems fine.

	/* ----------------------------------------- */
	/* Registry/ini related variables */
	bool reg_loaded{false};
	using IniContainer = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
	IniContainer registry;

	//Mion: track the last few sprite numbers loaded, for sprite data reuse
	static const int SPRITE_NUM_LAST_LOADS = 4;
	int last_loaded_sprite[SPRITE_NUM_LAST_LOADS];
	int last_loaded_sprite_ind;

	/* ---------------------------------------- */
	/* Parameter related variables */
	AnimationInfo *bar_info[MAX_PARAM_NUM]{}, *prnum_info[MAX_PARAM_NUM]{};

	/* ---------------------------------------- */
	/* Cursor related variables */
	enum {
		CURSOR_WAIT_NO    = 0,
		CURSOR_NEWPAGE_NO = 1
	};
	AnimationInfo cursor_info[2];

	void loadCursor(int no, const char *str, int x, int y, bool abs_flag = false);
	void lookupSavePath();
	void saveAll(bool no_error = false);
	void loadEnvData();
	void saveEnvData();

	/* ---------------------------------------- */
	/* Text related variables */
	AnimationInfo sentence_font_info;
	int erase_text_window_mode;
	bool draw_cursor_flag;
	int textgosub_clickstr_state;

public:
	int text_display_speed{0};
	int text_fade_duration{0};
	int page_enter_status; // 0 ... no enter, 1 ... body
private:
	std::unordered_map<std::string, std::function<bool(std::string &, std::string &, Fontinfo &)>> processFuncs;
	std::unordered_map<int, Fontinfo::TextStyleProperties> presets;
#ifdef USE_STD_REGEX
	std::vector<std::regex> regExps;
#else
	std::vector<std::pair<std::unique_ptr<char[]>, slre_regex_info>> regExps;
#endif
	std::vector<bool> conditions;
	bool dialogue_add_ends{false};

	int refreshMode();
	void setwindowCore();

public: // DialogueController wants access to this
	void resetGlyphCache();
	void renderGlyphValues(const GlyphValues &values, GPU_Rect *dst_clip, TextRenderingState::TextRenderingDst dst, float x, float y, float r, bool render_border, int alpha);
	const GlyphValues *renderUnicodeGlyph(Font *font, GlyphParams *key);
	const GlyphValues *measureUnicodeGlyph(Font *font, GlyphParams *key);
	bool isAlphanumeric(char16_t codepoint);
	void processSpecialCharacters(std::u16string &text, Fontinfo &info, Fontinfo::InlineOverrides &io);
	bool processTransformedCharacterSequence(std::u16string &string, Fontinfo &info);
	bool processSmartQuote(std::u16string &string, Fontinfo &info);
	bool processInlineCommand(std::u16string &string, Fontinfo &info, Fontinfo::InlineOverrides &io);
	bool processHashColor(std::u16string &string, Fontinfo &info);
	bool processIgnored(std::u16string &string, Fontinfo &info);
	bool executeInlineTextCommand(std::string &command, std::string &param, Fontinfo &info);
	void addTextWindowClip(DirtyRect &rect);
	int getCharacterPreDisplayDelay(char16_t codepoint, int speed);
	int getCharacterPostDisplayDelay(char16_t codepoint, int speed);
	int unpackInlineCall(const char *cmd, int &val);
	const char *getFontPath(int i, bool fallback = true);
    const char *getFontDir();

private:
	void enterTextDisplayMode();
	void leaveTextDisplayMode(bool force_leave_flag = false, bool perform_effect = true);

	void renderDynamicTextWindow(GPU_Target *target, GPU_Rect *canvas_clip_dst, int refresh_mode, bool useCamera = true);

	bool doClickEnd();
	bool clickWait();
	bool clickNewPage();
	int textCommand();
	void displayDialogue();

	/* ---------------------------------------- */
	/* Skip mode */
public:
	enum {
		SKIP_NONE   = 0,
		SKIP_NORMAL = 1, // skip endlessly/to unread text (press 's' button)
		//SKIP_TO_EOP  = 2, // skip to end of page (press 'o' button)
		SKIP_TO_WAIT   = 4, // skip to next clickwait
		SKIP_TO_EOL    = 8, // skip to end of line
		SKIP_SUPERSKIP = 16 // no i/o, just execute all the commands as fast as possible and do the i/o later
	};

	int skip_mode{SKIP_NONE};
	bool deferredLoadingEnabled{false};

private:
	bool skip_unread{true};

	struct SuperSkipData {
		std::string dst_lbl;
		int dst_var{0};
		ScriptHandler::ScriptLoanStorable callerState;
	} superSkipData;

	bool tryEndSuperSkip(bool force);

	/* ---------------------------------------- */
	/* Effect related variables */
	DirtyRect dirty_rect_scene; // only this scene region is updated
	DirtyRect dirty_rect_hud;   // only this hud region is updated

	// during an effect, specifies the area of the before-scene that needs redrawing this frame
	DirtyRect before_dirty_rect_scene, before_dirty_rect_hud;

	int effect_counter{0};  // counter in each effect
	int effect_duration{1}; //avoid possible div by zero
	int effect_previous_time{0};
	int effect_tmp{0}; //tmp variable for use by effect routines
	// Added to support effects being applied via constant refresh
	EffectLink *effect_current{nullptr};
	bool effect_first_time{false};
	bool effect_set{false};
	bool effect_rect_cleanup{false};
	int effect_refresh_mode_src, effect_refresh_mode_dst;

	bool effectskip_flag;
	bool skip_effect{false};
	enum {
		EFFECTSPEED_NORMAL  = 0,
		EFFECTSPEED_QUICKER = 1,
		EFFECTSPEED_INSTANT = 2
	};
	int effectspeed{EFFECTSPEED_NORMAL};

	void dirtySpriteRect(AnimationInfo *ai, bool before = false);
	void dirtySpriteRect(int num, bool lsp2, bool before = false);
	void dirtyRectForZLevel(int num, GPU_Rect &rect);
	int getAIno(AnimationInfo *info, bool old_ai, bool &lsp2);
	bool isHudAI(AnimationInfo *info, bool before = false);

public:
	void backupState(AnimationInfo *info);

private:
	void commitSpriteset(SpritesetInfo *si);
	void resetSpritesets();
	void fillCanvas(bool after = true, bool before = false);
	bool constantRefreshEffect(EffectLink *effect, bool clear_dirty_rect_when_done, bool async = false, int refresh_mode_src = -1, int refresh_mode_dst = -1);
	bool setEffect();
	bool doEffect();
	void mergeForEffect(GPU_Image *dst, GPU_Rect *scene_rect, GPU_Rect *hud_rect, int refresh_mode);
	void sendToPreScreen(bool refreshSrc, std::function<PooledGPUImage(GPUTransformableCanvasImage &)> applyTransform, int refresh_mode_src, int refresh_mode_dst);
	void effectTrvswave(const char *params, int duration);
	void effectWhirl(const char *params, int duration);
	void effectBreakupParser(const char *params, int refresh_mode_src, int refresh_mode_dst);
	void effectBrokenGlassParser(const char *params, int refresh_mode_src, int refresh_mode_dst);

	/* ---------------------------------------- */
	/* Breakup related */

	bool new_breakup_implementation{true};

	struct BreakupData {
		int n_cells, tot_frames;
		int prev_frame; // Literally unused by new breakup, and functionally unused by old breakup if refactored a tiny amount
		cmp::optional<int> breakup_mode;
		cmp::optional<TriangleBlitter> blitter; // Only used by new breakup
		float wInCellsFloat, hInCellsFloat;
		int cellFactor;
		int numCellsX, numCellsY;            // Only used by new breakup
		int maxDiagonalToContainBrokenCells; // Only used by new breakup
		std::vector<BreakupCell> breakup_cells;
		std::vector<BreakupCell *> diagonals;
	};

	std::unordered_map<BreakupID, BreakupData> breakupData;

	GPU_Image *breakup_cellforms_gpu{nullptr}, *breakup_cellform_index_grid{nullptr};
	SDL_Surface *breakup_cellform_index_surface{nullptr};

	void buildBreakupCellforms();
	bool breakupInitRequired(BreakupID id);
	void initBreakup(BreakupID id, GPU_Image *src, GPU_Rect *src_rect);
	void deinitBreakup(BreakupID id);
	void oncePerBreakupEffectBreakupSetup(BreakupID id, int breakupDirectionFlagset, int numCellsX, int numCellsY);

	void oncePerFrameBreakupSetup(BreakupID id, int breakupDirectionFlagset, int numCellsX, int numCellsY);
	void effectBreakupNew(BreakupID id, int breakupFactor);
	void effectBreakupOld(BreakupID id, int breakupFactor);

	/* ---------------------------------------- */
	/* Glass smash related */

	bool new_glass_smash_implementation{true};

	struct GlassSmashData {
		static constexpr int DotWidth{10};
		static constexpr int DotHeight{10};
		static constexpr int RectWidth{DotWidth - 1};
		static constexpr int RectHeight{DotHeight - 1};
		static constexpr int DotNum{DotWidth * DotHeight};
		static constexpr int TriangleNum{RectWidth * RectHeight * 2};
		static constexpr int DefaultParameter{1000}; // passed by ps3
		cmp::optional<TriangleBlitter> blitter;
		bool initialised{false};
		int smashParameter{DefaultParameter};
		std::array<std::pair<float, float>, DotNum> points;
		std::array<int, TriangleNum> triangleSeeds;
	};

	GlassSmashData glassSmashData;

	/* ---------------------------------------- */
	/* StringTree related */

	StringTree dataTrees[MAX_TEXT_TREES];

	int executeSingleCommandFromTreeNode(StringTree &command_node);

	/* ---------------------------------------- */
	/* Select related variables */
	enum {
		SELECT_GOTO_MODE  = 0,
		SELECT_GOSUB_MODE = 1,
		SELECT_NUM_MODE   = 2,
		SELECT_CSEL_MODE  = 3
	};
	struct SelectLink {
		SelectLink *next{nullptr};
		char *text{nullptr};
		char *label{nullptr};

		~SelectLink() {
			delete[] text;
			delete[] label;
		}
	} root_select_link;
	NestInfo select_label_info;

	void deleteSelectLink();
	AnimationInfo *getSentence(char *buffer, Fontinfo *info, int num_cells, bool flush_flag = true, bool nofile_flag = false, bool skip_whitespace = true);
	ButtonLink *getSelectableSentence(char *buffer, Fontinfo *info, bool flush_flag = true, bool nofile_flag = false, bool skip_whitespace = true);

	/* ---------------------------------------- */
	/* Sound related variables */
	enum {
		SOUND_NONE     = 0,
		SOUND_PRELOAD  = 1,
		SOUND_CHUNK    = 2, // WAV, Ogg Vorbis
		SOUND_MUSIC    = 4, // WAV, MP3, Ogg Vorbis (streaming)
		SOUND_SEQMUSIC = 8, //MIDI/XM/MOD
		SOUND_OTHER    = 16
	};
	bool cdaudio_on_flag{false}; // false if mute
public:                          // MediaLayer wants access to these
	bool volume_on_flag{true};   // NScr uses for wave on/off, ons-en uses for mute (false)
	bool script_mute{false};
	SDL_AudioSpec default_audio_format{};
	SDL_AudioSpec audio_format{};

private:
	bool audio_open_flag{false};

	bool wave_play_loop_flag;
	char *wave_file_name{nullptr};

	bool seqmusic_play_loop_flag;
	char *seqmusic_file_name{nullptr};
	Mix_Music *seqmusic_info{nullptr};

	int current_cd_track;
	bool cd_play_loop_flag;
	bool music_play_loop_flag;
	bool mp3save_flag;
	char *music_file_name{nullptr};
	uint8_t *music_buffer{nullptr}; // for looped music
	long music_buffer_length{0};
	Mix_Music *music_info{nullptr};
	char *loop_bgm_name[2]{};
	int playSoundThreadedLock;

	uint32_t channelvolumes[ONS_MIX_CHANNELS]{}; //insani's addition
	bool channel_preloaded[ONS_MIX_CHANNELS]{};  //seems we need to track this...
public:
	std::shared_ptr<Wrapped_Mix_Chunk> wave_sample[ONS_MIX_CHANNELS + ONS_MIX_EXTRA_CHANNELS]{};
	std::shared_ptr<Wrapped_Mix_Chunk> pending_cache_chunk[2]{};

	void setVolume(uint32_t channel, uint32_t level, bool flag) {
		if (wave_sample[channel])
			Mix_Volume(channel, flag ? level * MIX_MAX_VOLUME / DEFAULT_VOLUME : 0);
		// Loop bgm channels and similar are not registered in channelVolumes for some reason.
		if (channel < std::extent<decltype(channelvolumes)>::value)
			channelvolumes[channel] = level;
	}

	void setMusicVolume(uint32_t level, bool flag) {
		Mix_VolumeMusic(flag ? level * MIX_MAX_VOLUME / DEFAULT_VOLUME : 0);
	}

private:
	char *seqmusic_cmd{nullptr};

	void loadSoundIntoCache(int id, const std::string &filename_str, bool async = false);
	int trySoundCache(const char *filename, int format, bool loop_flag, int channel);
	int playSoundThreaded(const char *filename, int format, bool loop_flag, int channel = 0, bool waitevent = true);
	int playSound(const char *filename, int format, bool loop_flag, int channel = 0);
	void playCDAudio();
	int playWave(const std::shared_ptr<Wrapped_Mix_Chunk> &chunk, int format, bool loop_flag, int channel);
	int playSequencedMusic(bool loop_flag);
	// Mion: for music status and fades
	int playingMusic();
	int setCurMusicVolume(int volume);
	int setVolumeMute(bool do_mute);

	enum { WAVE_PLAY        = 0,
		   WAVE_PRELOAD     = 1,
		   WAVE_PLAY_LOADED = 2
	};
	void stopBGM(bool continue_flag);
	void stopDWAVE(int channel);
	void stopAllDWAVE();
	void playClickVoice();
	void startLvPlayback();
	void stopLvPlayback();

	/* ---------------------------------------- */
	/* Text event related variables */
	bool skip_enabled;
	bool skipIsAllowed();
	bool new_line_skip_flag;

	void clearCurrentPage();
	void newPage(bool next_flag, bool no_flush = false);
	int parseLine();
	void readToken();
	void mouseOverCheck(int x, int y, bool forced = false);

public:
	bool isBuiltInCommand(const char *cmd);
	int evaluateBuiltInCommand(const char *cmd);
	void flush(int refresh_mode, GPU_Rect *scene_rect = nullptr, GPU_Rect *hud_rect = nullptr, bool clear_dirty_flag = true, bool direct_flag = false, bool wait_for_cr = false);
	void flushDirect(GPU_Rect &scene_rect, GPU_Rect &hud_rect, int refresh_mode);
	int game_fps{0};
	bool should_flip{true};

private:
	void combineWithCamera(GPU_Image *scene, GPU_Image *hud, GPU_Target *dst, GPU_Rect &scene_rect, GPU_Rect &hud_rect, int refresh_mode);
	bool constant_refresh_executed{false};
	bool pre_screen_render{false};
	int constant_refresh_mode{REFRESH_NONE_MODE};
	bool screenChanged{false};
	bool saveNextFrame{false};
	Clock warpClock;
	float warpSpeed{0};
	float warpWaveLength{1000};
	float warpAmplitude{0};
	uint16_t onionAlphaFactor{0};
	double onionAlphaScale{1000};
	double onionAlphaCooldown{0};

	/* ---------------------------------------- */
	/* Animation */
	int proceedAnimation();
	int proceedCursorAnimation();
	int estimateNextDuration(AnimationInfo *anim, GPU_Rect &rect, int minimum, bool old_ai = false);
	void advanceAIclocks(uint64_t ns);
	void advanceSpecificAIclocks(uint64_t ns, int i, int type, bool old_ai = false);

	//AnimationInfos that have old_ai to be freed in commitVisualState
	std::vector<AnimationInfo *> queueAnimationInfo;

	void commitVisualState();
	void buildGPUImage(AnimationInfo &ai);
	void freeRedundantSurfaces(AnimationInfo &ai);
	void setupAnimationInfo(AnimationInfo *anim, Fontinfo *info = nullptr);
	void postSetupAnimationInfo(AnimationInfo *anim);
	void buildAIImage(AnimationInfo *anim);
	bool treatAsSameImage(const AnimationInfo &anim1, const AnimationInfo &anim2);
	void parseTaggedString(AnimationInfo *anim, bool is_mask = false);
	void cleanSpritesetCache(SpritesetInfo *spriteset, bool before);
	void drawSpritesetToGPUTarget(GPU_Target *target, SpritesetInfo *spriteset, GPU_Rect *clip, int rm);
	void layoutSpecialScrollable(AnimationInfo *info); // doesn't belonggggggggggggggggg
	void calculateDynamicElementHeight(StringTree &element, int width, int tightlyFit);
	std::vector<std::string>::iterator getScrollableElementsVisibleAt(AnimationInfo::ScrollableInfo *si, StringTree &tree, int y);
	void setRectForScrollableElement(StringTree *elem, GPU_Rect &rect);
	void mouseOverSpecialScrollable(int aiSpriteNo, int x, int y);
	void changeScrollableHoveredElement(AnimationInfo *info, Direction d);
	void snapScrollableByOffset(AnimationInfo *info, int rowsDownwards);
	void snapScrollableToElement(AnimationInfo *info, long elementId, AnimationInfo::ScrollSnap snapType, bool instant = false);
	void drawSpecialScrollable(GPU_Target *target, AnimationInfo *info, int refresh_mode, GPU_Rect *clip);
	void drawBigImage(GPU_Target *target, AnimationInfo *info, int refresh_mode, GPU_Rect *clip, bool centre_coordinates = false);
	void drawToGPUTarget(GPU_Target *target, AnimationInfo *info, int refresh_mode, GPU_Rect *clip, bool centre_coordinates = false);
	void stopCursorAnimation(int click);
	void createScreenshot(GPU_Image *first, GPU_Rect *first_r, GPU_Image *second = nullptr, GPU_Rect *second_r = nullptr);

	/* ---------------------------------------- */
	/* File I/O */
	bool readSaveFileHeader(int no, SaveFileInfo *save_file_info = nullptr);
	void writeSaveFileHeader(const char *savestr);
	void loadSaveFileData();
	void saveSaveFileData();
	bool verifyChecksum();
	void writeChecksum();
	int loadSaveFile(int no);
	int saveSaveFile(int no, const char *savestr = nullptr, bool no_error = false);
	bool readIniFile(const char *path, IniContainer &result);
	bool readAdler32Hash(const char *path, uint32_t &adler);

	void loadReadLabels(const char *filename);
	void saveReadLabels(const char *filename);

	//TODO: These should communicate with a separate File I/O class and be class member functions
	void readFontinfo(Fontinfo &fi);
	void writeFontinfo(Fontinfo &fi);
	void readWindowCtrl(TextWindowController &wnd);
	void writeWindowCtrl(TextWindowController &wnd);
	void readAnimationInfo(AnimationInfo &ai);
	void writeAnimationInfo(AnimationInfo &ai);
	void readCamera(Camera &camera);
	void writeCamera(Camera &camera);
	void readTransforms(AnimationInfo::SpriteTransforms &transforms);
	void writeTransforms(AnimationInfo::SpriteTransforms &transforms);
	void readSpritesetInfo(std::map<int, SpritesetInfo> &si);
	void writeSpritesetInfo(std::map<int, SpritesetInfo> &si);
	void readSoundData();
	void writeSoundData();
	void readGlobalFlags();
	void writeGlobalFlags();
	void readNestedInfo();
	void writeNestedInfo();
	void readParamData(AnimationInfo *&p, bool bar, int id);
	void writeParamData(AnimationInfo *&p, bool bar);

	/* ---------------------------------------- */
	/* Our caches :) */
	LRUCache<GlyphParams, GlyphValues *, std::unordered_map, GlyphParamsHash, GlyphParamsEqual> glyphCache;
	LRUCache<GlyphParams, GlyphValues *, std::unordered_map, GlyphParamsHash, GlyphParamsEqual> glyphMeasureCache;
	GlyphAtlasController glyphAtlas;

	ImageCacheController imageCache;
	SoundCacheController soundCache;

	bool shouldExit{false};
	bool canExit{true};
	std::atomic<ExitType> exitCode{ExitType::None};
	SDL_threadID mainThreadId{0};

public:
	inline void preventExit(bool state) {
		//assert(canExit != state);
		canExit = !state;
		if (canExit && exitCode.load(std::memory_order_relaxed) != ExitType::None)
			requestQuit(exitCode);
	}

	Uint64 commandExecutionTime{0};

	uint32_t last_clock_time = 0;
	void printClock(const char *str, bool print_time = true);

private:
	/* ---------------------------------------- */
	/* Image processing */
	void loadImageIntoCache(int id, const std::string &filename_str, bool allow_rgb = false);
	void dropCache(int *id, const std::string &filename_str);
	SDL_Surface *loadImage(const char *filename, bool *has_alpha = nullptr, bool allow_rgb = false);
	GPU_Image *loadGpuImage(const char *file_name, bool allow_rgb = false);
	SDL_Surface *createRectangleSurface(const char *filename);
	SDL_Surface *createSurfaceFromFile(const char *filename);

	void shiftHoveredButtonInDirection(int diff);

	void effectBlendToCombinedImage(GPU_Image *mask_gpu, int trans_mode, uint32_t mask_value, GPU_Image *image);
	void effectBlendGPU(GPU_Image *mask_gpu, int trans_mode,
	                    uint32_t mask_value = 255, GPU_Rect *clip = nullptr,
	                    GPU_Image *src1 = nullptr, GPU_Image *src2 = nullptr,
	                    GPU_Image *dst = nullptr);
	bool colorGlyph(const GlyphParams *key, GlyphValues *glyph, SDL_Color *color, bool border = false, GlyphAtlasController *atlas = nullptr);

	bool use_text_gradients, use_text_gradients_for_sprites;

	void makeNegaTarget(GPU_Target *target, GPU_Rect clip);
	void makeMonochromeTarget(GPU_Target *target, GPU_Rect clip, bool before_scene);
	void makeBlurTarget(GPU_Target *target, GPU_Rect clip, bool before_scene);
	void makeWarpedTarget(GPU_Target *target, GPU_Rect clip, bool before_scene);

	void refreshSceneTo(GPU_Target *target, GPU_Rect *passed_script_clip_dst, int refresh_mode = REFRESH_NORMAL_MODE);
	void refreshHudTo(GPU_Target *target, GPU_Rect *passed_script_clip_dst, int refresh_mode = REFRESH_NORMAL_MODE);

	void setupZLevels(int refresh_mode);
	void drawSpritesBetween(int upper_inclusive, int lower_exclusive, GPU_Target *target, GPU_Rect *clip_dst, int refresh_mode);
	void loadBreakupCellforms();
	void createBackground();
	void loadDrawImages();
	void unloadDrawImages();
	void clearDrawImages(int r = 0, int g = 0, int b = 0, bool clear_screen = false);

	/* ---------------------------------------- */
	/* system call */

	bool executeSystemCall(int mode);

	void executeSystemSkip();
	void executeSystemAutomode();
	void executeSystemReset();
	void executeSystemEnd();

	void updateButtonsToDefaultState(GPU_Rect &check_src_rect, GPU_Rect &check_dst_rect);

	void doHoverButton(bool hovering, int buttonNumber, int buttonLinkIndex, ButtonLink *buttonLink);

	int getTotalButtonCount() const;

	int buttonNumberToLinkIndex(int buttonNo);
};

extern ONScripter ons;
