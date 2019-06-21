/**
 *  ParserCommand.cpp
 *  ONScripter-RU
 *
 *  Define command executer.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/Parser.hpp"
#include "Engine/Layers/Furu.hpp"
#include "Engine/Layers/Media.hpp"
#include "Engine/Layers/ObjectFall.hpp"
#include "Engine/Layers/Subtitle.hpp"
#include "Engine/Components/Dialogue.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Readers/Nsa.hpp"
#include "Support/Unicode.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <cmath>

int ScriptParser::zenkakkoCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("zenkakko: not in the define section");
	script_h.setZenkakko(true);

	return RET_CONTINUE;
}

int ScriptParser::windowchipCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("windowchip: not in the define section");
	windowchip_sprite_no = script_h.readInt();

	return RET_CONTINUE;
}

int ScriptParser::windowbackCommand() {
	errorAndExit("windowback is a mad idea, in implementation and in model. Remove windowback and use the commands 'humanz', 'hudz', and 'windowz' to control the z position of standing sprites, HUD, and text window respectively.");

	return RET_CONTINUE;
}

int ScriptParser::versionstrCommand() {
	delete[] version_str;

	script_h.readStr();
	const char *save_buf = script_h.saveStringBuffer();

	const char *buf = script_h.readStr();
	version_str     = new char[std::strlen(save_buf) + std::strlen(buf) + std::strlen("\n") * 2 + 1];
	std::sprintf(version_str, "%s\n%s\n", save_buf, buf);

	return RET_CONTINUE;
}

int ScriptParser::usewheelCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("usewheel: not in the define section");

	usewheel_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::useescspcCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("useescspc: not in the define section");

	if (!force_button_shortcut_flag)
		useescspc_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::uninterruptibleCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("uninterruptible: not in the define section");

	uninterruptibleLabels.clear();

	const char *label;

	do {
		label = script_h.readLabel();
		uninterruptibleLabels.insert(script_h.lookupLabel(label + 1)->start_address);
	} while (script_h.hasMoreArgs());

	return RET_CONTINUE;
}

int ScriptParser::underlineCommand() {
	underline_value = script_h.readInt();

	return RET_CONTINUE;
}

int ScriptParser::transmodeCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("transmode: not in the define section");

	if (script_h.compareString("leftup"))
		trans_mode = AnimationInfo::TRANS_TOPLEFT;
	else if (script_h.compareString("copy"))
		trans_mode = AnimationInfo::TRANS_COPY;
	else if (script_h.compareString("alpha"))
		trans_mode = AnimationInfo::TRANS_ALPHA;
	else if (script_h.compareString("righttup"))
		trans_mode = AnimationInfo::TRANS_TOPRIGHT;
	script_h.readName();

	return RET_CONTINUE;
}

int ScriptParser::timeStampCommand() {
	time_t t = time(nullptr);
	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, static_cast<int32_t>(t));

	return RET_CONTINUE;
}

int ScriptParser::timeCommand() {
	time_t t = time(nullptr);
	tm *tm   = localtime(&t);

	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, tm->tm_hour);

	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, tm->tm_min);

	if (script_h.hasMoreArgs()) {
		script_h.readVariable();
		script_h.setInt(&script_h.current_variable, tm->tm_sec);
	}

	return RET_CONTINUE;
}

int ScriptParser::textgosubCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("textgosub: not in the define section");

	script_h.setStr(&textgosub_label, script_h.readLabel() + 1);
	script_h.enableTextgosub(true);

	return RET_CONTINUE;
}

int ScriptParser::skipgosubCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("skipgosub: not in the define section");

	if (!textgosub_label)
		errorAndExit("skipgosub: no textgosub label");

	script_h.setStr(&skipgosub_label, script_h.readLabel() + 1);

	return RET_CONTINUE;
}

int ScriptParser::tanCommand() {
	script_h.readInt();
	script_h.pushVariable();

	int val = script_h.readInt();
	script_h.setInt(&script_h.pushed_variable, static_cast<int>(std::tan(M_PI * val / 180.0) * 1000.0));

	return RET_CONTINUE;
}

int ScriptParser::subCommand() {
	int val1 = script_h.readInt();
	script_h.pushVariable();

	int val2 = script_h.readInt();
	script_h.setInt(&script_h.pushed_variable, val1 - val2);

	return RET_CONTINUE;
}

int ScriptParser::straliasCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("stralias: not in the define section");

	script_h.readName();
	const char *save_buf = script_h.saveStringBuffer();
	const char *buf      = script_h.readStr();

	script_h.addStrAlias(save_buf, buf);

	return RET_CONTINUE;
}

int ScriptParser::soundpressplginCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("soundpressplgin: not in the define section");

	const char *buf = script_h.readStr();
	int buf_len     = static_cast<int>(std::strlen(buf));
	char buf2[1024];
	if (buf_len + 1 > 1024)
		return RET_NOMATCH;
	copystr(buf2, buf, sizeof(buf2));

	// only nbzplgin.dll and jpgplgin.dll are "supported"
	for (int i = 0; i < 12; i++)
		if (buf2[i] >= 'A' && buf2[i] <= 'Z')
			buf2[i] += 'a' - 'A';
	if (std::strncmp(buf2, "nbzplgin.dll", 12) != 0 && std::strncmp(buf2, "jpgplgin.dll", 12) != 0) {
		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "soundpressplgin: plugin %s is not available.", buf);
		errorAndCont(script_h.errbuf);
		return RET_CONTINUE;
	}

	while (*buf && *buf != '|') buf++;
	if (*buf == 0)
		return RET_CONTINUE;

	buf++;

	return RET_CONTINUE;
}

int ScriptParser::skipCommand() {
	int line = current_label_info->start_line + current_line + script_h.readInt();

	const char *buf    = script_h.getAddressByLine(line);
	current_label_info = script_h.getLabelByAddress(buf);
	current_line       = script_h.getLineByAddress(buf, current_label_info);

	script_h.setCurrent(buf);

	return RET_CONTINUE;
}

int ScriptParser::sinCommand() {
	script_h.readInt();
	script_h.pushVariable();

	int val = script_h.readInt();
	script_h.setInt(&script_h.pushed_variable, static_cast<int>(std::sin(M_PI * val / 180.0) * 1000.0));

	return RET_CONTINUE;
}

int ScriptParser::shadedistanceCommand() {
	int x = script_h.readInt();
	int y = script_h.readInt();

	Fontinfo *fi = &sentence_font;

	if (script_h.hasMoreArgs()) {
		bool is_colour;
		const char *buf;

		buf = script_h.readColor(&is_colour);

		if (script_h.hasMoreArgs())
			fi = &name_font;

		if (is_colour)
			readColor(&fi->changeStyle().shadow_color, buf);
	}

	fi->changeStyle().shadow_distance[0] = x;
	fi->changeStyle().shadow_distance[1] = y;

	fi->changeStyle().is_shadow = !(fi->style().shadow_distance[0] == 0 &&
	                                fi->style().shadow_distance[1] == 0);

	if (fi == &name_font)
		script_h.readInt();

	return RET_CONTINUE;
}

int ScriptParser::borderstyleCommand() {
	int border   = script_h.readInt() * 25;
	Fontinfo *fi = &sentence_font;
	if (border <= 0) {
		if (script_h.hasMoreArgs()) {
			fi = &name_font;
			script_h.readInt();
		}
		fi->changeStyle().is_border    = false;
		fi->changeStyle().border_width = 0;
		return RET_CONTINUE;
	}

	if (script_h.hasMoreArgs()) {
		bool is_colour;
		const char *buf;

		buf = script_h.readColor(&is_colour);

		if (script_h.hasMoreArgs())
			fi = &name_font;

		if (is_colour)
			readColor(&fi->changeStyle().border_color, buf);
	}

	fi->changeStyle().is_border    = true;
	fi->changeStyle().border_width = border;

	if (fi == &name_font)
		script_h.readInt();

	return RET_CONTINUE;
}

//Mion
int ScriptParser::setlayerCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("setlayer: not in the define section");

	int no          = script_h.readInt();
	int interval    = script_h.readInt();
	const char *dll = script_h.readStr();

	Layer *handler     = nullptr;
	const char *bslash = std::strrchr(dll, '\\');
	if ((bslash && !std::strncmp(bslash + 1, "snow.dll", 8)) ||
	    !std::strncmp(dll, "snow.dll", 8)) {
		handler = new FuruLayer(window.script_width, window.script_height, false, &script_h.reader);
	} else if ((bslash && !std::strncmp(bslash + 1, "hana.dll", 8)) ||
	           !std::strncmp(dll, "hana.dll", 8)) {
		handler = new FuruLayer(window.script_width, window.script_height, true, &script_h.reader);
	} else if ((bslash && !std::strncmp(bslash + 1, "video.dll", 9)) ||
	           !std::strncmp(dll, "video.dll", 9)) {
		if (video_layer >= 0) {
			errorAndCont("You have already created video layer");
			return RET_CONTINUE;
		}

		handler     = new MediaLayer(window.script_width, window.script_height, &script_h.reader);
		video_layer = no;
	} else if ((bslash && !std::strncmp(bslash + 1, "fall.dll", 8)) ||
	           !std::strncmp(dll, "fall.dll", 8)) {
		handler = new ObjectFallLayer(window.script_width, window.script_height);
	} else if ((bslash && !std::strncmp(bslash + 1, "ass.dll", 7)) ||
	           !std::strncmp(dll, "ass.dll", 7)) {
		handler = new SubtitleLayer(window.script_width, window.script_height, &script_h.reader);
	} else {
		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "setlayer: layer effect '%s' is not implemented.", dll);
		errorAndCont(script_h.errbuf);
		return RET_CONTINUE;
	}

	auto props = handler->properties();
	for (auto &&prop : props)
		dynamicProperties.registerProperty(prop.first, std::move(prop.second));

	sendToLog(LogLevel::Info, "Setup layer effect for '%s'.\n", dll);
	LayerInfo *layer = new LayerInfo();
	layer->num       = no;
	layer->interval  = interval;
	layer->handler   = std::unique_ptr<Layer>(handler);
	layer->next      = layer_info;
	layer_info       = layer;

	return RET_CONTINUE;
}

//Mion: for kinsoku
int ScriptParser::setkinsokuCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("setkinsoku: not in the define section");

	script_h.readStr();
	const char *start = script_h.saveStringBuffer();
	const char *end   = script_h.readStr();
	setKinsoku(start, end, false);
	if (debug_level > 0)
		sendToLog(LogLevel::Info, "setkinsoku: \"%s\",\"%s\"\n", start, end);

	return RET_CONTINUE;
}

int ScriptParser::selectvoiceCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("selectvoice: not in the define section");

	for (auto &i : selectvoice_file_name)
		script_h.setStr(&i, script_h.readFilePath());

	return RET_CONTINUE;
}

int ScriptParser::selectcolorCommand() {
	const char *buf = readColorStr();
	readColor(&sentence_font.on_color, buf);

	buf = readColorStr();
	readColor(&sentence_font.off_color, buf);

	return RET_CONTINUE;
}

int ScriptParser::savenumberCommand() {
	num_save_file = script_h.readInt();

	return RET_CONTINUE;
}

int ScriptParser::savenameCommand() {
	errorAndExit("savename: without a response, your voice echoes in the darkness");

	return RET_CONTINUE;
}

int ScriptParser::savedirCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("savedir: not in the define section");

	const char *buf = script_h.readFilePath();

	// Only allow setting the savedir once, no empty path
	if ((*buf != '\0') && (!savedir)) {
		// Note that savedir is relative to save_path
		script_h.setStr(&savedir, buf);
		script_h.setSavedir(buf);
	}

	return RET_CONTINUE;
}

int ScriptParser::rubyonCommand() {
	errorAndExit("rubyon / rubyon2 are currently unsupported for new dialogue model.");
	return RET_CONTINUE;
}

int ScriptParser::rubyoffCommand() {
	errorAndExit("rubyoff is currently unsupported for new dialogue model.");
	return RET_CONTINUE;
}

int ScriptParser::roffCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("roff: not in the define section");
	rmode_flag = false;

	return RET_CONTINUE;
}

int ScriptParser::rmenuCommand() {
	errorAndExit("rmenu: this command is no more supported in ONScripter-RU");

	return RET_CONTINUE;
}

int ScriptParser::rgosubCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("rgosub: not in the define section");

	errorAndExit("rgosub: implement this manually");

	return RET_CONTINUE;
}

int ScriptParser::eventCallbackCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("event_callback: not in the define section");

	script_h.setStr(&event_callback_label, script_h.readLabel() + 1);

	return RET_CONTINUE;
}

int ScriptParser::returnCommand() {
	if (callStack.empty() || callStack.back().nest_mode != NestInfo::LABEL)
		errorAndExit("return: not in gosub");

	auto &b = callStack.back();

	current_label_info = b.label ? b.label : script_h.getLabelByAddress(b.next_script);
	current_line       = b.line >= 0 ? b.line : script_h.getLineByAddress(b.next_script, current_label_info);

	const char *label = script_h.readStr();
	if (label[0] != '*')
		script_h.setCurrent(b.next_script);
	else
		setCurrentLabel(label + 1);

	bool textgosub_flag = b.textgosub_flag;

	int ret = RET_CONTINUE;
	// Hook for alerting dialogueController to returns from dialogue inline commands
	if (b.dialogueEventOnReturn) {
		//sendToLog(LogLevel::Info, "returnCommand dialogue event\n");
		dlgCtrl.events.emplace();
		dlgCtrl.events.back().dialogueInlineCommandEnd = true;
		ret                                            = RET_NO_READ;
	}
	if (b.noReadOnReturn) {
		ret = RET_NO_READ;
	}

	callStack.pop_back();
	callStackHasUninterruptible = std::any_of(callStack.begin(), callStack.end(), [](NestInfo &n) { return n.uninterruptible; });

	if (textgosub_flag) {
		string_buffer_offset = script_h.popStringBuffer();
		if (script_h.getStringBuffer()[string_buffer_offset] != 0)
			return RET_NO_READ;

		errorAndExit("RET_EOT, this should not happen");
		//return ret==RET_NO_READ?ret:(RET_CONTINUE | RET_EOT);
	}

	return ret;
}

int ScriptParser::pretextgosubCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("pretextgosub: not in the define section");

	script_h.setStr(&pretextgosub_label, script_h.readStr() + 1);

	return RET_CONTINUE;
}

int ScriptParser::pagetagCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("pagetag: not in the define section");

	pagetag_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::numaliasCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("numalias: numalias: not in the define section");

	script_h.readName();
	const char *save_buf = script_h.saveStringBuffer();

	int no = script_h.readInt();
	script_h.addNumAlias(save_buf, no);

	return RET_CONTINUE;
}

int ScriptParser::nsadirCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("nsadir: not in the define section");

	const char *buf = script_h.readFilePath();

	nsa_path = DirPaths(buf);

	delete script_h.reader;
	script_h.reader = new NsaReader(archive_path, nsa_offset);
	if (script_h.reader->open(nsa_path.getAllPaths().c_str())) {
		errorAndCont("nsadir: couldn't open any NSA archives");
	}

	return RET_CONTINUE;
}

int ScriptParser::nsaCommand() {
	//Mion: WARNING - commands "ns2" and "ns3" have nothing to do with
	// archive files named "*.ns2", they are for "*.nsa" files.
	// I suggest using command-line options "--nsa-offset 1" and
	// "--nsa-offset 2" instead of these commands
	if (script_h.isName("ns2")) {
		nsa_offset = 1;
	} else if (script_h.isName("ns3")) {
		nsa_offset = 2;
	}

	delete script_h.reader;
	script_h.reader = new NsaReader(archive_path, nsa_offset);
	if (script_h.reader->open(nsa_path.getAllPaths().c_str())) {
		errorAndCont("nsa: couldn't open any NSA archives");
	}

	return RET_CONTINUE;
}

int ScriptParser::nextCommand() {
	//Mion: apparently NScr allows 'break' outside of a for loop, it just skips ahead to 'next'
	if (callStack.empty() || callStack.back().nest_mode != NestInfo::FOR) {
		errorAndCont("next: not in for loop\n");
		break_flag = false;
		return RET_CONTINUE;
	}

	auto &b = callStack.back();

	int val;
	if (!break_flag) {
		val = script_h.getVariableData(b.var_no).num;
		script_h.setNumVariable(b.var_no, val + b.step);
	}

	val = script_h.getVariableData(b.var_no).num;

	if (break_flag ||
	    ((b.step > 0) && (val > b.to)) ||
	    ((b.step < 0) && (val < b.to))) {
		break_flag = false;
		callStack.pop_back();
	} else {
		script_h.setCurrent(b.next_script);
		current_label_info =
		    script_h.getLabelByAddress(b.next_script);
		current_line =
		    script_h.getLineByAddress(b.next_script, current_label_info);
	}

	return RET_CONTINUE;
}

int ScriptParser::mulCommand() {
	int val1 = script_h.readInt();
	script_h.pushVariable();

	int val2 = script_h.readInt();
	script_h.setInt(&script_h.pushed_variable, val1 * val2);

	return RET_CONTINUE;
}

int ScriptParser::movCommand() {
	if (script_h.isName("movs")) {
		script_h.readVariable();

		if (script_h.current_variable.type != VariableInfo::TypeStr)
			errorAndExit("First variable should be string!");

		script_h.pushVariable();
		const char *buf = script_h.readStr();

		if (equalstr(buf, "LF")) {
			script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, "\n");
		} else if (equalstr(buf, "QT")) {
			script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, R"(")");
		} else {
			errorAndExit("Incorrect 2nd parameter!");
		}

		return RET_CONTINUE;
	}

	int count = 1;

	if (script_h.isName("mov10")) {
		count = 10;
	} else if (script_h.isName("movl")) {
		count = -1; // infinite
	} else if (script_h.getStringBuffer()[3] >= '3' && script_h.getStringBuffer()[3] <= '9') {
		count = script_h.getStringBuffer()[3] - '0';
	}

	script_h.readVariable();

	if (script_h.current_variable.type == VariableInfo::TypeInt ||
	    script_h.current_variable.type == VariableInfo::TypeArray) {
		script_h.pushVariable();
		bool loop_flag = (script_h.hasMoreArgs());
		int i          = 0;
		while ((count == -1 || i < count) && loop_flag) {
			int no    = script_h.readInt();
			loop_flag = (script_h.hasMoreArgs());
			script_h.setInt(&script_h.pushed_variable, no, i++);
		}
	} else if (script_h.current_variable.type == VariableInfo::TypeStr) {
		script_h.pushVariable();
		const char *buf = script_h.readStr();
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, buf);
	} else
		errorAndExit("mov: no variable");

	return RET_CONTINUE;
}

int ScriptParser::mode_wave_demoCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("mode_wave_demo: not in the define section");
	mode_wave_demo_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::mode_sayaCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("mode_saya: not in the define section");
	mode_saya_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::mode_extCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("mode_ext: not in the define section");
	mode_ext_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::modCommand() {
	int val1 = script_h.readInt();
	script_h.pushVariable();

	int val2 = script_h.readInt();
	script_h.setInt(&script_h.pushed_variable, val1 % val2);

	return RET_CONTINUE;
}

int ScriptParser::midCommand() {
	script_h.readVariable();
	if (script_h.current_variable.type != VariableInfo::TypeStr)
		errorAndExit("mid: no string variable");
	int no = script_h.current_variable.var_no;

	auto wstr    = decodeUTF8StringShort(script_h.readStr());
	size_t start = script_h.readInt();
	size_t len   = script_h.readInt();

	script_h.setStr(&script_h.getVariableData(no).str,
	                decodeUTF16String(wstr.substr(start, len)).c_str());

	return RET_CONTINUE;
}

int ScriptParser::menusetwindowCommand() {
	errorAndExit("menusetwindow: Gone with the wind in ONScripter-RU");

	return RET_CONTINUE;
}

int ScriptParser::menuselectvoiceCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("menuselectvoice: not in the define section");

	errorAndExit("menuselectvoice: don't worry, it will not work anyway");

	return RET_CONTINUE;
}

int ScriptParser::menuselectcolorCommand() {
	errorAndExit("menuselectcolor: killed with gentle");

	return RET_CONTINUE;
}

int ScriptParser::maxkaisoupageCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("maxkaisoupage: not in the define section");

	errorAndExit("maxkaisoupage: not in the memory section");

	return RET_CONTINUE;
}

int ScriptParser::luasubCommand() {
	const char *cmd = script_h.readName();

	if (cmd[0] >= 'a' && cmd[0] <= 'z') {
		user_func_lut.emplace(HashedString(cmd, true), true);
	}

	return RET_CONTINUE;
}

int ScriptParser::luacallCommand() {
#ifdef USE_LUA
	const char *label = nullptr;
	label             = script_h.readLabel();
#else
	script_h.readLabel();
#endif

#ifdef USE_LUA
	lua_handler.addCallback(label);
#endif

	return RET_CONTINUE;
}

int ScriptParser::lookbackspCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("lookbacksp: not in the define section");

	for (int &i : lookback_sp)
		i = script_h.readInt();

	if (filelog_flag) {
		script_h.findAndAddLog(script_h.log_info[ScriptHandler::FILE_LOG], DEFAULT_LOOKBACK_NAME0, true);
		script_h.findAndAddLog(script_h.log_info[ScriptHandler::FILE_LOG], DEFAULT_LOOKBACK_NAME1, true);
		script_h.findAndAddLog(script_h.log_info[ScriptHandler::FILE_LOG], DEFAULT_LOOKBACK_NAME2, true);
		script_h.findAndAddLog(script_h.log_info[ScriptHandler::FILE_LOG], DEFAULT_LOOKBACK_NAME3, true);
	}

	return RET_CONTINUE;
}

int ScriptParser::lookbackcolorCommand() {
	const char *buf = readColorStr();
	readColor(&lookback_color, buf);

	return RET_CONTINUE;
}

int ScriptParser::loadgosubCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("loadgosub: not in the define section");

	script_h.setStr(&loadgosub_label, script_h.readStr() + 1);

	return RET_CONTINUE;
}

int ScriptParser::linepageCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("linepage: not in the define section");

	if (script_h.isName("linepage2")) {
		linepage_mode = 2;
		clickstr_line = script_h.readInt();
	} else
		linepage_mode = 1;

	script_h.setLinepage(true);

	return RET_CONTINUE;
}

int ScriptParser::lenCommand() {
	script_h.readInt();
	script_h.pushVariable();

	const char *buf = script_h.readStr();

	script_h.setInt(&script_h.pushed_variable, static_cast<int32_t>(std::strlen(buf)));

	return RET_CONTINUE;
}

int ScriptParser::labellogCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("labellog: not in the define section");

	labellog_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::labelexistCommand() {
	script_h.readInt();
	script_h.pushVariable();
	bool exists = script_h.hasLabel(script_h.readLabel() + 1);

	script_h.setInt(&script_h.pushed_variable, exists ? 1 : 0);

	return RET_CONTINUE;
}

int ScriptParser::kidokuskipCommand() {
	kidokuskip_flag = true;
	kidokumode_flag = true;
	script_h.loadKidokuData();

	return RET_CONTINUE;
}

int ScriptParser::kidokumodeCommand() {
	kidokumode_flag = script_h.readInt() == 1;

	return RET_CONTINUE;
}

int ScriptParser::itoaCommand() {
	bool itoa2_flag = false;

	if (script_h.isName("itoa2"))
		itoa2_flag = true;

	script_h.readVariable();
	if (script_h.current_variable.type != VariableInfo::TypeStr)
		errorAndExit("itoa: no string variable.");
	int no = script_h.current_variable.var_no;

	int val = script_h.readInt();

	char val_str[20];
	if (itoa2_flag)
		script_h.getStringFromInteger(val_str, val, -1, false, true);
	else
		std::sprintf(val_str, "%d", val);
	script_h.setStr(&script_h.getVariableData(no).str, val_str);

	return RET_CONTINUE;
}

int ScriptParser::intlimitCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("intlimit: not in the define section");

	int no = script_h.readInt();

	script_h.getVariableData(no).num_limit_flag  = true;
	script_h.getVariableData(no).num_limit_lower = script_h.readInt();
	script_h.getVariableData(no).num_limit_upper = script_h.readInt();

	return RET_CONTINUE;
}

int ScriptParser::incCommand() {
	int val = script_h.readInt();
	script_h.setInt(&script_h.current_variable, val + 1);

	return RET_CONTINUE;
}

int ScriptParser::ifCommand() {
	//sendToLog(LogLevel::Info, "ifCommand\n");
	int condition_status = 0; // 0 ... none, 1 ... and, 2 ... or
	bool f = false, condition_flag = false;
	const char *op_buf, *buf;

	bool if_flag = true;
	if (script_h.isName("notif"))
		if_flag = false;

	while (true) {
		if (script_h.compareString("fchk")) {
			script_h.readName();
			buf = script_h.readStr();
			if (*buf == '\0')
				f = false;
			else {
				if (filelog_flag)
					f = (script_h.findAndAddLog(script_h.log_info[ScriptHandler::FILE_LOG], buf, false) != nullptr);
				else
					errorAndExit("filelog command is not called but file logging is requested");
				//sendToLog(LogLevel::Info, "fchk %s(%d) ", tmp_string_buffer, (findAndAddFileLog(tmp_string_buffer, fasle)));
			}
		} else if (script_h.compareString("lchk")) {
			script_h.readName();
			buf = script_h.readLabel();
			if (*buf == '\0')
				f = false;
			else {
				if (labellog_flag)
					f = (script_h.findAndAddLog(script_h.log_info[ScriptHandler::LABEL_LOG], buf + 1, false) != nullptr);
				else
					errorAndExit("labellog command is not called but label logging is requested");
				//sendToLog(LogLevel::Info, "lchk %s (%d)\n", buf, f);
			}
		} else {
			int no = script_h.readInt();
			if (script_h.current_variable.type & VariableInfo::TypeInt ||
			    script_h.current_variable.type & VariableInfo::TypeArray) {
				int left_value = no;
				//sendToLog(LogLevel::Info, "left (%d) ", left_value);

				op_buf = script_h.getNext();
				if ((op_buf[0] == '>' && op_buf[1] == '=') ||
				    (op_buf[0] == '<' && op_buf[1] == '=') ||
				    (op_buf[0] == '=' && op_buf[1] == '=') ||
				    (op_buf[0] == '!' && op_buf[1] == '=') ||
				    (op_buf[0] == '<' && op_buf[1] == '>'))
					script_h.setCurrent(op_buf + 2);
				else if (op_buf[0] == '<' ||
				         op_buf[0] == '>' ||
				         op_buf[0] == '=')
					script_h.setCurrent(op_buf + 1);
				//sendToLog(LogLevel::Info, "current %c%c ", op_buf[0], op_buf[1]);

				int right_value = script_h.readInt();
				//sendToLog(LogLevel::Info, "right (%d) ", right_value);

				if (op_buf[0] == '>' && op_buf[1] == '=')
					f = (left_value >= right_value);
				else if (op_buf[0] == '<' && op_buf[1] == '=')
					f = (left_value <= right_value);
				else if (op_buf[0] == '=' && op_buf[1] == '=')
					f = (left_value == right_value);
				else if (op_buf[0] == '!' && op_buf[1] == '=')
					f = (left_value != right_value);
				else if (op_buf[0] == '<' && op_buf[1] == '>')
					f = (left_value != right_value);
				else if (op_buf[0] == '<')
					f = (left_value < right_value);
				else if (op_buf[0] == '>')
					f = (left_value > right_value);
				else if (op_buf[0] == '=')
					f = (left_value == right_value);
			} else {
				script_h.setCurrent(script_h.getCurrent());
				script_h.readStr();
				const char *save_buf = script_h.saveStringBuffer();

				op_buf = script_h.getNext();

				if ((op_buf[0] == '>' && op_buf[1] == '=') ||
				    (op_buf[0] == '<' && op_buf[1] == '=') ||
				    (op_buf[0] == '=' && op_buf[1] == '=') ||
				    (op_buf[0] == '!' && op_buf[1] == '=') ||
				    (op_buf[0] == '<' && op_buf[1] == '>'))
					script_h.setCurrent(op_buf + 2);
				else if (op_buf[0] == '<' ||
				         op_buf[0] == '>' ||
				         op_buf[0] == '=')
					script_h.setCurrent(op_buf + 1);

				buf = script_h.readStr();

				int val = std::strcmp(save_buf, buf);
				if (op_buf[0] == '>' && op_buf[1] == '=')
					f = (val >= 0);
				else if (op_buf[0] == '<' && op_buf[1] == '=')
					f = (val <= 0);
				else if (op_buf[0] == '=' && op_buf[1] == '=')
					f = (val == 0);
				else if (op_buf[0] == '!' && op_buf[1] == '=')
					f = (val != 0);
				else if (op_buf[0] == '<' && op_buf[1] == '>')
					f = (val != 0);
				else if (op_buf[0] == '<')
					f = (val < 0);
				else if (op_buf[0] == '>')
					f = (val > 0);
				else if (op_buf[0] == '=')
					f = (val == 0);
			}
		}

		f = if_flag == f;
		condition_flag |= f;
		op_buf = script_h.getNext();
		if (op_buf[0] == '|') {
			if (condition_status == 1)
				errorAndExit("if: using & and | at the same time is not supported.");
			while (*op_buf == '|') op_buf++;
			script_h.setCurrent(op_buf);
			condition_status = 2;
			continue;
		}

		if ((condition_status == 2 && !condition_flag) ||
		    (condition_status != 2 && !f))
			return RET_SKIP_LINE;

		if (op_buf[0] == '&') {
			if (condition_status == 2)
				errorAndExit("if: using & and | at the same time is not supported.");
			while (*op_buf == '&') op_buf++;
			script_h.setCurrent(op_buf);
			condition_status = 1;
			continue;
		}
		break;
	};

	/* Execute command */
	return RET_CONTINUE;
}

int ScriptParser::spritesetzCommand() {
	int spriteset_no                 = script_h.readInt();
	int z                            = script_h.readInt();
	z_order_spritesets[spriteset_no] = z;
	return RET_CONTINUE;
}

int ScriptParser::humanzCommand() {
	z_order_ld = script_h.readInt();
	return RET_CONTINUE;
}

int ScriptParser::ignoreCommandCommand() {
	if (script_h.isName("ignore_cmd_clear")) {
		ignored_func_lut.clear();
	} else if (script_h.isName("ignore_inl_cmd_clear")) {
		ignored_inline_func_lut.clear();
	} else if (script_h.isName("ignore_inl_cmd")) {
		do {
			ignored_inline_func_lut.emplace(HashedString(script_h.readStr(), true));
		} while (script_h.hasMoreArgs());
	} else {
		do {
			ignored_func_lut.emplace(HashedString(script_h.readStr(), true));
		} while (script_h.hasMoreArgs());
	}

	return RET_CONTINUE;
}

int ScriptParser::hudzCommand() {
	z_order_hud = script_h.readInt();
	return RET_CONTINUE;
}

int ScriptParser::windowzCommand() {
	z_order_window = script_h.readInt();
	return RET_CONTINUE;
}

int ScriptParser::textzCommand() {
	z_order_text = script_h.readInt();
	return RET_CONTINUE;
}

int ScriptParser::humanposCommand() {
	for (int &humanpo : humanpos)
		humanpo = script_h.readInt();

	return RET_CONTINUE;
}

int ScriptParser::gotoCommand() {
	setCurrentLabel(script_h.readLabel() + 1);

	return RET_CONTINUE;
}

void ScriptParser::gosubReal(const char *label, const char *next_script,
                             bool textgosub_flag) {
	callStack.emplace_back();
	auto &b       = callStack.back();
	b.next_script = next_script;
	b.label       = current_label_info;
	b.line        = current_line;

	if (textgosub_flag) {
		script_h.pushStringBuffer(string_buffer_offset);
		b.textgosub_flag = true;
	}

	setCurrentLabel(label);

	if (uninterruptibleLabels.count(script_h.getCurrent())) {
		callStackHasUninterruptible = true;
		b.uninterruptible           = true;
	}
}

int ScriptParser::gosubCommand() {
	const char *buf = script_h.readLabel();
	gosubReal(buf + 1, script_h.getNext());

	return RET_CONTINUE;
}

int ScriptParser::globalonCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("globalon: not in the define section");
	globalon_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::getparamCommand() {
	if (callStack.empty() || callStack.back().nest_mode != NestInfo::LABEL)
		errorAndExit("getparam: not in a subroutine");

	if (inVariableQueueSubroutine)
		setVariableQueue(true);

	auto &b = callStack.back();

	int end_status;
	do {
		script_h.readVariable();
		script_h.pushVariable();

		/*auto cur = b.next_script;
		auto firstRN0 = strpbrk(cur, "\r\n\0");
		int eol = firstRN0 ? firstRN0 - cur : 0;
		sendToLog(LogLevel::Info, "getparam's next_script: \"%.*s\"\n", eol, cur);*/

		script_h.pushCurrent(b.next_script);

		end_status = script_h.getEndStatus();

		if (script_h.pushed_variable.type & VariableInfo::TypePtr) {
			script_h.readVariable();
			script_h.setInt(&script_h.pushed_variable, script_h.current_variable.var_no);
		} else if (script_h.pushed_variable.type & VariableInfo::TypeInt ||
		           script_h.pushed_variable.type & VariableInfo::TypeArray) {
			script_h.setInt(&script_h.pushed_variable, script_h.readInt());
		} else if (script_h.pushed_variable.type & VariableInfo::TypeStr) {
			const char *buf = script_h.readStr();
			script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, buf);
		}

		b.next_script = script_h.getNext();

		script_h.popCurrent();
	} while (end_status & ScriptHandler::END_COMMA);

	if (inVariableQueueSubroutine) {
		setVariableQueue(false);
		inVariableQueueSubroutine = false; // We are done with main function params, don't confuse later ones
	}

	return RET_CONTINUE;
}

int ScriptParser::getStraliasCommand() {
	script_h.readStr();
	script_h.pushVariable();
	const char *buf = script_h.readStr();

	std::string alias_data;

	if (!script_h.findStrAlias(buf, &alias_data)) {
		char err[MAX_ERRBUF_LEN];
		// @vit, looks like errbuf is scripthandler? this is parser
		std::snprintf(err, MAX_ERRBUF_LEN, "Undefined string alias '%s'", buf);
		errorAndExit(err);
	}

	script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, alias_data.c_str());

	return RET_CONTINUE;
}

int ScriptParser::forCommand() {
	callStack.emplace_back();
	auto &b     = callStack.back();
	b.nest_mode = NestInfo::FOR;
	b.label     = current_label_info;
	b.line      = current_line;

	script_h.readVariable();
	if (script_h.current_variable.type != VariableInfo::TypeInt)
		errorAndExit("for: no integer variable.");

	b.var_no = script_h.current_variable.var_no;

	script_h.pushVariable();

	if (!script_h.compareString("="))
		errorAndExit("for: missing '='");

	script_h.setCurrent(script_h.getNext() + 1);
	int from = script_h.readInt();
	script_h.setInt(&script_h.pushed_variable, from);

	if (!script_h.compareString("to"))
		errorAndExit("for: missing 'to'");

	script_h.readName();

	b.to = script_h.readInt();

	if (script_h.compareString("step")) {
		script_h.readName();
		b.step = script_h.readInt();
	} else {
		b.step = 1;
	}

	break_flag = ((b.step > 0) && (from > b.to)) ||
	             ((b.step < 0) && (from < b.to));

	/* ---------------------------------------- */
	/* Step forward callee's label info */
	b.next_script = script_h.getNext();

	return RET_CONTINUE;
}

int ScriptParser::filelogCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("filelog: not in the define section");

	filelog_flag = true;
	readLog(script_h.log_info[ScriptHandler::FILE_LOG]);

	return RET_CONTINUE;
}

int ScriptParser::errorsaveCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("errorsave: not in the define section.");
	errorsave = true;

	return RET_CONTINUE;
}

int ScriptParser::englishCommand() {
	english_mode = true;
	script_h.setEnglishMode(true);

	return RET_CONTINUE;
}

int ScriptParser::effectcutCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("effectcut: not in the define section.");

	effect_cut_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::effectblankCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("effectblank: not in the define section");

	effect_blank = script_h.readInt();

	return RET_CONTINUE;
}

int ScriptParser::effectCommand() {

	if (script_h.isName("windoweffect", true)) {
		readEffect(&window_effect);
	} else {
		if (current_mode != DEFINE_MODE)
			errorAndExit("effect: not in the define section");

		effect_links.emplace_back();
		EffectLink &elink = effect_links.back();
		elink.no          = script_h.readInt();
		if (elink.no < 2 || elink.no > 255)
			errorAndExit("effect: effect number out of range");

		readEffect(&elink);
	}

	return RET_CONTINUE;
}

int ScriptParser::divCommand() {
	int val1 = script_h.readInt();
	script_h.pushVariable();

	int val2 = script_h.readInt();
	script_h.setInt(&script_h.pushed_variable, val1 / val2);

	return RET_CONTINUE;
}

int ScriptParser::dimCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("dim: not in the define section");

	script_h.declareDim();

	return RET_CONTINUE;
}

int ScriptParser::defvoicevolCommand() {
	int vol = script_h.readInt();
	if (use_default_volume)
		voice_volume = vol;

	return RET_CONTINUE;
}

int ScriptParser::defsubCommand() {
	const char *cmd = script_h.readName();

	if (cmd[0] >= 'a' && cmd[0] <= 'z') {
		user_func_lut.emplace(HashedString(cmd, true), false);
	}

	return RET_CONTINUE;
}

int ScriptParser::defsevolCommand() {
	int vol = script_h.readInt();
	if (use_default_volume)
		se_volume = vol;

	return RET_CONTINUE;
}

int ScriptParser::defmp3volCommand() {
	int vol = script_h.readInt();
	if (use_default_volume)
		music_volume = vol;

	return RET_CONTINUE;
}

int ScriptParser::defvideovolCommand() {
	int vol = script_h.readInt();
	if (use_default_volume)
		video_volume = vol;

	return RET_CONTINUE;
}

int ScriptParser::defaultspeedCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("defaultspeed: not in the define section");

	errorAndExit("defaultspeed: you are not allowed to change it");

	return RET_CONTINUE;
}

int ScriptParser::setdefaultspeedCommand() {
	errorAndExit("setdefaultspeed: you are not allowed to change it");

	return RET_CONTINUE;
}

int ScriptParser::disablespeedbuttonsCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("disablespeedbuttons: not in the define section");

	errorAndExit("disablespeedbuttons: these buttons do nothing by default now");

	return RET_CONTINUE;
}

int ScriptParser::defaultfontCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("defaultfont: not in the define section");

	script_h.setStr(&default_env_font, script_h.readStr());

	return RET_CONTINUE;
}

int ScriptParser::decCommand() {
	int val = script_h.readInt();
	script_h.setInt(&script_h.current_variable, val - 1);

	return RET_CONTINUE;
}

int ScriptParser::dateCommand() {
	time_t t      = time(nullptr);
	struct tm *tm = localtime(&t);

	script_h.readInt();
	script_h.setInt(&script_h.current_variable, tm->tm_year % 100);

	script_h.readInt();
	script_h.setInt(&script_h.current_variable, tm->tm_mon + 1);

	script_h.readInt();
	script_h.setInt(&script_h.current_variable, tm->tm_mday);

	return RET_CONTINUE;
}

int ScriptParser::cosCommand() {
	script_h.readInt();
	script_h.pushVariable();

	int val = script_h.readInt();
	script_h.setInt(&script_h.pushed_variable, static_cast<int>(std::cos(M_PI * val / 180.0) * 1000.0));

	return RET_CONTINUE;
}

int ScriptParser::cmpCommand() {
	script_h.readInt();
	script_h.pushVariable();

	script_h.readStr();
	const char *save_buf = script_h.saveStringBuffer();

	const char *buf = script_h.readStr();

	script_h.setInt(&script_h.pushed_variable, cmp::clamp(std::strcmp(save_buf, buf), -1, 1));

	return RET_CONTINUE;
}

int ScriptParser::clickvoiceCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("clickvoice: not in the define section");

	for (auto &i : clickvoice_file_name)
		script_h.setStr(&i, script_h.readFilePath());

	return RET_CONTINUE;
}

int ScriptParser::clickstrCommand() {
	script_h.readStr();
	const char *buf = script_h.saveStringBuffer();

	clickstr_line = script_h.readInt();

	script_h.setClickstr(buf);

	return RET_CONTINUE;
}

int ScriptParser::clickskippageCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("clickskippage: not in the define section");

	clickskippage_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::btnnowindoweraseCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("btnnowindowerase: not in the define section");

	btnnowindowerase_flag = true;

	return RET_CONTINUE;
}

int ScriptParser::breakCommand() {
	//Mion: apparently NScr allows 'break' outside of a for loop, it just skips ahead to 'next'
	bool unnested = false;
	if (callStack.empty() || callStack.back().nest_mode != NestInfo::FOR) {
		unnested = true;
		errorAndCont("break: not in 'for' loop");
	}

	const char *buf = script_h.getNext();
	if (buf[0] == '*') {
		if (!unnested) {
			callStack.pop_back();
		}
		setCurrentLabel(script_h.readLabel() + 1);
	} else {
		break_flag = true;
	}

	return RET_CONTINUE;
}

int ScriptParser::atoiCommand() {
	script_h.readInt();
	script_h.pushVariable();

	const char *buf = script_h.readStr();

	script_h.setInt(&script_h.pushed_variable, static_cast<int>(strtol(buf, nullptr, 10)));

	return RET_CONTINUE;
}

int ScriptParser::arcCommand() {
	char *buf = copystr(script_h.readStr());

	size_t i = 0;
	while (buf[i] != '|' && buf[i] != '\0') i++;
	buf[i] = '\0';

	if (equalstr(script_h.reader->getArchiveName(), "direct")) {
		delete script_h.reader;
		script_h.reader = new SarReader(archive_path);
		if (script_h.reader->open(buf)) {
			std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
			              "arc: couldn't open archive '%s'", buf);
			errorAndCont(script_h.errbuf);
		}
	} else if (equalstr(script_h.reader->getArchiveName(), "sar")) {
		if (script_h.reader->open(buf)) {
			std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
			              "arc: couldn't open archive '%s'", buf);
			errorAndCont(script_h.errbuf);
		}
	}
	// skipping "arc" commands after "ns?" command

	delete[] buf;

	return RET_CONTINUE;
}

int ScriptParser::addnsadirCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("addnsadir: not in the define section");

	const char *buf = script_h.readFilePath();

	nsa_path.add(buf);

	delete script_h.reader;
	script_h.reader = new NsaReader(archive_path, nsa_offset);
	if (script_h.reader->open(nsa_path.getAllPaths().c_str())) {
		errorAndCont("addnsadir: couldn't open any NSA archives");
	}

	return RET_CONTINUE;
}

//Mion: for kinsoku
int ScriptParser::addkinsokuCommand() {
	if (current_mode != DEFINE_MODE)
		errorAndExit("addkinsoku: not in the define section");

	script_h.readStr();
	const char *start = script_h.saveStringBuffer();
	const char *end   = script_h.readStr();
	setKinsoku(start, end, true);
	if (debug_level > 0)
		sendToLog(LogLevel::Info, "addkinsoku: \"%s\",\"%s\"\n", start, end);

	return RET_CONTINUE;
}

int ScriptParser::addCommand() {
	script_h.readVariable();

	if (script_h.current_variable.type == VariableInfo::TypeInt ||
	    script_h.current_variable.type == VariableInfo::TypeArray) {
		int val = script_h.getIntVariable(&script_h.current_variable);
		script_h.pushVariable();

		script_h.setInt(&script_h.pushed_variable, val + script_h.readInt());
	} else if (script_h.current_variable.type == VariableInfo::TypeStr) {
		int no = script_h.current_variable.var_no;

		const char *buf  = script_h.readStr();
		VariableData &vd = script_h.getVariableData(no);
		char *tmp_buffer = vd.str;

		if (tmp_buffer) {
			size_t res_len = std::strlen(tmp_buffer) + std::strlen(buf) + 1;
			vd.str         = new char[res_len];
			copystr(vd.str, tmp_buffer, res_len);
			appendstr(vd.str, buf, res_len);
			delete[] tmp_buffer;
		} else {
			vd.str = copystr(buf);
		}
	} else {
		errorAndExit("add: no variable.");
	}

	return RET_CONTINUE;
}

int ScriptParser::dsoundCommand() {
	//added to remove "unsupported command" warnings for 'dsound'
	return RET_CONTINUE;
}
