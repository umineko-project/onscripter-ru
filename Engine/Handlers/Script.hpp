/**
 *  Script.hpp
 *  ONScripter-RU
 *
 *  Script manipulation handler.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Entities/Variable.hpp"

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstddef>

const uint32_t VARIABLE_RANGE = 9999;
const size_t MAX_ERRBUF_LEN   = 512;

#ifdef PUBLIC_RELEASE
const char DEFAULT_SCRIPT_NAME[] = "script.file";
#else
const char DEFAULT_SCRIPT_NAME[] = "0.txt";
#endif
const char CFG_FILE[]         = "ons.cfg";
const char DEFAULT_CFG_FILE[] = "default.cfg";

class ONScripter;
class BaseReader;

struct LabelInfo {
	char *name;
	const char *label_header;
	const char *start_address;
	int start_line;
	int num_of_lines;
};

struct NestInfo {
	enum {
		LABEL = 0,
		FOR   = 1
	};
	int nest_mode{LABEL};
	const char *next_script{nullptr}; // points into script_buffer; used in gosub and for -- does not need to be deleted
	int var_no{0}, to{0}, step{0};    // used in for
	bool textgosub_flag{false};       // used in textgosub and pretextgosub
	//MARK: defaults to CLICK_NONE = 0
	bool dialogueEventOnReturn{false};
	bool noReadOnReturn{false};
	bool gosubReturnCall{false};
	bool uninterruptible{false};
	LabelInfo *label{nullptr}; // Always a pointer into the label_info array
	int line{-1};
};

struct DialogueDataEntry {
	std::string text;
	std::vector<std::unordered_map<int, std::string>> voices;
	short volume{100};
	bool jumpable{true};
};

struct LogEntry {
	uint32_t labelIndex;
	uint32_t choiceVectorSize;
};

struct LogState {
	// Maps a label index to data that's unique per dialogue
	std::unordered_map<unsigned int, DialogueDataEntry> dialogueData;

	// Maps log entry number to log entry data (intermediate structure, to be used instead of DataTree for all purposes except rendering)
	std::vector<LogEntry> logEntries;

	std::vector<bool> readLabels;
	std::vector<std::unordered_map<int, std::string>> tmpVoices;
	uint32_t tmpVolume{100};
	bool tmpVoiceGroupStarted{false};
	uint32_t currVoiceVolume{100};
	int32_t currVoiceDialogueLabelIndex{-1};
	int32_t currVoiceSet{-1};
	uint32_t currDialogueLabelIndex{0};
	bool unreadDialogue{false};
	uint32_t logEntryIndexToLabelIndex(uint32_t logEntryIndex) {
		return logEntries[logEntryIndex].labelIndex;
	}
	DialogueDataEntry &logEntryIndexToDialogueData(int logEntryIndex) {
		return dialogueData[logEntryIndexToLabelIndex(logEntryIndex)];
	}
	bool logEntryIndexToIsRead(int logEntryIndex) {
		return readLabels[logEntryIndexToLabelIndex(logEntryIndex)];
	}
};

struct ChoiceState {
	std::vector<uint32_t> choiceVector;
	uint32_t acceptChoiceNextIndex{0};
	int32_t acceptChoiceVectorSize{-1};
	// Since accept_choice can only be called in superskip,
	// perhaps the acceptChoice variables should be part of SuperSkipData...?
};

struct HashedString {
	uint32_t hash;
	bool copied;
	char *str;

	// Warning: it does not copy passed string bytes by default
	HashedString(const char *k, bool copy = false) {
		hash       = static_cast<uint8_t>(k[0]) << 8;
		size_t len = std::strlen(k);
		hash |= static_cast<uint32_t>(len << 24) | (len > 0 ? k[len - 1] : 0);
		copied = copy;
		if (copy) {
			str = new char[len + 1];
			copystr(str, k, len + 1);
		} else {
			str = const_cast<char *>(k);
		}
	}
	HashedString(const HashedString &a) {
		hash   = a.hash;
		copied = a.copied;
		if (!a.copied) {
			str = a.str;
		} else {
			str = copystr(a.str);
		}
	}

	HashedString &operator=(const HashedString &) = delete;
	HashedString(HashedString &&a) noexcept {
		str    = a.str;
		a.str  = nullptr;
		hash   = a.hash;
		copied = a.copied;
	}

	FORCE_INLINE uint32_t getNum() const {
		return hash;
	}
	FORCE_INLINE const char *getStr() const {
		return str;
	}
	~HashedString() {
		if (copied) {
			freearr(&str);
		}
	}
};

namespace std {
template <>
struct hash<HashedString> {
	FORCE_INLINE size_t operator()(const HashedString &x) const {
		return x.hash;
	}
};
template <>
struct equal_to<HashedString> {
	FORCE_INLINE bool operator()(const HashedString &one, const HashedString &two) const {
		return equalstr(one.str, two.str);
	}
};
} // namespace std

class ScriptHandler {
public:
	enum { END_NONE       = 0,
		   END_COMMA      = 1,
		   END_1BYTE_CHAR = 2,
		   END_COMMA_READ = 4 // for LUA
	};

	enum class CmdType {
		None,
		BuiltIn,
		UserDef,
		Unknown
	};

	LogState logState;
	ChoiceState choiceState;

	ScriptHandler();
	ScriptHandler(const ScriptHandler &) = delete;
	ScriptHandler &operator=(const ScriptHandler &) = delete;
	~ScriptHandler();

	void reset();
	const char *getSavePath(const char *filename);

	void setSavedir(const char *dir);

	// basic parser function
	const char *readToken(bool check_pretext);
	const char *readName();
	const char *readColor(bool *is_color = nullptr);
	const char *readLabel();
	void readVariable(bool reread_flag = false);
	const char *readToEol();
	const char *readRaw();
	const char *readStr();
	const char *readFilePath();
	int32_t readInt();
	int32_t parseInt(const char **buf, bool signFlip=false);
	void skipToken();

	// function for string access
	std::string &getStringBufferRW() {
		return string_buffer;
	}
	const std::string &getStringBufferR() {
		return string_buffer;
	}
	const char *getStringBuffer() {
		return string_buffer.c_str();
	}
	const char *getSavedStringBuffer() {
		return saved_string_buffer.c_str();
	}
	const char *saveStringBuffer();
	void addStringBuffer(char ch);
	void trimStringBuffer(unsigned int n);
	void pushStringBuffer(int offset); // used in textgosub and pretextgosub
	int popStringBuffer();             // used in textgosub and pretextgosub

	// function for direct manipulation of script address
	const char *getCurrent() {
		return current_script;
	}
	const char *getNext() {
		return next_script;
	}
	void setCurrent(const char *pos, bool nowarn = false);
	void pushCurrent(const char *pos);
	void popCurrent();

	void enterExternalScript(char *pos); // LUA
	void leaveExternalScript();
	bool isExternalScript();

	size_t getScriptLength();
	ptrdiff_t getOffset(const char *pos);
	const char *getAddress(int offset);
	int getLineByAddress(const char *address, LabelInfo *guaranteeInLabel = nullptr);
	const char *getAddressByLine(int line);
	uint32_t getLabelIndex(LabelInfo *label);
	LabelInfo *getLabelByLogEntryIndex(int index);
	LabelInfo *getLabelByIndex(uint32_t index);
	LabelInfo *getLabelByAddress(const char *address);
	LabelInfo *getLabelByLine(int line);

	struct ScriptLoanStorable {
		std::string string_buffer;       // update only be readToken
		std::string saved_string_buffer; // updated only by saveStringBuffer
		std::string str_string_buffer;   // updated only by readStr
		std::string gosub_string_buffer; // used in textgosub and pretextgosub
		int gosub_string_offset{0};      // used in textgosub and pretextgosub
		const char *current_script{nullptr};
		const char *next_script{nullptr};
		CmdType current_cmd_type{CmdType::None};
		CmdType gosub_cmd_type{CmdType::None};
		int end_status{0};
		std::deque<NestInfo> callStack;
		LabelInfo *current_label_info{nullptr};
		int current_line{0};
		int string_buffer_offset{0};
		ScriptLoanStorable() {
			string_buffer.reserve(8192);
			saved_string_buffer.reserve(8192);
			str_string_buffer.reserve(8192);
			str_string_buffer.reserve(8192);
		}
	};
	ScriptLoanStorable getScriptStateData();
	void swapScriptStateData(ScriptLoanStorable &);

	bool isName(const char *name, bool attack_end = true);
	bool compareString(const char *buf);
	void setEndStatus(int val) {
		end_status |= val;
	}
	int getEndStatus() {
		return end_status;
	}
	bool hasMoreArgs();
	void skipLine(int no = 1);
	void setLinepage(bool val) {
		linepage_flag = val;
	}
	void setZenkakko(bool val) {
		zenkakko_flag = val;
	}
	void setEnglishMode(bool val) {
		english_mode = val;
	}

	// function for kidoku history
	bool isKidoku();
	void markAsKidoku(const char *address = nullptr);
	void setKidokuskip(bool kidokuskip_flag);
	void saveKidokuData(bool no_error = false);
	void loadKidokuData();

	void addStrVariable(const char **buf);
	void addIntVariable(const char **buf, bool no_zenkaku = false);
	void declareDim();

	void enableTextgosub(bool val);
	void setClickstr(const char *list);
	int checkClickstr(const char *buf, bool recursive_flag = false);

	void setInt(VariableInfo *var_info, int32_t val, int32_t offset = 0);
	void setNumVariable(int32_t no, int32_t val);
	void pushVariable();
	int32_t getIntVariable(VariableInfo *var_info = nullptr);

	void setStr(char **dst, const char *src, long num = -1);

	int32_t getStringFromInteger(char *buffer, int32_t no, int32_t num_column,
	                             bool is_zero_inserted = false,
	                             bool use_zenkaku      = false);

	size_t preprocessScript(uint8_t *buf, size_t size);
	bool isScript(const std::string &filename);
	int readScript();
	int labelScript();

	LabelInfo *lookupLabel(const char *label);
	LabelInfo *lookupLabelNext(const char *label);
	bool hasLabel(const char *label);

	ArrayVariable *getRootArrayVariable();

	inline void addNumAlias(const char *str, int32_t no) {
		num_alias.emplace(HashedString(str, true), no);
	}

	inline void addStrAlias(const char *str1, const char *str2) {
		str_alias.emplace(HashedString(str1, true), HashedString(str2, true));
	}

	bool findNumAlias(const char *str, int32_t *value);
	bool findStrAlias(const char *str, std::string *buffer);

	enum { LABEL_LOG = 0,
		   FILE_LOG  = 1
	};
	struct LogLink {
		LogLink *next;
		char *name;

		LogLink() {
			next = nullptr;
			name = nullptr;
		}
		~LogLink() {
			delete[] name;
		}
	};
	struct LogInfo {
		LogLink root_log;
		LogLink *current_log{nullptr};
		size_t num_logs{0};
		const char *filename{nullptr};
	} log_info[2];
	LogLink *findAndAddLog(LogInfo &info, const char *name, bool add_flag);
	void resetLog(LogInfo &info);

	std::deque<std::string> debugCommandLog;

	/* ---------------------------------------- */
	/* Variable */

	VariableData &getVariableData(uint32_t no);

	VariableInfo current_variable, pushed_variable;

	enum class ScreenSize {
		Sz640x480,
		Sz800x600,
		Sz400x300,
		Sz320x240,
		Sz1920x1080,
		Sz1280x720,
		Sz480x272
	};

	ScreenSize screen_size{ScreenSize::Sz1920x1080};
	int canvas_width{0}, canvas_height{0};

	int32_t global_variable_border{200};

	std::string game_identifier;
	char *save_path{nullptr};
	//Mion: savedir is set by savedirCommand, stores save files
	// and main stored gamedata files except envdata
	char *savedir{nullptr};
	uint32_t game_hash{0};

	//Mion: for more helpful error msgs
	bool strict_warnings{false};
	char current_cmd[64]{};

	CmdType current_cmd_type{CmdType::None}, gosub_cmd_type{CmdType::None};
	char errbuf[MAX_ERRBUF_LEN]{}; //intended for use creating error messages
	                               // before they are passed to errorAndExit,
	                               // simpleErrorAndExit or processError
	void processError(const char *str, const char *title = nullptr,
	                  const char *detail = nullptr, bool is_warning = false,
	                  bool is_simple = false, bool force_message = false);

	BaseReader *reader;

	enum class ScriptEncoding {
		Undefined,
		SinglePlain,
		MultiPlain,
		SingleCompressed
	};

#ifdef PUBLIC_RELEASE
	struct CompressedHeader {
		uint32_t magic;
		uint32_t compressed;
		uint32_t decompressed;
		uint32_t version;
	};
#endif

private:
	enum Operator {
		Invalid = 0b000,
		Plus    = 0b010,
		Minus   = 0b011,
		Mult    = 0b100,
		Div     = 0b101,
		Mod     = 0b110,
		HighPri = 0b100
	};

	int findLabel(const char *label);

	const char *checkComma(const char *buf);
	void parseStr(const char **buf);
	int32_t parseIntExpression(const char **buf, bool signFlip=false);
	void readNextOp(const char **buf, Operator *op, int32_t *num);
	int32_t calcArithmetic(int32_t num1, Operator op, int32_t num2);
	int32_t parseArray(const char **buf, ArrayVariable &array);
	int32_t *getArrayPtr(int32_t no, ArrayVariable &array, int32_t offset);

	/* ---------------------------------------- */
	/* Variable */
	VariableData variable_data[VARIABLE_RANGE];
	struct ExtendedVariableData {
		uint32_t no;
		VariableData vd;
	} * extended_variable_data{nullptr};
	uint32_t num_extended_variable_data{0};
	uint32_t max_extended_variable_data{1};

	std::unordered_map<HashedString, int32_t> num_alias;
	std::unordered_map<HashedString, HashedString> str_alias;

	ArrayVariable *root_array_variable{nullptr}, *current_array_variable;

	void errorAndExit(const char *str, const char *detail = nullptr, const char *title = nullptr, bool is_warning = false);
	void errorAndCont(const char *str, const char *detail = nullptr, const char *title = nullptr);
	void simpleErrorAndExit(const char *str, const char *title = nullptr, const char *detail = nullptr, bool is_warning = false);

	size_t script_buffer_length;
	char *script_buffer{nullptr};

	std::string string_buffer;       // updated only by readToken
	std::string saved_string_buffer; // updated only by saveStringBuffer
	std::string str_string_buffer;   // updated only by readStr
	std::string gosub_string_buffer; // used in textgosub and pretextgosub
	int gosub_string_offset{0};      // used in textgosub and pretextgosub

	LabelInfo *label_info;
	std::unordered_map<std::string, uint32_t> labelsByName;
	uint32_t num_of_labels{0};

	bool skip_enabled;
	bool kidokuskip_flag;
	char *kidoku_buffer{nullptr};

	bool zenkakko_flag{false};
	int end_status;
	bool linepage_flag;
	bool textgosub_flag;
	char *clickstr_list{nullptr};
	bool english_mode;

	const char *current_script;
	const char *next_script;

	const char *pushed_current_script;
	const char *pushed_next_script;

	const char *internal_current_script;
	const char *internal_next_script;
	int internal_end_status;
	VariableInfo internal_current_variable, internal_pushed_variable;
};
