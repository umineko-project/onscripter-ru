/**
 *  Script.cpp
 *  ONScripter-RU
 *
 *  Script manipulation handler.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Handlers/Script.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Support/Unicode.hpp"
#include "Support/FileIO.hpp"
#include "External/slre.h"

#include <sys/stat.h>
#include <sys/types.h>

#ifdef PUBLIC_RELEASE
#include <zlib.h>
#endif

#include <algorithm>
#include <vector>
#include <unordered_map>

template <typename T>
inline void SKIP_SPACE(T *&p) {
	while (*p == ' ' || *p == '\t')
		p++;
}

ScriptHandler::ScriptHandler() {
	log_info[LABEL_LOG].filename = "NScrllog.dat";
	log_info[FILE_LOG].filename  = "NScrflog.dat";
}

ScriptHandler::~ScriptHandler() {
	reset();

	freearr(&script_buffer);
	freearr(&kidoku_buffer);

	freearr(&save_path);
	freearr(&savedir);
}

void ScriptHandler::reset() {
	for (uint32_t i = 0; i < VARIABLE_RANGE; i++)
		variable_data[i].reset(true);

	freearr(&extended_variable_data);
	num_extended_variable_data = 0;
	max_extended_variable_data = 1;

	ArrayVariable *av = root_array_variable;
	while (av) {
		ArrayVariable *tmp = av;
		av                 = av->next;
		delete tmp;
	}
	root_array_variable = current_array_variable = nullptr;

	// reset log info
	resetLog(log_info[LABEL_LOG]);
	resetLog(log_info[FILE_LOG]);

	// reset number alias
	num_alias.clear();

	// reset string alias
	str_alias.clear();

	// reset misc. variables
	end_status       = END_NONE;
	kidokuskip_flag  = false;
	current_cmd[0]   = '\0';
	current_cmd_type = gosub_cmd_type = CmdType::None;
	zenkakko_flag                     = false;
	linepage_flag                     = false;
	english_mode                      = false;
	textgosub_flag                    = false;
	skip_enabled                      = false;
	if (clickstr_list) {
		delete[] clickstr_list;
		clickstr_list = nullptr;
	}
	internal_current_script = nullptr;
}

const char *ScriptHandler::getSavePath(const char *filename) {
	if (!savedir || equalstr(filename, "envdata")) {
		if (save_path)
			return save_path;
		else
			throw std::runtime_error("Null save_path!");
	}

	return savedir;
}

void ScriptHandler::setSavedir(const char *dir) {
	savedir = new char[std::strlen(dir) + std::strlen(save_path) + 2];
	std::sprintf(savedir, "%s%s%c", save_path, dir, DELIMITER);
	FileIO::makeDir(savedir, nullptr, true);
}

// basic parser function
const char *ScriptHandler::readToken(bool /*check_pretext*/) {
	current_script        = next_script;
	const char *buf       = current_script;
	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;

	current_cmd_type = CmdType::None;

	SKIP_SPACE(buf);

	markAsKidoku(buf);

	while (true) {
		string_buffer.clear();
		char ch = *buf;
		if (ch == ';') { // comment
			while (ch != 0x0a && ch != '\0') {
				addStringBuffer(ch);
				ch = *++buf;
			}
		} else if ((ch >= 'a' && ch <= 'z') ||
		           (ch >= 'A' && ch <= 'Z') ||
		           ch == '_') { // command
			do {
				if (ch >= 'A' && ch <= 'Z')
					ch += 'a' - 'A';
				addStringBuffer(ch);
				ch = *++buf;
			} while ((ch >= 'a' && ch <= 'z') ||
			         (ch >= 'A' && ch <= 'Z') ||
			         (ch >= '0' && ch <= '9') ||
			         ch == '_');
		} else if (ch == '*') { // label
			return readLabel();
		} else if (ch == 0x0a) {
			addStringBuffer(ch);
			markAsKidoku(buf++);
		} else if (ch == '~' || ch == ':') {
			addStringBuffer(ch);
			markAsKidoku(buf++);
		} else if (ch != '\0') {
			std::snprintf(errbuf, MAX_ERRBUF_LEN,
			              "readToken: skipping unknown heading character %c (%x)", ch, ch);
			errorAndCont(errbuf);
			buf++;
			continue;
		}

		break;
	}
	next_script = checkComma(buf);

	//sendToLog(LogLevel::Info, "readToken [%s] len=%d [%c(%x)] %p\n", string_buffer.c_str(), string_buffer.length(), ch, ch, next_script);

	return string_buffer.c_str();
}

const char *ScriptHandler::readName() {
	// bare word - not a string variable
	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;
	const char *buf;
	auto &varQueue  = ons.variableQueue;
	bool varQueueOn = ons.getVariableQueue();

	if (!varQueue.empty() && varQueueOn) {
		buf = varQueue.front().c_str();
	} else if (varQueueOn) {
		string_buffer.clear();
		return string_buffer.c_str();
	} else {
		current_script = next_script;
		SKIP_SPACE(current_script);
		buf = current_script;
	}

	string_buffer.clear();
	char ch = *buf;
	if (((ch >= 'a') && (ch <= 'z')) ||
	    ((ch >= 'A') && (ch <= 'Z')) ||
	    (ch == '_')) {
		if ((ch >= 'A') && (ch <= 'Z'))
			ch += 'a' - 'A';
		addStringBuffer(ch);
		ch = *(++buf);
		while (((ch >= 'a') && (ch <= 'z')) ||
		       ((ch >= 'A') && (ch <= 'Z')) ||
		       ((ch >= '0') && (ch <= '9')) ||
		       (ch == '_')) {
			if ((ch >= 'A') && (ch <= 'Z'))
				ch += 'a' - 'A';
			addStringBuffer(ch);
			ch = *(++buf);
		}
	}

	buf = checkComma(buf);

	if (!varQueue.empty() && varQueueOn) {
		varQueue.pop();
	} else {
		next_script = buf;
	}

	return string_buffer.c_str();
}

const char *ScriptHandler::readColor(bool *is_color) {
	// bare color type - not a string variable
	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;
	const char *buf;
	auto &varQueue  = ons.variableQueue;
	bool varQueueOn = ons.getVariableQueue();

	if (!varQueue.empty() && varQueueOn) {
		buf = varQueue.front().c_str();
	} else if (varQueueOn) {
		errorAndExit("readColor: not a valid color type.");
		return nullptr; //dummy
	} else {
		current_script = next_script;
		SKIP_SPACE(current_script);
		buf = current_script;
	}

	string_buffer.clear();
	addStringBuffer('#');
	char ch = *(++buf);
	int i;
	for (i = 0; i < 7; i++) {
		if (((ch >= '0') && (ch <= '9')) ||
		    ((ch >= 'a') && (ch <= 'f')) ||
		    ((ch >= 'A') && (ch <= 'F'))) {
			addStringBuffer(ch);
			ch = *(++buf);
		} else
			break;
	}
	if (i != 6) {
		if (is_color != nullptr) {
			*is_color = false;
			string_buffer.clear();
			return string_buffer.c_str();
		}
		string_buffer.clear();
		if (!varQueue.empty() && varQueueOn) {
			string_buffer.insert(0, varQueue.front(), 0, 16);
		} else {
			string_buffer.insert(0, current_script, 16);
		}
		errorAndExit("readColor: not a valid color type.");
	}

	buf = checkComma(buf);

	if (!varQueue.empty() && varQueueOn) {
		varQueue.pop();
	} else {
		next_script = buf;
	}

	if (is_color != nullptr)
		*is_color = true;

	return string_buffer.c_str();
}

const char *ScriptHandler::readLabel() {
	// *NAME, "*NAME", or $VAR that equals "*NAME"
	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;
	const char *buf;
	auto &varQueue  = ons.variableQueue;
	bool varQueueOn = ons.getVariableQueue();

	if (!varQueue.empty() && varQueueOn) {
		buf = varQueue.front().c_str();
	} else if (varQueueOn) {
		errorAndExit("readLabel: not a valid label.");
		return nullptr; //dummy
	} else {
		current_script = next_script;
		SKIP_SPACE(current_script);
		buf = current_script;
	}
	const char *tmp{nullptr};

	string_buffer.clear();
	char ch = *buf;
	if ((ch == '$') || (ch == '"') || (ch == '`')) {
		parseStr(&buf);
		tmp = buf;
		string_buffer.clear();
		buf = str_string_buffer.c_str();
		SKIP_SPACE(buf);
		ch = *buf;
	}
	if (ch == '*') {
		while (ch == '*') {
			addStringBuffer(ch);
			ch = *(++buf);
		}
		SKIP_SPACE(buf);

		ch = *buf;
		while (((ch >= 'a') && (ch <= 'z')) ||
		       ((ch >= 'A') && (ch <= 'Z')) ||
		       ((ch >= '0') && (ch <= '9')) ||
		       (ch == '_')) {
			if ((ch >= 'A') && (ch <= 'Z'))
				ch += 'a' - 'A';
			addStringBuffer(ch);
			ch = *(++buf);
		}
	}
	if (string_buffer.empty()) {
		if (!varQueue.empty() && varQueueOn) {
			buf = varQueue.front().c_str();
		} else {
			buf = current_script;
		}
		while (*buf && (*buf != 0x0a))
			++buf;
		if (tmp != nullptr) {
			std::snprintf(errbuf, MAX_ERRBUF_LEN,
			              "Invalid label specification '%s' ('%s')",
			              !varQueue.empty() && varQueueOn ? varQueue.front().c_str() : current_script,
			              str_string_buffer.c_str());
			errorAndExit(errbuf);
		} else {
			std::snprintf(errbuf, MAX_ERRBUF_LEN,
			              "Invalid label specification '%s'",
			              !varQueue.empty() && varQueueOn ? varQueue.front().c_str() : current_script);
			errorAndExit(errbuf);
		}
	}
	if (tmp != nullptr)
		buf = tmp;

	buf = checkComma(buf);

	if (!varQueue.empty() && varQueueOn) {
		varQueue.pop();
	} else {
		next_script = buf;
	}

	return string_buffer.c_str();
}

const char *ScriptHandler::readToEol() {
	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;

	current_script = next_script;
	SKIP_SPACE(current_script);
	next_script = current_script;

	string_buffer.clear();

	for (int i = 0; current_script[i] != '\n'; i++, next_script++) {
		string_buffer += current_script[i];
	}

	return string_buffer.c_str();
}

const char *ScriptHandler::readRaw() {
	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;
	const char *buf;

	if (ons.getVariableQueue()) {
		errorAndExit("readRaw shouldn't be used in case of variable queues");
		return nullptr; //dummy
	}

	current_script = next_script;
	SKIP_SPACE(current_script);

	string_buffer.clear();

	// Note, deliberately unsafe with messing , know what you do!
	for (buf = current_script; *buf != ',' && *buf != '\n'; buf++) {
		addStringBuffer(*buf);
	}

	if (*buf == ',') {
		end_status |= END_COMMA;
		buf++;
	}

	next_script = buf;

	return string_buffer.c_str();
}

const char *ScriptHandler::readStr() {
	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;
	const char *buf;
	auto &varQueue  = ons.variableQueue;
	bool varQueueOn = ons.getVariableQueue();

	if (!varQueue.empty() && varQueueOn) {
		buf = varQueue.front().c_str();
	} else if (varQueueOn) {
		string_buffer.clear();
		return string_buffer.c_str();
	} else {
		current_script = next_script;
		SKIP_SPACE(current_script);
		buf = current_script;
	}
	string_buffer.clear();

	while (true) {
		parseStr(&buf);
		buf = checkComma(buf);
		string_buffer += str_string_buffer;
		if (buf[0] != '+')
			break;
		buf++;
	}

	if (!varQueue.empty() && varQueueOn) {
		varQueue.pop();
	} else {
		next_script = buf;
	}

	return string_buffer.c_str();
}

const char *ScriptHandler::readFilePath() {
	ScriptHandler::readStr();
	translatePathSlashes(string_buffer);
	return string_buffer.c_str();
}

int32_t ScriptHandler::readInt() {
	string_buffer.clear();

	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;
	const char *buf;
	auto &varQueue  = ons.variableQueue;
	bool varQueueOn = ons.getVariableQueue();

	if (!varQueue.empty() && varQueueOn) {
		buf = varQueue.front().c_str();
	} else if (varQueueOn) {
		return 0;
	} else {
		current_script = next_script;
		SKIP_SPACE(current_script);
		buf = current_script;
	}

	int32_t ret = parseIntExpression(&buf);
	buf         = checkComma(buf);

	if (!varQueue.empty() && varQueueOn) {
		varQueue.pop();
	} else {
		next_script = buf;
	}

	return ret;
}

void ScriptHandler::skipToken() {
	SKIP_SPACE(current_script);
	const char *buf = current_script;

	bool quote_flag = false;
	while (true) {
		if (*buf == 0x0a ||
		    (!quote_flag && (*buf == ':' || *buf == ';')))
			break;
		if (*buf == '"')
			quote_flag = !quote_flag;
		buf++;
	}
	if (*buf == 0x0a)
		buf++;

	next_script = buf;
}

// string access function
const char *ScriptHandler::saveStringBuffer() {
	saved_string_buffer = string_buffer;
	return saved_string_buffer.c_str();
}

// script address direct manipulation function
void ScriptHandler::setCurrent(const char *pos, bool nowarn) {
	// warn if directly setting current_script outside the script_buffer
	if (!nowarn &&
	    ((pos < script_buffer) || (pos >= script_buffer + script_buffer_length)))
		errorAndCont("setCurrent: outside script bounds", nullptr, "Address Issue");

	current_script = next_script = pos;
}

void ScriptHandler::pushCurrent(const char *pos) {
	// push to use a temporary address for quick buffer parsing
	pushed_current_script = current_script;
	pushed_next_script    = next_script;

	setCurrent(pos, true);
}

void ScriptHandler::popCurrent() {
	current_script = pushed_current_script;
	next_script    = pushed_next_script;
}

void ScriptHandler::enterExternalScript(char *pos) {
	internal_current_script = current_script;
	internal_next_script    = next_script;
	setCurrent(pos);
	internal_end_status       = end_status;
	internal_current_variable = current_variable;
	internal_pushed_variable  = pushed_variable;
}

void ScriptHandler::leaveExternalScript() {
	setCurrent(internal_current_script);
	internal_current_script = nullptr;
	next_script             = internal_next_script;
	end_status              = internal_end_status;
	current_variable        = internal_current_variable;
	pushed_variable         = internal_pushed_variable;
}

bool ScriptHandler::isExternalScript() {
	return (internal_current_script != nullptr);
}

size_t ScriptHandler::getScriptLength() {
	return script_buffer_length;
}

ptrdiff_t ScriptHandler::getOffset(const char *pos) {
	return pos - script_buffer;
}

const char *ScriptHandler::getAddress(int offset) {
	return script_buffer + offset;
}

int ScriptHandler::getLineByAddress(const char *address, LabelInfo *guaranteeInLabel) {
	if ((address < script_buffer) || (address >= script_buffer + script_buffer_length)) {
		errorAndExit("getLineByAddress: outside script bounds", nullptr, "Address Error");
		return -1; //dummy
	}

	LabelInfo *label = guaranteeInLabel ? guaranteeInLabel : getLabelByAddress(address);

	const char *addr = label->label_header;
	int line         = 0;
	while (address > addr && line < label->num_of_lines) {
		if (*addr == 0x0a)
			line++;
		addr++;
	}
	return line;
}

const char *ScriptHandler::getAddressByLine(int line) {
	LabelInfo *label = getLabelByLine(line);

	int l            = line - label->start_line;
	const char *addr = label->label_header;
	while (l > 0) {
		while (*addr != 0x0a) addr++;
		addr++;
		l--;
	}
	return addr;
}

uint32_t ScriptHandler::getLabelIndex(LabelInfo *label) {
	ptrdiff_t ret = label - label_info;
	if (ret < 0 || static_cast<uint32_t>(ret) >= num_of_labels) {
		errorAndExit("getLabelIndex: label not present in label_info?");
	}
	return static_cast<uint32_t>(ret);
}

LabelInfo *ScriptHandler::getLabelByLogEntryIndex(int index) {
	return getLabelByIndex(logState.logEntryIndexToLabelIndex(index));
}

LabelInfo *ScriptHandler::getLabelByIndex(uint32_t index) {
	if (index >= num_of_labels) {
		errorAndExit("getLabelByIndex: label not present in label_info?");
	}
	return &label_info[index];
}

LabelInfo *ScriptHandler::getLabelByAddress(const char *address) {
	if (address == nullptr) {
		sendToLog(LogLevel::Error, "Requested getLabelByAddress(nullptr).\n");
		return &label_info[num_of_labels];
	}

	static LRUCache<const char *, LabelInfo *, std::unordered_map> addressCache(100, false);
	LabelInfo *label = nullptr;
	try {
		label = addressCache.get(address);
		if (label)
			return label;
	} catch (int) {
		uint32_t i;
		for (i = 0; i < num_of_labels - 1; i++) {
			if (label_info[i + 1].start_address > address) {
				addressCache.set(address, &label_info[i]);
				return &label_info[i];
			}
		}
		addressCache.set(address, &label_info[i]);
		return &label_info[i];
	}
	return &label_info[num_of_labels];
}

LabelInfo *ScriptHandler::getLabelByLine(int line) {
	static LRUCache<int, LabelInfo *, std::unordered_map> lineCache(100, false);
	LabelInfo *label = nullptr;
	try {
		label = lineCache.get(line);
		if (label)
			return label;
	} catch (int) {
		uint32_t i;
		for (i = 0; i < num_of_labels - 1; i++) {
			if (label_info[i + 1].start_line > line) {
				lineCache.set(line, &label_info[i]);
				return &label_info[i];
			}
		}
		if (i == num_of_labels - 1) {
			int num_lines = label_info[i].start_line + label_info[i].num_of_lines;
			if (line >= num_lines) {
				std::snprintf(errbuf, MAX_ERRBUF_LEN,
				              "getLabelByLine: line %d outside script bounds (%d lines)",
				              line, num_lines);
				errorAndExit(errbuf, nullptr, "Address Error");
			}
		}
		lineCache.set(line, &label_info[i]);
		return &label_info[i];
	}
	return &label_info[num_of_labels];
	/*	//sendToLog(LogLevel::Error, "Trying to find line with number %i ...\n", line);
	LabelInfo *label = labelsMapByLine(line);
	//sendToLog(LogLevel::Error, "label = %p.\n", label);
	if (label) return *label;
	//sendToLog(LogLevel::Error, "Failed.\n");
	return label_info[num_of_labels];*/
}

bool ScriptHandler::isName(const char *name, bool attack_end) {
	int n_len = static_cast<int>(std::strlen(name));

	if (ons.getVariableQueue()) {
		if (static_cast<int>(ons.variableQueueCommand.length()) < n_len)
			return false;
		auto cmd = ons.variableQueueCommand.c_str();
		if (attack_end) {
			for (int i = n_len - 1; i >= 0; i--)
				if (name[i] != cmd[i])
					return false;
		} else {
			for (int i = 0; i < n_len; i++)
				if (name[i] != cmd[i])
					return false;
		}
	} else {
		if (static_cast<int>(string_buffer.length()) < n_len)
			return false;
		auto cmd = string_buffer.c_str();
		if (attack_end) {
			for (int i = n_len - 1; i >= 0; i--)
				if (name[i] != cmd[i])
					return false;
		} else {
			for (int i = 0; i < n_len; i++)
				if (name[i] != cmd[i])
					return false;
		}
	}
	return true;
}

bool ScriptHandler::compareString(const char *buf) {
	SKIP_SPACE(next_script);
	size_t i, num = std::strlen(buf);
	for (i = 0; i < num; i++) {
		char ch = next_script[i];
		if ('A' <= ch && 'Z' >= ch)
			ch += 'a' - 'A';
		if (ch != buf[i])
			break;
	}
	return i == num;
}

bool ScriptHandler::hasMoreArgs() {
	if (ons.getVariableQueue())
		return !ons.variableQueue.empty();
	return getEndStatus() & END_COMMA;
}

void ScriptHandler::skipLine(int no) {
	for (int i = 0; i < no; i++) {
		while (*current_script != 0x0a) current_script++;
		current_script++;
	}
	next_script = current_script;
}

// function for kidoku history
bool ScriptHandler::isKidoku() {
	return skip_enabled;
}

void ScriptHandler::markAsKidoku(const char *address) {
	if (!kidokuskip_flag || internal_current_script != nullptr)
		return;

	ptrdiff_t offset = current_script - script_buffer;
	if (address)
		offset = address - script_buffer;
	//sendToLog(LogLevel::Info, "mark (%c)%x:%x = %d\n", *current_script, offset /8, offset%8, kidoku_buffer[ offset/8 ] & ((char)1 << (offset % 8)));
	skip_enabled = (kidoku_buffer[offset / 8] & (static_cast<char>(1) << (offset % 8))) != 0;
	kidoku_buffer[offset / 8] |= (static_cast<char>(1) << (offset % 8));
}

void ScriptHandler::setKidokuskip(bool kidokuskip_flag) {
	this->kidokuskip_flag = kidokuskip_flag;
}

void ScriptHandler::saveKidokuData(bool no_error) {
	FILE *fp = FileIO::openFile("kidoku.dat", "wb", getSavePath("kidoku.dat"));

	if (!fp) {
		if (!no_error)
			errorAndCont("can't open kidoku.dat for writing", nullptr, "I/O Warning");
		return;
	}

	if (std::fwrite(kidoku_buffer, 1, script_buffer_length / 8, fp) !=
	    size_t(script_buffer_length / 8)) {
		if (!no_error)
			errorAndCont("couldn't write to kidoku.dat", nullptr, "I/O Warning");
	}
	std::fclose(fp);
}

void ScriptHandler::loadKidokuData() {
	setKidokuskip(true);
	kidoku_buffer = new char[script_buffer_length / 8 + 1];
	std::memset(kidoku_buffer, 0, script_buffer_length / 8 + 1);

	FILE *fp = FileIO::openFile("kidoku.dat", "rb", getSavePath("kidoku.dat"));
	if (fp) {
		if (std::fread(kidoku_buffer, 1, script_buffer_length / 8, fp) != script_buffer_length / 8) {
			if (std::ferror(fp))
				errorAndCont("couldn't read from kidoku.dat", nullptr, "I/O Warning");
		}
		std::fclose(fp);
	}
}

void ScriptHandler::addIntVariable(const char **buf, bool no_zenkaku) {
	char num_buf[20];
	int32_t no = parseInt(buf);

	size_t len = getStringFromInteger(num_buf, no, -1, false, !no_zenkaku);
	for (size_t i = 0; i < len; i++)
		addStringBuffer(num_buf[i]);
}

void ScriptHandler::addStrVariable(const char **buf) {
	(*buf)++;
	int32_t no       = parseInt(buf);
	VariableData &vd = getVariableData(no);
	if (vd.str) {
		for (size_t i = 0, sz = std::strlen(vd.str); i < sz; i++) {
			addStringBuffer(vd.str[i]);
		}
	}
}

void ScriptHandler::enableTextgosub(bool val) {
	textgosub_flag = val;
}

void ScriptHandler::setClickstr(const char *list) {
	delete[] clickstr_list;
	clickstr_list = new char[std::strlen(list) + 2];
	std::memcpy(clickstr_list, list, std::strlen(list) + 1);
	clickstr_list[std::strlen(list) + 1] = '\0';
}

int ScriptHandler::checkClickstr(const char *buf, bool recursive_flag) {
	if ((buf[0] == '\\') && (buf[1] == '@'))
		return -2; //clickwait-or-page
	if ((buf[0] == '@') || (buf[0] == '\\'))
		return -1;

	if (clickstr_list == nullptr)
		return 0;
	//bool only_double_byte_check = true;
	char *click_buf = clickstr_list;

	uint32_t state         = 0;
	uint32_t buf_codepoint = 0;
	int ch_len             = 0;
	while (decodeUTF8(&state, &buf_codepoint, click_buf[ch_len])) ch_len++;
	ch_len++; //irrelevant
	uint32_t codepoint;

	while (click_buf[0]) {
		state     = 0;
		codepoint = 0;
		while (decodeUTF8(&state, &codepoint, click_buf[ch_len])) ch_len++;
		ch_len++;

		if (codepoint == '`') {
			click_buf++;
			// ignore completely.
			continue;
		}

		if (codepoint == buf_codepoint) {
			if (!recursive_flag && checkClickstr(buf + ch_len, true) != 0)
				return 0;
			return ch_len;
		}

		click_buf += ch_len;
	}

	return 0;
}

int32_t ScriptHandler::getIntVariable(VariableInfo *var_info) {
	if (var_info == nullptr)
		var_info = &current_variable;

	if (var_info->type == VariableInfo::TypeInt)
		return getVariableData(var_info->var_no).num;
	if (var_info->type == VariableInfo::TypeArray)
		return *getArrayPtr(var_info->var_no, var_info->array, 0);
	return 0;
}

void ScriptHandler::readVariable(bool reread_flag) {
	end_status            = END_NONE;
	current_variable.type = VariableInfo::TypeNone;
	if (reread_flag)
		next_script = current_script;
	current_script  = next_script;
	const char *buf = current_script;

	SKIP_SPACE(buf);

	bool ptr_flag = false;
	if (*buf == 'i' || *buf == 's') {
		ptr_flag = true;
		buf++;
	}

	if (*buf == '%') {
		buf++;
		current_variable.var_no = parseInt(&buf);
		current_variable.type   = VariableInfo::TypeInt;
	} else if (*buf == '?') {
		ArrayVariable av;
		current_variable.var_no = parseArray(&buf, av);
		current_variable.type   = VariableInfo::TypeArray;
		current_variable.array  = av;
	} else if (*buf == '$') {
		buf++;
		current_variable.var_no = parseInt(&buf);
		current_variable.type   = VariableInfo::TypeStr;
	}

	if (ptr_flag)
		current_variable.type |= VariableInfo::TypePtr;

	next_script = checkComma(buf);
}

void ScriptHandler::setInt(VariableInfo *var_info, int32_t val, int32_t offset) {
	if (var_info->type & VariableInfo::TypeInt) {
		setNumVariable(var_info->var_no + offset, val);
	} else if (var_info->type & VariableInfo::TypeArray) {
		*getArrayPtr(var_info->var_no, var_info->array, offset) = val;
	} else {
		errorAndExit("setInt: no integer variable.");
	}
}

void ScriptHandler::setStr(char **dst, const char *src, long num) {
	freearr(dst);

	if (src) {
		if (num >= 0) {
			*dst = new char[num + 1];
			std::memcpy(*dst, src, num);
			(*dst)[num] = '\0';
		} else {
			*dst = copystr(src);
		}
	}
}

void ScriptHandler::pushVariable() {
	pushed_variable = current_variable;
}

void ScriptHandler::setNumVariable(int32_t no, int32_t val) {
	VariableData &vd = getVariableData(no);
	if (vd.num_limit_flag) {
		if (val < vd.num_limit_lower)
			val = vd.num_limit_lower;
		else if (val > vd.num_limit_upper)
			val = vd.num_limit_upper;
	}
	vd.num = val;
}

int32_t ScriptHandler::getStringFromInteger(char *buffer, int32_t no, int32_t num_column,
                                            bool is_zero_inserted,
                                            bool use_zenkaku) {
	int32_t i, num_space = 0, num_minus = 0;
	if (no < 0) {
		num_minus = 1;
		no        = -no;
	}
	int32_t num_digit = 1, no2 = no;
	while (no2 >= 10) {
		no2 /= 10;
		num_digit++;
	}

	if (num_column < 0)
		num_column = num_digit + num_minus;
	if (num_digit + num_minus <= num_column)
		num_space = num_column - (num_digit + num_minus);
	else {
		for (i = 0; i < num_digit + num_minus - num_column; i++)
			no /= 10;
		num_digit -= num_digit + num_minus - num_column;
	}

	if (!use_zenkaku) {
		if (num_minus == 1)
			no = -no;
		char format[6];
		if (is_zero_inserted)
			std::snprintf(format, sizeof(format), "%%0%dd", num_column);
		else
			std::snprintf(format, sizeof(format), "%%%dd", num_column);

		std::sprintf(buffer, format, no);
		return num_column;
	}

	int32_t c = 0;
	if (is_zero_inserted) {
		for (i = 0; i < num_space; i++) {
			buffer[c++] = ("０")[0];
			buffer[c++] = ("０")[1];
		}
	} else {
		for (i = 0; i < num_space; i++) {
			buffer[c++] = ("　")[0];
			buffer[c++] = ("　")[1];
		}
	}
	if (num_minus == 1) {
		buffer[c++] = "−"[0];
		buffer[c++] = "−"[1];
	}
	c              = (num_column - 1) * 2;
	char num_str[] = "０１２３４５６７８９";
	for (i = 0; i < num_digit; i++) {
		buffer[c]     = num_str[no % 10 * 2];
		buffer[c + 1] = num_str[no % 10 * 2 + 1];
		no /= 10;
		c -= 2;
	}
	buffer[num_column * 2] = '\0';

	return num_column * 2;
}

size_t ScriptHandler::preprocessScript(uint8_t *buf, size_t size) {
	size_t count = 0, extra = 0, pos = 0;
	bool newline_flag = true;

	while (pos < size) {
		pos        = count + extra;
		uint8_t ch = buf[pos];

		if (ch == '*' && newline_flag) {
			if (num_of_labels == std::numeric_limits<decltype(num_of_labels)>::max()) {
				ons.errorAndExit("Maximum label amount reached!");
				return 0; // dummy
			}
			num_of_labels++;
		}

		if (ch == '\n') {
			newline_flag = true;
		} else if (ch == '\r') {
			extra++;
			continue;
		} else if (ch != '\t' && ch != ' ') {
			newline_flag = false;
		}

		buf[count] = ch;
		count++;
	}

	if (count < 10 || buf[count - 2] != '\0' || buf[count - 3] != '\n') {
		ons.errorAndExit("Invalid script discovered!");
		return 0; // dummy
	}

	return count - 2;
}

#ifdef PUBLIC_RELEASE
static constexpr uint32_t CompressedCrcA[2]{0x86, 0x23};
static constexpr uint32_t CompressedCrcB[2]{0x45, 0x71};

static constexpr uint32_t CompressedMagic{0x32534E4F};
static constexpr uint32_t CompressedVersion{110};
static constexpr uint32_t CompressedMin{0x10};
static constexpr uint32_t CompressedMax{0x10000000};

static constexpr uint8_t CompressedConversionTable[256]{
    0x37, 0x6a, 0x09, 0x5e, 0x7a, 0xaf, 0xf5, 0xa4, 0xba, 0x78, 0x84, 0x58, 0x35, 0x1e, 0x6b, 0x0c,
    0x49, 0xc6, 0xc3, 0x44, 0x40, 0x9e, 0x6f, 0x65, 0xe4, 0xf6, 0xfe, 0x22, 0xe2, 0x95, 0xc7, 0x38,
    0xf0, 0x1a, 0x82, 0xe0, 0x5b, 0x2a, 0xd8, 0xe5, 0xce, 0x2f, 0x74, 0x25, 0xec, 0x59, 0xc0, 0x45,
    0x4b, 0x64, 0x43, 0xdc, 0xb0, 0xb9, 0x30, 0x6d, 0x28, 0xd1, 0x16, 0xbb, 0x66, 0x98, 0x92, 0x90,
    0x2c, 0xa7, 0xf1, 0x80, 0xc1, 0xd4, 0x8b, 0xd6, 0xdf, 0x24, 0x2d, 0xf7, 0xfb, 0x88, 0x4d, 0x3c,
    0x72, 0xf3, 0xdb, 0x2b, 0x93, 0x73, 0xef, 0x85, 0x83, 0xee, 0xc2, 0x8d, 0x5c, 0xb2, 0x0b, 0x94,
    0x3d, 0xa8, 0x3f, 0x1c, 0x4c, 0x6e, 0x03, 0x7b, 0x1d, 0x5a, 0x51, 0xa1, 0x70, 0x41, 0xd0, 0xaa,
    0xa0, 0x7e, 0xcd, 0xd5, 0x15, 0xa9, 0x18, 0x76, 0xc9, 0x7d, 0x7f, 0x0e, 0x3a, 0x99, 0xbf, 0xab,
    0x3b, 0x14, 0x3e, 0x9a, 0x04, 0xda, 0x02, 0xfd, 0x63, 0xd9, 0xfa, 0x9f, 0x4e, 0xe3, 0x61, 0xbe,
    0x07, 0x11, 0xa6, 0x1b, 0x19, 0x55, 0x8e, 0x77, 0x0a, 0x47, 0xe6, 0xf8, 0x0d, 0xcf, 0xd7, 0x33,
    0x23, 0x1f, 0xbc, 0x62, 0xde, 0x9b, 0x29, 0x53, 0x68, 0xe8, 0x21, 0xb6, 0x34, 0x52, 0x87, 0xcb,
    0x08, 0x79, 0xf4, 0x67, 0x69, 0x54, 0xe7, 0x86, 0xea, 0xb4, 0x20, 0x71, 0x01, 0xbd, 0x06, 0x31,
    0x00, 0x50, 0xc8, 0xb8, 0xac, 0x5d, 0x57, 0x7c, 0x89, 0xeb, 0xb7, 0x36, 0x8f, 0xf2, 0xe1, 0x56,
    0x81, 0x4a, 0xd2, 0x8c, 0xf9, 0xad, 0x60, 0xa5, 0x42, 0x10, 0x5f, 0x12, 0xb3, 0xff, 0x4f, 0xdd,
    0x46, 0x26, 0xa2, 0x17, 0xc5, 0x75, 0x91, 0x27, 0xb5, 0x8a, 0xd3, 0x13, 0x2e, 0xc4, 0xe9, 0x9d,
    0x97, 0x39, 0x32, 0x05, 0x0f, 0xca, 0xcc, 0x48, 0xfc, 0xae, 0x96, 0xed, 0x6c, 0x9c, 0xb1, 0xa3};

static bool verifyHeader(ScriptHandler::CompressedHeader &header) {
	return header.magic == CompressedMagic && header.version == CompressedVersion &&
	       cmp::clamp(header.decompressed, CompressedMin, CompressedMax) == header.decompressed &&
	       cmp::clamp(header.compressed, CompressedMin, CompressedMax) == header.compressed;
}

#endif

bool ScriptHandler::isScript(const std::string &filename) {
	auto pos = filename.find_last_of('.');

	if (pos == std::string::npos ||
#ifdef PUBLIC_RELEASE
	    filename.substr(pos + 1) != "file"
#else
	    filename.substr(pos + 1) != "txt"
#endif
	)
		return false;

	FILE *fp = FileIO::openFile(filename.c_str(), "rb", ons.script_path);
	if (!fp)
		return false;

#ifdef PUBLIC_RELEASE
	CompressedHeader header;
	// Do only a brief sanity check
	if (std::fread(&header, sizeof(CompressedHeader), 1, fp) != 1 || !verifyHeader(header)) {
		std::fclose(fp);
		return false;
	}
#else
	char fb;
	if (std::fread(&fb, sizeof(char), 1, fp) != 1 || (fb != '*' && fb != ';')) {
		std::fclose(fp);
		return false;
	}
#endif

	std::fclose(fp);
	return true;
}

int ScriptHandler::readScript() {
	FILE *fp = nullptr;
#ifndef PUBLIC_RELEASE
	ScriptEncoding coding_mode = ScriptEncoding::MultiPlain;
#endif
	const char *filename = nullptr;

	// If we have script set it must exist in the main directory
	if (ons.script_is_set) {
		const char *ext = std::strrchr(ons.game_script, '.');
		if (ext && equalstr(ext, std::strrchr(DEFAULT_SCRIPT_NAME, '.'))) {
			filename = ons.game_script;
#ifndef PUBLIC_RELEASE
			coding_mode = ScriptEncoding::SinglePlain;
#endif
		}
	} else {
		filename = DEFAULT_SCRIPT_NAME;
	}

	if (filename)
		fp = FileIO::openFile(filename, "rb", ons.script_path);

	if (!fp) {
		if (filename)
			sendToLog(LogLevel::Error, "File %s was not found.\n", filename);
		simpleErrorAndExit("No compatible game script found.", "Can't open any script file", "Missing game data");
		return -1; // dummy
	}

	std::vector<uint8_t> script_data;
	uint8_t *tmp_buffer = nullptr;
	size_t tmp_length   = 0;

	auto appendScript = [&script_data, &tmp_buffer, &tmp_length]() {
		if (tmp_length == 0)
			return false;
		script_data.insert(script_data.end(), &tmp_buffer[0], &tmp_buffer[tmp_length]);
		script_data.emplace_back('\n');
		freearr(&tmp_buffer);
		return true;
	};

	bool script_valid = false;

	FileIO::readFile(fp, tmp_length, &tmp_buffer, true);

#ifndef PUBLIC_RELEASE
	if (appendScript()) {
		if (coding_mode == ScriptEncoding::MultiPlain) {
			for (size_t i = 1; i < 100; i++) {
				char filename[8];
				std::snprintf(filename, sizeof(filename), "%zu.txt", i);
				if (!FileIO::readFile(filename, ons.script_path, tmp_length, &tmp_buffer)) {
					std::snprintf(filename, sizeof(filename), "%02zu.txt", i);
					if (!FileIO::readFile(filename, ons.script_path, tmp_length, &tmp_buffer))
						break;
				}
				if (!appendScript())
					break;
			}
		}
		script_valid = true;
	}
#else
	if (appendScript() && tmp_length > sizeof(CompressedHeader)) {
		auto header = reinterpret_cast<CompressedHeader *>(script_data.data());

		if (verifyHeader(*header)) {
			uint8_t *compressed = script_data.data() + sizeof(CompressedHeader);
			for (size_t i = 0; i < header->compressed; i++) {
				compressed[i] = CompressedConversionTable[compressed[i] ^ CompressedCrcA[0]] ^ CompressedCrcA[1];
			}

			uLongf decompressed_length = header->decompressed;
			std::vector<uint8_t> decompressed(decompressed_length);
			int z = uncompress(decompressed.data(), &decompressed_length, compressed, header->compressed);
			if (z == Z_OK) {
				for (uLongf i = 0; i < decompressed_length; i++) {
					decompressed[i] = CompressedConversionTable[decompressed[i] ^ CompressedCrcB[0]] ^ CompressedCrcB[1];
				}
				decompressed.emplace_back('\n');
				script_data = decompressed;
				script_valid = true;
			}
		}
	}
#endif

	if (!script_valid) {
		simpleErrorAndExit("Discovered script file is either empty or corrupt.", "Invalid script file", "Corrupt game data");
		return -1; // dummy
	}

	script_data.emplace_back('\0');
	freearr(&script_buffer); // Why did we decide to free the buffer here?
	script_buffer_length = preprocessScript(script_data.data(), script_data.size());
	script_buffer        = copyarr(reinterpret_cast<char *>(script_data.data()), script_buffer_length + 1);
	game_hash            = static_cast<uint32_t>(script_buffer_length); // Reasonable "hash" value

	//sendToLog(LogLevel::Info,"num_of_labels %d\n",num_of_labels);

	// Haeleth: Search for gameid file (this overrides any builtin
	// ;gameid directive, or serves its purpose if none is available)
	// Mion: only if gameid not already set
	if (game_identifier.empty()) {
		uint8_t *gameid_buffer = nullptr;
		size_t gameid_length   = 0;
		//Mion: search only the script path
		if (FileIO::readFile("game.id", ons.script_path, gameid_length, &gameid_buffer) && gameid_length > 0) {
			game_identifier.assign(reinterpret_cast<char *>(gameid_buffer), gameid_length);
			freearr(&gameid_buffer);
			// Remove trailing new lines
			auto term = game_identifier.find_first_of("\r\n");
			if (term != std::string::npos)
				game_identifier.erase(game_identifier.begin() + term, game_identifier.end());
			// Ignore too large gameids
			if (game_identifier.size() > PATH_MAX)
				game_identifier.clear();
			if (game_identifier.empty())
				sendToLog(LogLevel::Warn, "Warning: couldn't read valid game ID from game.id\n");
		}
	}

	/* ---------------------------------------- */
	/* screen size and value check */
	const char *buf = script_buffer;

	const std::unordered_map<std::string, ScreenSize> screens{
	    {"640", ScreenSize::Sz640x480},
	    {"800", ScreenSize::Sz800x600},
	    {"400", ScreenSize::Sz400x300},
	    {"320", ScreenSize::Sz320x240},
	    {"1920", ScreenSize::Sz1920x1080},
	    {"1280", ScreenSize::Sz1280x720},
	    {"480", ScreenSize::Sz480x272}};

	while (buf[0] == ';' || buf[0] == ',') {
		buf++;

		if (!std::strncmp(buf, "mode", 4)) {
			buf += 4;
			screen_size = ScreenSize::Sz1920x1080;
			for (auto &scr : screens) {
				size_t len = scr.first.length();
				if (!scr.first.compare(0, len, buf, len)) {
					screen_size = scr.second;
					buf += len;
					break;
				}
			}

			if (*buf == '@') {
				buf++;
				canvas_width = parseIntExpression(&buf);
				buf++;
				canvas_height = parseIntExpression(&buf);
			}
		} else if (!std::strncmp(buf, "value", 5)) {
			buf += 5;
			SKIP_SPACE(buf);
			global_variable_border = parseIntExpression(&buf);
			if (global_variable_border < 0)
				global_variable_border = 0;
			//sendToLog(LogLevel::Info, "set global_variable_border: %d\n", global_variable_border);
		} else if (game_identifier.empty() && !std::strncmp(buf, "gameid ", 7)) {
			buf += 7;
			SKIP_SPACE(buf);
			size_t len = 0;
			while (buf[++len] != '\n')
				;
			game_identifier.assign(buf, len);
		} else {
			break;
		}

		while (*buf == '\n')
			buf++;
	}

	auto r = labelScript();
	if (r)
		return r;

	// Check for label duplicates
	std::unordered_map<std::string, uint32_t> labels;
	for (uint32_t i = 0; i < num_of_labels; i++) {
		auto it = labels.find(label_info[i].name);
		if (it != labels.end()) {
			char broken[1024];
			std::snprintf(broken, sizeof(broken), "Duplicate label *%s at line %d and line %d",
			              label_info[i].name, label_info[i].start_line, label_info[it->second].start_line);
			errorAndExit(broken);
		}
		labels.emplace(label_info[i].name, i);
	}

	return 0;
}

int ScriptHandler::labelScript() {
	int label_counter = -1;
	int current_line  = 0;
	const char *buf   = script_buffer;
	label_info        = new LabelInfo[num_of_labels + 1];
	logState.readLabels.resize(num_of_labels + 1); // unsure if +1 is required

	while (buf < script_buffer + script_buffer_length) {
		SKIP_SPACE(buf);
		if (*buf == '*') {
			setCurrent(buf);
			readLabel();
			size_t len                       = string_buffer.length();
			label_info[++label_counter].name = new char[len];
			copystr(label_info[label_counter].name, string_buffer.c_str() + 1, len);
			label_info[label_counter].label_header = buf;
			label_info[label_counter].num_of_lines = 1;
			label_info[label_counter].start_line   = current_line;

			buf = getNext();
			if (*buf == '\n') {
				buf++;
				SKIP_SPACE(buf);
				current_line++;
			}
			label_info[label_counter].start_address = buf;

			std::string name(label_info[label_counter].name);
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);
			labelsByName[name] = label_counter;
		} else {
			if (label_counter >= 0)
				label_info[label_counter].num_of_lines++;
			while (*buf != '\n') buf++;
			buf++;
			current_line++;
		}
	}

	label_info[num_of_labels].start_address = nullptr;

	return 0;
}

LabelInfo *ScriptHandler::lookupLabel(const char *label) {
	int i = findLabel(label);

	if (i == -1) {
		std::snprintf(errbuf, MAX_ERRBUF_LEN, R"(Label "*%s" not found.)", label);
		errorAndExit(errbuf, nullptr, "Label Error");
	}

	if (ons.labellog_flag)
		findAndAddLog(log_info[LABEL_LOG], label_info[i].name, true);
	return &label_info[i];
}

LabelInfo *ScriptHandler::lookupLabelNext(const char *label) {
	int i = findLabel(label);

	if (i == -1) {
		std::snprintf(errbuf, MAX_ERRBUF_LEN, R"(Label "*%s" not found.)", label);
		errorAndExit(errbuf, nullptr, "Label Error");
	}

	if (static_cast<uint32_t>(i + 1) < num_of_labels) {
		if (ons.labellog_flag)
			findAndAddLog(log_info[LABEL_LOG], label_info[i + 1].name, true);
		return &label_info[i + 1];
	}

	return &label_info[num_of_labels];
}

bool ScriptHandler::hasLabel(const char *label) {
	return (findLabel(label) != -1);
}

ScriptHandler::LogLink *ScriptHandler::findAndAddLog(LogInfo &info, const char *name, bool add_flag) {
	char capital_name[256];
	for (size_t i = 0, len = std::strlen(name) + 1; i < len; i++) {
		capital_name[i] = name[i];
		if ('a' <= capital_name[i] && capital_name[i] <= 'z')
			capital_name[i] += 'A' - 'a';
		else if (capital_name[i] == '/')
			capital_name[i] = '\\';
	}

	LogLink *cur = info.root_log.next;
	while (cur) {
		if (equalstr(cur->name, capital_name))
			break;
		cur = cur->next;
	}
	if (!add_flag || cur)
		return cur;

	LogLink *link = new LogLink();
	size_t len    = std::strlen(capital_name) + 1;
	link->name    = new char[len];
	copystr(link->name, capital_name, len);
	info.current_log->next = link;
	info.current_log       = info.current_log->next;
	info.num_logs++;

	return link;
}

void ScriptHandler::resetLog(LogInfo &info) {
	LogLink *link = info.root_log.next;
	while (link) {
		LogLink *tmp = link;
		link         = link->next;
		delete tmp;
	}

	info.root_log.next = nullptr;
	info.current_log   = &info.root_log;
	info.num_logs      = 0;
}

ArrayVariable *ScriptHandler::getRootArrayVariable() {
	return root_array_variable;
}

bool ScriptHandler::findNumAlias(const char *str, int *value) {
	auto it = num_alias.find(str);
	if (it != num_alias.end()) {
		*value = it->second;
		return true;
	}
	return false;
}

bool ScriptHandler::findStrAlias(const char *str, std::string *buffer) {
	auto it = str_alias.find(str);
	if (it != str_alias.end()) {
		*buffer = it->second.str;
		return true;
	}
	return false;
}

void ScriptHandler::processError(const char *str, const char *title, const char *detail, bool is_warning, bool is_simple, bool force_message) {
	//if not yet running the script, no line no/cmd - keep it simple
	if (script_buffer == nullptr)
		is_simple = true;

	if (title == nullptr)
		title = "Error";
	const char *type = is_warning ? "Warning" : "Fatal";

	if (is_simple) {
		sendToLog(LogLevel::Error, " ***[%s] %s: %s ***\n", type, title, str);
		if (detail)
			sendToLog(LogLevel::Error, "\t%s\n", detail);

		if (is_warning && !strict_warnings && !force_message)
			return;

		if (!ons.doErrorBox(title, str, true, is_warning))
			return;

		if (is_warning)
			sendToLog(LogLevel::Error, " ***[Fatal] User terminated at warning ***\n");

	} else {

		char errhist[1024], errcmd[128];

		const char *cur  = getCurrent();
		LabelInfo *label = getLabelByAddress(cur);
		int lblinenum = -1, linenum = -1;
		if ((cur >= script_buffer) && (cur < script_buffer + script_buffer_length)) {
			lblinenum = getLineByAddress(getCurrent()); // OPTIMIZE ME: we can probably pass in &label as second param to speed things up? check
			linenum   = label->start_line + lblinenum + 1;
		}

		errcmd[0] = '\0';
		if (current_cmd[0] != '\0') {
			if (current_cmd_type == CmdType::BuiltIn)
				std::snprintf(errcmd, 128, R"(, cmd "%s")", current_cmd);
			else if (current_cmd_type == CmdType::UserDef)
				std::snprintf(errcmd, 128, R"(, user-defined cmd "%s")", current_cmd);
		}
		if (linenum < 0) {
			sendToLog(LogLevel::Error, " ***[%s] %s at line ?? (*%s:)%s - %s ***\n",
			          type, title, label->name, errcmd, str);
		} else {
			sendToLog(LogLevel::Error, " ***[%s] %s at line %d (*%s:%d)%s - %s ***\n",
			          type, title, linenum, label->name, lblinenum, errcmd, str);
		}
		if (detail)
			sendToLog(LogLevel::Error, "\t%s\n", detail);
		if (!string_buffer.empty())
			sendToLog(LogLevel::Error, "\t(String buffer: [%s])\n", string_buffer.c_str());

		if (is_warning && !strict_warnings && !force_message)
			return;

		if (is_warning) {
			if (linenum < 0) {
				std::snprintf(errhist, 1024, "%s\nat line ?? (*%s:)%s\n%s",
				              str, label->name, errcmd,
				              detail ? detail : "");
			} else {
				std::snprintf(errhist, 1024, "%s\nat line %d (*%s:%d)%s\n%s",
				              str, linenum, label->name, lblinenum, errcmd,
				              detail ? detail : "");
			}

			if (!ons.doErrorBox(title, errhist, false, true))
				return;

			sendToLog(LogLevel::Error, " ***[Fatal] User terminated at warning ***\n");
		}

		//Mion: grabbing the current line in the script & up to 2 previous ones,
		std::string line[3];
		const char *end = getCurrent(), *buf;
		while (*end && *end != 0x0a) end++;
		for (int i = 2; i >= 0; i--) {
			if (linenum + i - 3 > 0) { // for 2+ lines
				buf = end - 1;
				while (*buf != 0x0a && *buf != 0x00) buf--;
				line[i].insert(0, buf + 1, end - 1 - buf);
				end = buf;
			} else if (linenum + i - 3 == 0) { // for 1 line
				end = buf = script_buffer;
				while (*end != 0x0a && *buf != 0x00) end++;
				line[i].insert(0, buf, end - buf);
			}
		}

		std::snprintf(errhist, 1024, "%s\nat line %d (*%s:%d)%s\n\n| %s\n| %s\n> %s",
		              str, linenum, label->name, lblinenum, errcmd,
		              line[0].c_str(), line[1].c_str(), line[2].c_str());

		sendToLog(LogLevel::Error, "Last executed command lines:\n");
		for (auto &log : debugCommandLog)
			sendToLog(LogLevel::Error, "%s\n", log.c_str());

		if (!ons.doErrorBox(title, errhist, false, false))
			return;
	}

	ons.requestQuit(ONScripter::ExitType::Error);
}

void ScriptHandler::errorAndExit(const char *str, const char *detail, const char *title, bool is_warning) {
	if (title == nullptr)
		title = "Script Error";

	processError(str, title, detail, is_warning);
}

void ScriptHandler::errorAndCont(const char *str, const char *detail, const char *title) {
	if (title == nullptr)
		title = "Script Warning";

	processError(str, title, detail, true);
}

void ScriptHandler::simpleErrorAndExit(const char *str, const char *title, const char *detail, bool is_warning) {
	if (title == nullptr)
		title = "Script Error";

	processError(str, detail, title, is_warning, true);
}

void ScriptHandler::addStringBuffer(char ch) {
	string_buffer += ch;
}

void ScriptHandler::trimStringBuffer(unsigned int n) {
	long string_counter = string_buffer.length() - n;
	if (string_counter < 0)
		string_counter = 0;

	string_buffer.erase(string_counter);
}

void ScriptHandler::pushStringBuffer(int offset) {
	gosub_string_buffer = string_buffer;
	gosub_string_offset = offset;
	gosub_cmd_type      = current_cmd_type;
}

int ScriptHandler::popStringBuffer() {
	string_buffer    = gosub_string_buffer;
	current_cmd_type = gosub_cmd_type;
	return gosub_string_offset;
}

VariableData &ScriptHandler::getVariableData(uint32_t no) {
	if (no < VARIABLE_RANGE)
		return variable_data[no];

	for (uint32_t i = 0; i < num_extended_variable_data; i++)
		if (extended_variable_data[i].no == no)
			return extended_variable_data[i].vd;

	num_extended_variable_data++;
	if (num_extended_variable_data == max_extended_variable_data) {
		ExtendedVariableData *tmp = extended_variable_data;
		extended_variable_data    = new ExtendedVariableData[max_extended_variable_data * 2];
		if (tmp) {
			std::memcpy(extended_variable_data, tmp, sizeof(ExtendedVariableData) * max_extended_variable_data);
			delete[] tmp;
		}
		max_extended_variable_data *= 2;
	}

	extended_variable_data[num_extended_variable_data - 1].no = no;

	return extended_variable_data[num_extended_variable_data - 1].vd;
}

// ----------------------------------------
// Private methods

int ScriptHandler::findLabel(const char *label) {
	if (!label)
		return -1;

	std::string name(label);
	std::transform(name.begin(), name.end(), name.begin(), [](char c) {
		return std::tolower(c);
	});

	if (labelsByName.count(name) == 0)
		return -1;

	return labelsByName[name];
}

const char *ScriptHandler::checkComma(const char *buf) {
	SKIP_SPACE(buf);
	if (*buf == ',') {
		end_status |= END_COMMA;
		buf++;
		SKIP_SPACE(buf);
	}

	return buf;
}

void ScriptHandler::parseStr(const char **buf) {
	SKIP_SPACE(*buf);

	if (**buf == '(') {
		// (foo) bar baz : apparently returns bar if foo has been
		// viewed, baz otherwise.
		// (Rather like a trigram implicitly using "fchk")

		(*buf)++;
		parseStr(buf);
		SKIP_SPACE(*buf);
		if ((*buf)[0] != ')')
			errorAndExit("parseStr: missing ')'.");
		(*buf)++;

		if (!ons.filelog_flag) {
			errorAndExit("filelog command is not called but file logging is requested");
		}

		if (findAndAddLog(log_info[FILE_LOG], str_string_buffer.c_str(), false)) {
			parseStr(buf);
			std::string tmp_buf = str_string_buffer;
			parseStr(buf);
			str_string_buffer = std::move(tmp_buf);
		} else {
			parseStr(buf);
			parseStr(buf);
		}
		current_variable.type |= VariableInfo::TypeConst;
	} else if (**buf == '$') {
		(*buf)++;
		int32_t no       = parseInt(buf);
		VariableData &vd = getVariableData(no);

		str_string_buffer.clear();
		if (vd.str)
			str_string_buffer.insert(0, vd.str);
		current_variable.type   = VariableInfo::TypeStr;
		current_variable.var_no = no;
	} else if (**buf == '"') {
		str_string_buffer.clear();
		(*buf)++;
		while (**buf != '"' && **buf != 0x0a)
			str_string_buffer += *(*buf)++;
		if (**buf == '"')
			(*buf)++;
		current_variable.type |= VariableInfo::TypeConst;
	} else if (**buf == '`') {
		str_string_buffer.clear();
		str_string_buffer += *(*buf)++;
		while (**buf != '`' && **buf != 0x0a)
			str_string_buffer += *(*buf)++;
		if (**buf == '`')
			(*buf)++;
		current_variable.type |= VariableInfo::TypeConst;
		end_status |= END_1BYTE_CHAR;
	} else if (**buf == '#') { // for color
		str_string_buffer.clear();
		for (int i = 0; i < 7; i++)
			str_string_buffer += *(*buf)++;
		current_variable.type = VariableInfo::TypeNone;
	} else if (**buf == '*') { // label
		str_string_buffer.clear();
		str_string_buffer += *(*buf)++;
		SKIP_SPACE(*buf);
		char ch = **buf;
		while ((ch >= 'a' && ch <= 'z') ||
		       (ch >= 'A' && ch <= 'Z') ||
		       (ch >= '0' && ch <= '9') ||
		       ch == '_') {
			if (ch >= 'A' && ch <= 'Z')
				ch += 'a' - 'A';
			str_string_buffer += ch;
			ch = *++(*buf);
		}
		current_variable.type |= VariableInfo::TypeConst;
	} else { // str alias
		char ch, alias_buf[512];
		int alias_buf_len = 0;
		bool first_flag   = true;

		while (true) {
			if (alias_buf_len == 511)
				break;
			ch = **buf;

			if ((ch >= 'a' && ch <= 'z') ||
			    (ch >= 'A' && ch <= 'Z') ||
			    ch == '_') {
				if (ch >= 'A' && ch <= 'Z')
					ch += 'a' - 'A';
				first_flag                 = false;
				alias_buf[alias_buf_len++] = ch;
			} else if (ch >= '0' && ch <= '9') {
				if (first_flag)
					errorAndExit("parseStr: string alias cannot start with a digit.");
				first_flag                 = false;
				alias_buf[alias_buf_len++] = ch;
			} else
				break;
			(*buf)++;
		}
		alias_buf[alias_buf_len] = '\0';

		if (alias_buf_len == 0) {
			str_string_buffer.clear();
			current_variable.type = VariableInfo::TypeNone;
			return;
		}

		if (!findStrAlias(const_cast<const char *>(alias_buf), &str_string_buffer)) {
			std::snprintf(errbuf, MAX_ERRBUF_LEN, "Undefined string alias '%s'", alias_buf);
			errorAndExit(errbuf);
		}
		current_variable.type |= VariableInfo::TypeConst;
	}
}

int32_t ScriptHandler::parseInt(const char **buf, bool flipSign) {
	int32_t ret = 0;

	SKIP_SPACE(*buf);

	if (**buf == '%') {
		(*buf)++;
		current_variable.var_no = parseInt(buf);
		current_variable.type   = VariableInfo::TypeInt;
		auto v = getVariableData(current_variable.var_no).num;
		return flipSign ? -v : v;
	}
	if (**buf == '?') {
		ArrayVariable av;
		current_variable.var_no = parseArray(buf, av);
		current_variable.type   = VariableInfo::TypeArray;
		current_variable.array  = av;
		auto v = *getArrayPtr(current_variable.var_no, current_variable.array, 0);
		return flipSign ? -v : v;
	}

	char ch, alias_buf[256];
	int alias_buf_len = 0, alias_no = 0;
	bool direct_num_flag = false;
	bool num_alias_flag  = false;

	const char *buf_start = *buf;
	while (true) {
		ch = **buf;

		if ((ch >= 'a' && ch <= 'z') ||
		    (ch >= 'A' && ch <= 'Z') ||
		    ch == '_') {
			if (ch >= 'A' && ch <= 'Z')
				ch += 'a' - 'A';
			if (direct_num_flag)
				break;
			num_alias_flag             = true;
			alias_buf[alias_buf_len++] = ch;
		} else if (ch >= '0' && ch <= '9') {
			if (!num_alias_flag)
				direct_num_flag = true;
			if (direct_num_flag) {
				if (flipSign)
					alias_no = alias_no * 10 - (ch - '0');
				else
					alias_no = alias_no * 10 + (ch - '0');
			} else {
				alias_buf[alias_buf_len++] = ch;
			}
		} else
			break;
		(*buf)++;
	}

	if (*buf - buf_start == 0) {
		current_variable.type = VariableInfo::TypeNone;
		return 0;
	}

	/* ---------------------------------------- */
	/* Solve num aliases */
	if (num_alias_flag) {
		alias_buf[alias_buf_len] = '\0';

		if (!findNumAlias(alias_buf, &alias_no)) {
			//sendToLog(LogLevel::Info, "can't find num alias for %s... assume 0.\n", alias_buf);
			current_variable.type = VariableInfo::TypeNone;
			*buf                  = buf_start;
			return 0;
		}
	}

	current_variable.type = VariableInfo::TypeInt | VariableInfo::TypeConst;
	ret                   = alias_no;

	SKIP_SPACE(*buf);

	return ret;
}

int32_t ScriptHandler::parseIntExpression(const char **buf, bool flipSign) {
	int32_t num[3];
	Operator op[2];

	SKIP_SPACE(*buf);

	readNextOp(buf, nullptr, &num[0]); // overflow here.

	readNextOp(buf, &op[0], &num[1]);
	if (op[0] == Operator::Invalid)
		return num[0];

	while (true) {
		readNextOp(buf, &op[1], &num[2]);
		if (op[1] == Operator::Invalid)
			break;

		if (!(op[0] & Operator::HighPri) && (op[1] & Operator::HighPri)) {
			num[1] = calcArithmetic(num[1], op[1], num[2]);
		} else {
			num[0] = calcArithmetic(num[0], op[0], num[1]);
			op[0]  = op[1];
			num[1] = num[2];
		}
	}

	auto ret = calcArithmetic(num[0], op[0], num[1]);
	// This is for -(expr) cases, so expr should always be <= MAX_INT.
	if (flipSign)
		ret = -ret;
	return ret;
}

/*
 * Internal buffer looks like this.
 *   num[0] op[0] num[1] op[1] num[2]
 * If priority of op[0] is higher than op[1], (num[0] op[0] num[1]) is computed,
 * otherwise (num[1] op[1] num[2]) is computed.
 * Then, the next op and num is read from the script.
 * Num is an immediate value, a variable or a bracketed expression.
 */
void ScriptHandler::readNextOp(const char **buf, Operator *op, int32_t *num) {
	bool minus_flag = false;
	SKIP_SPACE(*buf);
	const char *buf_start = *buf;

	if (op) {
		if ((*buf)[0] == '+')
			*op = Operator::Plus;
		else if ((*buf)[0] == '-')
			*op = Operator::Minus;
		else if ((*buf)[0] == '*')
			*op = Operator::Mult;
		else if ((*buf)[0] == '/')
			*op = Operator::Div;
		else if ((*buf)[0] == 'm' &&
		         (*buf)[1] == 'o' &&
		         (*buf)[2] == 'd' &&
		         ((*buf)[3] == ' ' ||
		          (*buf)[3] == '\t' ||
		          (*buf)[3] == '$' ||
		          (*buf)[3] == '%' ||
		          (*buf)[3] == '?' ||
		          ((*buf)[3] >= '0' && (*buf)[3] <= '9')))
			*op = Operator::Mod;
		else {
			*op = Operator::Invalid;
			return;
		}
		if (*op == Operator::Mod)
			*buf += 3;
		else
			(*buf)++;
		SKIP_SPACE(*buf);
	} else {
		if ((*buf)[0] == '-') {
			minus_flag = true;
			(*buf)++;
			SKIP_SPACE(*buf);
		}
	}

	if ((*buf)[0] == '(') {
		(*buf)++;
		*num = parseIntExpression(buf, minus_flag);
		SKIP_SPACE(*buf);
		if ((*buf)[0] != ')')
			errorAndExit("Missing ')' in expression");
		(*buf)++;
	} else {
		*num = parseInt(buf, minus_flag);
		if (current_variable.type == VariableInfo::TypeNone) {
			if (op)
				*op = Operator::Invalid;
			*buf = buf_start;
		}
	}
}

int32_t ScriptHandler::calcArithmetic(int32_t num1, Operator op, int32_t num2) {
	int32_t ret = 0;

	if (op == Operator::Plus)
		ret = num1 + num2;
	else if (op == Operator::Minus)
		ret = num1 - num2;
	else if (op == Operator::Mult)
		ret = num1 * num2;
	else if (op == Operator::Div)
		ret = num1 / num2;
	else if (op == Operator::Mod)
		ret = num1 % num2;

	current_variable.type = VariableInfo::TypeInt | VariableInfo::TypeConst;

	return ret;
}

int32_t ScriptHandler::parseArray(const char **buf, ArrayVariable &array) {
	SKIP_SPACE(*buf);

	(*buf)++; // skip '?'
	int32_t no = parseInt(buf);

	SKIP_SPACE(*buf);
	array.num_dim = 0;
	while (**buf == '[') {
		(*buf)++;
		array.dim[array.num_dim] = parseIntExpression(buf);
		array.num_dim++;
		SKIP_SPACE(*buf);
		if (**buf != ']')
			errorAndExit("parseArray: missing ']'.");
		(*buf)++;
	}
	for (int32_t i = array.num_dim; i < 20; i++) array.dim[i] = 0;

	return no;
}

int32_t *ScriptHandler::getArrayPtr(int32_t no, ArrayVariable &array, int32_t offset) {
	ArrayVariable *av = root_array_variable;
	while (av) {
		if (av->no == no)
			break;
		av = av->next;
	}
	if (av == nullptr) {
		std::snprintf(errbuf, MAX_ERRBUF_LEN, "Undeclared array number %d", no);
		errorAndExit(errbuf, nullptr, "Access Error");
		return nullptr; //dummy
	}

	int32_t dim = 0, i;
	for (i = 0; i < av->num_dim; i++) {
		if (av->dim[i] <= array.dim[i])
			errorAndExit("Array access out of bounds", "dim[i] <= array.dim[i]", "Access Error");
		dim = dim * av->dim[i] + array.dim[i];
	}
	if (av->dim[i - 1] <= array.dim[i - 1] + offset)
		errorAndExit("Array access out of bounds", "dim[i-1] <= array.dim[i-1] + offset", "Access Error");

	return &av->data[dim + offset];
}

void ScriptHandler::declareDim() {
	current_script  = next_script;
	const char *buf = current_script;

	if (current_array_variable) {
		current_array_variable->next = new ArrayVariable();
		current_array_variable       = current_array_variable->next;
	} else {
		root_array_variable    = new ArrayVariable();
		current_array_variable = root_array_variable;
	}

	ArrayVariable array;
	current_array_variable->no = parseArray(&buf, array);

	size_t dim                      = 1;
	current_array_variable->num_dim = array.num_dim;
	for (int32_t i = 0; i < array.num_dim; i++) {
		current_array_variable->dim[i] = array.dim[i] + 1;
		dim *= (array.dim[i] + 1);
	}
	current_array_variable->data = new int32_t[dim];
	std::memset(current_array_variable->data, 0, sizeof(int32_t) * dim);

	next_script = buf;
}

ScriptHandler::ScriptLoanStorable ScriptHandler::getScriptStateData() {
	ScriptLoanStorable sl;

	sl.string_buffer        = string_buffer;
	sl.saved_string_buffer  = saved_string_buffer;
	sl.str_string_buffer    = str_string_buffer;
	sl.gosub_string_buffer  = gosub_string_buffer;
	sl.gosub_string_offset  = gosub_string_offset;
	sl.current_script       = current_script;
	sl.next_script          = next_script;
	sl.current_cmd_type     = current_cmd_type;
	sl.gosub_cmd_type       = gosub_cmd_type;
	sl.end_status           = end_status;
	sl.callStack            = ons.callStack;
	sl.current_label_info   = ons.current_label_info;
	sl.current_line         = ons.current_line;
	sl.string_buffer_offset = ons.string_buffer_offset;

	return sl;
}

void ScriptHandler::swapScriptStateData(ScriptLoanStorable &sl) {
	std::swap(string_buffer, sl.string_buffer);
	std::swap(saved_string_buffer, sl.saved_string_buffer);
	std::swap(str_string_buffer, sl.str_string_buffer);
	std::swap(gosub_string_buffer, sl.gosub_string_buffer);
	std::swap(gosub_string_offset, sl.gosub_string_offset);
	std::swap(current_script, sl.current_script);
	std::swap(next_script, sl.next_script);
	std::swap(current_cmd_type, sl.current_cmd_type);
	std::swap(gosub_cmd_type, sl.gosub_cmd_type);
	std::swap(end_status, sl.end_status);
	std::swap(ons.callStack, sl.callStack);
	std::swap(ons.current_label_info, sl.current_label_info);
	std::swap(ons.current_line, sl.current_line);
	std::swap(ons.string_buffer_offset, sl.string_buffer_offset);
}
