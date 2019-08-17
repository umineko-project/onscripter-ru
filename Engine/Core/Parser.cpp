/**
 *  Parser.cpp
 *  ONScripter-RU
 *
 *  Define block parser.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/Parser.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Readers/Direct.hpp"
#include "Support/FileIO.hpp"
#include "Resources/Support/Version.hpp"

#include <utility>

using CommandFunc = int (ScriptParser::*)();
static std::unordered_map<HashedString, CommandFunc> func_lut{
    {"windowz", &ScriptParser::windowzCommand},
    {"uninterruptible", &ScriptParser::uninterruptibleCommand},
    {"timestamp", &ScriptParser::timeStampCommand},
    {"textz", &ScriptParser::textzCommand},
    {"skipgosub", &ScriptParser::skipgosubCommand},
    {"setdefaultspeed", &ScriptParser::setdefaultspeedCommand},
    {"movs", &ScriptParser::movCommand},
    {"ignore_inl_cmd_clear", &ScriptParser::ignoreCommandCommand},
    {"ignore_inl_cmd", &ScriptParser::ignoreCommandCommand},
    {"ignore_cmd_clear", &ScriptParser::ignoreCommandCommand},
    {"ignore_cmd", &ScriptParser::ignoreCommandCommand},
    {"hudz", &ScriptParser::hudzCommand},
    {"getstralias", &ScriptParser::getStraliasCommand},
    {"event_callback", &ScriptParser::eventCallbackCommand},
    {"disablespeedbuttons", &ScriptParser::disablespeedbuttonsCommand},
    {"borderstyle", &ScriptParser::borderstyleCommand},

    {"zenkakko", &ScriptParser::zenkakkoCommand},
    {"windoweffect", &ScriptParser::effectCommand},
    {"windowchip", &ScriptParser::windowchipCommand},
    {"windowback", &ScriptParser::windowbackCommand},
    {"versionstr", &ScriptParser::versionstrCommand},
    {"usewheel", &ScriptParser::usewheelCommand},
    {"useescspc", &ScriptParser::useescspcCommand},
    {"underline", &ScriptParser::underlineCommand},
    {"transmode", &ScriptParser::transmodeCommand},
    {"time", &ScriptParser::timeCommand},
    {"textgosub", &ScriptParser::textgosubCommand},
    {"tan", &ScriptParser::tanCommand},
    {"sub", &ScriptParser::subCommand},
    {"stralias", &ScriptParser::straliasCommand},
    {"spritesetz", &ScriptParser::spritesetzCommand},
    {"spi", &ScriptParser::soundpressplginCommand},
    {"soundpressplgin", &ScriptParser::soundpressplginCommand},
    {"skip", &ScriptParser::skipCommand},
    {"sin", &ScriptParser::sinCommand},
    {"shadedistance", &ScriptParser::shadedistanceCommand},
    {"setlayer", &ScriptParser::setlayerCommand},
    {"setkinsoku", &ScriptParser::setkinsokuCommand},
    {"selectvoice", &ScriptParser::selectvoiceCommand},
    {"selectcolor", &ScriptParser::selectcolorCommand},
    {"savenumber", &ScriptParser::savenumberCommand},
    {"savename", &ScriptParser::savenameCommand},
    {"savedir", &ScriptParser::savedirCommand},
    {"sar", &ScriptParser::nsaCommand},
    {"rubyon2", &ScriptParser::rubyonCommand},
    {"rubyon", &ScriptParser::rubyonCommand},
    {"rubyoff", &ScriptParser::rubyoffCommand},
    {"roff", &ScriptParser::roffCommand},
    {"rmenu", &ScriptParser::rmenuCommand},
    {"rgosub", &ScriptParser::rgosubCommand},
    {"return", &ScriptParser::returnCommand},
    {"pretextgosub", &ScriptParser::pretextgosubCommand},
    {"pagetag", &ScriptParser::pagetagCommand},
    {"numalias", &ScriptParser::numaliasCommand},
    {"nsadir", &ScriptParser::nsadirCommand},
    {"nsa", &ScriptParser::nsaCommand},
    {"notif", &ScriptParser::ifCommand},
    {"next", &ScriptParser::nextCommand},
    {"ns3", &ScriptParser::nsaCommand},
    {"ns2", &ScriptParser::nsaCommand},
    {"mul", &ScriptParser::mulCommand},
    {"movl", &ScriptParser::movCommand},
    {"mov10", &ScriptParser::movCommand},
    {"mov9", &ScriptParser::movCommand},
    {"mov8", &ScriptParser::movCommand},
    {"mov7", &ScriptParser::movCommand},
    {"mov6", &ScriptParser::movCommand},
    {"mov5", &ScriptParser::movCommand},
    {"mov4", &ScriptParser::movCommand},
    {"mov3", &ScriptParser::movCommand},
    {"mov", &ScriptParser::movCommand},
    {"mode_wave_demo", &ScriptParser::mode_wave_demoCommand},
    {"mode_saya", &ScriptParser::mode_sayaCommand},
    {"mode_ext", &ScriptParser::mode_extCommand},
    {"mod", &ScriptParser::modCommand},
    {"mid", &ScriptParser::midCommand},
    {"menusetwindow", &ScriptParser::menusetwindowCommand},
    {"menuselectvoice", &ScriptParser::menuselectvoiceCommand},
    {"menuselectcolor", &ScriptParser::menuselectcolorCommand},
    {"maxkaisoupage", &ScriptParser::maxkaisoupageCommand},
    {"luasub", &ScriptParser::luasubCommand},
    {"luacall", &ScriptParser::luacallCommand},
    {"lookbacksp", &ScriptParser::lookbackspCommand},
    {"lookbackcolor", &ScriptParser::lookbackcolorCommand},
    //{"lookbackbutton",      &ScriptParser::lookbackbuttonCommand},
    {"loadgosub", &ScriptParser::loadgosubCommand},
    {"linepage2", &ScriptParser::linepageCommand},
    {"linepage", &ScriptParser::linepageCommand},
    {"len", &ScriptParser::lenCommand},
    {"labellog", &ScriptParser::labellogCommand},
    {"labelexist", &ScriptParser::labelexistCommand},
    {"kidokuskip", &ScriptParser::kidokuskipCommand},
    {"kidokumode", &ScriptParser::kidokumodeCommand},
    {"itoa2", &ScriptParser::itoaCommand},
    {"itoa", &ScriptParser::itoaCommand},
    {"intlimit", &ScriptParser::intlimitCommand},
    {"inc", &ScriptParser::incCommand},
    {"if", &ScriptParser::ifCommand},
    {"humanz", &ScriptParser::humanzCommand},
    {"humanpos", &ScriptParser::humanposCommand},
    {"gosub", &ScriptParser::gosubCommand},
    {"globalon", &ScriptParser::globalonCommand},
    {"getparam", &ScriptParser::getparamCommand},
    //{"game",    &ScriptParser::gameCommand},
    {"for", &ScriptParser::forCommand},
    {"filelog", &ScriptParser::filelogCommand},
    {"errorsave", &ScriptParser::errorsaveCommand},
    {"english", &ScriptParser::englishCommand},
    {"effectcut", &ScriptParser::effectcutCommand},
    {"effectblank", &ScriptParser::effectblankCommand},
    {"effect", &ScriptParser::effectCommand},
    {"dsound", &ScriptParser::dsoundCommand},
    {"div", &ScriptParser::divCommand},
    {"dim", &ScriptParser::dimCommand},
    {"defvoicevol", &ScriptParser::defvoicevolCommand},
    {"defsub", &ScriptParser::defsubCommand},
    {"defsevol", &ScriptParser::defsevolCommand},
    {"defmp3vol", &ScriptParser::defmp3volCommand},
    {"defbgmvol", &ScriptParser::defmp3volCommand},
    {"defvideovol", &ScriptParser::defvideovolCommand},
    {"defaultspeed", &ScriptParser::defaultspeedCommand},
    {"defaultfont", &ScriptParser::defaultfontCommand},
    {"dec", &ScriptParser::decCommand},
    {"date", &ScriptParser::dateCommand},
    {"cos", &ScriptParser::cosCommand},
    {"cmp", &ScriptParser::cmpCommand},
    {"clickvoice", &ScriptParser::clickvoiceCommand},
    {"clickstr", &ScriptParser::clickstrCommand},
    {"clickskippage", &ScriptParser::clickskippageCommand},
    {"btnnowindowerase", &ScriptParser::btnnowindoweraseCommand},
    {"break", &ScriptParser::breakCommand},
    {"automode", &ScriptParser::mode_extCommand},
    {"atoi", &ScriptParser::atoiCommand},
    {"arc", &ScriptParser::arcCommand},
    {"addnsadir", &ScriptParser::addnsadirCommand},
    {"addkinsoku", &ScriptParser::addkinsokuCommand},
    {"add", &ScriptParser::addCommand}};

int ScriptParser::ownDeinit() {
	reset();

	tmp_effect.anim.reset();
	window_effect.anim.reset();

	freearr(&version_str);

	freearr(&start_kinsoku);
	freearr(&end_kinsoku);

	freearr(&cmdline_game_id);
	freearr(&savedir);

	return 0;
}

void ScriptParser::reset() {
	resetDefineFlags();

	int i;
	user_func_lut.clear();
	ignored_func_lut.clear();

	// reset misc variables
	nsa_path = DirPaths();

	freearr(&version_str);
	version_str = new char[std::strlen(VERSION_STR1) +
						   std::strlen("\n") +
						   std::strlen(VERSION_STR2) +
						   std::strlen("\n") +
	                       +1];
	std::sprintf(version_str, "%s\n%s\n", VERSION_STR1, VERSION_STR2);

	/* Text related variables */
	sentence_font.reset();
	name_font.reset();
	current_font = &sentence_font;

	textgosub_label      = nullptr;
	pretextgosub_label   = nullptr;
	loadgosub_label      = nullptr;
	event_callback_label = nullptr;

	/* ---------------------------------------- */
	/* Sound related variables */
	for (i = 0; i < CLICKVOICE_NUM; i++)
		script_h.setStr(&clickvoice_file_name[i], nullptr);
	for (i = 0; i < SELECTVOICE_NUM; i++)
		script_h.setStr(&selectvoice_file_name[i], nullptr);

	/* Effect related variables */
	effect_links.resize(1);
	deleteLayerInfo();

	readLog(script_h.log_info[ScriptHandler::LABEL_LOG]);
}

void ScriptParser::resetDefineFlags() {
	globalon_flag      = false;
	labellog_flag      = false;
	filelog_flag       = false;
	kidokuskip_flag    = false;
	clickskippage_flag = false;

	rmode_flag            = true;
	btnnowindowerase_flag = false;
	usewheel_flag         = false;
	useescspc_flag        = false;
	mode_wave_demo_flag   = false;
	mode_saya_flag        = false;
	//NScr 2.82+ enables mode_ext (automode) by default, let's do so too
	mode_ext_flag        = true;
	pagetag_flag         = false;
	windowchip_sprite_no = -1;
	string_buffer_offset = 0;

	break_flag = false;
	trans_mode = AnimationInfo::TRANS_TOPLEFT;

	/* ---------------------------------------- */
	/* Lookback related variables */
	lookback_sp[0] = lookback_sp[1] = -1;
	lookback_color                  = {0xff, 0xff, 0x00};

	/* ---------------------------------------- */
	/* Save/Load related variables */
	num_save_file = 9;

	/* ---------------------------------------- */
	/* Text related variables */
	//shade_distance[0] = 1;
	//shade_distance[1] = 1;

	clickstr_line  = 0;
	clickstr_state = CLICK_NONE;
	linepage_mode  = 0;
	english_mode   = false;

	/* ---------------------------------------- */
	/* Effect related variables */
	effect_blank    = 10;
	effect_cut_flag = false;

	auto &effect    = effect_links.front();
	effect.no       = 0;
	effect.effect   = 0;
	effect.duration = 0;

	window_effect.effect   = 1;
	window_effect.duration = 0;

	current_mode = DEFINE_MODE;

	uninterruptibleLabels.clear();
}

int ScriptParser::open() {
	script_h.reader = new DirectReader(archive_path);
	script_h.reader->open();

	if (cmdline_game_id) {
		script_h.game_identifier = cmdline_game_id;
		freearr(&cmdline_game_id);
	}

	if (script_h.readScript())
		return -1;

	int script_width = 0, script_height = 0;
	switch (script_h.screen_size) {
		case ScriptHandler::ScreenSize::Sz800x600:
			script_width  = 800;
			script_height = 600;
			break;
		case ScriptHandler::ScreenSize::Sz400x300:
			script_width  = 400;
			script_height = 300;
			break;
		case ScriptHandler::ScreenSize::Sz320x240:
			script_width  = 320;
			script_height = 240;
			break;
		case ScriptHandler::ScreenSize::Sz640x480:
			script_width  = 640;
			script_height = 480;
			break;
		case ScriptHandler::ScreenSize::Sz1280x720:
			script_width  = 1280;
			script_height = 720;
			break;
		case ScriptHandler::ScreenSize::Sz480x272:
			script_width  = 480;
			script_height = 272;
			break;
		case ScriptHandler::ScreenSize::Sz1920x1080:
			script_width  = 1920;
			script_height = 1080;
			break;
	}

	window.applyDimensions(script_width, script_height, script_h.canvas_width, script_h.canvas_height, preferred_width);

	underline_value = script_height - 1;
	for (int i = 0; i < 3; i++)
		humanpos[i] = (script_width / 4) * (i + 1);
	if (debug_level > 0)
		sendToLog(LogLevel::Info, "humanpos: %d,%d,%d; underline: %d\n", humanpos[0], humanpos[1],
		          humanpos[2], underline_value);

	return 0;
}

uint8_t ScriptParser::convHexToDec(char ch) {
	if ('0' <= ch && ch <= '9')
		return ch - '0';
	if ('a' <= ch && ch <= 'f')
		return ch - 'a' + 10;
	if ('A' <= ch && ch <= 'F')
		return ch - 'A' + 10;
	errorAndExit("convHexToDec: not valid character for color.");

	return 0;
}

void ScriptParser::readColor(uchar3 *color, const char *buf) {
	if (buf[0] != '#')
		errorAndExit("readColor: no preceding #.");
	color->x = convHexToDec(buf[1]) << 4 | convHexToDec(buf[2]);
	color->y = convHexToDec(buf[3]) << 4 | convHexToDec(buf[4]);
	color->z = convHexToDec(buf[5]) << 4 | convHexToDec(buf[6]);
}

void ScriptParser::add_debug_level() {
	debug_level++;
}

void ScriptParser::errorAndCont(const char *str, const char *reason, const char *title, bool is_simple, bool force_message) {
	if (title == nullptr)
		title = "Parse Issue";
	script_h.processError(str, title, reason, true, is_simple, force_message);
}

void ScriptParser::errorAndExit(const char *str, const char *reason, const char *title, bool is_simple) {
	static std::atomic<int> nested;

	if (nested.fetch_add(1, std::memory_order_relaxed) == 0) {
		if (title == nullptr)
			title = "Parse Error";
		script_h.processError(str, title, reason, false, is_simple);
	}
}

bool ScriptParser::isBuiltInCommand(const char *cmd) {
	return func_lut.count(cmd[0] == '_' ? cmd + 1 : cmd);
}

int ScriptParser::evaluateCommand(const char *cmd, bool builtin, bool textgosub_flag, bool no_error) {
	if (builtin) {
		auto it = func_lut.find(cmd[0] == '_' ? cmd + 1 : cmd);
		if (it != func_lut.end())
			return (this->*it->second)();
	} else {
		copystr(script_h.current_cmd, cmd, sizeof(script_h.current_cmd));
		//Check against user-defined cmds
		if (cmd[0] >= 'a' && cmd[0] <= 'z') {
			auto it = user_func_lut.find(cmd);
			if (it != user_func_lut.end()) {
				if (it->second) {
#ifdef USE_LUA
					if (lua_handler.callFunction(false, cmd))
						errorAndExit(lua_handler.error_str, nullptr, "Lua Error");
#endif
				} else {
					gosubReal(cmd, script_h.getNext(), textgosub_flag);
				}
				return RET_CONTINUE;
			}
		}
	}

	if (no_error) {
		return RET_NOMATCH;
	}

	char error[4096];
	std::sprintf(error, "Failed to evaluate a command: %s builtin: %d", cmd, builtin);
	errorAndExit(error);

	return RET_CONTINUE;
}

int ScriptParser::parseLine() {
	const char *cmd = script_h.getStringBuffer();

	if (debug_level > 1 && *cmd != ':' && *cmd != '\n') {
		sendToLog(LogLevel::Info, "ScriptParser::Parseline %s\n", cmd);
		std::fflush(stdout);
	}

	script_h.current_cmd[0]   = '\0';
	script_h.current_cmd_type = ScriptHandler::CmdType::None;

	if (*cmd == ';')
		return RET_CONTINUE;
	if (*cmd == '*')
		return RET_CONTINUE;
	if (*cmd == ':')
		return RET_CONTINUE;

	auto hash = HashedString(cmd[0] == '_' ? cmd + 1 : cmd);

	if (ignored_func_lut.count(hash)) {
		script_h.readToEol();
		return RET_CONTINUE;
	}

	if (script_h.getStringBufferR().length() >= sizeof(script_h.current_cmd))
		errorAndExit("command overflow");
	copystr(script_h.current_cmd, cmd[0] == '_' ? cmd + 1 : cmd, sizeof(script_h.current_cmd));

	if (*cmd != '_') {
		//Check against user-defined cmds
		auto it = user_func_lut.find(hash);
		if (it != user_func_lut.end()) {
			if (it->second) {
#ifdef USE_LUA
				if (lua_handler.callFunction(false, cmd))
					errorAndExit(lua_handler.error_str, nullptr, "Lua Error");
#endif
			} else {
				gosubReal(cmd, script_h.getNext());
			}
			return RET_CONTINUE;
		}
	}

	//Check against builtin cmds
	auto it = func_lut.find(hash);
	if (it != func_lut.end())
		return (this->*it->second)();

	return RET_NOMATCH;
}

int ScriptParser::getSystemCallNo(const char *buffer) {
	if (equalstr(buffer, "skip"))
		return SYSTEM_SKIP;
	if (equalstr(buffer, "reset"))
		return SYSTEM_RESET;
	if (equalstr(buffer, "automode"))
		return SYSTEM_AUTOMODE;
	if (equalstr(buffer, "end"))
		return SYSTEM_END;
	if (equalstr(buffer, "sync"))
		return SYSTEM_SYNC;
	sendToLog(LogLevel::Warn, "Unsupported system call %s\n", buffer);
	return -1;
}

void ScriptParser::setArchivePath(const char *path) {
	archive_path = DirPaths(path);
	sendToLog(LogLevel::Info, "set:archive_path: \"%s\"\n", archive_path.getAllPaths().c_str());
}

void ScriptParser::setSavePath(const char *path) {
	if ((path == nullptr) || (*path == '\0') ||
	    (path[std::strlen(path) - 1] == DELIMITER)) {
		script_h.setStr(&script_h.save_path, path);
	} else {
		freearr(&script_h.save_path);
		script_h.save_path = new char[std::strlen(path) + 2];
		std::sprintf(script_h.save_path, "%s%c", path, DELIMITER);
	}

	if (!FileIO::accessFile(script_h.save_path, FileType::Directory) &&
		!FileIO::makeDir(script_h.save_path, true))
		errorAndExit("Failed to create missing save directory!");

	if (debug_level > 0) {
		sendToLog(LogLevel::Info, "setting save path to '%s'\n", script_h.save_path);
		if (debug_level > 1) {
			//dump the byte values (for debugging cmd-line codepage settings)
			sendToLog(LogLevel::Info, "save_path:");
			if (script_h.save_path)
				for (size_t i = 0, len = std::strlen(script_h.save_path); i < len; i++)
					sendToLog(LogLevel::Info, " %02x", static_cast<uint8_t>(script_h.save_path[i]));
			sendToLog(LogLevel::Info, "\n");
		}
	}
}

void ScriptParser::setNsaOffset(const char *off) {
	int offset = static_cast<int>(std::strtol(off, nullptr, 0));
	if (offset > 0)
		nsa_offset = offset;
}

void ScriptParser::saveGlovalData(bool no_error) {
	if (!globalon_flag)
		return;

	file_io_buf.clear();
	writeVariables(script_h.global_variable_border, VARIABLE_RANGE);

	if (saveFileIOBuf("gloval.sav") && !no_error) {
		char error[PATH_MAX * 2];
		snprintf(error, sizeof(error), "Can't open gloval.sav for writing.\nMake sure %s is writable!",
		         script_h.savedir ? script_h.savedir : script_h.save_path);
		errorAndExit(error, nullptr, "I/O Error", true);
	}
}

int ScriptParser::saveFileIOBuf(const char *filename) {
	// all files except envdata go in savedir
	bool usesavedir = !equalstr(filename, "envdata");

	//Mion: create a temporary file, to avoid overwriting valid files
	// (if an error occurs)
	const char *root = script_h.save_path;
	if (usesavedir && script_h.savedir)
		root = script_h.savedir;

	auto savefile = std::string(root) + filename;
	auto tmpfile  = savefile + ".tmp";
	if (!FileIO::writeFile(tmpfile, file_io_buf.data(), file_io_buf.size()) ||
	    !FileIO::renameFile(tmpfile, savefile, true))
		return -1;

	return 0;
}

int ScriptParser::loadFileIOBuf(const char *filename, bool savedata) {
	const char *root = nullptr;
	if (savedata)
		root = script_h.getSavePath(filename);

	if (!FileIO::readFile(filename, root, file_io_read_len, file_io_buf))
		return -1;

	if (file_io_read_len == 0)
		return -2;

	file_io_buf_ptr               = 0;
	file_io_buf[file_io_read_len] = 0;

	return 0;
}

void ScriptParser::write8s(int8_t i) {
	file_io_buf.emplace_back(i);
}

int8_t ScriptParser::read8s() {
	if (file_io_buf_ptr + sizeof(int8_t) > file_io_read_len)
		return 0;
	return static_cast<int8_t>(file_io_buf[file_io_buf_ptr++]);
}

void ScriptParser::write16s(int16_t i) {
	file_io_buf.emplace_back(i & 0xff);
	file_io_buf.emplace_back((i >> 8) & 0xff);
}

int16_t ScriptParser::read16s() {
	if (file_io_buf_ptr + sizeof(int16_t) > file_io_read_len)
		return 0;

	int16_t i =
	    static_cast<uint32_t>(file_io_buf[file_io_buf_ptr + 1]) << 8 |
	    static_cast<uint32_t>(file_io_buf[file_io_buf_ptr]);
	file_io_buf_ptr += sizeof(int16_t);

	return i;
}

void ScriptParser::write32s(int32_t i) {
	file_io_buf.emplace_back(i & 0xff);
	file_io_buf.emplace_back((i >> 8) & 0xff);
	file_io_buf.emplace_back((i >> 16) & 0xff);
	file_io_buf.emplace_back((i >> 24) & 0xff);
}

int32_t ScriptParser::read32s() {
	if (file_io_buf_ptr + sizeof(int16_t) > file_io_read_len)
		return 0;

	int32_t i =
	    static_cast<uint32_t>(file_io_buf[file_io_buf_ptr + 3]) << 24 |
	    static_cast<uint32_t>(file_io_buf[file_io_buf_ptr + 2]) << 16 |
	    static_cast<uint32_t>(file_io_buf[file_io_buf_ptr + 1]) << 8 |
	    static_cast<uint32_t>(file_io_buf[file_io_buf_ptr]);
	file_io_buf_ptr += sizeof(int32_t);

	return i;
}

void ScriptParser::write32u(uint32_t i) {
	ConvBytes c;
	c.u32 = i;
	write32s(c.i32);
}

uint32_t ScriptParser::read32u() {
	ConvBytes c;
	c.i32 = read32s();
	return c.u32;
}

void ScriptParser::writeFloat(float i) {
	ConvBytes c;
	c.f = i;
	write32s(c.i32);
}

float ScriptParser::readFloat() {
	ConvBytes c;
	c.i32 = read32s();
	return c.f;
}

void ScriptParser::writeStr(const char *s) {
	if (s && s[0]) {
		size_t len = std::strlen(s);
		file_io_buf.insert(file_io_buf.end(), reinterpret_cast<const uint8_t *>(&s[0]),
		                   reinterpret_cast<const uint8_t *>(&s[len]));
	}
	write8s(0);
}

void ScriptParser::readStr(char **s) {
	size_t counter = 0;

	while (file_io_buf_ptr + counter < file_io_read_len) {
		if (file_io_buf[file_io_buf_ptr + counter++] == 0)
			break;
	}

	freearr(s);

	if (counter > 1) {
		*s = new char[counter + 1];
		std::memcpy(*s, file_io_buf.data() + file_io_buf_ptr, counter);
		(*s)[counter] = 0;
	}
	file_io_buf_ptr += counter;
}

void ScriptParser::readFilePath(char **s) {
	readStr(s);
	if (*s)
		translatePathSlashes(*s);
}

void ScriptParser::writeVariables(uint32_t from, uint32_t to) {
	for (uint32_t i = from; i < to; i++) {
		write32s(script_h.getVariableData(i).num);
		writeStr(script_h.getVariableData(i).str);
	}
}

void ScriptParser::readVariables(uint32_t from, uint32_t to) {
	for (uint32_t i = from; i < to; i++) {
		script_h.getVariableData(i).num = read32s();
		readStr(&script_h.getVariableData(i).str);
	}
}

void ScriptParser::writeArrayVariable() {
	ArrayVariable *av = script_h.getRootArrayVariable();

	while (av) {
		int i, dim = 1;
		for (i = 0; i < av->num_dim; i++)
			dim *= av->dim[i];

		for (i = 0; i < dim; i++) {
			write32s(av->data[i]);
		}
		av = av->next;
	}
}

void ScriptParser::readArrayVariable() {
	ArrayVariable *av = script_h.getRootArrayVariable();

	while (av) {
		int i, dim = 1;
		for (i = 0; i < av->num_dim; i++)
			dim *= av->dim[i];

		for (i = 0; i < dim; i++) {
			av->data[i] = read32s();
		}
		av = av->next;
	}
}

void ScriptParser::writeLog(ScriptHandler::LogInfo &info) {
	file_io_buf.clear();

	char buf[10];

	std::sprintf(buf, "%zu", info.num_logs);
	for (size_t i = 0, len = std::strlen(buf); i < len; i++)
		write8s(buf[i]);
	write8s(0x0a);

	ScriptHandler::LogLink *cur = info.root_log.next;
	for (size_t i = 0; i < info.num_logs; i++) {
		write8s('"');
		for (size_t j = 0, len = std::strlen(cur->name); j < len; j++)
			write8s(cur->name[j] ^ 0x84);
		write8s('"');
		cur = cur->next;
	}

	if (saveFileIOBuf(info.filename)) {
		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "can't write to '%s'", info.filename);
		errorAndExit(script_h.errbuf, nullptr, "I/O Error");
	}
}

void ScriptParser::readLog(ScriptHandler::LogInfo &info) {
	script_h.resetLog(info);

	if (script_h.save_path && loadFileIOBuf(info.filename) == 0) {
		int i, ch, count = 0;
		char buf[100];

		while ((ch = read8s()) != 0x0a) {
			count = count * 10 + ch - '0';
		}

		for (i = 0; i < count; i++) {
			read8s();
			int j = 0;
			while ((ch = read8s()) != '"') buf[j++] = ch ^ 0x84;
			buf[j] = '\0';

			script_h.findAndAddLog(info, buf, true);
		}
	}
}

void ScriptParser::deleteNestInfo() {
	callStack.clear();
}

void ScriptParser::deleteLayerInfo() {
	while (layer_info) {
		LayerInfo *tmp = layer_info;
		layer_info     = layer_info->next;
		delete tmp;
	}
}

void ScriptParser::setCurrentLabel(const char *label) {
	current_label_info = script_h.lookupLabel(label);
	current_line       = script_h.getLineByAddress(current_label_info->start_address); // OPTIMIZE ME: this can probably be changed to = 0? check it
	script_h.setCurrent(current_label_info->start_address);
}

int ScriptParser::readEffect(EffectLink *effect) {
	int num = 1;

	effect->effect = script_h.readInt();
	if (script_h.hasMoreArgs()) {
		num++;
		effect->duration = script_h.readInt();
		if (script_h.hasMoreArgs()) {
			num++;
			const char *buf = script_h.readStr();
			effect->anim.setImageName(buf);
		} else
			effect->anim.remove();
	} else if (effect->effect < 0 || effect->effect > 255) {
		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "effect %d out of range, changing to 0", effect->effect);
		errorAndCont(script_h.errbuf);
		effect->effect = 0; // to suppress error
	}

	//sendToLog(LogLevel::Info, "readEffect %d: %d %d %s\n", num, effect->effect, effect->duration, effect->anim.image_name);
	return num;
}

ScriptParser::EffectLink *ScriptParser::parseEffect(bool init_flag) {
	//sendToLog(LogLevel::Info, "parseEffect start\n");

	if (init_flag)
		tmp_effect.anim.remove();

	int num = readEffect(&tmp_effect);

	if (num > 1)
		return &tmp_effect;
	if (tmp_effect.effect == 0 || tmp_effect.effect == 1)
		return &tmp_effect;

	for (auto &ef : effect_links) {
		if (ef.no == tmp_effect.effect)
			return &ef;
	}

	std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
	              "effect %d not found", tmp_effect.effect);
	errorAndExit(script_h.errbuf);

	//sendToLog(LogLevel::Info, "parseEffect return\n");
	return nullptr;
}

//Mion: for kinsoku
void ScriptParser::setKinsoku(const char *start_chrs, const char *end_chrs, bool add) {
	int num_start, num_end, i;
	const char *kchr;
	Kinsoku *tmp;

	// count chrs
	num_start = 0;
	kchr      = start_chrs;
	while (*kchr != '\0') {
		kchr++;
		num_start++;
	}

	num_end = 0;
	kchr    = end_chrs;
	while (*kchr != '\0') {
		kchr++;
		num_end++;
	}

	if (add) {
		if (start_kinsoku != nullptr)
			tmp = start_kinsoku;
		else {
			tmp               = new Kinsoku[1];
			num_start_kinsoku = 0;
		}
	} else {
		freearr(&start_kinsoku);
		tmp               = new Kinsoku[1];
		num_start_kinsoku = 0;
	}
	start_kinsoku = new Kinsoku[num_start_kinsoku + num_start];
	kchr          = start_chrs;
	for (i = 0; i < num_start_kinsoku + num_start; i++) {
		if (i < num_start_kinsoku)
			start_kinsoku[i].chr[0] = tmp[i].chr[0];
		else
			start_kinsoku[i].chr[0] = *kchr++;
		start_kinsoku[i].chr[1] = '\0';
	}
	num_start_kinsoku += num_start;
	freearr(&tmp);

	if (add) {
		if (end_kinsoku != nullptr)
			tmp = end_kinsoku;
		else {
			tmp             = new Kinsoku[1];
			num_end_kinsoku = 0;
		}
	} else {
		freearr(&end_kinsoku);
		tmp             = new Kinsoku[1];
		num_end_kinsoku = 0;
	}
	end_kinsoku = new Kinsoku[num_end_kinsoku + num_end];
	kchr        = end_chrs;
	for (i = 0; i < num_end_kinsoku + num_end; i++) {
		if (i < num_end_kinsoku)
			end_kinsoku[i].chr[0] = tmp[i].chr[0];
		else
			end_kinsoku[i].chr[0] = *kchr++;
		end_kinsoku[i].chr[1] = '\0';
	}
	num_end_kinsoku += num_end;
	freearr(&tmp);
}

bool ScriptParser::isStartKinsoku(const char *str) {
	for (int i = 0; i < num_start_kinsoku; i++) {
		if ((start_kinsoku[i].chr[0] == *str) &&
		    (start_kinsoku[i].chr[1] == *(str + 1)))
			return true;
	}
	return false;
}

bool ScriptParser::isEndKinsoku(const char *str) {
	for (int i = 0; i < num_end_kinsoku; i++) {
		if ((end_kinsoku[i].chr[0] == *str) &&
		    (end_kinsoku[i].chr[1] == *(str + 1)))
			return true;
	}
	return false;
}

void ScriptParser::setVariableQueue(bool state, std::string cmd) {
	if (state == variableQueueEnabled) {
		errorAndExit("Variable queue is already using the same mode");
		return; //dummy
	}

	if (!state && !variableQueue.empty()) {
		// Uncomment this for debugging purposes
		//	errorAndCont("Variable queue has unused args");
		while (!variableQueue.empty()) variableQueue.pop();
	}

	if (!state) {
		variableQueueCommand = "";
	}

	if (state)
		variableQueueCommand = std::move(cmd);

	variableQueueEnabled = state;
}
