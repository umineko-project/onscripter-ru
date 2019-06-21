/**
 *  Parser.hpp
 *  ONScripter-RU
 *
 *  Define block parser.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Support/DirPaths.hpp"
#include "Engine/Entities/Animation.hpp"
#include "Engine/Entities/Font.hpp"
#include "Engine/Layers/Layer.hpp"
#include "Engine/Handlers/Script.hpp"
#ifdef USE_LUA
#include "Engine/Handlers/LUA.hpp"
#endif

#include <SDL2/SDL_mixer.h>

#include <list>
#include <queue>
#include <stack>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>

const char DEFAULT_LOOKBACK_NAME0[] = "uoncur.bmp";
const char DEFAULT_LOOKBACK_NAME1[] = "uoffcur.bmp";
const char DEFAULT_LOOKBACK_NAME2[] = "doncur.bmp";
const char DEFAULT_LOOKBACK_NAME3[] = "doffcur.bmp";

// Mion: kinsoku
const char DEFAULT_START_KINSOKU[] = "」』）］｝、。，．・？！ヽヾゝゞ々ー";
const char DEFAULT_END_KINSOKU[]   = "「『（［｛";

class ScriptParser : public BaseController {
public:
	template <typename T>
	ScriptParser(T *inst)
	    : BaseController(inst) {
		resetDefineFlags();

		std::srand(static_cast<unsigned>(time(nullptr)));

		//Default kinsoku
		setKinsoku(DEFAULT_START_KINSOKU, DEFAULT_END_KINSOKU, false);
	}
	int ownDeinit() override;

	void reset();
	void resetDefineFlags(); // for resetting (non-pointer) variables
	int open();
	bool isBuiltInCommand(const char *cmd);
	int evaluateCommand(const char *cmd, bool builtin = true, bool textgosub_flag = false, bool no_error = false);
	int parseLine();
	void setCurrentLabel(const char *label);
	void gosubReal(const char *label, const char *next_script, bool textgosub_flag = false);

	void saveGlovalData(bool no_error = false);
	void setArchivePath(const char *path);
	void setSavePath(const char *path);
	void setNsaOffset(const char *off);

	/* Command */
	int windowzCommand();
	int uninterruptibleCommand();
	int textzCommand();
	int ignoreCommandCommand();
	int hudzCommand();
	int timeStampCommand();
	int skipgosubCommand();
	int setdefaultspeedCommand();
	int getStraliasCommand();
	int eventCallbackCommand();
	int disablespeedbuttonsCommand();
	int borderstyleCommand();

	int zenkakkoCommand();
	int windowchipCommand();
	int windowbackCommand();
	int versionstrCommand();
	int usewheelCommand();
	int useescspcCommand();
	int underlineCommand();
	int transmodeCommand();
	int timeCommand();
	int textgosubCommand();
	int tanCommand();
	int subCommand();
	int straliasCommand();
	int spritesetzCommand();
	int soundpressplginCommand();
	int skipCommand();
	int sinCommand();
	int shadedistanceCommand();
	int setlayerCommand();
	int setkinsokuCommand();
	int selectvoiceCommand();
	int selectcolorCommand();
	int savenumberCommand();
	int savenameCommand();
	int savedirCommand();
	int rubyonCommand();
	int rubyoffCommand();
	int roffCommand();
	int rmenuCommand();
	int rgosubCommand();
	int returnCommand();
	int pretextgosubCommand();
	int pagetagCommand();
	int numaliasCommand();
	int nsadirCommand();
	int nsaCommand();
	int nextCommand();
	int mulCommand();
	int movCommand();
	int mode_wave_demoCommand();
	int mode_sayaCommand();
	int mode_extCommand();
	int modCommand();
	int midCommand();
	int menusetwindowCommand();
	int menuselectvoiceCommand();
	int menuselectcolorCommand();
	int maxkaisoupageCommand();
	int luasubCommand();
	int luacallCommand();
	int lookbackspCommand();
	int lookbackcolorCommand();
	int loadgosubCommand();
	int linepageCommand();
	int lenCommand();
	int labellogCommand();
	int labelexistCommand();
	int kidokuskipCommand();
	int kidokumodeCommand();
	int itoaCommand();
	int intlimitCommand();
	int incCommand();
	int ifCommand();
	int humanzCommand();
	int humanposCommand();
	int gotoCommand();
	int gosubCommand();
	int globalonCommand();
	int getparamCommand();
	int forCommand();
	int filelogCommand();
	int errorsaveCommand();
	int englishCommand();
	int effectcutCommand();
	int effectblankCommand();
	int effectCommand();
	int dsoundCommand();
	int divCommand();
	int dimCommand();
	int defvoicevolCommand();
	int defsubCommand();
	int defsevolCommand();
	int defmp3volCommand();
	int defvideovolCommand();
	int defaultspeedCommand();
	int defaultfontCommand();
	int decCommand();
	int dateCommand();
	int cosCommand();
	int cmpCommand();
	int clickvoiceCommand();
	int clickstrCommand();
	int clickskippageCommand();
	int btnnowindoweraseCommand();
	int breakCommand();
	int atoiCommand();
	int arcCommand();
	int addnsadirCommand();
	int addkinsokuCommand();
	int addCommand();

	void add_debug_level();
	void errorAndExit(const char *str, const char *reason = nullptr, const char *title = nullptr, bool is_simple = false);
	void errorAndCont(const char *str, const char *reason = nullptr, const char *title = nullptr, bool is_simple = false, bool force_message = false);

	//Mion: syntax flags
	bool allow_color_type_only{false};     // only allow color type (#RRGGBB) for args of color type,
	                                       // i.e. not string variables
	bool set_tag_page_origin_to_1{false};  // 'gettaglog' will consider the current page as 1, not 0
	bool answer_dialog_with_yes_ok{false}; // give 'yesnobox' and 'okcancelbox' 'yes/ok' results
	const char *readColorStr() {
		if (allow_color_type_only)
			return script_h.readColor();
		return script_h.readStr();
	}
	const char *getPath(size_t n) {
		return archive_path.getPathNum() > n ? archive_path.getPath(n) : "";
	}

	cmp::optional<const char *> currentCommandPosition;

	bool getVariableQueue() {
		return variableQueueEnabled;
	}
	void setVariableQueue(bool state, std::string cmd = "");
	std::string variableQueueCommand;
	std::queue<std::string> variableQueue;
	bool inVariableQueueSubroutine{false};

protected:
	bool variableQueueEnabled{false};

	// command, lua_flag
	std::unordered_map<HashedString, bool> user_func_lut;
	std::unordered_set<HashedString> ignored_func_lut;
	std::unordered_set<HashedString> ignored_inline_func_lut;

	NestInfo last_tilde; // guess i'll leave that...

	enum {
		SYSTEM_NULL  = 0,
		SYSTEM_SKIP  = 1,
		SYSTEM_RESET = 2,
		//SYSTEM_SAVE        = 3,
		//SYSTEM_LOAD        = 4,
		//SYSTEM_LOOKBACK    = 5,
		//SYSTEM_WINDOWERASE = 6,
		//SYSTEM_MENU        = 7,
		//SYSTEM_YESNO       = 8,
		SYSTEM_AUTOMODE = 9,
		SYSTEM_END      = 10,
		SYSTEM_SYNC     = 11
	};
	enum { RET_NOMATCH   = 0,
		   RET_SKIP_LINE = 1,
		   RET_CONTINUE  = 2,
		   RET_NO_READ   = 4,
		   RET_EOL       = 8, // end of line (0x0a is found)
		                      //RET_EOT       = 16 // end of text (the end of string_buffer is reached)
	};
	enum { CLICK_NONE    = 0,
		   CLICK_WAIT    = 1,
		   CLICK_NEWPAGE = 2,
		   CLICK_WAITEOL = 4
	};
	enum { NORMAL_MODE,
		   DEFINE_MODE };
	int current_mode;

public:
	int debug_level{0};
	bool labellog_flag;
	bool filelog_flag;

protected:
	char *cmdline_game_id{nullptr};
	DirPaths archive_path;
	DirPaths nsa_path;
	int nsa_offset{0};
	bool globalon_flag;

	bool kidokuskip_flag{false};
	bool kidokumode_flag{false};

	bool clickskippage_flag;

	int z_order_ld{499}, z_order_hud{99}, z_order_window{49}, z_order_text{-1};
	std::map<int, int> z_order_spritesets;

	bool rmode_flag;
	bool btnnowindowerase_flag;
	bool usewheel_flag;
	bool useescspc_flag;
	bool mode_wave_demo_flag;
	bool mode_saya_flag;
	bool mode_ext_flag; //enables automode capability
	bool force_button_shortcut_flag{false};
	bool pagetag_flag;
	int windowchip_sprite_no;

public:
	int string_buffer_offset;
	std::deque<NestInfo> callStack;          //TODO: give NestInfo a proper name
	bool callStackHasUninterruptible{false}; // callStackHasUninterruptible -> scriptExecutionHasPriority
	std::unordered_set<const char *> uninterruptibleLabels;
	LabelInfo *current_label_info;
	bool use_text_atlas{false};
	int current_line;
	//CHECKME: not initialized? any of this? resetDefineFlags seems to be the thing initting our stuff and it is missing current_line + other things?

protected:
#ifdef USE_LUA
	LUAHandler lua_handler;
#endif

	/* ---------------------------------------- */
	/* Global definitions */
	int preferred_width{0};

	char *version_str{nullptr};
	int underline_value;
	int humanpos[3]; // l,c,r
	char *savedir{nullptr};

	void deleteNestInfo();

	/* ---------------------------------------- */
	/* Effect related variables */
	struct EffectLink {
		int no{0};
		int effect{10};
		int duration{0};
		AnimationInfo anim;
	} window_effect, tmp_effect;

	std::list<EffectLink> effect_links;

	int effect_blank;
	bool effect_cut_flag;

	int readEffect(EffectLink *effect);
	EffectLink *parseEffect(bool init_flag);

	/* ---------------------------------------- */
	/* Layer related variables */ //Mion
	struct LayerInfo {
		LayerInfo *next{nullptr};
		std::unique_ptr<Layer> handler;
		unsigned int num{0xFFFFFFFF};
		int interval{0}; //-1 means equal to fps
		uint32_t last_update{0};
		void commit() {
			if (handler.get())
				handler->commit();
			if (next)
				next->commit();
		}
	} * layer_info{nullptr};

	template <typename T>
	T *getLayer(unsigned int num, bool die = true) {
		LayerInfo *tmp = layer_info;
		while (tmp) {
			if (tmp->num == num)
				break;
			tmp = tmp->next;
		}

		T *handler = tmp ? dynamic_cast<T *>(tmp->handler.get()) : nullptr;

		if ((!tmp || !handler) && die) {
			errorAndExit("Invalid layer id");
			return nullptr; //dummy
		}

		return handler;
	}
	LayerInfo *getLayerInfo(unsigned int num, bool die = false) {
		LayerInfo *tmp = layer_info;
		while (tmp) {
			if (tmp->num == num)
				break;
			tmp = tmp->next;
		}

		if (!tmp && die) {
			errorAndExit("Invalid layer id");
			return nullptr; //dummy
		}

		return tmp;
	}

	void deleteLayerInfo();
	int video_layer{-1};

	/* ---------------------------------------- */
	/* Lookback related variables */
	//char *lookback_image_name[4];
	int lookback_sp[2];
	uchar3 lookback_color;

	/* ---------------------------------------- */
	/* For loop related variables */
	bool break_flag;

	/* ---------------------------------------- */
	/* Transmode related variables */
	int trans_mode;

	/* ---------------------------------------- */
	/* Save/Load related variables */
	struct SaveFileInfo {
		int month, day, year, hour, minute;
		std::unique_ptr<char[]> descr;
		int version;
	};
	unsigned int num_save_file;

	std::vector<uint8_t> save_data_buf;
	std::vector<uint8_t> file_io_buf;
	size_t file_io_buf_ptr{0};
	size_t file_io_read_len{0};

	bool errorsave{false};

	/* ---------------------------------------- */
	/* Text related variables */
	char *default_env_font{nullptr};

	int clickstr_line;
	int clickstr_state;
	int linepage_mode;
	bool english_mode;

	struct Kinsoku {
		char chr[2];
	} * start_kinsoku{nullptr}, *end_kinsoku{nullptr}; //Mion: for kinsoku chars
	int num_start_kinsoku{0}, num_end_kinsoku{0};
	void setKinsoku(const char *start_chrs, const char *end_chrs, bool add); //Mion
	bool isStartKinsoku(const char *str);
	bool isEndKinsoku(const char *str);

	/* ---------------------------------------- */
	/* Sound related variables */
	uint32_t music_volume;
	uint32_t voice_volume;
	uint32_t se_volume;

public: // MediaLayer wants access to this
	uint32_t video_volume;

protected:
	bool use_default_volume;

	enum { CLICKVOICE_NORMAL  = 0,
		   CLICKVOICE_NEWPAGE = 1,
		   CLICKVOICE_NUM     = 2
	};
	char *clickvoice_file_name[CLICKVOICE_NUM]{};

	enum { SELECTVOICE_OPEN   = 0,
		   SELECTVOICE_OVER   = 1,
		   SELECTVOICE_SELECT = 2,
		   SELECTVOICE_NUM    = 3
	};
	char *selectvoice_file_name[SELECTVOICE_NUM]{};

	/* ---------------------------------------- */
	/* Font related variables */
public: // DialogueController wants access to these
	Fontinfo *current_font{nullptr}, sentence_font, name_font;

protected:
	int getSystemCallNo(const char *buffer);
	uint8_t convHexToDec(char ch);

	void readColor(uchar3 *color, const char *buf);

	void allocFileIOBuf();
	int saveFileIOBuf(const char *filename);
	int loadFileIOBuf(const char *filename, bool savedata = true);

	//TODO: separate from ScriptParser
	union ConvBytes {
		float f;
		int32_t i32;
		uint32_t u32;
		int16_t i16;
		uint16_t u16;
		int8_t i8;
		uint8_t u8;
	};

	void write8s(int8_t c);
	int8_t read8s();
	void write32s(int32_t i);
	int32_t read32s();
	void write32u(uint32_t i);
	uint32_t read32u();
	void writeFloat(float i);
	float readFloat();
	void write16s(int16_t i);
	int16_t read16s();
	void writeStr(const char *s);
	void readStr(char **s);
	void readFilePath(char **s);
	void writeVariables(uint32_t from, uint32_t to);
	void readVariables(uint32_t from, uint32_t to);
	void writeArrayVariable();
	void readArrayVariable();
	void writeLog(ScriptHandler::LogInfo &info);
	void readLog(ScriptHandler::LogInfo &info);

	/* ---------------------------------------- */
	/* System customize related variables */
	char *textgosub_label{nullptr};
	char *skipgosub_label{nullptr}; // textgosub for skip
	char *pretextgosub_label{nullptr};
	char *loadgosub_label{nullptr};
	char *event_callback_label{nullptr};
	bool eventCallbackRequired{false}; // Just because I am lazy

	ScriptHandler script_h;
};
