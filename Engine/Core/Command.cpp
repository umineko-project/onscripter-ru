/**
 *  Command.cpp
 *  ONScripter-RU
 *
 *  Command executer for core commands.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Layers/Media.hpp"
#include "External/slre.h"
#include "Resources/Support/Version.hpp"
#include "Support/FileIO.hpp"

#include <SDL2/SDL_thread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>

#define DEFAULT_CURSOR_WAIT ":l/3,160,2;cursor0.bmp"
#define DEFAULT_CURSOR_NEWPAGE ":l/3,160,2;cursor1.bmp"

int ONScripter::yesnoboxCommand() {
	bool is_yesnobox = script_h.isName("yesnobox", true);

	script_h.readVariable();
	script_h.pushVariable();

	char *msg      = copystr(script_h.readStr());
	char *title    = copystr(script_h.readStr());
	char *positive = copystr(script_h.hasMoreArgs() ?
	                             script_h.readStr() :
	                             (is_yesnobox ? "Yes" : "Ok"));
	char *negative = copystr(script_h.hasMoreArgs() ?
	                             script_h.readStr() :
	                             (is_yesnobox ? "No" : "Cancel"));

	int res = answer_dialog_with_yes_ok;

	if (!answer_dialog_with_yes_ok) {
		SDL_MessageBoxButtonData buttons[2];

		buttons[0].buttonid = 1;
		buttons[0].flags    = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
		buttons[0].text     = positive;

		buttons[1].buttonid = 0;
		buttons[1].flags    = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		buttons[1].text     = negative;

		if (window.showMessageBox(SDL_MESSAGEBOX_INFORMATION, title, msg, 2, buttons, res) && res < 0)
			res = 0; // Closed
	}

	script_h.setInt(&script_h.pushed_variable, res);
	sendToLog(LogLevel::Info, "%s: Got dialog '%s': '%s', returned value of %d\n",
	          is_yesnobox ? "yesnobox" : "okcancelbox", title, msg, res);

	freearr(&msg);
	freearr(&title);
	freearr(&positive);
	freearr(&negative);

	return RET_CONTINUE;
}

int ONScripter::waveCommand() {
	wave_play_loop_flag = false;

	if (script_h.isName("waveloop"))
		wave_play_loop_flag = true;

	wavestopCommand();

	script_h.setStr(&wave_file_name, script_h.readFilePath());
	playSoundThreaded(wave_file_name, SOUND_CHUNK, wave_play_loop_flag, MIX_WAVE_CHANNEL);

	return RET_CONTINUE;
}

int ONScripter::wavestopCommand() {
	if (audio_open_flag && wave_sample[MIX_WAVE_CHANNEL]) {
		Mix_Pause(MIX_WAVE_CHANNEL);
		wave_sample[MIX_WAVE_CHANNEL] = nullptr;
	}
	script_h.setStr(&wave_file_name, nullptr);

	return RET_CONTINUE;
}

int ONScripter::waittimerCommand() {
	int count = script_h.readInt() + internal_timer - SDL_GetTicks();
	if (count < 0)
		count = 0;

	if (skip_mode & SKIP_SUPERSKIP)
		count = 0;
	if (count == 0)
		return RET_CONTINUE;

	WaitTimerAction *action{WaitTimerAction::create()};

	//sendToLog(LogLevel::Info, "Starting a timer set to %d\n", count);
	action->event_mode = WAIT_WAITTIMER_MODE;
	action->clock.setCountdown(count);

	Lock lock(&ons.registeredCRActions);
	registeredCRActions.emplace_back(action);
	return RET_CONTINUE;
}

int ONScripter::waitCommand() {
	//using insani's skippable wait concept (modified)
	bool skippable          = !static_cast<bool>(script_h.isName("wait2"));
	bool entirely_skippable = script_h.isName("waits");

	int count          = script_h.readInt();
	int requestedCount = count;
	int act_event_mode{0};

	if (skippable) {
		if ((skip_mode & (SKIP_NORMAL | SKIP_TO_WAIT)) || keyState.ctrl) {
			//Mion: instead of skipping entirely, let's do a shortened wait (safer)
			if (count > 100) {
				count = count / 10;
			} else if (count > 10) {
				count = 10;
			}
		}
		if (count < 0)
			count = 0;
		act_event_mode = WAIT_WAIT_MODE;
	} else {
		if (count < 0)
			count = 0;
		act_event_mode = WAIT_WAIT2_MODE;
	}

	if (skip_mode & SKIP_SUPERSKIP)
		count = 0;
	if (count == 0)
		return RET_CONTINUE;

	if (skippable && internal_slowdown_counter > 0) {
		uint32_t q = entirely_skippable ? count : count / 6;
		if (q > internal_slowdown_counter) {
			count -= internal_slowdown_counter;
			internal_slowdown_counter = 0;
		} else {
			count -= q;
			internal_slowdown_counter -= q;
		}
	}

	WaitAction *action{WaitAction::create()};
	action->event_mode = act_event_mode;

	if (requestedCount > count) {
		action->advanceProperties = requestedCount - count;
	}

	action->clock.setCountdown(count);

	Lock lock(&ons.registeredCRActions);
	registeredCRActions.emplace_back(action);
	return RET_CONTINUE;
}

int ONScripter::vspCommand() {
	leaveTextDisplayMode();

	bool vsp2_flag        = script_h.isName("vsp2");
	AnimationInfo *s_info = vsp2_flag ? sprite2_info : sprite_info;

	int no1, no2;
	no1 = no2 = validSprite(script_h.readInt());
	int v     = script_h.readInt();

	if (script_h.hasMoreArgs()) {
		no2 = validSprite(v);
		v   = script_h.readInt();
		if (no2 < no1)
			std::swap(no1, no2);
	}

	bool visible = v == 1;

	for (auto i = no1; i <= no2; i++) {
		backupState(&s_info[i]);
		if (s_info[i].exists && visible != s_info[i].visible)
			dirtySpriteRect(s_info[i].id, vsp2_flag);
		s_info[i].visible = visible;
		if (vsp2_flag && v == 0 && s_info[i].is_animatable) {
			s_info[i].current_cell = 0;
			s_info[i].direction    = 1;
		}
	}

	return RET_CONTINUE;
}

int ONScripter::voicevolCommand() {
	voice_volume = validVolume(script_h.readInt());
	setVolume(0, voice_volume, volume_on_flag);

	return RET_CONTINUE;
}

int ONScripter::vCommand() {
	char buf[256];

	std::snprintf(buf, sizeof(buf), "voice%c%s.wav", DELIMITER, script_h.getStringBuffer() + 1);
	playSoundThreaded(buf, SOUND_CHUNK, false, MIX_WAVE_CHANNEL);

	return RET_CONTINUE;
}

int ONScripter::trapCommand() {
	lrTrap = LRTrap();

	if (script_h.isName("lr_trap", true)) {
		lrTrap.left  = true;
		lrTrap.right = true;
	} else if (script_h.isName("r_trap", true)) {
		lrTrap.right = true;
	} else if (script_h.isName("trap", true)) {
		lrTrap.left = true;
	} else {
		sendToLog(LogLevel::Info, "trapCommand: cmd [%s] not recognized\n", script_h.getStringBuffer());
		lrTrap = LRTrap();
		return RET_CONTINUE;
	}

	if (script_h.compareString("off")) {
		script_h.readName();
		lrTrap = LRTrap();
		return RET_CONTINUE;
	}

	const char *buf = script_h.readLabel();
	if (buf[0] == '*') {
		script_h.setStr(&lrTrap.dest, buf + 1);
	} else {
		sendToLog(LogLevel::Info, "trapCommand: [%s] is not supported\n", buf);
		lrTrap = LRTrap();
	}

	return RET_CONTINUE;
}

int ONScripter::transbtnCommand() {
	transbtn_flag = true;

	return RET_CONTINUE;
}

int ONScripter::textspeeddefaultCommand() {
	errorAndExit("textspeeddefault: this command is not supported");

	return RET_CONTINUE;
}

int ONScripter::textspeedCommand() {
	errorAndExit("textspeed: this command is not supported, use text_speed");

	return RET_CONTINUE;
}

int ONScripter::textshowCommand() {
	dirty_rect_hud.fill(window.canvas_width, window.canvas_height);
	refresh_window_text_mode = REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE | REFRESH_TEXT_MODE;
	constantRefreshEffect(&window_effect, false, false,
	                      REFRESH_BEFORESCENE_MODE | REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE,                    // from no text (on beforescene)
	                      REFRESH_BEFORESCENE_MODE | REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE | REFRESH_TEXT_MODE // to text      (on beforescene)
	);
	return RET_CONTINUE;
}

int ONScripter::textonCommand() {
	if (windowchip_sprite_no >= 0)
		sprite_info[windowchip_sprite_no].visible = true;

	enterTextDisplayMode();

	return RET_CONTINUE;
}

int ONScripter::textoffCommand() {
	if (windowchip_sprite_no >= 0)
		sprite_info[windowchip_sprite_no].visible = false;

	leaveTextDisplayMode(true, !script_h.isName("textoff2"));

	return RET_CONTINUE;
}

int ONScripter::texthideCommand() {
	dirty_rect_hud.fill(window.canvas_width, window.canvas_height);
	refresh_window_text_mode = REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE;
	constantRefreshEffect(&window_effect, false, false,
	                      REFRESH_BEFORESCENE_MODE | REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE | REFRESH_TEXT_MODE, // from text  (on beforescene)
	                      REFRESH_BEFORESCENE_MODE | REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE                      // to no text (on beforescene)
	);
	return RET_CONTINUE;
}

int ONScripter::textexbtnCommand() {
	int txtbtn_no   = script_h.readInt();
	const char *buf = script_h.readStr();

	TextButtonInfoLink *info  = text_button_info.next;
	TextButtonInfoLink *found = nullptr;
	while (info) {
		if (info->no == txtbtn_no)
			found = info;
		info = info->next;
	}

	if (found) {
		ButtonLink *button = found->button;
		while (button) {
			button->exbtn_ctl = copystr(buf);
			button            = button->same;
		}
	}
	is_exbtn_enabled = true;

	return RET_CONTINUE;
}

int ONScripter::textclearCommand() {
	newPage(false);
	return RET_CONTINUE;
}

int ONScripter::textbtnstartCommand() {
	txtbtn_start_num = script_h.readInt();

	return RET_CONTINUE;
}

int ONScripter::textbtnoffCommand() {
	txtbtn_show = false;

	return RET_CONTINUE;
}

int ONScripter::texecCommand() {
	if (textgosub_clickstr_state == CLICK_NEWPAGE) {
		if (script_h.isName("texec3")) {
			newPage(true, true);
		} else {
			newPage(true);
		}
		clickstr_state = CLICK_NONE;
	} else if (textgosub_clickstr_state == CLICK_WAITEOL) {
		if (!sentence_font.isLineEmpty() && !new_line_skip_flag) {
			sentence_font.newLine();
		}
	}

	return RET_CONTINUE;
}

int ONScripter::tateyokoCommand() {
	errorAndExit("tateyoko: vertical text rendering is currently unsupported");

	return RET_CONTINUE;
}

int ONScripter::talCommand() {
	leaveTextDisplayMode();

	if (script_h.isName("talsp")) {
		int no = validSprite(script_h.readInt());

		backupState(&sprite_info[no]);
		sprite_info[no].trans = script_h.readInt();
		if (sprite_info[no].trans > 255)
			sprite_info[no].trans = 255;
		else if (sprite_info[no].trans < 0)
			sprite_info[no].trans = 0;

		dirtySpriteRect(no, false);

		return RET_CONTINUE;
	}

	char loc = script_h.readName()[0];
	int no = -1, trans = 0;
	if (loc == 'l')
		no = 0;
	else if (loc == 'c')
		no = 1;
	else if (loc == 'r')
		no = 2;

	if (no >= 0) {
		trans = script_h.readInt();
		if (trans > 255)
			trans = 255;
		else if (trans < 0)
			trans = 0;

		backupState(&tachi_info[no]);
		tachi_info[no].trans = trans;
		dirty_rect_scene.add(tachi_info[no].pos);
	}

	EffectLink *el = parseEffect(true);
	constantRefreshEffect(el, true);
	return RET_CONTINUE;
}

int ONScripter::tablegotoCommand() {
	int count = 0;
	int no    = script_h.readInt();

	while (script_h.hasMoreArgs()) {
		const char *buf = script_h.readLabel();
		if (count++ == no) {
			setCurrentLabel(buf + 1);
			break;
		}
	}

	return RET_CONTINUE;
}

int ONScripter::systemcallCommand() {
	int mode = getSystemCallNo(script_h.readName());
	executeSystemCall(mode);

	return RET_CONTINUE;
}

int ONScripter::strspCommand() {
	leaveTextDisplayMode();

	bool v = true;

	if (script_h.isName("strsph"))
		v = false;

	int sprite_no = script_h.readInt();
	backupState(&sprite_info[sprite_no]);
	AnimationInfo *ai = &sprite_info[sprite_no];
	if (ai->gpu_image && ai->visible)
		dirtySpriteRect(sprite_no, false);
	ai->remove();
	script_h.setStr(&ai->file_name, script_h.readFilePath());

	Fontinfo fi;
	ai->orig_pos.x = script_h.readInt();
	ai->orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(ai);
	script_h.readInt();
	script_h.readInt();
	int size_x                 = script_h.readInt();
	int size_y                 = script_h.readInt();
	fi.changeStyle().font_size = size_x > size_y ? size_x : size_y;
	script_h.readInt(); // dummy read for pitch x
	script_h.readInt(); // dummy read for pitch y
	fi.changeStyle().is_bold   = script_h.readInt();
	fi.changeStyle().is_shadow = script_h.readInt();

	const char *buffer = script_h.getNext();
	while (script_h.hasMoreArgs()) {
		ai->num_of_cells++;
		script_h.readStr();
	}
	if (ai->num_of_cells == 0) {
		ai->num_of_cells  = 1;
		ai->color_list    = new uchar3[ai->num_of_cells];
		ai->color_list[0] = {0xff, 0xff, 0xff};
	} else {
		ai->color_list = new uchar3[ai->num_of_cells];
		script_h.setCurrent(buffer);
		for (int i = 0; i < ai->num_of_cells; i++)
			readColor(&ai->color_list[i], readColorStr());
	}

	ai->trans_mode = AnimationInfo::TRANS_STRING;
	ai->trans      = 255;
	ai->flip       = FLIP_NONE;
	ai->visible    = v;
	setupAnimationInfo(ai, &fi);
	if (ai->visible)
		dirtySpriteRect(sprite_no, false);

	return RET_CONTINUE;
}

int ONScripter::stopCommand() {
	wavestopCommand();
	//NScr doesn't stop loopbgm w/this cmd
	return mp3stopCommand();
}

int ONScripter::sp_rgb_gradationCommand() {
	leaveTextDisplayMode();

	int no         = script_h.readInt();
	int upper_r    = script_h.readInt();
	int upper_g    = script_h.readInt();
	int upper_b    = script_h.readInt();
	int lower_r    = script_h.readInt();
	int lower_g    = script_h.readInt();
	int lower_b    = script_h.readInt();
	ONSBuf key_r   = script_h.readInt();
	ONSBuf key_g   = script_h.readInt();
	ONSBuf key_b   = script_h.readInt();
	uint32_t alpha = script_h.readInt();

	AnimationInfo *si;
	if (no == -1)
		si = &sentence_font_info;
	else
		si = &sprite_info[no];
	backupState(si);

	SDL_Surface *surface = si->image_surface;
	if (surface == nullptr)
		return RET_CONTINUE; //FIXME: alloc image instead?

	SDL_PixelFormat *fmt = surface->format;

	ONSBuf key_mask = (key_r >> fmt->Rloss) << fmt->Rshift |
	                  (key_g >> fmt->Gloss) << fmt->Gshift |
	                  (key_b >> fmt->Bloss) << fmt->Bshift;
	ONSBuf rgb_mask = fmt->Rmask | fmt->Gmask | fmt->Bmask;

	// check upper and lower bound
	int i, j;
	int upper_bound = 0, lower_bound = 0;
	bool is_key_found = false;
	for (i = 0; i < surface->h; i++) {
		ONSBuf *buf = static_cast<ONSBuf *>(surface->pixels) + surface->w * i;
		for (j = 0; j < surface->w; j++, buf++) {
			if ((*buf & rgb_mask) == key_mask) {
				if (!is_key_found) {
					is_key_found = true;
					upper_bound = lower_bound = i;
				} else {
					lower_bound = i;
				}
				break;
			}
		}
	}

	// replace pixels of the key-color with the specified color in gradation
	for (i = upper_bound; i <= lower_bound; i++) {
		ONSBuf *buf     = static_cast<ONSBuf *>(surface->pixels) + surface->w * i;
		uint8_t *alphap = reinterpret_cast<uint8_t *>(buf) + 3;

		uint32_t color = alpha << surface->format->Ashift;
		if (upper_bound != lower_bound) {
			color |= (((lower_r - upper_r) * (i - upper_bound) / (lower_bound - upper_bound) + upper_r) >> fmt->Rloss) << fmt->Rshift;
			color |= (((lower_g - upper_g) * (i - upper_bound) / (lower_bound - upper_bound) + upper_g) >> fmt->Gloss) << fmt->Gshift;
			color |= (((lower_b - upper_b) * (i - upper_bound) / (lower_bound - upper_bound) + upper_b) >> fmt->Bloss) << fmt->Bshift;
		} else {
			color |= (upper_r >> fmt->Rloss) << fmt->Rshift;
			color |= (upper_g >> fmt->Gloss) << fmt->Gshift;
			color |= (upper_b >> fmt->Bloss) << fmt->Bshift;
		}

		for (j = 0; j < surface->w; j++, buf++) {
			if ((*buf & rgb_mask) == key_mask) {
				*buf    = color;
				*alphap = alpha;
			}
			alphap += 4;
		}
	}

	if (si->visible)
		dirtySpriteRect(no, false);

	return RET_CONTINUE;
}

int ONScripter::spstrCommand() {
	decodeExbtnControl(script_h.readStr());

	return RET_CONTINUE;
}

int ONScripter::spreloadCommand() {
	leaveTextDisplayMode();

	int no = script_h.readInt();
	AnimationInfo *si;
	if (no == -1)
		si = &sentence_font_info;
	else
		si = &sprite_info[no];
	backupState(si);

	parseTaggedString(si);
	setupAnimationInfo(si);

	if (si->visible && no != -1)
		dirtySpriteRect(no, false);
	else if (si->visible)
		dirty_rect_hud.add(si->pos);

	return RET_CONTINUE;
}

int ONScripter::spclclkCommand() {
	if (!force_button_shortcut_flag)
		spclclk_flag = true;
	return RET_CONTINUE;
}

int ONScripter::spbtnCommand() {
	bool cellcheck_flag = false;

	if (script_h.isName("cellcheckspbtn"))
		cellcheck_flag = true;

	int sprite_no = script_h.readInt();
	int no        = script_h.readInt();

	if (cellcheck_flag) {
		if (sprite_info[sprite_no].num_of_cells < 2)
			return RET_CONTINUE;
	} else {
		if (sprite_info[sprite_no].num_of_cells == 0)
			return RET_CONTINUE;
	}

	ButtonLink *button = new ButtonLink();
	root_button_link.insert(button);

	button->button_type = ButtonLink::SPRITE_BUTTON;
	button->sprite_no   = sprite_no;
	button->no          = no;

	if (sprite_info[sprite_no].gpu_image ||
	    sprite_info[sprite_no].trans_mode == AnimationInfo::TRANS_STRING) {
		button->image_rect = button->select_rect = sprite_info[sprite_no].pos;
	}

	return RET_CONTINUE;
}

int ONScripter::skipoffCommand() {
	skip_mode &= ~SKIP_NORMAL;

	return RET_CONTINUE;
}

int ONScripter::shellCommand() {
	std::string url(script_h.readStr());

	if (!FileIO::shellOpen(url, FileType::URL)) {
		window.showSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "ONScripter-RU", ("Visit " + url).c_str());
		sendToLog(LogLevel::Error, "[shell] %s\n", url.c_str());
		sendToLog(LogLevel::Error, "[shell] command failed or unsupported on this OS\n");
	}

	return RET_CONTINUE;
}

int ONScripter::sevolCommand() {
	se_volume = validVolume(script_h.readInt());

	for (int i = 1; i < ONS_MIX_CHANNELS; i++)
		setVolume(i, se_volume, volume_on_flag);

	setVolume(MIX_LOOPBGM_CHANNEL0, se_volume, volume_on_flag);
	setVolume(MIX_LOOPBGM_CHANNEL1, se_volume, volume_on_flag);

	return RET_CONTINUE;
}

void ONScripter::setwindowCore() {
	sentence_font.top_xy[0] = script_h.readInt();
	sentence_font.top_xy[1] = script_h.readInt();
	script_h.readInt();
	script_h.readInt();
	int size_x                            = script_h.readInt();
	int size_y                            = script_h.readInt();
	sentence_font.changeStyle().font_size = size_x > size_y ? size_x : size_y;

	script_h.readInt(); // dummy read for pitch x
	script_h.readInt(); // dummy read for pitch y
	script_h.readInt(); // dummy read for wait_time
	sentence_font.changeStyle().is_bold   = script_h.readInt();
	sentence_font.changeStyle().is_shadow = script_h.readInt();

	bool is_color = false;
	const char *buf;
	if (allow_color_type_only) {
		buf = script_h.readColor(&is_color);
		if (!is_color)
			buf = script_h.readStr();
	} else {
		buf = script_h.readStr();
		if (buf[0] == '#')
			is_color = true;
	}

	backupState(&sentence_font_info);
	sentence_font_info.deleteImage();

	if (is_color) {
		sentence_font_info.stale_image = true;
		sentence_font.is_transparent   = true;
		readColor(&sentence_font.window_color, buf);

		sentence_font_info.orig_pos.x = script_h.readInt();
		sentence_font_info.orig_pos.y = script_h.readInt();
		sentence_font_info.orig_pos.w = script_h.readInt() - sentence_font_info.orig_pos.x;
		sentence_font_info.orig_pos.h = script_h.readInt() - sentence_font_info.orig_pos.y;
		UpdateAnimPosXY(&sentence_font_info);
		UpdateAnimPosWH(&sentence_font_info);

		if (!sentence_font_info.gpu_image)
			sentence_font_info.gpu_image = gpu.createImage(sentence_font_info.pos.w, sentence_font_info.pos.h, 4);
		GPU_GetTarget(sentence_font_info.gpu_image);
		gpu.clearWholeTarget(sentence_font_info.gpu_image->target,
		                     sentence_font.window_color.x, sentence_font.window_color.y, sentence_font.window_color.z, 0xFF);
		gpu.multiplyAlpha(sentence_font_info.gpu_image);
		sentence_font_info.blending_mode = BlendModeId::MUL;
		sentence_font_info.trans_mode    = AnimationInfo::TRANS_COPY;
	} else {
		sentence_font.is_transparent = false;
		sentence_font_info.setImageName(buf);
		parseTaggedString(&sentence_font_info);
		setupAnimationInfo(&sentence_font_info);
		sentence_font_info.orig_pos.x = script_h.readInt();
		sentence_font_info.orig_pos.y = script_h.readInt();
		UpdateAnimPosXY(&sentence_font_info);
		sentence_font.window_color       = {0xff, 0xff, 0xff};
		sentence_font_info.blending_mode = BlendModeId::NORMAL;
	}

	dirty_rect_hud.add(sentence_font_info.pos);
}

int ONScripter::setwindow3Command() {
	setwindowCore();

	display_mode = DISPLAY_MODE_NORMAL;
	commitVisualState(); //might be a bug...
	flush(refreshMode(), nullptr, &sentence_font_info.pos);

	return RET_CONTINUE;
}

int ONScripter::setwindow2Command() {
	bool refresh = !script_h.isName("setwindow5");

	bool is_color = false;
	const char *buf;
	if (allow_color_type_only) {
		buf = script_h.readColor(&is_color);
		if (!is_color)
			buf = script_h.readStr();
	} else {
		buf = script_h.readStr();
		if (buf[0] == '#')
			is_color = true;
	}

	backupState(&sentence_font_info);

	if (is_color) {
		sentence_font.is_transparent = true;
		readColor(&sentence_font.window_color, buf);

		GPU_GetTarget(sentence_font_info.gpu_image);
		gpu.clearWholeTarget(sentence_font_info.gpu_image->target,
		                     sentence_font.window_color.x, sentence_font.window_color.y, sentence_font.window_color.z, 0xFF);
		gpu.multiplyAlpha(sentence_font_info.gpu_image);
		sentence_font_info.trans_mode    = AnimationInfo::TRANS_COPY;
		sentence_font_info.blending_mode = BlendModeId::MUL;
	} else {
		sentence_font.is_transparent = false;
		sentence_font_info.setImageName(buf);
		parseTaggedString(&sentence_font_info);
		setupAnimationInfo(&sentence_font_info);
		sentence_font_info.blending_mode = BlendModeId::NORMAL;

		//Extra name param
		if (script_h.hasMoreArgs()) {
			GPU_Image *image = loadGpuImage(script_h.readFilePath());
			if (image) {
				GPU_SetBlending(image, false);
				GPU_GetTarget(sentence_font_info.gpu_image);
				gpu.copyGPUImage(image, nullptr, nullptr, sentence_font_info.gpu_image->target);
				GPU_SetBlending(image, true);
				gpu.freeImage(image);
			}
		}
	}

	if (refresh)
		repaintCommand();

	return RET_CONTINUE;
}

int ONScripter::setwindowCommand() {
	setwindowCore();

	lookbackflushCommand();
	page_enter_status = 0;
	display_mode      = DISPLAY_MODE_NORMAL;
	commitVisualState(); //might be a bug
	flush(refreshMode(), nullptr, &sentence_font_info.pos);

	return RET_CONTINUE;
}

int ONScripter::seteffectspeedCommand() {
	int no = script_h.readInt();

	effectspeed = EFFECTSPEED_NORMAL;
	if (no == 1)
		effectspeed = EFFECTSPEED_QUICKER;
	else if (no == 2)
		effectspeed = EFFECTSPEED_INSTANT;

	return RET_CONTINUE;
}

int ONScripter::setcursorCommand() {
	bool abs_flag;

	abs_flag = script_h.isName("abssetcursor");

	int no = script_h.readInt();
	script_h.readStr();
	const char *buf = script_h.saveStringBuffer();
	int x           = script_h.readInt();
	int y           = script_h.readInt();

	loadCursor(no, buf, x, y, abs_flag);

	return RET_CONTINUE;
}

int ONScripter::selectCommand() {
	if (isWaitingForUserInput() || isWaitingForUserInterrupt()) {
		errorAndExit("Cannot run this command at the moment");
		return RET_CONTINUE; //dummy
	}

	enterTextDisplayMode();

	int select_mode = SELECT_GOTO_MODE;
	SelectLink *last_select_link;

	if (script_h.isName("selnum"))
		select_mode = SELECT_NUM_MODE;
	else if (script_h.isName("selgosub"))
		select_mode = SELECT_GOSUB_MODE;
	else if (script_h.isName("select"))
		select_mode = SELECT_GOTO_MODE;
	else if (script_h.isName("csel"))
		select_mode = SELECT_CSEL_MODE;

	if (select_mode == SELECT_NUM_MODE) {
		script_h.readVariable();
		script_h.pushVariable();
	}

	bool comma_flag = true;
	if (select_mode == SELECT_CSEL_MODE) {
		saveoffCommand();
	}
	lastKnownHoveredButtonLinkIndex = -1; // Not sure why this is here... Model-wise it makes no sense to erase this data

	if (selectvoice_file_name[SELECTVOICE_OPEN])
		playSoundThreaded(selectvoice_file_name[SELECTVOICE_OPEN], SOUND_CHUNK, false, MIX_WAVE_CHANNEL);

	last_select_link = &root_select_link;

	while (true) {
		if (script_h.getNext()[0] != 0x0a && comma_flag) {

			const char *buf = script_h.readStr();
			comma_flag      = (script_h.hasMoreArgs());
			if (select_mode != SELECT_NUM_MODE && !comma_flag)
				errorAndExit("select: missing comma.");

			// Text part
			SelectLink *slink = new SelectLink();
			script_h.setStr(&slink->text, buf);
			//sendToLog(LogLevel::Info, "Select text %s\n", slink->text);

			// Label part
			if (select_mode != SELECT_NUM_MODE) {
				script_h.readLabel();
				script_h.setStr(&slink->label, script_h.getStringBuffer() + 1);
				//sendToLog(LogLevel::Info, "Select label %s\n", slink->label);
			}
			last_select_link->next = slink;
			last_select_link       = last_select_link->next;

			comma_flag = (script_h.hasMoreArgs());
			//sendToLog(LogLevel::Info, "2 comma %d %c %x\n", comma_flag, script_h.getCurrent()[0], script_h.getCurrent()[0]);
		} else if (script_h.getNext()[0] == 0x0a) {
			//sendToLog(LogLevel::Info, "comma %d\n", comma_flag);
			const char *buf = script_h.getNext() + 1; // consume eol
			while (*buf == ' ' || *buf == '\t') buf++;

			if (comma_flag && *buf == ',')
				errorAndExit("select: double comma.");

			bool comma2_flag = false;
			if (*buf == ',') {
				comma2_flag = true;
				buf++;
				while (*buf == ' ' || *buf == '\t') buf++;
			}
			script_h.setCurrent(buf);

			if (*buf == 0x0a) {
				comma_flag |= comma2_flag;
				continue;
			}

			if (!comma_flag && !comma2_flag) {
				select_label_info.next_script = buf;
				//sendToLog(LogLevel::Info, "select: stop at the end of line\n");
				break;
			}

			//sendToLog(LogLevel::Info, "continue\n");
			comma_flag = true;
		} else { // if select ends at the middle of the line
			select_label_info.next_script = script_h.getNext();
			//sendToLog(LogLevel::Info, "select: stop at the middle of the line\n");
			break;
		}
	}

	if (select_mode != SELECT_CSEL_MODE) {
		last_select_link = root_select_link.next;
		int counter      = 1;
		while (last_select_link) {
			if (*last_select_link->text) {
				ButtonLink *button = getSelectableSentence(last_select_link->text, &sentence_font);
				root_button_link.insert(button);
				button->no = counter;
			}
			counter++;
			last_select_link = last_select_link->next;
		}
	}

	if (select_mode == SELECT_CSEL_MODE) {
		setCurrentLabel("customsel");
		return RET_CONTINUE;
	}
	skip_mode &= ~SKIP_NORMAL;
	automode_flag = false;

	commitVisualState();
	flush(refreshMode());

	refreshButtonHoverState();

	bool actual_rmode = rmode_flag;
	rmode_flag        = false;
	event_mode        = WAIT_TEXT_MODE | WAIT_BUTTON_MODE | WAIT_TIMER_MODE;
	do {
		waitEvent(-1);
	} while (!current_button_state.valid_flag ||
	         (current_button_state.button <= 0));
	rmode_flag = actual_rmode;

	if (selectvoice_file_name[SELECTVOICE_SELECT])
		playSoundThreaded(selectvoice_file_name[SELECTVOICE_SELECT], SOUND_CHUNK, false, MIX_WAVE_CHANNEL);

	deleteButtonLink();

	int counter      = 1;
	last_select_link = root_select_link.next;
	while (last_select_link) {
		if (current_button_state.button == counter++)
			break;
		last_select_link = last_select_link->next;
	}

	if (select_mode == SELECT_GOTO_MODE && last_select_link) {
		setCurrentLabel(last_select_link->label);
	} else if (select_mode == SELECT_GOSUB_MODE && last_select_link) {
		gosubReal(last_select_link->label, select_label_info.next_script);
	} else { // selnum
		script_h.setInt(&script_h.pushed_variable, current_button_state.button - 1);
		current_label_info = script_h.getLabelByAddress(select_label_info.next_script);
		current_line       = script_h.getLineByAddress(select_label_info.next_script, current_label_info);
		script_h.setCurrent(select_label_info.next_script);
	}
	deleteSelectLink();

	newPage(true);

	return RET_CONTINUE;
}

int ONScripter::savetimeCommand() {
	int no = script_h.readInt();

	script_h.readVariable();
	SaveFileInfo info;
	if (!readSaveFileHeader(no, &info)) {
		script_h.setInt(&script_h.current_variable, 0);
		for (int i = 0; i < 3; i++)
			script_h.readVariable();
		return RET_CONTINUE;
	}

	script_h.setInt(&script_h.current_variable, info.month);
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, info.day);
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, info.hour);
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, info.minute);

	return RET_CONTINUE;
}

int ONScripter::savescreenshotCommand() {
	bool erase           = !script_h.isName("savescreenshot2");
	const char *filename = script_h.readFilePath();

	const char *ext = std::strrchr(filename, '.');
	if (ext) {
		GPU_FileFormatEnum format = GPU_FILE_AUTO;
		if (equalstr(ext + 1, "PNG") || equalstr(ext + 1, "png")) {
			format = GPU_FILE_PNG;
		} else if (equalstr(ext + 1, "BMP") || equalstr(ext + 1, "bmp")) {
			format = GPU_FILE_BMP;
		} else {
			sendToLog(LogLevel::Error, "savescreenshot: file %s is not supported.\n", filename);
		}

		if (format != GPU_FILE_AUTO) {
			if (screenshot_gpu == nullptr) {
				sendToLog(LogLevel::Error, "savescreenshot: no screenshot buffer, creating a blank 1x1 GPU_Image.\n");
				screenshot_gpu = gpu.createImage(1, 1, 4);
				GPU_GetTarget(screenshot_gpu);
				gpu.clearWholeTarget(screenshot_gpu->target);
			}

			char *savedir = FileIO::extractDirpath(filename);
			FileIO::makeDir(savedir, script_h.save_path, true);
			freearr(&savedir);

			FILE *fp = FileIO::openFile(filename, "wb", script_h.save_path);
			if (fp) {
				auto rwops = SDL_RWFromFP(fp, SDL_TRUE);
				GPU_SaveImage_RW(screenshot_gpu, rwops, true, format);
			} else {
				sendToLog(LogLevel::Error, "savescreenshot: failed to save the screenshot.\n");
			}
		}
	} else {
		sendToLog(LogLevel::Error, "savescreenshot: file %s has invalid extension.\n", filename);
	}

	if (erase && screenshot_gpu) {
		gpu.freeImage(screenshot_gpu);
		screenshot_gpu = nullptr;
	}

	return RET_CONTINUE;
}

int ONScripter::saveonCommand() {
	saveon_flag = true;

	return RET_CONTINUE;
}

int ONScripter::saveoffCommand() {
	saveon_flag = false;

	return RET_CONTINUE;
}

int ONScripter::savegameCommand() {
	bool savegame2_flag = false;
	if (script_h.isName("savegame2"))
		savegame2_flag = true;

	int no = script_h.readInt();

	const char *savestr = nullptr;
	if (savegame2_flag)
		savestr = script_h.readStr();

	if (no < 0)
		errorAndExit("savegame: save number is less than 0.");
	else
		saveSaveFile(no, savestr);

	return RET_CONTINUE;
}

int ONScripter::savefileexistCommand() {
	script_h.readInt();
	script_h.pushVariable();
	int no = script_h.readInt();

	script_h.setInt(&script_h.pushed_variable, readSaveFileHeader(no));

	return RET_CONTINUE;
}

int ONScripter::rndCommand() {
	int upper, lower;

	if (script_h.isName("rnd2")) {
		script_h.readInt();
		script_h.pushVariable();

		lower = script_h.readInt();
		upper = script_h.readInt();
	} else {
		script_h.readInt();
		script_h.pushVariable();

		lower = 0;
		upper = script_h.readInt() - 1;
	}

	script_h.setInt(&script_h.pushed_variable, lower + static_cast<int>(static_cast<double>(upper - lower + 1) * rand() / (RAND_MAX + 1.0)));

	return RET_CONTINUE;
}

int ONScripter::rmodeCommand() {
	rmode_flag = script_h.readInt() == 1;

	return RET_CONTINUE;
}

int ONScripter::resettimerCommand() {
	internal_timer = SDL_GetTicks();

	return RET_CONTINUE;
}

int ONScripter::resetCommand() {
	//clear out the event queue
	//there still is a chance of some event sneaking in,
	//but that was the same in the original implementation.
	updateEventQueue();
	if (takeEventsOut(SDL_QUIT))
		endCommand();
	localEventQueue.clear();

	int effect             = window_effect.effect;
	int duration           = window_effect.duration;
	window_effect.effect   = 1; //don't use window effect during a reset
	window_effect.duration = 0;
	resetSub();
	window_effect.effect   = effect;
	window_effect.duration = duration;
	reopenAudioOnMismatch(default_audio_format);
	clearCurrentPage();
	string_buffer_offset = 0;

	setCurrentLabel("start");
	saveSaveFile(-1);

	return RET_CONTINUE;
}

int ONScripter::repaintCommand() {
	fillCanvas(true, true);

	commitVisualState();
	flush(refreshMode());

	return RET_CONTINUE;
}

int ONScripter::puttextCommand() {
	errorAndExit("puttext: please, use d command for this now");

	return RET_CONTINUE;
}

int ONScripter::prnumclearCommand() {
	leaveTextDisplayMode();

	for (auto &i : prnum_info) {
		if (i) {
			dirty_rect_hud.add(i->pos);
			delete i;
			i = nullptr;
		}
	}
	return RET_CONTINUE;
}

int ONScripter::prnumCommand() {
	leaveTextDisplayMode();

	int no = script_h.readInt();
	if (no < 0 || no >= MAX_PARAM_NUM) {

		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "prnum: label id %d outside allowed range 0-%d, skipping",
		              no, MAX_PARAM_NUM - 1);
		errorAndCont(script_h.errbuf);

		script_h.readInt();
		script_h.readInt();
		script_h.readInt();
		script_h.readInt();
		script_h.readInt();
		script_h.readStr();
		return RET_CONTINUE;
	}

	backupState(prnum_info[no]);

	if (prnum_info[no]) {
		dirty_rect_hud.add(prnum_info[no]->pos);
		delete prnum_info[no];
	}
	prnum_info[no]               = new AnimationInfo();
	prnum_info[no]->type         = SPRITE_PRNUM;
	prnum_info[no]->id           = no;
	prnum_info[no]->trans_mode   = AnimationInfo::TRANS_STRING;
	prnum_info[no]->num_of_cells = 1;
	prnum_info[no]->setCell(0);
	prnum_info[no]->color_list = new uchar3[prnum_info[no]->num_of_cells];

	prnum_info[no]->param      = script_h.readInt();
	prnum_info[no]->orig_pos.x = script_h.readInt();
	prnum_info[no]->orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(prnum_info[no]);
	prnum_info[no]->font_size_xy[0] = script_h.readInt();
	prnum_info[no]->font_size_xy[1] = script_h.readInt();

	const char *buf = readColorStr();
	readColor(&prnum_info[no]->color_list[0], buf);

	char num_buf[7];
	// Use fullwidth digits
	script_h.getStringFromInteger(num_buf, prnum_info[no]->param, 3, false, true);
	script_h.setStr(&prnum_info[no]->file_name, num_buf);

	setupAnimationInfo(prnum_info[no]);
	dirty_rect_hud.add(prnum_info[no]->pos);

	return RET_CONTINUE;
}

int ONScripter::printCommand() {
	// Make sure the previous asynchronous effect is over
	if (effect_current) {
		event_mode = IDLE_EVENT_MODE;
		do {
			waitEvent(0);
		} while (effect_current);
	}

	bool async = false;
	if (script_h.isName("print2"))
		async = true;

	EffectLink *el = parseEffect(true);

	constantRefreshEffect(el, true, async);

	return RET_CONTINUE;
}

int ONScripter::playCommand() {
	bool loop_flag = true;
	if (script_h.isName("playonce"))
		loop_flag = false;

	const char *buf = script_h.readStr();
	if (buf[0] == '*') {
		cd_play_loop_flag = loop_flag;
		int new_cd_track  = static_cast<int>(std::strtol(buf + 1, nullptr, 0));
		if (current_cd_track != new_cd_track) {
			stopBGM(false);
			current_cd_track = new_cd_track;
			playCDAudio();
		}
	} else { // play MIDI
		stopBGM(false);

		script_h.setStr(&seqmusic_file_name, buf);
		translatePathSlashes(seqmusic_file_name);
		seqmusic_play_loop_flag = loop_flag;
		if (playSoundThreaded(seqmusic_file_name, SOUND_SEQMUSIC, seqmusic_play_loop_flag) != SOUND_SEQMUSIC) {
			sendToLog(LogLevel::Error, "can't play sequenced music file %s\n", seqmusic_file_name);
		}
	}

	return RET_CONTINUE;
}

int ONScripter::ofscopyCommand() {
	//gpu.copyGPUImage(gpu.copyImageFromTarget(screen_target), nullptr, nullptr, accumulation_gpu->target);

	return RET_CONTINUE;
}

int ONScripter::negaCommand() {
	nega_mode[BeforeScene] = nega_mode[AfterScene];
	nega_mode[AfterScene]  = script_h.readInt();

	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);

	return RET_CONTINUE;
}

int ONScripter::mvCommand() {
	char buf[256];

	std::snprintf(buf, sizeof(buf), "voice%c%s.mp3", DELIMITER, script_h.getStringBuffer() + 2);

	script_h.setStr(&music_file_name, buf);

	//don't bother with playback or fadeins if there's no audio
	if (!audio_open_flag)
		return RET_CONTINUE;

	mp3stopCommand();

	playSoundThreaded(buf, SOUND_MUSIC, false, MIX_BGM_CHANNEL);

	return RET_CONTINUE;
}

int ONScripter::mspCommand() {
	leaveTextDisplayMode();

	bool msp2_flag = false;
	if (script_h.isName("msp2"))
		msp2_flag = true;

	int no = script_h.readInt();

	AnimationInfo *si = nullptr;
	if (msp2_flag) {
		si = &sprite2_info[no];
		dirtySpriteRect(no, true);
	} else {
		si = &sprite_info[no];
		dirtySpriteRect(no, false);
	}
	backupState(si);

	int dx = script_h.readInt();
	int dy = script_h.readInt();
	si->orig_pos.x += dx;
	si->orig_pos.y += dy;
	UpdateAnimPosXY(si);

	if (msp2_flag) {
		si->scale_x += script_h.readInt();
		si->scale_y += script_h.readInt();
		si->rot += script_h.readInt();
		si->calcAffineMatrix(window.script_width, window.script_height);
		dirtySpriteRect(no, true);
	} else {
		dirtySpriteRect(no, false);
	}

	if (script_h.hasMoreArgs())
		si->trans += script_h.readInt();
	if (si->trans > 255)
		si->trans = 255;
	else if (si->trans < 0)
		si->trans = 0;

	return RET_CONTINUE;
}

int ONScripter::mp3volCommand() {
	music_volume = script_h.readInt();

	setCurMusicVolume(music_volume);

	return RET_CONTINUE;
}

int ONScripter::mp3stopCommand() {
	stopBGM(false);
	return RET_CONTINUE;
}

//Mion: integrating mp3fadeout as it's supposed to work.
int ONScripter::mp3fadeoutCommand() {
	errorAndExit("mp3fadeout: use bgm properties");

	return RET_CONTINUE;
}

int ONScripter::mp3fadeinCommand() {
	errorAndExit("mp3fadein: use bgm properties");

	return RET_CONTINUE;
}

int ONScripter::mp3Command() {
	bool loop_flag = false;
	if (script_h.isName("mp3save")) {
		mp3save_flag = true;
	} else if (script_h.isName("bgmonce")) {
		mp3save_flag = false;
	} else if (script_h.isName("mp3loop") ||
	           script_h.isName("bgm")) {
		mp3save_flag = true;
		loop_flag    = true;
	} else {
		mp3save_flag = false;
	}

	mp3stopCommand();

	music_play_loop_flag = loop_flag;

	const char *buf = script_h.readFilePath();
	if (buf[0] != '\0') {
		int tmp = music_volume;
		script_h.setStr(&music_file_name, buf);

		//don't bother with playback or fadeins if there's no audio
		if (!audio_open_flag)
			return RET_CONTINUE;

		if (bgmdownmode_flag && wave_sample[0] && Mix_Playing(0)) {
			music_volume /= 2;
		}

		playSoundThreaded(music_file_name,
		                  SOUND_MUSIC | SOUND_SEQMUSIC | SOUND_CHUNK,
		                  music_play_loop_flag, MIX_BGM_CHANNEL);

		music_volume = tmp;
	}

	return RET_CONTINUE;
}

int ONScripter::movieCommand() {
	bool loadNew = script_h.isName("video");
	if (script_h.isName("stopvideo") || loadNew) {
		if (video_layer < 0) {
			errorAndCont("no video layer found");
			//Cleanup
			script_h.readToEol();
			return RET_CONTINUE;
		}

		// Firstly stop any playback
		//TODO: request last frame here
		auto layer = getLayer<MediaLayer>(video_layer);

		while (layer && layer->isPlaying(!loadNew)) {
			request_video_shutdown = true;
			waitEvent(0);
		}

		if (layer && loadNew) {
			//PARAMS: filename, click, no_loop, alpha=0, audio=1, subtitles=0, sub_file=""

			std::string vidfile(script_h.readFilePath());

			video_skip_mode = script_h.readInt() == 1 ? VideoSkip::Normal : VideoSkip::Trap;
			bool loop       = script_h.readInt() == 0;

			int alpha_masked{0}, audio_track{1}, subtitle_track{0};

			for (auto param : {&alpha_masked, &audio_track, &subtitle_track}) {
				if (!(script_h.hasMoreArgs()))
					break;
				*param = script_h.readInt();
			}

			std::string subfile;
			if (script_h.hasMoreArgs()) {
				subfile        = script_h.readFilePath();
				subtitle_track = 0;
			}

			if (!layer->loadVideo(vidfile, audio_track, subtitle_track)) {
				errorAndCont("failed to load the video");
				return RET_CONTINUE;
			}

			if (!layer->loadPresentation(alpha_masked, loop, subfile)) {
				errorAndCont("failed to present the video");
				return RET_CONTINUE;
			}

			layer->startProcessing();

			waitEvent(50); // return to main game loop for a brief moment while we load the first frames
		}
		return RET_CONTINUE;
	}

	sendToLog(LogLevel::Error, "movie and mpegplay commands are not supported, use video instead\n");

	return RET_CONTINUE;
}

int ONScripter::movemousecursorCommand() {
	int x = script_h.readInt();
	int y = script_h.readInt();
	window.translateScriptToWindowCoords(x, y);
	window.setMousePosition(x, y);
	return RET_CONTINUE;
}

int ONScripter::mousemodeCommand() {
	int no = script_h.readInt();
	cursorState(no != 0);

	return RET_CONTINUE;
}

int ONScripter::monocroCommand() {
	leaveTextDisplayMode();

	if (script_h.compareString("off")) {
		script_h.readName();
		monocro_flag[BeforeScene] = monocro_flag[AfterScene];
		monocro_flag[AfterScene]  = false;
	} else {
		monocro_flag[BeforeScene] = monocro_flag[AfterScene];
		monocro_flag[AfterScene]  = true;
		uchar3 color;
		readColor(&color, readColorStr());

		monocro_color[BeforeScene] = monocro_color[AfterScene];

		monocro_color[AfterScene] = {color.x, color.y, color.z, 0xFF};
	}

	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);

	return RET_CONTINUE;
}

int ONScripter::minimizewindowCommand() {
	window.setMinimize(true);
	return RET_CONTINUE;
}

int ONScripter::mesboxCommand() {
	char *msg         = copystr(script_h.readStr());
	const char *title = script_h.readStr();

	window.showSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, msg);
	sendToLog(LogLevel::Info, "Got message box '%s': '%s'\n", title, msg);

	freearr(&msg);

	return RET_CONTINUE;
}

int ONScripter::menu_windowCommand() {
#if !defined(IOS) && !defined(DROID)
	window.changeMode(true, false, 0);
#endif
	return RET_CONTINUE;
}

int ONScripter::menu_fullCommand() {
	window.changeMode(true, false, 1);
	return RET_CONTINUE;
}

int ONScripter::menu_waveonCommand() {
	volume_on_flag = true;
	sendToLog(LogLevel::Info, "menu_waveon: setting main volume to on\n");

	return RET_CONTINUE;
}

int ONScripter::menu_waveoffCommand() {
	volume_on_flag = false;
	sendToLog(LogLevel::Info, "menu_waveoff: setting main volume to off\n");

	return RET_CONTINUE;
}

int ONScripter::menu_click_pageCommand() {
	errorAndExit("menu_click_page: page-at-once mode unsupported");

	return RET_CONTINUE;
}

int ONScripter::menu_click_defCommand() {
	errorAndExit("menu_click_def: page-at-once mode unsupported");

	return RET_CONTINUE;
}

int ONScripter::menu_automodeCommand() {
	automode_flag = true;
	skip_mode &= ~SKIP_NORMAL;
	sendToLog(LogLevel::Info, "menu_automode: change to automode\n");

	return RET_CONTINUE;
}

int ONScripter::lsp2Command() {
	leaveTextDisplayMode();

	bool v = true;

	if (script_h.isName("lsph2") || script_h.isName("lsph2add") ||
	    script_h.isName("lsph2sub") || script_h.isName("lsph2mul"))
		v = false;

	BlendModeId blend_mode = BlendModeId::NORMAL;
	if (script_h.isName("lsp2add") || script_h.isName("lsph2add"))
		blend_mode = BlendModeId::ADD;
	else if (script_h.isName("lsp2sub") || script_h.isName("lsph2sub"))
		blend_mode = BlendModeId::SUB;
	else if (script_h.isName("lsp2mul") || script_h.isName("lsph2mul"))
		blend_mode = BlendModeId::MUL;

	bool big_image = script_h.isName("lbsp2");

	int no = validSprite(script_h.readInt());
	backupState(&sprite2_info[no]);
	if (sprite2_info[no].exists && sprite2_info[no].visible)
		dirtySpriteRect(no, true);

	const char *buf = script_h.readStr();
	sprite2_info[no].setImageName(buf);
	parseTaggedString(&sprite2_info[no]);

	sprite2_info[no].visible       = v;
	sprite2_info[no].blending_mode = blend_mode;
	sprite2_info[no].is_big_image  = big_image;

	sprite2_info[no].orig_pos.x = script_h.readInt();
	sprite2_info[no].orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(&sprite2_info[no]);

	if (script_h.hasMoreArgs()) {
		sprite2_info[no].scale_x = script_h.readInt();
		sprite2_info[no].scale_y = script_h.readInt();
	} else {
		sprite2_info[no].scale_x = 100;
		sprite2_info[no].scale_y = 100;
	}

	if (script_h.hasMoreArgs())
		sprite2_info[no].rot = script_h.readInt();
	else
		sprite2_info[no].rot = 0;

	if (script_h.hasMoreArgs())
		sprite2_info[no].trans = script_h.readInt();
	else
		sprite2_info[no].trans = 255;

	sprite2_info[no].flip = FLIP_NONE;

	setupAnimationInfo(&sprite2_info[no]);
	postSetupAnimationInfo(&sprite2_info[no]);

	return RET_CONTINUE;
}

int ONScripter::lspCommand() {

	//printClock("lspCommand start");

	leaveTextDisplayMode();

	bool v = true;

	if (script_h.isName("lsph"))
		v = false;

	int no = validSprite(script_h.readInt());

	backupState(&sprite_info[no]);

	if (sprite_info[no].exists && sprite_info[no].visible)
		dirtySpriteRect(no, false);

	const char *buf = script_h.readStr();
	sprite_info[no].setImageName(buf);

	parseTaggedString(&sprite_info[no]);

	bool is_reuseable = true;
	if ((sprite_info[no].trans_mode == AnimationInfo::TRANS_STRING) ||
	    (sprite_info[no].trans_mode == AnimationInfo::TRANS_LAYER)) {
		//let's see if the same sprite has been loaded recently, for reuse,
		//but don't bother for string sprites, since they can get messed up
		//if the image_name contains a string variable, or for layers,
		//since they aren't meant to be static images
		is_reuseable = false;
	}

	if (sprite_info[no].stale_image && is_reuseable) {
		int x = last_loaded_sprite_ind;
		for (int i = 0; i < SPRITE_NUM_LAST_LOADS; i++) {
			if (last_loaded_sprite[x] < 0)
				continue;
			AnimationInfo *anim = &sprite_info[last_loaded_sprite[x]];
			if (!anim->stale_image &&
			    treatAsSameImage(*anim, sprite_info[no])) {
				sprite_info[no].deepcopy(*anim);
				sprite_info[no].current_cell = 0;
				sprite_info[no].direction    = 1;
				sprite_info[no].stale_image  = false;
				break;
			}
			x += SPRITE_NUM_LAST_LOADS - 1;
			x %= SPRITE_NUM_LAST_LOADS;
		}
	}

	sprite_info[no].visible    = v;
	sprite_info[no].orig_pos.x = script_h.readInt();
	sprite_info[no].orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(&sprite_info[no]);
	if (script_h.hasMoreArgs())
		sprite_info[no].trans = script_h.readInt();
	else
		sprite_info[no].trans = 255;

	sprite_info[no].flip = FLIP_NONE;

	setupAnimationInfo(&sprite_info[no], nullptr);

	if (is_reuseable) {
		//only save the index of reuseable sprites
		last_loaded_sprite_ind                     = (1 + last_loaded_sprite_ind) % SPRITE_NUM_LAST_LOADS;
		last_loaded_sprite[last_loaded_sprite_ind] = no;
	}

	if (sprite_info[no].visible)
		dirtySpriteRect(no, false);

	//printClock("lspCommand end");

	return RET_CONTINUE;
}

int ONScripter::loopbgmstopCommand() {
	if (wave_sample[MIX_LOOPBGM_CHANNEL0]) {
		Mix_Pause(MIX_LOOPBGM_CHANNEL0);
		wave_sample[MIX_LOOPBGM_CHANNEL0] = nullptr;
	}
	if (wave_sample[MIX_LOOPBGM_CHANNEL1]) {
		Mix_Pause(MIX_LOOPBGM_CHANNEL1);
		wave_sample[MIX_LOOPBGM_CHANNEL1] = nullptr;
	}
	script_h.setStr(&loop_bgm_name[0], nullptr);

	return RET_CONTINUE;
}

int ONScripter::loopbgmCommand() {
	const char *buf = script_h.readFilePath();
	script_h.setStr(&loop_bgm_name[0], buf);
	buf = script_h.readFilePath();
	script_h.setStr(&loop_bgm_name[1], buf);

	playSoundThreaded(loop_bgm_name[1], SOUND_PRELOAD | SOUND_CHUNK, false, MIX_LOOPBGM_CHANNEL1);
	playSoundThreaded(loop_bgm_name[0], SOUND_CHUNK, false, MIX_LOOPBGM_CHANNEL0);

	return RET_CONTINUE;
}

int ONScripter::lookbackflushCommand() {
	clearCurrentPage();

	return RET_CONTINUE;
}

int ONScripter::lookbackbuttonCommand() {
	for (int i = 0; i < 4; i++)
		script_h.readStr();
	return RET_CONTINUE;
}

int ONScripter::logspCommand() {
	leaveTextDisplayMode();

	bool logsp2_flag = false;

	if (script_h.isName("logsp2"))
		logsp2_flag = true;

	int sprite_no = script_h.readInt();

	backupState(&sprite_info[sprite_no]);

	AnimationInfo &si = sprite_info[sprite_no];
	if (si.exists && si.visible)
		dirtySpriteRect(sprite_no, false);
	si.remove();
	script_h.setStr(&si.file_name, script_h.readFilePath());

	si.orig_pos.x = script_h.readInt();
	si.orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(&si);

	si.trans_mode = AnimationInfo::TRANS_STRING;
	if (logsp2_flag) {
		si.font_size_xy[0] = script_h.readInt();
		si.font_size_xy[1] = script_h.readInt();
		script_h.readInt(); // dummy read for x pitch
		script_h.readInt(); // dummy read for y pitch
	} else {
		si.font_size_xy[0] = si.font_size_xy[1] = sentence_font.style().font_size;
	}

	const char *current = script_h.getNext();
	int num             = 0;
	while (script_h.hasMoreArgs()) {
		script_h.readStr();
		num++;
	}

	script_h.setCurrent(current);
	if (num == 0) {
		si.num_of_cells = 1;
		si.color_list   = new uchar3[si.num_of_cells];
		readColor(&si.color_list[0], "#ffffff");
	} else {
		si.num_of_cells = num;
		si.color_list   = new uchar3[si.num_of_cells];
		for (int i = 0; i < num; i++) {
			readColor(&si.color_list[i], readColorStr());
		}
	}

	si.skip_whitespace = false;
	setupAnimationInfo(&si);
	si.visible = true;
	dirtySpriteRect(sprite_no, false);

	return RET_CONTINUE;
}

int ONScripter::locateCommand() {
	errorAndExit("locate: Despite your best efforts, you find nothing");

	return RET_CONTINUE;
}

// Supporting this with both {x,y} as pixels will require introducing a y_px parameter.
// Currently our y position works on cur_xy[1] * lineHeight(), so locating to y in pixels is impossible
// as there is no property to set to the new y value.
int ONScripter::locatePxCommand() {
	return 0;
}

int ONScripter::loadgameCommand() {
	int no = script_h.readInt();

	if (no < 0)
		errorAndExit("loadgame: save number is less than 0.");

	// Avoid accidental repaints before entering the loadgosub
	skip_mode = SKIP_NORMAL | SKIP_SUPERSKIP;

	if (!loadSaveFile(no)) {
		fillCanvas(true, true);
		commitVisualState();
		flush(refreshMode());

		saveon_flag          = true;
		internal_saveon_flag = true;
		skip_mode &= ~SKIP_NORMAL;
		automode_flag = false;
		deleteButtonLink();
		deleteSelectLink();
		keyState.pressedFlag = false;
		page_enter_status    = 0;
		string_buffer_offset = 0;
		break_flag           = false;

		refreshButtonHoverState();

		if (loadgosub_label) {
			should_flip = 0;
			gosubReal(loadgosub_label, script_h.getCurrent());
		}
	}

	skip_mode = 0;

	return RET_CONTINUE;
}

int ONScripter::linkcolorCommand() {
	const char *buf;

	buf = readColorStr();
	readColor(&linkcolor[0], buf);
	buf = readColorStr();
	readColor(&linkcolor[1], buf);

	return RET_CONTINUE;
}

int ONScripter::ldCommand() {
	leaveTextDisplayMode();

	char loc = script_h.readName()[0];
	int no   = -1;
	if (loc == 'l')
		no = 0;
	else if (loc == 'c')
		no = 1;
	else if (loc == 'r')
		no = 2;

	const char *buf = nullptr;

	if (no >= 0) {
		buf = script_h.readStr();
		if (tachi_info[no].gpu_image)
			dirty_rect_scene.add(tachi_info[no].pos);
		backupState(&tachi_info[no]);
		tachi_info[no].setImageName(buf);
		parseTaggedString(&tachi_info[no]);

		setupAnimationInfo(&tachi_info[no]);

		if (tachi_info[no].gpu_image) {
			tachi_info[no].visible = true;
			//start with "orig_pos" at the center-bottom, for easier scaling
			tachi_info[no].orig_pos.x = humanpos[no];
			tachi_info[no].orig_pos.y = underline_value + 1;
			UpdateAnimPosXY(&tachi_info[no]);
			tachi_info[no].pos.x -= tachi_info[no].pos.w / 2;
			tachi_info[no].pos.y -= tachi_info[no].pos.h;
			tachi_info[no].orig_pos.x -= tachi_info[no].orig_pos.w / 2;
			tachi_info[no].orig_pos.y -= tachi_info[no].orig_pos.h;
			dirty_rect_scene.add(tachi_info[no].pos);
		}
	}

	EffectLink *el = parseEffect(true);
	constantRefreshEffect(el, true);
	return RET_CONTINUE;
}

int ONScripter::layermessageCommand() {
	int no              = script_h.readInt();
	const char *message = script_h.readStr();

	getLayer<Layer>(no)->message(message, getret_int);
	//sendToLog(LogLevel::Info, "layermessage returned: '%s', %d\n", getret_str, getret_int);

	return RET_CONTINUE;
}

int ONScripter::languageCommand() {
	const char *which = script_h.readName();
	if (equalstr(which, "japanese")) {
		script_language = ScriptLanguage::Japanese;
	} else if (equalstr(which, "english")) {
		script_language = ScriptLanguage::English;
	} else {
		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "language: unknown language '%s'", which);
		errorAndExit(script_h.errbuf, "valid options are 'japanese' and 'english'");
	}
	return RET_CONTINUE;
}

int ONScripter::jumpfCommand() {
	jumpToTilde(true);
	return RET_CONTINUE;
}

int ONScripter::jumpbCommand() {
	script_h.setCurrent(last_tilde.next_script);
	current_label_info = script_h.getLabelByAddress(last_tilde.next_script);
	current_line       = script_h.getLineByAddress(last_tilde.next_script, current_label_info);

	return RET_CONTINUE;
}

int ONScripter::ispageCommand() {
	script_h.readInt();

	if (textgosub_clickstr_state == CLICK_NEWPAGE)
		script_h.setInt(&script_h.current_variable, 1);
	else
		script_h.setInt(&script_h.current_variable, 0);

	return RET_CONTINUE;
}

int ONScripter::isfullCommand() {
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, window.getFullscreen());

	return RET_CONTINUE;
}

int ONScripter::isskipCommand() {
	script_h.readInt();

	if (automode_flag)
		script_h.setInt(&script_h.current_variable, 2);
	else if (skip_mode & SKIP_NORMAL)
		script_h.setInt(&script_h.current_variable, 1);
	else if (keyState.ctrl)
		script_h.setInt(&script_h.current_variable, 3);
	else
		script_h.setInt(&script_h.current_variable, 0);

	return RET_CONTINUE;
}

int ONScripter::isdownCommand() {
	script_h.readInt();

	if (current_button_state.down_flag)
		script_h.setInt(&script_h.current_variable, 1);
	else
		script_h.setInt(&script_h.current_variable, 0);

	return RET_CONTINUE;
}

int ONScripter::inputCommand() {
	script_h.readStr();

	if (script_h.current_variable.type != VariableInfo::TypeStr)
		errorAndExit("input: no string variable.");
	int no = script_h.current_variable.var_no;

	script_h.readStr();                   // description
	const char *buf = script_h.readStr(); // default value
	script_h.setStr(&script_h.getVariableData(no).str, buf);

	sendToLog(LogLevel::Info, "*** inputCommand(): $%d is set to the default value: %s\n",
	          no, buf);
	script_h.readInt(); // maxlen
	script_h.readInt(); // widechar flag
	if (script_h.hasMoreArgs()) {
		script_h.readInt(); // window width
		script_h.readInt(); // window height
		script_h.readInt(); // text box width
		script_h.readInt(); // text box height
	}

	return RET_CONTINUE;
}

int ONScripter::indentCommand() {
	script_h.readInt();

	return RET_CONTINUE;
}

int ONScripter::humanorderCommand() {
	leaveTextDisplayMode();

	const char *buf = script_h.readStr();
	int i;
	for (i = 0; i < 3; i++) {
		if (buf[i] == 'l')
			human_order[i] = 0;
		else if (buf[i] == 'c')
			human_order[i] = 1;
		else if (buf[i] == 'r')
			human_order[i] = 2;
		else
			human_order[i] = -1;
	}

	for (i = 0; i < 3; i++)
		if (tachi_info[i].gpu_image) {
			backupState(&tachi_info[i]);
			dirty_rect_scene.add(tachi_info[i].pos);
		}

	EffectLink *el = parseEffect(true);
	constantRefreshEffect(el, true);
	return RET_CONTINUE;
}

int ONScripter::getzxcCommand() {
	getzxc_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getvoicevolCommand() {
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, voice_volume);
	return RET_CONTINUE;
}

int ONScripter::getversionCommand() {
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, NSC_VERSION);

	return RET_CONTINUE;
}

int ONScripter::gettimerCommand() {
	bool gettimer_flag = false;

	if (script_h.isName("gettimer", true)) {
		gettimer_flag = true;
	} else if (script_h.isName("getbtntimer", true)) {
	}

	script_h.readInt();

	if (gettimer_flag) {
		script_h.setInt(&script_h.current_variable, SDL_GetTicks() - internal_timer);
	} else {
		script_h.setInt(&script_h.current_variable, btnwait_time);
	}

	return RET_CONTINUE;
}

int ONScripter::gettextbtnstrCommand() {
	script_h.readVariable();
	script_h.pushVariable();

	int txtbtn_no = script_h.readInt();

	TextButtonInfoLink *info  = text_button_info.next;
	TextButtonInfoLink *found = nullptr;
	while (info) {
		if (info->no == txtbtn_no)
			found = info;
		info = info->next;
	}

	if (found)
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, found->text);
	else
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, nullptr);

	return RET_CONTINUE;
}

int ONScripter::gettextCommand() {
	errorAndExit("gettext: what next?!");

	return RET_CONTINUE;
}

int ONScripter::gettaglogCommand() {
	errorAndExit("gettaglog: no Tagalog translation offered, sorry");

	return RET_CONTINUE;
}

int ONScripter::gettagCommand() {
	errorAndExit("gettag: not supported");

	return RET_CONTINUE;
}

int ONScripter::gettabCommand() {
	gettab_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getspsizeCommand() {
	bool lsp2 = script_h.isName("getspsize2");
	int no    = validSprite(script_h.readInt());

	AnimationInfo &sprite = lsp2 ? sprite2_info[no] : sprite_info[no];

	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, sprite.orig_pos.w);
	script_h.readVariable();
	script_h.setInt(&script_h.current_variable, sprite.orig_pos.h);
	if (script_h.hasMoreArgs()) {
		script_h.readVariable();
		script_h.setInt(&script_h.current_variable, sprite.num_of_cells);
	}

	return RET_CONTINUE;
}

int ONScripter::getspmodeCommand() {
	script_h.readVariable();
	script_h.pushVariable();

	int no = validSprite(script_h.readInt());
	script_h.setInt(&script_h.pushed_variable, sprite_info[no].visible ? 1 : 0);

	return RET_CONTINUE;
}

int ONScripter::getskipoffCommand() {
	getskipoff_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getsevolCommand() {
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, se_volume);
	return RET_CONTINUE;
}

int ONScripter::getscreenshotCommand() {
	int w = script_h.readInt();
	int h = script_h.readInt();

	if (w == 0)
		w = 1;
	if (h == 0)
		h = 1;

	if (!screenshot_gpu || screenshot_gpu->w != w || screenshot_gpu->h != h) {
		if (screenshot_gpu)
			gpu.freeImage(screenshot_gpu);
		screenshot_gpu = gpu.createImage(w, h, 4);
		GPU_GetTarget(screenshot_gpu);
	}

	GPU_Image *script_image = gpu.createImage(window.script_width, window.script_height, 3);
	GPU_GetTarget(script_image);
	auto combined_camera = camera.center_pos;
	combined_camera.x -= camera.pos.x;
	combined_camera.y -= camera.pos.y;

	gpu.copyGPUImage(accumulation_gpu, &combined_camera, nullptr, script_image->target);
	gpu.copyGPUImage(hud_gpu, &camera.center_pos, nullptr, script_image->target);

	float scale_x = w / static_cast<float>(window.script_width);
	float scale_y = h / static_cast<float>(window.script_height);

	if (scale_x < 1 || scale_y < 1) {
		GPU_FlushBlitBuffer();
		GPU_GenerateMipmaps(script_image);
		GPU_SetImageFilter(script_image, GPU_FILTER_LINEAR_MIPMAP);
		GPU_FlushBlitBuffer(); // Just in case
		gpu.copyGPUImage(script_image, nullptr, nullptr, screenshot_gpu->target, w / 2.0, h / 2.0, scale_x, scale_y, 0, true);
		GPU_FreeImage(script_image); // It is not safe to reuse this image in case of mipmaps (SDL_gpu bug?)
	} else {
		gpu.copyGPUImage(script_image, nullptr, nullptr, screenshot_gpu->target, w / 2.0, h / 2.0, scale_x, scale_y, 0, true);
		gpu.freeImage(script_image);
	}

	return RET_CONTINUE;
}

int ONScripter::getsavestrCommand() {
	script_h.readVariable();
	if (script_h.current_variable.type != VariableInfo::TypeStr)
		errorAndExit("getsavestr: no string variable");

	int var_no = script_h.current_variable.var_no;
	int no     = script_h.readInt();

	SaveFileInfo info;
	if (!readSaveFileHeader(no, &info)) {
		sendToLog(LogLevel::Info, "getsavestr: couldn't read save slot %d\n", no);
	}

	if (info.descr)
		script_h.setStr(&script_h.getVariableData(var_no).str, info.descr.get());
	else
		script_h.setStr(&script_h.getVariableData(var_no).str, "");
	//sendToLog(LogLevel::Info, "getsavestr: got '%s'\n", script_h.getVariableData(var_no).str);

	return RET_CONTINUE;
}

int ONScripter::getpageupCommand() {
	getpageup_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getpageCommand() {
	getpageup_flag   = true;
	getpagedown_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getretCommand() {
	script_h.readVariable();

	if (script_h.current_variable.type == VariableInfo::TypeInt ||
	    script_h.current_variable.type == VariableInfo::TypeArray) {
		script_h.setInt(&script_h.current_variable, getret_int);
	} else if (script_h.current_variable.type == VariableInfo::TypeStr) {
		int no = script_h.current_variable.var_no;
		script_h.setStr(&script_h.getVariableData(no).str, getret_str);
	} else
		errorAndExit("getret: no variable.");

	return RET_CONTINUE;
}

int ONScripter::getregCommand() {
	script_h.readVariable();

	if (script_h.current_variable.type != VariableInfo::TypeStr)
		errorAndExit("getreg: no string variable.");
	int no = script_h.current_variable.var_no;

	std::string sect(script_h.readStr());
	std::string key(script_h.readStr());

	sendToLog(LogLevel::Info, "Reading registry file for [%s] %s\n", sect.c_str(), key.c_str());

	IniContainer container;
	if (readIniFile(registry_file, container)) {
		auto sectv = container.find(sect);
		if (sectv != container.end()) {
			auto keyv = sectv->second.find(key);
			if (keyv != sectv->second.end()) {
				script_h.setStr(&script_h.getVariableData(no).str, keyv->second.c_str());
				return RET_CONTINUE;
			}
		}
	}

	sendToLog(LogLevel::Info, "  The key is not found.\n");
	//Is unchanged value the way they performed error checking? :x
	//script_h.setStr(&script_h.getVariableData(no).str, "");

	return RET_CONTINUE;
}

int ONScripter::getmp3volCommand() {
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, music_volume);
	return RET_CONTINUE;
}

int ONScripter::getmouseposCommand() {
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, current_button_state.x);

	script_h.readInt();
	script_h.setInt(&script_h.current_variable, current_button_state.y);

	return RET_CONTINUE;
}

int ONScripter::getmouseoverCommand() {
	getmouseover_flag = true;
	getmouseover_min  = script_h.readInt();
	getmouseover_max  = script_h.readInt();

	return RET_CONTINUE;
}

int ONScripter::getmclickCommand() {
	getmclick_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getlogCommand() {

	bool dlgCtrlMode = script_h.isName("getlog2");
	script_h.readVariable();
	script_h.pushVariable();

	if (dlgCtrlMode) {
		script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, dlgCtrl.textPart.c_str());
	} else {
		errorAndExit("getlog: please, use getlog2 instead");
	}

	return RET_CONTINUE;
}

int ONScripter::getinsertCommand() {
	getinsert_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getfunctionCommand() {
	getfunction_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getenterCommand() {
	if (!force_button_shortcut_flag)
		getenter_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getcursorposCommand() {
	if (dlgCtrl.dialogueProcessingState.active && dlgCtrl.dialogueRenderState.segmentIndex != -1) {
		// unsure that second condition is needed here
		auto &segment = dlgCtrl.dialogueRenderState.segments[dlgCtrl.dialogueRenderState.segmentIndex];
		script_h.readInt();
		script_h.setInt(&script_h.current_variable, segment.cursorPosition.x);
		script_h.readInt();
		script_h.setInt(&script_h.current_variable, segment.cursorPosition.y - (wndCtrl.usingDynamicTextWindow ? wndCtrl.extension : 0));
	} else {
		Fontinfo fi(sentence_font);
		if (script_h.isName("getnextline")) {
			fi.newLine();
		}

		script_h.readInt();
		script_h.setInt(&script_h.current_variable, fi.x());

		script_h.readInt();
		script_h.setInt(&script_h.current_variable, fi.y());
	}

	return RET_CONTINUE;
}

int ONScripter::getcursorCommand() {
	if (!force_button_shortcut_flag)
		getcursor_flag = true;

	return RET_CONTINUE;
}

int ONScripter::getcselstrCommand() {
	script_h.readVariable();
	script_h.pushVariable();

	int csel_no = script_h.readInt();

	int counter      = 0;
	SelectLink *link = root_select_link.next;
	while (link) {
		if (csel_no == counter++)
			break;
		link = link->next;
	}
	if (!link) {
		//NScr doesn't exit if getcselstr accesses a non-existent select link,
		//so just give a warning and set the string to null
		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "getcselstr: no select link at index %d (max index is %d)",
		              csel_no, counter - 1);
		errorAndCont(script_h.errbuf);
	}
	script_h.setStr(&script_h.getVariableData(script_h.pushed_variable.var_no).str, link ? (link->text) : nullptr);

	return RET_CONTINUE;
}

int ONScripter::getcselnumCommand() {
	int count = 0;

	SelectLink *link = root_select_link.next;
	while (link) {
		count++;
		link = link->next;
	}
	script_h.readInt();
	script_h.setInt(&script_h.current_variable, count);

	return RET_CONTINUE;
}

int ONScripter::gameCommand() {
	int i;
	current_mode = NORMAL_MODE;
	effectspeed  = EFFECTSPEED_NORMAL;

	/* ---------------------------------------- */
	/* Load default cursor */
	loadCursor(CURSOR_WAIT_NO, DEFAULT_CURSOR_WAIT, 0, 0);
	loadCursor(CURSOR_NEWPAGE_NO, DEFAULT_CURSOR_NEWPAGE, 0, 0);

	clearCurrentPage();

	/* ---------------------------------------- */
	/* Initialize local variables */
	for (i = 0; i < script_h.global_variable_border; i++)
		script_h.getVariableData(i).reset(false);

	setCurrentLabel("start");
	saveSaveFile(-1);

	return RET_CONTINUE;
}

int ONScripter::flushoutCommand() {
	sendToLog(LogLevel::Error, "flushout is unimplemented\n");

	return RET_CONTINUE;
}

int ONScripter::fileexistCommand() {
	script_h.readVariable();
	script_h.pushVariable();

	const char *buf = script_h.readFilePath();
	size_t length   = 0;
	bool found      = script_h.reader->getFile(buf, length);

	script_h.setInt(&script_h.pushed_variable, found);

	if (script_h.hasMoreArgs()) {
		script_h.readVariable();
		script_h.setInt(&script_h.current_variable, static_cast<int32_t>(length));
	}

	return RET_CONTINUE;
}

int ONScripter::exec_dllCommand() {
	std::string dllcmd(script_h.readStr());

	auto param = dllcmd.find('/');
	if (param == std::string::npos)
		param = dllcmd.size();
	std::string dllname(dllcmd, 0, param);

	sendToLog(LogLevel::Info, "Reading %s for [%s]\n", dll_file, dllcmd.c_str());

	IniContainer container;
	if (readIniFile(dll_file, container)) {
		auto sectv = container.find(dllcmd);
		if (sectv == container.end())
			sectv = container.find(dllname);
		if (sectv != container.end()) {
			auto &values = sectv->second;
			auto keystr  = values.find("str");
			bool has     = false;
			if (keystr != values.end()) {
				script_h.setStr(&getret_str, keystr->second.c_str());
				sendToLog(LogLevel::Info, "  getret_str = %s\n", getret_str);
				has = true;
			}
			auto keyint = values.find("ret");
			if (keyint != values.end()) {
				getret_int = std::stoi(keyint->second);
				sendToLog(LogLevel::Info, "  getret_int = %d\n", getret_int);
				has = true;
			}
			if (has)
				return RET_CONTINUE;
		}
	}

	sendToLog(LogLevel::Info, "  The DLL is not found in %s.\n", dll_file);
	return RET_CONTINUE;
}

int ONScripter::exbtnCommand() {
	int sprite_no = -1, no = 0;
	ButtonLink *button;

	if (script_h.isName("exbtn_d")) {
		button = &exbtn_d_button_link;
		delete[] button->exbtn_ctl;
	} else {
		bool cellcheck_flag = false;

		if (script_h.isName("cellcheckexbtn"))
			cellcheck_flag = true;

		sprite_no = script_h.readInt();
		no        = script_h.readInt();

		if ((cellcheck_flag && (sprite_info[sprite_no].num_of_cells < 2)) ||
		    (!cellcheck_flag && (sprite_info[sprite_no].num_of_cells == 0))) {
			script_h.readStr();
			return RET_CONTINUE;
		}

		button = new ButtonLink();
		root_button_link.insert(button);
	}
	is_exbtn_enabled = true;

	const char *buf = script_h.readStr();

	button->button_type = ButtonLink::EX_SPRITE_BUTTON;
	button->sprite_no   = sprite_no;
	button->no          = no;
	button->exbtn_ctl   = copystr(buf);

	if (sprite_no >= 0 &&
	    (sprite_info[sprite_no].gpu_image ||
	     sprite_info[sprite_no].trans_mode == AnimationInfo::TRANS_STRING)) {
		button->image_rect = button->select_rect = sprite_info[sprite_no].pos;
	}

	return RET_CONTINUE;
}

int ONScripter::erasetextwindowCommand() {
	erase_text_window_mode = script_h.readInt();
	did_leavetext          = false;

	return RET_CONTINUE;
}

int ONScripter::erasetextbtnCommand() {
	if (!txtbtn_visible)
		return RET_CONTINUE;

	TextButtonInfoLink *info = text_button_info.next;
	while (info) {
		ButtonLink *cur_button_link = info->button;
		while (cur_button_link) {
			cur_button_link->show_flag     = true;
			cur_button_link->anim->visible = true;
			cur_button_link->anim->setCell(0);
			dirty_rect_hud.add(cur_button_link->image_rect);
			cur_button_link = cur_button_link->same;
		}
		info = info->next;
	}
	commitVisualState();
	flush(refreshMode());

	return RET_CONTINUE;
}

int ONScripter::endCommand() {
#if defined(IOS) || defined(DROID)
	window.showSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "ONScripter-RU", "The game will close now...");
#endif
	sendToLog(LogLevel::Info, "Quitting...\n");
	requestQuit(ExitType::Normal);
	return RET_CONTINUE; // dummy
}

int ONScripter::effectskipCommand() {
	effectskip_flag = script_h.readInt();

	return RET_CONTINUE;
}

int ONScripter::dwavestopCommand() {
	stopDWAVE(script_h.readInt());

	return RET_CONTINUE;
}

int ONScripter::dwaveCommand() {
	int play_mode  = WAVE_PLAY;
	bool loop_flag = false;

	if (script_h.isName("dwaveloop")) {
		loop_flag = true;
	} else if (script_h.isName("dwaveload")) {
		play_mode = WAVE_PRELOAD;
	} else if (script_h.isName("dwaveplayloop")) {
		play_mode = WAVE_PLAY_LOADED;
		loop_flag = true;
	} else if (script_h.isName("dwaveplay")) {
		play_mode = WAVE_PLAY_LOADED;
		loop_flag = false;
	}

	auto ch = validChannel(script_h.readInt());
	if (play_mode == WAVE_PLAY_LOADED) {
		if (!audio_open_flag)
			return RET_CONTINUE;
		if (lipsChannels[ch].has()) {
			LipsAnimationAction *lipsAction = LipsAnimationAction::create();
			lipsAction->channel             = ch;
			{
				Lock lock(&ons.registeredCRActions);
				registeredCRActions.emplace_back(lipsAction);
			}
			if (wave_sample[ch] == nullptr)
				errorAndExit("Cannot play a not loaded channel");
			Mix_PlayChannel(ch, wave_sample[ch]->chunk, loop_flag ? -1 : 0);
			lipsChannels[ch].get().lipsData.speechStart = SDL_GetTicks();
		} else {
			if (wave_sample[ch] == nullptr)
				errorAndExit("Cannot play a not loaded channel");
			Mix_PlayChannel(ch, wave_sample[ch]->chunk, loop_flag ? -1 : 0);
		}
	} else {
		const char *buf = script_h.readFilePath();
		if (!audio_open_flag)
			return RET_CONTINUE;
		int fmt               = SOUND_CHUNK;
		channel_preloaded[ch] = false;
		stopDWAVE(ch);
		if (play_mode == WAVE_PRELOAD) {
			fmt |= SOUND_PRELOAD;
			channel_preloaded[ch] = true;
		}
		playSoundThreaded(buf, fmt, loop_flag, ch);
	}
	if ((ch == 0) && bgmdownmode_flag && (play_mode != WAVE_PRELOAD))
		setCurMusicVolume(music_volume);

	return RET_CONTINUE;
}

int ONScripter::dvCommand() {
	char buf[256];

	std::sprintf(buf, "voice%c%s.wav", DELIMITER, script_h.getStringBuffer() + 2);
	playSoundThreaded(buf, SOUND_CHUNK, false, 0);

	return RET_CONTINUE;
}

int ONScripter::drawtextCommand() {
	float x{0}, y{0};
	if (!canvasTextWindow) {
		x += camera.center_pos.x;
		y += camera.center_pos.y;
	}
	gpu.copyGPUImage(text_gpu, nullptr, nullptr, hud_gpu->target, x, y);

	return RET_CONTINUE;
}

int ONScripter::drawsp3Command() {
	/*int sprite_no = */ script_h.readInt();
	/*int cell_no = */ script_h.readInt();
	/*int alpha = */ script_h.readInt();

	/*int x = */ script_h.readInt();
	/*int y = */ script_h.readInt();

	script_h.readInt();
	script_h.readInt();
	script_h.readInt();
	script_h.readInt();

	errorAndCont("drawsp3 is currently unsupported");

	return RET_CONTINUE;
}

int ONScripter::drawsp2Command() {
	int sprite_no = script_h.readInt();
	int cell_no   = script_h.readInt();
	int alpha     = script_h.readInt();

	AnimationInfo si(sprite_info[sprite_no]);
	si.orig_pos.x = script_h.readInt();
	si.orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(&si);
	si.scale_x = script_h.readInt();
	si.scale_y = script_h.readInt();
	si.rot     = script_h.readInt();
	si.trans   = alpha;
	si.visible = true;
	si.calcAffineMatrix(window.script_width, window.script_height);
	si.setCell(cell_no);

	drawToGPUTarget(draw_gpu->target, &si, refreshMode(), nullptr);

	return RET_CONTINUE;
}

int ONScripter::drawspCommand() {
	int sprite_no = script_h.readInt();
	int cell_no   = script_h.readInt();
	int alpha     = script_h.readInt();
	int x         = script_h.readInt();
	int y         = script_h.readInt();

	AnimationInfo &si = sprite_info[sprite_no];
	if (!si.gpu_image)
		return RET_CONTINUE;
	int old_cell_no = si.current_cell;
	si.visible      = true;
	si.setCell(cell_no);
	GPU_Rect pos{si.current_cell * si.pos.w, 0, si.pos.w, si.pos.h};
	if (alpha < 255)
		GPU_SetRGBA(si.gpu_image, alpha, alpha, alpha, alpha);
	gpu.copyGPUImage(si.gpu_image, &pos, nullptr, draw_gpu->target, x, y);
	si.setCell(old_cell_no);
	if (alpha < 255)
		GPU_SetRGBA(si.gpu_image, 255, 255, 255, 255);

	return RET_CONTINUE;
}

int ONScripter::drawfillCommand() {
	int r = script_h.readInt();
	int g = script_h.readInt();
	int b = script_h.readInt();

	if (!draw_gpu) {
		draw_gpu = gpu.createImage(window.script_width, window.script_height, 4);
		GPU_GetTarget(draw_gpu);
	}

	gpu.clearWholeTarget(draw_gpu->target, r, g, b, 0xFF);

	return RET_CONTINUE;
}

int ONScripter::drawendCommand() {
	unloadDrawImages();

	return RET_CONTINUE;
}

int ONScripter::drawclearCommand() {
	clearDrawImages(0, 0, 0, false);

	return RET_CONTINUE;
}

int ONScripter::drawbgCommand() {
	loadDrawImages();

	drawToGPUTarget(draw_gpu->target, &bg_info, refreshMode(), nullptr);

	display_draw = true;

	return RET_CONTINUE;
}

int ONScripter::drawbg2Command() {
	AnimationInfo bi(bg_info);
	bi.orig_pos.x = script_h.readInt();
	bi.orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(&bi);
	bi.scale_x = script_h.readInt();
	bi.scale_y = script_h.readInt();
	bi.rot     = script_h.readInt();
	bi.calcAffineMatrix(window.script_width, window.script_height);

	loadDrawImages();

	drawToGPUTarget(draw_gpu->target, &bi, refreshMode(), nullptr);

	return RET_CONTINUE;
}

int ONScripter::drawCommand() {
	gpu.copyGPUImage(draw_gpu, nullptr, nullptr, draw_screen_gpu->target);

	display_draw = true;

	repaintCommand();

	return RET_CONTINUE;
}

int ONScripter::deletescreenshotCommand() {
	if (screenshot_gpu) {
		gpu.freeImage(screenshot_gpu);
		screenshot_gpu = nullptr;
	}
	return RET_CONTINUE;
}

int ONScripter::delayCommand() {
	int count          = script_h.readInt();
	int requestedCount = count;

	//Mion: use a shorter delay during skip mode
	if ((skip_mode & (SKIP_NORMAL | SKIP_TO_WAIT)) || keyState.ctrl) {
		count = 0;
	}

	if (skip_mode & SKIP_SUPERSKIP)
		count = 0;
	if (count == 0)
		return RET_CONTINUE;

	DelayAction *action{DelayAction::create()};

	if (requestedCount > count) {
		action->advanceProperties = requestedCount;
	}

	action->clock.setCountdown(count);
	action->event_mode = WAIT_DELAY_MODE;

	Lock lock(&ons.registeredCRActions);
	registeredCRActions.emplace_back(action);
	return RET_CONTINUE;
}

int ONScripter::defineresetCommand() {
	//clear out the event queue
	updateEventQueue();
	if (takeEventsOut(SDL_QUIT))
		endCommand();
	localEventQueue.clear();

	if (ons.initialised())
		saveAll();

	if (reg_loaded) {
		registry.clear();
		reg_loaded = false;
	}

	video_layer = -1;

	script_h.reset();
	ScriptParser::reset();
	reset();
	reopenAudioOnMismatch(default_audio_format);

	for (auto &textTree : dataTrees)
		textTree.clear();

	setCurrentLabel("define");

	return RET_CONTINUE;
}

int ONScripter::cspCommand() {
	leaveTextDisplayMode();

	bool csp2_flag        = script_h.isName("csp2");
	AnimationInfo *s_info = csp2_flag ? sprite2_info : sprite_info;

	int no1, no2;
	no1 = no2 = script_h.readInt();

	if (script_h.hasMoreArgs()) {
		no2 = script_h.readInt();
		if (no2 < no1)
			std::swap(no1, no2);
	}

	std::function<void(AnimationInfo &)> killSprite = [this, csp2_flag](AnimationInfo &sp) {
		backupState(&sp);
		if (sp.exists && sp.visible) {
			dirtySpriteRect(sp.id, csp2_flag);
		}
		if (!csp2_flag) {
			root_button_link.removeSprite(sp.id);
			previouslyHoveredButtonLink = nullptr;
		}
		sp.remove();
	};

	if (no1 == -1) {
		for (auto sp : sprites(csp2_flag ? SPRITE_LSP2 : SPRITE_LSP))
			killSprite(*sp);
	} else {
		std::for_each(&s_info[validSprite(no1)], &s_info[validSprite(no2) + 1], killSprite);
	}

	return RET_CONTINUE;
}

int ONScripter::cselgotoCommand() {
	int csel_no = script_h.readInt();

	int counter      = 0;
	SelectLink *link = root_select_link.next;
	while (link) {
		if (csel_no == counter++)
			break;
		link = link->next;
	}
	if (!link) {
		std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
		              "cselgoto: no select link at index %d (max index is %d)",
		              csel_no, counter - 1);
		errorAndExit(script_h.errbuf);
		return RET_CONTINUE; //dummy
	}

	setCurrentLabel(link->label);

	deleteSelectLink();
	newPage(true);

	return RET_CONTINUE;
}

int ONScripter::cselbtnCommand() {
	int csel_no   = script_h.readInt();
	int button_no = script_h.readInt();

	Fontinfo csel_info  = sentence_font;
	csel_info.top_xy[0] = script_h.readInt();
	csel_info.top_xy[1] = script_h.readInt();

	int counter      = 0;
	SelectLink *link = root_select_link.next;
	while (link) {
		if (csel_no == counter++)
			break;
		link = link->next;
	}
	if (link == nullptr || link->text == nullptr || *link->text == '\0')
		return RET_CONTINUE;

	//csel_info.setLineArea(std::strlen(link->text)/2+1);
	csel_info.clear();
	ButtonLink *button = getSelectableSentence(link->text, &csel_info);
	root_button_link.insert(button);
	button->no        = button_no;
	button->sprite_no = csel_no;

	return RET_CONTINUE;
}

int ONScripter::clickCommand() {
	if (isWaitingForUserInput() || isWaitingForUserInterrupt()) {
		errorAndExit("Cannot run this command at the moment");
		return RET_CONTINUE; //dummy
	}

	bool lrclick_flag = false;
	if (script_h.isName("lrclick"))
		lrclick_flag = true;

	//Mion: NScr doesn't stop skip-to-choice mode for a "click" command
	if (skip_mode & SKIP_NORMAL)
		return RET_CONTINUE;

	skip_mode &= ~SKIP_TO_WAIT;
	keyState.pressedFlag = false;

	internal_slowdown_counter = 0;
	clickstr_state            = CLICK_WAIT;
	event_mode                = WAIT_TIMER_MODE | WAIT_INPUT_MODE;
	if (lrclick_flag)
		event_mode |= WAIT_RCLICK_MODE;
	waitEvent(-1);
	clickstr_state = CLICK_NONE;

	if (lrclick_flag)
		getret_int = (current_button_state.button == -1) ? 0 : 1;

	return RET_CONTINUE;
}

int ONScripter::clCommand() {
	leaveTextDisplayMode();

	char loc = script_h.readName()[0];

	if (loc == 'l' || loc == 'a') {
		dirty_rect_scene.add(tachi_info[0].pos);
		backupState(&tachi_info[0]);
		tachi_info[0].remove();
	}
	if (loc == 'c' || loc == 'a') {
		dirty_rect_scene.add(tachi_info[1].pos);
		backupState(&tachi_info[1]);
		tachi_info[1].remove();
	}
	if (loc == 'r' || loc == 'a') {
		dirty_rect_scene.add(tachi_info[2].pos);
		backupState(&tachi_info[2]);
		tachi_info[2].remove();
	}

	EffectLink *el = parseEffect(true);
	constantRefreshEffect(el, true);
	return RET_CONTINUE;
}

int ONScripter::chvolCommand() {
	auto ch  = validChannel(script_h.readInt());
	auto vol = validVolume(script_h.readInt());
	setVolume(ch, vol, volume_on_flag);

	return RET_CONTINUE;
}

int ONScripter::checkpageCommand() {
	errorAndExit("checkpage: checked, invalid");

	return RET_CONTINUE;
}

int ONScripter::checkkeyCommand() {
	script_h.readVariable();
	script_h.pushVariable();

	if (script_h.pushed_variable.type != VariableInfo::TypeInt &&
	    script_h.pushed_variable.type != VariableInfo::TypeArray)
		errorAndExit("checkpage: no integer variable.");

	const char *buf = script_h.readStr();
	if (!buf || (*buf == '\0')) {
		script_h.setInt(&script_h.pushed_variable, 0);
		return RET_CONTINUE;
	}
	char *keystr = new char[std::strlen(buf) + 1];

	for (unsigned int i = 0; i < std::strlen(keystr); i++)
		keystr[i] = std::toupper(buf[i]);
	keystr[std::strlen(buf)] = '\0';

	int ret = 1;
	if (std::strlen(keystr) == 1) {
		if ((last_keypress >= SDL_SCANCODE_1) && (last_keypress <= SDL_SCANCODE_0))
			ret = (last_keypress - SDL_SCANCODE_0) - (*keystr - '0');
		else if ((last_keypress >= SDL_SCANCODE_A) && (last_keypress <= SDL_SCANCODE_Z))
			ret = (last_keypress - SDL_SCANCODE_A) - (*keystr - 'A');
	}
	if (ret != 0) {
		switch (last_keypress) {
			default:
				ret = 1;
				break;
			case SDL_SCANCODE_RCTRL:
			case SDL_SCANCODE_LCTRL:
				ret = std::strcmp(keystr, "CTRL");
				break;
			case SDL_SCANCODE_RSHIFT:
			case SDL_SCANCODE_LSHIFT:
				ret = std::strcmp(keystr, "SHIFT");
				break;
			case SDL_SCANCODE_RETURN:
				ret = std::strcmp(keystr, "RETURN");
				if (ret != 0)
					ret = std::strcmp(keystr, "ENTER");
				break;
			case SDL_SCANCODE_SPACE:
				ret = std::strcmp(keystr, " ");
				if (ret != 0)
					ret = std::strcmp(keystr, "SPACE");
				break;
			case SDL_SCANCODE_PAGEUP:
				ret = std::strcmp(keystr, "PAGEUP");
				break;
			case SDL_SCANCODE_PAGEDOWN:
				ret = std::strcmp(keystr, "PAGEDOWN");
				break;
			case SDL_SCANCODE_UP:
				ret = std::strcmp(keystr, "UP");
				break;
			case SDL_SCANCODE_DOWN:
				ret = std::strcmp(keystr, "DOWN");
				break;
			case SDL_SCANCODE_LEFT:
				ret = std::strcmp(keystr, "LEFT");
				break;
			case SDL_SCANCODE_RIGHT:
				ret = std::strcmp(keystr, "RIGHT");
				break;
			case SDL_SCANCODE_F1: ret = std::strcmp(keystr, "F1"); break;
			case SDL_SCANCODE_F2: ret = std::strcmp(keystr, "F2"); break;
			case SDL_SCANCODE_F3: ret = std::strcmp(keystr, "F3"); break;
			case SDL_SCANCODE_F4: ret = std::strcmp(keystr, "F4"); break;
			case SDL_SCANCODE_F5: ret = std::strcmp(keystr, "F5"); break;
			case SDL_SCANCODE_F6: ret = std::strcmp(keystr, "F6"); break;
			case SDL_SCANCODE_F7: ret = std::strcmp(keystr, "F7"); break;
			case SDL_SCANCODE_F8: ret = std::strcmp(keystr, "F8"); break;
			case SDL_SCANCODE_F9: ret = std::strcmp(keystr, "F9"); break;
			case SDL_SCANCODE_F10: ret = std::strcmp(keystr, "F10"); break;
			case SDL_SCANCODE_F11: ret = std::strcmp(keystr, "F11"); break;
			case SDL_SCANCODE_F12: ret = std::strcmp(keystr, "F12"); break;
		}
	}
	if (ret == 0)
		sendToLog(LogLevel::Info, "checkkey: got key %s\n", keystr);
	script_h.setInt(&script_h.pushed_variable, (ret == 0) ? 1 : 0);
	delete[] keystr;

	return RET_CONTINUE;
}

int ONScripter::cellCommand() {
	bool lsp2 = script_h.isName("cell2");

	int sprite_no = script_h.readInt();
	int no        = script_h.readInt();

	AnimationInfo *ai = lsp2 ? &sprite2_info[sprite_no] : &sprite_info[sprite_no];

	backupState(ai);

	ai->setCell(no);
	dirtySpriteRect(sprite_no, lsp2);

	return RET_CONTINUE;
}

int ONScripter::captionCommand() {
	script_h.setStr(&wm_title_string, script_h.readStr());
	window.setTitle(wm_title_string);

	return RET_CONTINUE;
}

int ONScripter::btnwaitCommand() {
	if (!btnasync_active && isWaitingForUserInput()) {
		errorAndExit("Cannot run this command at the moment");
		return RET_CONTINUE; //dummy
	}

	internal_slowdown_counter = 0;

	bool del_flag = false, textbtn_flag = false;
	bool remove_window_flag = !((erase_text_window_mode == 0) ||
	                            btnnowindowerase_flag);

	if (script_h.isName("btnwait2")) {
	} else if (script_h.isName("btnwait")) {
		del_flag = true;
	} else if (script_h.isName("textbtnwait")) {
		textbtn_flag       = true;
		remove_window_flag = false;
	}

	if (remove_window_flag)
		leaveTextDisplayMode(remove_window_flag);

	script_h.readInt();

	bool skip_flag = (skip_mode & SKIP_NORMAL) || keyState.ctrl;

	current_button_state.reset();
	last_keypress = SDL_NUM_SCANCODES;

	uint32_t button_timer_start = SDL_GetTicks(); //set here so btnwait is correct

	if (skip_flag && textbtn_flag) {
		current_button_state.set(0);
		btnwaitCommandHandleResult(button_timer_start, &script_h.current_variable, current_button_state, del_flag);
		return RET_CONTINUE;
	}

	// --------- Command is not skipped; we need to start up an Action ---------
	lrTrap.enabled = false;
	ButtonWaitAction *action{btnasync_active ? nullptr : ButtonWaitAction::create()};

	skip_mode &= ~SKIP_NORMAL;

	if (txtbtn_show)
		txtbtn_visible = true;

	// Set all buttons to visible.
	ButtonLink *p_button_link = root_button_link.next;
	if(btnasync_active && !btnasync_draw_required) {
		// Resetting the button visibility in this case will cause button draw failure.
		// Once refreshButtonHoverState starts modifying the visual state of the buttons,
		// and the "hoveringButton" flags start being set, from that point forward,
		// only refreshButtonHoverState is allowed to modify the visual state,
		// until you call deleteButtonLink() (e.g. via btndef "") and unset the hoveringButton flags again.
	} else {
		while (p_button_link) {
			ButtonLink *cur_button_link = p_button_link;
			while (cur_button_link) {
				cur_button_link->show_flag = false;
				if (cur_button_link->button_type == ButtonLink::SPRITE_BUTTON ||
				    cur_button_link->button_type == ButtonLink::EX_SPRITE_BUTTON) {
					sprite_info[cur_button_link->sprite_no].visible = true;
					sprite_info[cur_button_link->sprite_no].setCell(0);
				} else if (cur_button_link->button_type == ButtonLink::TMP_SPRITE_BUTTON) {
					cur_button_link->show_flag = true;
					sprite_info[cur_button_link->sprite_no].setCell(0);
				} else if (cur_button_link->button_type == ButtonLink::TEXT_BUTTON) {
					if (txtbtn_visible) {
						cur_button_link->show_flag = true;
						sprite_info[cur_button_link->sprite_no].setCell(0);
					}
				}
				dirty_rect_hud.add(cur_button_link->image_rect);
				cur_button_link = cur_button_link->same;
			}
			p_button_link = p_button_link->next;
		}
		// Set buttons to default state as specified by exbtn_d. Moved after visibility set in case the default state sets some buttons invisible. Hopefully this will not cause problems.
		if (is_exbtn_enabled && exbtn_d_button_link.exbtn_ctl) {
			//should not be canvas, right?
			GPU_Rect check_src_rect{0, 0, static_cast<float>(window.script_width), static_cast<float>(window.script_height)};
			decodeExbtnControl(exbtn_d_button_link.exbtn_ctl, &check_src_rect);
		}
		refreshButtonHoverState();
		commitVisualState();
		flush(refreshMode()); //don't wait for CR here, it resets our event_mode and breaks automode by setting current_button_state earlier
		btnasync_draw_required = false;
	}

	if (action)
		action->event_mode = WAIT_BUTTON_MODE;

	int32_t t = -1;
	if (btntime_value > 0) {
		if (btntime2_flag && action)
			action->event_mode |= WAIT_VOICE_MODE;
		t = btntime_value;
	}
	button_timer_start = SDL_GetTicks();

	if (textbtn_flag) {
		if (action)
			action->event_mode |= WAIT_TEXTBTN_MODE;
		if (btntime_value == 0) {
			if (automode_flag) {
				if (action)
					action->event_mode |= WAIT_VOICE_MODE;
				if (automode_time < 0) {
					int timeToWait = -automode_time * dlgCtrl.dialogueRenderState.clickPartCharacterCount();
					if (t == -1 || t > timeToWait)
						t = timeToWait;
				} else {
					if (t == -1 || t > automode_time)
						t = automode_time;
				}
			} else if ((autoclick_time > 0) &&
			           (t == -1 || t > autoclick_time))
				t = autoclick_time;
		}
	}
	if (t <= 0)
		t = -1;

	if (action) {
		action->button_timer_start = button_timer_start;
		action->variableInfo       = std::make_shared<VariableInfo>(script_h.current_variable);
		action->del_flag           = del_flag;
	}

	bool voice_plays = wave_sample[0] && Mix_Playing(0) && !Mix_Paused(0);

	if (action && ((!voice_plays && automode_flag) ||
	               (!textbtn_flag && btntime_value > 0))) {
		action->event_mode |= WAIT_TIMER_MODE;
		if (t > 0) {
			action->clock.setCountdown(t);
			action->timer_set = true;
		}
	}

	if (voice_plays && action) {
		action->voiced_txtbtnwait = true;
		if (textgosub_clickstr_state == CLICK_NEWPAGE) {
			action->final_voiced_txtbtnwait = true;
		}
	}

	Lock lock(&registeredCRActions);

	if (btnasync_active) {
		const auto &list = fetchConstantRefreshActions<ButtonMonitorAction>();
		assert(list.size() == 1);
		auto bma = dynamic_cast<ButtonMonitorAction *>(list.front().get());
		btnwaitCommandHandleResult(button_timer_start, &script_h.current_variable, bma->buttonState, del_flag);
		if (bma->buttonState.valid_flag) {
			bma->terminate();
			btnasync_active = false;
		}
		return RET_CONTINUE;
	} else {
		registeredCRActions.emplace_back(action);
	}

	return RET_CONTINUE;
}

void ONScripter::btnwaitCommandHandleResult(uint32_t button_timer_start, VariableInfo *resultVar, ButtonState buttonState, bool del_flag) {

	if (!buttonState.valid_flag) {
		if (automode_flag || (autoclick_time > 0))
			buttonState.set(0);
		else if (btntime_value > 0) {
			if (usewheel_flag)
				buttonState.set(-5);
			else
				buttonState.set(-2);
		} else
			buttonState.set(0);
	}

	btnwait_time = SDL_GetTicks() - button_timer_start;

	script_h.setInt(resultVar, buttonState.button);
	//sendToLog(LogLevel::Info, "btnwait return value: %d\n", buttonValue);

	if (buttonState.button >= 1 && del_flag) {
		btndef_info.remove();
		deleteButtonLink();
	}

	disableGetButtonFlag();

	ButtonLink *p_button_link = root_button_link.next;
	while (p_button_link) {
		ButtonLink *cur_button_link = p_button_link;
		while (cur_button_link) {
			cur_button_link->show_flag = false;
			//It feels suspicious that rects weren't dirtied here... I'll put it in for now.
			dirty_rect_hud.add(cur_button_link->image_rect);
			cur_button_link            = cur_button_link->same;
		}
		p_button_link = p_button_link->next;
	}

	flush(refreshMode());

	lrTrap.enabled = true;
}

int ONScripter::btntimeCommand() {
	btntime2_flag = script_h.isName("btntime2");
	btntime_value = script_h.readInt();

	return RET_CONTINUE;
}

int ONScripter::btndownCommand() {
	btndown_flag = script_h.readInt() == 1;

	return RET_CONTINUE;
}

int ONScripter::btndefCommand() {
	if (isWaitingForUserInput()) {
		errorAndExit("Cannot run this command at the moment");
		return RET_CONTINUE; //dummy
	}

	if (script_h.compareString("clear")) {
		script_h.readName();
	} else {
		const char *buf = script_h.readStr();

		btndef_info.remove();

		if (buf[0] != '\0') {
			btndef_info.setImageName(buf);
			parseTaggedString(&btndef_info);
			//btndef_info.trans_mode = AnimationInfo::TRANS_COPY;
			setupAnimationInfo(&btndef_info);
			//SDL_SetSurfaceAlphaMod(btndef_info.image_surface, 0xFF);
			//SDL_SetSurfaceBlendMode(btndef_info.image_surface, SDL_BLENDMODE_NONE);
			//SDL_SetSurfaceRLE(btndef_info.image_surface, SDL_RLEACCEL);
		}
	}

	btntime_value = 0;
	deleteButtonLink();
	current_button_state.reset();

	last_keypress = SDL_NUM_SCANCODES;
	processTextButtonInfo();

	disableGetButtonFlag();

	return RET_CONTINUE;
}

int ONScripter::btnareaCommand() {
	btnarea_flag = true;
	btnarea_pos  = script_h.readInt();

	return RET_CONTINUE;
}

int ONScripter::btnCommand() {
	GPU_Rect src_rect;

	ButtonLink *button = new ButtonLink();

	button->no = script_h.readInt();

	button->image_rect.x = script_h.readInt();
	button->image_rect.y = script_h.readInt();
	button->image_rect.w = script_h.readInt();
	button->image_rect.h = script_h.readInt();

	button->select_rect = button->image_rect;

	src_rect.x = script_h.readInt();
	src_rect.y = script_h.readInt();

	if (!btndef_info.gpu_image) {
		button->image_rect.w = 0;
		button->image_rect.h = 0;
	}
	if (btndef_info.gpu_image &&
	    src_rect.x + button->image_rect.w > btndef_info.gpu_image->w) {
		button->image_rect.w = btndef_info.gpu_image->w - src_rect.x;
	}
	if (btndef_info.gpu_image &&
	    src_rect.y + button->image_rect.h > btndef_info.gpu_image->h) {
		button->image_rect.h = btndef_info.gpu_image->h - src_rect.y;
	}
	src_rect.w = button->image_rect.w;
	src_rect.h = button->image_rect.h;

	button->anim               = new AnimationInfo();
	button->anim->type         = SPRITE_BUTTONS;
	button->anim->num_of_cells = 1;
	button->anim->trans_mode   = AnimationInfo::TRANS_COPY;
	button->anim->pos.x        = button->image_rect.x;
	button->anim->pos.y        = button->image_rect.y;
	if (btndef_info.gpu_image) {
		button->anim->trans_mode = btndef_info.trans_mode;
		button->anim->setImage(gpu.createImage(button->image_rect.w, button->image_rect.h,
		                                       btndef_info.gpu_image->bytes_per_pixel));
		GPU_GetTarget(button->anim->gpu_image);
		GPU_SetBlending(btndef_info.gpu_image, false);
		gpu.copyGPUImage(btndef_info.gpu_image, &src_rect, nullptr, button->anim->gpu_image->target);
		GPU_SetBlending(btndef_info.gpu_image, true);
	}

	root_button_link.insert(button);

	return RET_CONTINUE;
}

int ONScripter::brCommand() {
	sentence_font.newLine();
	// It might not work anyway, but ok...

	return RET_CONTINUE;
}

int ONScripter::bltCommand() {
	int /*dx,dy,*/ dw, dh;
	int sx, sy, sw, sh;

	/*dx = */ script_h.readInt();
	/*dy = */ script_h.readInt();
	dw = script_h.readInt();
	dh = script_h.readInt();
	sx = script_h.readInt();
	sy = script_h.readInt();
	sw = script_h.readInt();
	sh = script_h.readInt();

	if (btndef_info.gpu_image == nullptr)
		return RET_CONTINUE;
	if (dw == 0 || dh == 0 || sw == 0 || sh == 0)
		return RET_CONTINUE;

	//if (sw == dw && sw > 0 && sh == dh && sh > 0) {

	GPU_Rect src_rect{static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sw), static_cast<float>(sh)};
	//SDL_Rect dst_rect {(short)dx,(short)dy,(uint16_t)dw,(uint16_t)dh};

	gpu.copyGPUImage(btndef_info.gpu_image, &src_rect, nullptr, screen_target);
	dirty_rect_scene.clear();
	dirty_rect_hud.clear();
	/*} else {
        ONSBuf *dst_buf = (ONSBuf*)accumulation_surface->pixels;
        ONSBuf *src_buf = (ONSBuf*)btndef_info.image_surface->pixels;
        int dst_width = accumulation_surface->pitch / 4;
        int src_width = btndef_info.image_surface->pitch / 4;

        int start_y = dy, end_y = dy+dh;
        if (dh < 0) {
            start_y = dy+dh;
            end_y = dy;
        }
        if (start_y < 0) start_y = 0;
        if (end_y > screen_height) end_y = screen_height;

        int start_x = dx, end_x = dx+dw;
        if (dw < 0) {
            start_x = dx+dw;
            end_x = dx;
        }
        if (start_x < 0) start_x = 0;
        if (end_x >= screen_width) end_x = screen_width;

        dst_buf += start_y*dst_width;
        for (int i=start_y ; i<end_y ; i++) {
            int y = sy+sh*(i-dy)/dh;
            for (int j=start_x ; j<end_x ; j++) {

                int x = sx+sw*(j-dx)/dw;
                if (x<0 || x>=btndef_info.image_surface->w ||
                    y<0 || y>=btndef_info.image_surface->h)
                    *(dst_buf+j) = 0;
                else
                    *(dst_buf+j) = *(src_buf+y*src_width+x);
            }
            dst_buf += dst_width;
        }

        SDL_Rect dst_rect {(short)start_x, (short)start_y, (uint16_t)(end_x-start_x), (uint16_t)(end_y-start_y)};
        flushDirect((SDL_Rect&)dst_rect, REFRESH_NONE_MODE);
    }*/

	return RET_CONTINUE;
}

int ONScripter::bgmdownmodeCommand() {
	bgmdownmode_flag = (script_h.readInt() != 0);

	return RET_CONTINUE;
}

int ONScripter::bgcopyCommand() {
	backupState(&bg_info);
	bg_info.num_of_cells = 1;
	bg_info.trans_mode   = AnimationInfo::TRANS_COPY;
	bg_info.pos.x        = -camera.center_pos.x;
	bg_info.pos.y        = -camera.center_pos.y;

	if (bg_info.image_surface)
		SDL_FreeSurface(bg_info.image_surface);
	bg_info.image_surface = nullptr;
	bg_info.gpu_image     = gpu.copyImage(accumulation_gpu);

	return RET_CONTINUE;
}

int ONScripter::bgCommand() {
	backupState(&bg_info);

	//Mion: prefer removing textwindow for bg change effects even during skip;
	//but don't remove text window if erasetextwindow == 0
	leaveTextDisplayMode((erase_text_window_mode != 0));

	const char *buf;
	if (script_h.compareString("white")) {
		buf = "white";
		script_h.readName();
	} else if (script_h.compareString("black")) {
		buf = "black";
		script_h.readName();
	} else {
		if (allow_color_type_only) {
			bool is_color = false;
			buf           = script_h.readColor(&is_color);
			if (!is_color)
				buf = script_h.readFilePath();
		} else {
			buf = script_h.readFilePath();
		}
	}

	for (auto &i : tachi_info) {
		backupState(&i);
		i.remove();
	}

	bg_info.remove();
	script_h.setStr(&bg_info.file_name, buf);

	createBackground();
	dirty_rect_scene.fill(window.canvas_width, window.canvas_height);

	EffectLink *el = parseEffect(true);
	constantRefreshEffect(el, true);
	return RET_CONTINUE;
}

int ONScripter::barclearCommand() {
	leaveTextDisplayMode();

	for (auto &i : bar_info) {
		if (i) {
			dirty_rect_hud.add(i->pos);
			delete i;
			i = nullptr;
		}
	}
	return RET_CONTINUE;
}

int ONScripter::barCommand() {
	int no = script_h.readInt();
	if (bar_info[no]) {
		backupState(bar_info[no]);
		dirty_rect_hud.add(bar_info[no]->pos);
		bar_info[no]->remove();
	} else {
		bar_info[no]       = new AnimationInfo();
		bar_info[no]->type = SPRITE_BAR;
		bar_info[no]->id   = no;
	}
	bar_info[no]->trans_mode   = AnimationInfo::TRANS_COPY;
	bar_info[no]->num_of_cells = 1;

	bar_info[no]->param      = script_h.readInt();
	bar_info[no]->orig_pos.x = script_h.readInt();
	bar_info[no]->orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(bar_info[no]);

	bar_info[no]->max_width  = script_h.readInt();
	bar_info[no]->orig_pos.h = script_h.readInt();
	bar_info[no]->pos.h      = bar_info[no]->orig_pos.h;
	bar_info[no]->max_param  = script_h.readInt();

	const char *buf = readColorStr();
	readColor(&bar_info[no]->color, buf);

	int w = bar_info[no]->max_width * bar_info[no]->param / bar_info[no]->max_param;
	if (bar_info[no]->max_width > 0 && w > 0) {
		bar_info[no]->pos.w = w;
		bar_info[no]->calculateImage(bar_info[no]->pos.w, bar_info[no]->pos.h);
		bar_info[no]->fill(bar_info[no]->color.x, bar_info[no]->color.y, bar_info[no]->color.z, 0xff);
		bar_info[no]->exists = true;
		dirty_rect_hud.add(bar_info[no]->pos);
	}

	return RET_CONTINUE;
}

int ONScripter::aviCommand() {
	script_h.readStr();
	script_h.readInt();

	sendToLog(LogLevel::Error, "avi command is not supported, use video instead\n");

	return RET_CONTINUE;
}

int ONScripter::automode_timeCommand() {
	automode_time = script_h.readInt();

	if (preferred_automode_time_set && (current_mode == DEFINE_MODE)) {
		//if cmd is the define block, and a preferred automode time was set,
		//use the preferred time instead
		sendToLog(LogLevel::Warn, "automode_time: overriding time of %d with user-preferred time %d\n",
		          automode_time, preferred_automode_time);
		automode_time = preferred_automode_time;
	}

	return RET_CONTINUE;
}

int ONScripter::autoclickCommand() {
	autoclick_time = script_h.readInt();

	return RET_CONTINUE;
}

int ONScripter::amspCommand() {
	leaveTextDisplayMode();

	bool amsp2_flag = false;
	if (script_h.isName("amsp2"))
		amsp2_flag = true;

	int no            = validSprite(script_h.readInt());
	AnimationInfo *si = nullptr;
	if (amsp2_flag) {
		si = &sprite2_info[no];
		dirtySpriteRect(no, true);
	} else {
		si = &sprite_info[no];
		dirtySpriteRect(no, false);
	}

	backupState(si);

	si->orig_pos.x = script_h.readInt();
	si->orig_pos.y = script_h.readInt();
	UpdateAnimPosXY(si);
	if (amsp2_flag) {
		si->scale_x = script_h.readInt();
		si->scale_y = script_h.readInt();
		si->rot     = script_h.readInt();
		si->calcAffineMatrix(window.script_width, window.script_height);
		dirtySpriteRect(no, true);
	} else {
		dirtySpriteRect(no, false);
	}

	if (script_h.hasMoreArgs())
		si->trans = script_h.readInt();

	if (si->trans > 255)
		si->trans = 255;
	else if (si->trans < 0)
		si->trans = 0;

	return RET_CONTINUE;
}

int ONScripter::allsp2resumeCommand() {
	all_sprite2_hide_flag = false;
	for (auto sptr : sprites(SPRITE_LSP2)) {
		backupState(sptr);
		if (sptr->exists && sptr->visible)
			dirtySpriteRect(sptr);
	}
	return RET_CONTINUE;
}

int ONScripter::allspresumeCommand() {
	all_sprite_hide_flag = false;
	for (auto sptr : sprites(SPRITE_LSP)) {
		backupState(sptr);
		if (sptr->exists && sptr->visible)
			dirtySpriteRect(sptr);
	}
	return RET_CONTINUE;
}

int ONScripter::allsp2hideCommand() {
	all_sprite2_hide_flag = true;
	for (auto sptr : sprites(SPRITE_LSP2)) {
		backupState(sptr);
		if (sptr->exists && sptr->visible)
			dirtySpriteRect(sptr);
	}
	return RET_CONTINUE;
}

int ONScripter::allsphideCommand() {
	all_sprite_hide_flag = true;
	for (auto sptr : sprites(SPRITE_LSP)) {
		backupState(sptr);
		if (sptr->exists && sptr->visible)
			dirtySpriteRect(sptr);
	}
	return RET_CONTINUE;
}

// Haeleth: Stub out some commands to suppress unwanted debug messages

int ONScripter::insertmenuCommand() {
	script_h.skipToken();
	return RET_CONTINUE;
}
int ONScripter::resetmenuCommand() {
	script_h.skipToken();
	return RET_CONTINUE;
}
