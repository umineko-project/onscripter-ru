/**
 *  File.cpp
 *  ONScripter-RU
 *
 *  FILE I/O handling (game saves).
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Readers/Base.hpp"
#include "Support/Unicode.hpp"
#include "Support/FileIO.hpp"

#include <zlib.h>

#include <ctime>

const uint32_t SAVEFILE_MAGIC_NUMBER = 0x534E4F52; // RONS
const uint32_t SAVEFILE_INIT_HASH    = 0x69F23B1B;
const uint32_t SAVEFILE_HASH_LENGH   = sizeof(uint32_t);
const int SAVEFILE_VERSION_MAJOR     = 4;
const int SAVEFILE_VERSION_MINOR     = 0;

void ONScripter::readFontinfo(Fontinfo &fi) {
	fi.clear();

	fi.top_xy[0]      = read32s();
	fi.top_xy[1]      = read32s();
	fi.borderPadding  = read32s();
	fi.is_transparent = read8s();

	fi.smart_quotes                                   = read8s();
	fi.smart_single_quotes_represented_by_dumb_double = read8s();
	fi.opening_single_quote                           = read32s();
	fi.closing_single_quote                           = read32s();
	fi.opening_double_quote                           = read32s();
	fi.closing_double_quote                           = read32s();
	fi.apostrophe                                     = read32s();

	auto &style = fi.changeStyle();

	style.font_size   = read32s();
	style.font_number = read32s();

	style.wrap_limit        = read32s();
	style.character_spacing = read32s();
	style.line_height       = read32s();
	style.border_width      = read32s();
	style.is_bold           = read32s();
	style.is_italic         = read32s();
	style.is_underline      = read32s();
	style.is_shadow         = read32s();
	style.is_border         = read32s();
	style.is_gradient       = read32s();
	style.is_centered       = read32s();
	style.is_fitted         = read32s();

	style.shadow_distance[0] = read32s();
	style.shadow_distance[1] = read32s();

	style.color.x = read8s();
	style.color.y = read8s();
	style.color.z = read8s();

	style.border_color.x = read8s();
	style.border_color.y = read8s();
	style.border_color.z = read8s();

	style.shadow_color.x = read8s();
	style.shadow_color.y = read8s();
	style.shadow_color.z = read8s();

	fi.window_color.x = read8s();
	fi.window_color.y = read8s();
	fi.window_color.z = read8s();
}

void ONScripter::writeFontinfo(Fontinfo &fi) {
	write32s(fi.top_xy[0]);
	write32s(fi.top_xy[1]);
	write32s(fi.borderPadding);
	write8s(fi.is_transparent);

	write8s(fi.smart_quotes);
	write8s(fi.smart_single_quotes_represented_by_dumb_double);
	write32s(fi.opening_single_quote);
	write32s(fi.closing_single_quote);
	write32s(fi.opening_double_quote);
	write32s(fi.closing_double_quote);
	write32s(fi.apostrophe);

	auto &style = fi.style();

	write32s(style.font_size);
	write32s(style.font_number);

	write32s(style.wrap_limit);
	write32s(style.character_spacing);
	write32s(style.line_height);
	write32s(style.border_width);
	write32s(style.is_bold);
	write32s(style.is_italic);
	write32s(style.is_underline);
	write32s(style.is_shadow);
	write32s(style.is_border);
	write32s(style.is_gradient);
	write32s(style.is_centered);
	write32s(style.is_fitted);

	write32s(style.shadow_distance[0]);
	write32s(style.shadow_distance[1]);

	write8s(style.color.x);
	write8s(style.color.y);
	write8s(style.color.z);

	write8s(style.border_color.x);
	write8s(style.border_color.y);
	write8s(style.border_color.z);

	write8s(style.shadow_color.x);
	write8s(style.shadow_color.y);
	write8s(style.shadow_color.z);

	write8s(fi.window_color.x);
	write8s(fi.window_color.y);
	write8s(fi.window_color.z);
}

void ONScripter::readWindowCtrl(TextWindowController &wnd) {
	wnd.usingDynamicTextWindow = read32s();

	wnd.mainRegionDimensions.x = read32s();
	wnd.mainRegionDimensions.y = read32s();
	wnd.mainRegionDimensions.w = read32s();
	wnd.mainRegionDimensions.h = read32s();
	wnd.mainRegionExtensionCol = read32s();

	wnd.noNameRegionDimensions.x = read32s();
	wnd.noNameRegionDimensions.y = read32s();
	wnd.noNameRegionDimensions.w = read32s();
	wnd.noNameRegionDimensions.h = read32s();
	wnd.noNameRegionExtensionCol = read32s();

	wnd.nameRegionDimensions.x = read32s();
	wnd.nameRegionDimensions.y = read32s();
	wnd.nameRegionDimensions.w = read32s();
	wnd.nameRegionDimensions.h = read32s();
	wnd.nameBoxExtensionCol    = read32s();
	wnd.nameBoxDividerCol      = read32s();
	wnd.nameRegionExtensionCol = read32s();
	wnd.nameBoxExtensionRow    = read32s();

	wnd.mainRegionPadding.top    = read32s();
	wnd.mainRegionPadding.right  = read32s();
	wnd.mainRegionPadding.bottom = read32s();
	wnd.mainRegionPadding.left   = read32s();

	wnd.nameBoxPadding.top    = read32s();
	wnd.nameBoxPadding.right  = read32s();
	wnd.nameBoxPadding.bottom = read32s();
	wnd.nameBoxPadding.left   = read32s();
}

void ONScripter::writeWindowCtrl(TextWindowController &wnd) {
	write32s(wnd.usingDynamicTextWindow);

	write32s(wnd.mainRegionDimensions.x);
	write32s(wnd.mainRegionDimensions.y);
	write32s(wnd.mainRegionDimensions.w);
	write32s(wnd.mainRegionDimensions.h);
	write32s(wnd.mainRegionExtensionCol);

	write32s(wnd.noNameRegionDimensions.x);
	write32s(wnd.noNameRegionDimensions.y);
	write32s(wnd.noNameRegionDimensions.w);
	write32s(wnd.noNameRegionDimensions.h);
	write32s(wnd.noNameRegionExtensionCol);

	write32s(wnd.nameRegionDimensions.x);
	write32s(wnd.nameRegionDimensions.y);
	write32s(wnd.nameRegionDimensions.w);
	write32s(wnd.nameRegionDimensions.h);
	write32s(wnd.nameBoxExtensionCol);
	write32s(wnd.nameBoxDividerCol);
	write32s(wnd.nameRegionExtensionCol);
	write32s(wnd.nameBoxExtensionRow);

	write32s(wnd.mainRegionPadding.top);
	write32s(wnd.mainRegionPadding.right);
	write32s(wnd.mainRegionPadding.bottom);
	write32s(wnd.mainRegionPadding.left);

	write32s(wnd.nameBoxPadding.top);
	write32s(wnd.nameBoxPadding.right);
	write32s(wnd.nameBoxPadding.bottom);
	write32s(wnd.nameBoxPadding.left);
}

void ONScripter::readAnimationInfo(AnimationInfo &ai) {
	ai.remove();
	ai.childImages.clear();

	readFilePath(&ai.image_name);

	ai.visible  = read8s();
	ai.abs_flag = read8s();
	ai.trans    = read32s();

	ai.orig_pos.x = read32s();
	ai.orig_pos.y = read32s();
	UpdateAnimPosXY(&ai);

	//Move to a separate function?
	if (ai.type == SPRITE_SENTENCE_FONT) {
		ai.orig_pos.w = read32s();
		ai.orig_pos.h = read32s();
		UpdateAnimPosWH(&ai);

		bool is_colour = read8s();

		if (!sentence_font.is_transparent && ai.image_name && !is_colour) {
			parseTaggedString(&ai);
			setupAnimationInfo(&ai);
		} else {
			if (!ai.gpu_image)
				ai.gpu_image = gpu.createImage(ai.pos.w, ai.pos.h, 4);
			GPU_GetTarget(ai.gpu_image);
			gpu.clearWholeTarget(ai.gpu_image->target,
			                     sentence_font.window_color.x,
			                     sentence_font.window_color.y,
			                     sentence_font.window_color.z, 0xFF);
			ai.trans_mode    = AnimationInfo::TRANS_COPY;
			ai.blending_mode = BlendModeId::MUL;
			gpu.multiplyAlpha(ai.gpu_image);
		}
	} else if (ai.image_name) {
		parseTaggedString(&ai);
		setupAnimationInfo(&ai);
	}

	if (ai.type == SPRITE_LSP || ai.type == SPRITE_LSP2) {
		readFilePath(&ai.lips_name);

		ai.rot          = read32s();
		ai.flip         = read32s();
		ai.scale_x      = read32s();
		ai.scale_y      = read32s();
		ai.current_cell = read32s();

		ai.blending_mode = static_cast<BlendModeId>(read32s());
		ai.darkenHue.r   = read8s();
		ai.darkenHue.g   = read8s();
		ai.darkenHue.b   = read8s();

		ai.has_z_order_override = read8s();
		ai.z_order_override     = read32s();
		ai.has_hotspot          = read8s();
		ai.hotspot.x            = readFloat();
		ai.hotspot.y            = readFloat();
		ai.has_scale_center     = read8s();
		ai.scale_center.x       = readFloat();
		ai.scale_center.y       = readFloat();

		readTransforms(ai.spriteTransforms);
		readCamera(ai.camera);

		AnimationInfo *set = ai.type == SPRITE_LSP ? sprite_info : sprite2_info;
		int32_t s          = read32s();
		for (int32_t i = 0; i < s; i++) {
			int32_t id                  = read32s();
			set[id].childImages[i].no   = read32s();
			set[id].childImages[i].lsp2 = read32s();
		}
		ai.parentImage.no   = read32s();
		ai.parentImage.lsp2 = read32s();
	}

	ai.calcAffineMatrix(window.script_width, window.script_height);
}

void ONScripter::writeAnimationInfo(AnimationInfo &ai) {
	writeStr(ai.image_name);

	write8s(ai.visible);
	write8s(ai.abs_flag);
	write32s(ai.trans);

	write32s(ai.orig_pos.x);
	write32s(ai.orig_pos.y);

	if (ai.type == SPRITE_SENTENCE_FONT) {
		write32s(ai.orig_pos.w);
		write32s(ai.orig_pos.h);
		write8s(ai.blending_mode == BlendModeId::MUL);
	}

	if (ai.type == SPRITE_LSP || ai.type == SPRITE_LSP2) {
		writeStr(ai.lips_name);

		write32s(ai.rot);
		write32s(ai.flip);
		write32s(ai.scale_x);
		write32s(ai.scale_y);
		write32s(ai.current_cell);

		write32s(static_cast<int32_t>(ai.blending_mode));
		write8s(ai.darkenHue.r);
		write8s(ai.darkenHue.g);
		write8s(ai.darkenHue.b);

		write8s(ai.has_z_order_override);
		write32s(ai.z_order_override);
		write8s(ai.has_hotspot);
		writeFloat(ai.hotspot.x);
		writeFloat(ai.hotspot.y);
		write8s(ai.has_scale_center);
		writeFloat(ai.scale_center.x);
		writeFloat(ai.scale_center.y);

		writeTransforms(ai.spriteTransforms);
		writeCamera(ai.camera);

		write32s(static_cast<int32_t>(ai.childImages.size()));
		for (auto &sp : ai.childImages) {
			write32s(sp.first);
			write32s(sp.second.no);
			write32s(sp.second.lsp2);
		}
		write32s(ai.parentImage.no);
		write32s(ai.parentImage.lsp2);
	}
}

void ONScripter::readCamera(Camera &camera) {
	camera.x_move.multiplier = read32s();
	camera.x_move.cycleTime  = read32s();
	camera.x_move.setAmplitude(read32s());
	camera.y_move.multiplier = read32s();
	camera.y_move.cycleTime  = read32s();
	camera.y_move.setAmplitude(read32s());
}

void ONScripter::writeCamera(Camera &camera) {
	write32s(camera.x_move.multiplier);
	write32s(camera.x_move.cycleTime);
	write32s(camera.x_move.getAmplitude());
	write32s(camera.y_move.multiplier);
	write32s(camera.y_move.cycleTime);
	write32s(camera.y_move.getAmplitude());
}

void ONScripter::readTransforms(AnimationInfo::SpriteTransforms &transforms) {
	transforms.sepia                   = read8s();
	transforms.negative1               = read8s();
	transforms.negative2               = read8s();
	transforms.greyscale               = read8s();
	transforms.blurFactor              = read32s();
	transforms.breakupFactor           = read32s();
	transforms.breakupDirectionFlagset = read32s();
	transforms.warpSpeed               = read32s();
	transforms.warpWaveLength          = read32s();
	transforms.warpAmplitude           = read32s();
}

void ONScripter::writeTransforms(AnimationInfo::SpriteTransforms &transforms) {
	write8s(transforms.sepia);
	write8s(transforms.negative1);
	write8s(transforms.negative2);
	write8s(transforms.greyscale);
	write32s(transforms.blurFactor);
	write32s(transforms.breakupFactor);
	write32s(transforms.breakupDirectionFlagset);
	write32s(transforms.warpSpeed);
	write32s(transforms.warpWaveLength);
	write32s(transforms.warpAmplitude);
}

void ONScripter::readGlobalFlags() {
	rmode_flag             = read8s();
	effectskip_flag        = read8s();
	skip_enabled           = read8s();
	dialogue_add_ends      = read8s();
	erase_text_window_mode = read8s();
	text_display_speed     = read32s();
	text_fade_duration     = read32s();

	monocro_flag[AfterScene]    = read8s();
	monocro_color[AfterScene].r = read8s();
	monocro_color[AfterScene].g = read8s();
	monocro_color[AfterScene].b = read8s();
	monocro_color[AfterScene].a = 255;
	nega_mode[AfterScene]       = read32s();
	blur_mode[AfterScene]       = read32s();
	warpAmplitude               = readFloat();
	warpWaveLength              = readFloat();
	warpSpeed                   = readFloat();
	readCamera(camera);

	for (auto &p : humanpos) {
		p = read32s();
	}
	underline_value = read32s();
}

void ONScripter::writeGlobalFlags() {
	write8s(rmode_flag);
	write8s(effectskip_flag);
	write8s(skip_enabled);
	write8s(dialogue_add_ends);
	write8s(erase_text_window_mode);
	write32s(text_display_speed);
	write32s(text_fade_duration);

	write8s(monocro_flag[AfterScene]);
	write8s(monocro_color[AfterScene].r);
	write8s(monocro_color[AfterScene].g);
	write8s(monocro_color[AfterScene].b);
	write32s(nega_mode[AfterScene]);
	write32s(blur_mode[AfterScene]);
	writeFloat(warpAmplitude);
	writeFloat(warpWaveLength);
	writeFloat(warpSpeed);
	writeCamera(camera);

	for (auto &p : humanpos) {
		write32s(p);
	}
	write32s(underline_value);
}

void ONScripter::readNestedInfo() {
	deleteNestInfo();
	int32_t num_nest = read32s();
	if (num_nest > 0) {
		file_io_buf_ptr += (num_nest - 1) * 4;
		while (num_nest > 0) {
			callStack.emplace_front();
			auto &front = callStack.front();

			int32_t i = read32s();
			if (i > 0) {
				front.nest_mode   = NestInfo::LABEL;
				front.next_script = script_h.getAddress(i);
				file_io_buf_ptr -= 8;
				num_nest--;
			} else {
				front.nest_mode   = NestInfo::FOR;
				front.next_script = script_h.getAddress(-i);
				file_io_buf_ptr -= 16;
				front.var_no = read32s();
				front.to     = read32s();
				front.step   = read32s();
				file_io_buf_ptr -= 16;
				num_nest -= 4;
			}
		}
		num_nest = read32s();
		file_io_buf_ptr += num_nest * 4;
	}
}

void ONScripter::writeNestedInfo() {
	int32_t num_nest = 0;
	for (auto &info : callStack) {
		if (info.nest_mode == NestInfo::LABEL)
			num_nest++;
		else if (info.nest_mode == NestInfo::FOR)
			num_nest += 4;
	}
	write32s(num_nest);

	for (auto &info : callStack) {
		if (info.nest_mode == NestInfo::LABEL) {
			write32s(static_cast<int32_t>(script_h.getOffset(info.next_script)));
		} else if (info.nest_mode == NestInfo::FOR) {
			write32s(info.var_no);
			write32s(info.to);
			write32s(info.step);
			write32s(static_cast<int32_t>(-script_h.getOffset(info.next_script)));
		}
	}
}

void ONScripter::readSpritesetInfo(std::map<int, SpritesetInfo> &si) {
	resetSpritesets();
	for (int32_t i = 0, s = read32s(); i < s; i++) {
		int32_t id = read32s();

		SpritesetInfo ss;
		ss.setEnable(read8s());
		ss.id                      = id;
		ss.pos.x                   = readFloat();
		ss.pos.y                   = readFloat();
		ss.pos.w                   = readFloat();
		ss.pos.h                   = readFloat();
		ss.maskSpriteNumber        = read32s();
		ss.trans                   = read32s();
		ss.flip                    = read32s();
		ss.rot                     = readFloat();
		ss.has_scale_center        = read8s();
		ss.scale_center_x          = readFloat();
		ss.scale_center_y          = readFloat();
		ss.scale_x                 = readFloat();
		ss.scale_y                 = readFloat();
		ss.blur                    = read32s();
		ss.breakupFactor           = read32s();
		ss.breakupDirectionFlagset = read32s();
		si[id]                     = std::move(ss);
		commitSpriteset(&si[id]);
	}
}

void ONScripter::writeSpritesetInfo(std::map<int, SpritesetInfo> &si) {
	write32s(static_cast<int32_t>(si.size()));
	for (auto &sp : si) {
		write32s(sp.first);

		write8s(sp.second.isEnabled());
		writeFloat(sp.second.pos.x);
		writeFloat(sp.second.pos.y);
		writeFloat(sp.second.pos.w);
		writeFloat(sp.second.pos.h);
		write32s(sp.second.maskSpriteNumber);
		write32s(sp.second.trans);
		write32s(sp.second.flip);
		writeFloat(sp.second.rot);
		write8s(sp.second.has_scale_center);
		writeFloat(sp.second.scale_center_x);
		writeFloat(sp.second.scale_center_y);
		writeFloat(sp.second.scale_x);
		writeFloat(sp.second.scale_y);
		write32s(sp.second.blur);
		write32s(sp.second.breakupFactor);
		write32s(sp.second.breakupDirectionFlagset);
	}
}

void ONScripter::readSoundData() {
	stopCommand();
	loopbgmstopCommand();
	stopAllDWAVE();

	readFilePath(&seqmusic_file_name); // MIDI file
	readFilePath(&wave_file_name);     // wave, waveloop
	current_cd_track = read32s();

	if (read8s()) { // play, playonce MIDI
		seqmusic_play_loop_flag = true;
		current_cd_track        = -2;
		playSoundThreaded(seqmusic_file_name, SOUND_SEQMUSIC, seqmusic_play_loop_flag);
	} else {
		seqmusic_play_loop_flag = false;
	}

	wave_play_loop_flag = read8s(); // wave, waveloop
	if (wave_file_name && wave_play_loop_flag) {
		playSoundThreaded(wave_file_name, SOUND_CHUNK, wave_play_loop_flag, MIX_WAVE_CHANNEL);
	}

	cd_play_loop_flag = read8s(); // play, playonce
	if (current_cd_track >= 0) {
		playCDAudio();
	}

	music_play_loop_flag = read8s(); // bgm, mp3, mp3loop
	mp3save_flag         = read8s();
	readFilePath(&music_file_name);
	if (music_file_name) {
		playSoundThreaded(music_file_name, SOUND_MUSIC | SOUND_SEQMUSIC,
		                  music_play_loop_flag, MIX_BGM_CHANNEL);
	}

	readFilePath(&loop_bgm_name[0]);
	readFilePath(&loop_bgm_name[1]);
	if (loop_bgm_name[0]) {
		if (loop_bgm_name[1]) {
			playSoundThreaded(loop_bgm_name[1], SOUND_PRELOAD | SOUND_CHUNK, false, MIX_LOOPBGM_CHANNEL1);
		}
		playSoundThreaded(loop_bgm_name[0], SOUND_CHUNK, false, MIX_LOOPBGM_CHANNEL0);
	}
}

void ONScripter::writeSoundData() {
	writeStr(seqmusic_file_name); // MIDI file
	writeStr(wave_file_name);     // wave, waveloop
	write32s(current_cd_track);   // play CD

	write8s(seqmusic_play_loop_flag); // play, playonce MIDI
	write8s(wave_play_loop_flag);     // wave, waveloop
	write8s(cd_play_loop_flag);       // play, playonce
	write8s(music_play_loop_flag);    // bgm, mp3, mp3loop
	write8s(mp3save_flag);
	writeStr(mp3save_flag ? music_file_name : nullptr);
	writeStr(loop_bgm_name[0]);
	writeStr(loop_bgm_name[1]);
}

void ONScripter::readParamData(AnimationInfo *&p, bool bar, int id) {
	if (!read8s()) {
		return;
	}

	p               = new AnimationInfo();
	p->id           = id;
	p->num_of_cells = 1;
	p->param        = read32s();
	p->orig_pos.x   = read32s();
	p->orig_pos.y   = read32s();
	UpdateAnimPosXY(p);

	if (bar) {
		p->type       = SPRITE_BAR;
		p->trans_mode = AnimationInfo::TRANS_COPY;

		p->max_width  = read32s();
		p->orig_pos.h = read32s();
		p->max_param  = read32s();
		p->color.x    = read8s();
		p->color.y    = read8s();
		p->color.z    = read8s();

		int w    = p->max_width * p->param / p->max_param;
		p->pos.h = p->orig_pos.h;
		if (p->max_width > 0 && w > 0) {
			p->pos.w = w;
			p->calculateImage(p->pos.w, p->pos.h);
			p->fill(p->color.x, p->color.y, p->color.z, 0xff);
		}

	} else {
		p->type       = SPRITE_PRNUM;
		p->trans_mode = AnimationInfo::TRANS_STRING;

		p->color_list      = new uchar3[1];
		p->font_size_xy[0] = read32s();
		p->font_size_xy[1] = read32s();
		p->color_list[0].x = read8s();
		p->color_list[0].y = read8s();
		p->color_list[0].z = read8s();

		char num_buf[7];
		script_h.getStringFromInteger(num_buf, p->param, 3, false, true);
		script_h.setStr(&p->file_name, num_buf);

		setupAnimationInfo(p);
	}
}

void ONScripter::writeParamData(AnimationInfo *&p, bool bar) {
	if (!p) {
		write8s(0);
		return;
	}

	write8s(1);
	write32s(p->param);
	write32s(p->orig_pos.x);
	write32s(p->orig_pos.y);

	if (bar) {
		write32s(p->max_width);
		write32s(p->orig_pos.h);
		write32s(p->max_param);
		write8s(p->color.x);
		write8s(p->color.y);
		write8s(p->color.z);
	} else {
		write32s(p->font_size_xy[0]);
		write32s(p->font_size_xy[1]);
		write8s(p->color_list[0].x);
		write8s(p->color_list[0].y);
		write8s(p->color_list[0].z);
	}
}

void ONScripter::loadSaveFileData() {
	// Variable data
	readVariables(0, script_h.global_variable_border);
	readArrayVariable();

	// Textbox data
	dlgCtrl.setDialogueActive(false);
	readFontinfo(name_font);
	readFontinfo(sentence_font);
	readWindowCtrl(wndCtrl);
	char *str = nullptr;
	readStr(&str);
	dlgCtrl.setDialogueName(str ? str : "");
	freearr(&str);
	window_effect.effect   = read32s();
	window_effect.duration = read32s();
	window_effect.anim.remove();
	readStr(&window_effect.anim.image_name);

	// Command data
	readStr(&str);
	auto label = script_h.lookupLabel(str);
	if (!label) {
		errorAndExit("Failed to find save label!");
		return; //dummy
	}
	current_label_info = label;
	current_line       = read32s();
	int32_t command    = read32s();
	//sendToLog(LogLevel::Info, "load %d:%d:%d\n", current_label_info->start_line, current_line, command);

	const char *buf = script_h.getAddressByLine(label->start_line + current_line);
	for (int32_t i = 0; i < command; i++) {
		while (*buf != ':') buf++;
		buf++;
	}
	script_h.setCurrent(buf);

	// AnimationInfo data
	readAnimationInfo(cursor_info[0]);
	readAnimationInfo(cursor_info[1]);
	readAnimationInfo(sentence_font_info);
	bg_info.remove();
	readFilePath(&bg_info.file_name);
	createBackground();

	for (auto &tachi : tachi_info) {
		readAnimationInfo(tachi);
	}

	for (int i = 0; i < MAX_SPRITE_NUM; i++) {
		readAnimationInfo(sprite_info[i]);
		readAnimationInfo(sprite2_info[i]);
	}

	btndef_info.remove();
	readStr(&btndef_info.image_name);
	if (btndef_info.image_name && btndef_info.image_name[0] != '\0') {
		parseTaggedString(&btndef_info);
		setupAnimationInfo(&btndef_info);
		SDL_SetSurfaceAlphaMod(btndef_info.image_surface, 0xFF);
		SDL_SetSurfaceBlendMode(btndef_info.image_surface, SDL_BLENDMODE_NONE);
		//SDL_SetSurfaceRLE(btndef_info.image_surface, SDL_RLEACCEL);
	}

	nontransitioningSprites.clear();
	for (int32_t i = 0, s = read32s(); i < s; i++) {
		auto ais = read8s() == 1 ? sprite2_info : sprite_info;
		nontransitioningSprites.insert(&ais[read32s()]);
	}

	readSpritesetInfo(spritesets);
	readNestedInfo();
	readGlobalFlags();
	readSoundData();

	// Param data
	barclearCommand();
	prnumclearCommand();
	for (int i = 0; i < MAX_PARAM_NUM; i++) {
		readParamData(prnum_info[i], false, i);
		readParamData(bar_info[i], true, i);
	}

	// Apply data
	display_mode             = DISPLAY_MODE_NORMAL;
	refresh_window_text_mode = REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE | REFRESH_TEXT_MODE;
	clickstr_state           = CLICK_NONE;
	draw_cursor_flag         = false;

	if (wndCtrl.usingDynamicTextWindow) {
		wndCtrl.setWindow(sentence_font_info.pos);
	}
}

void ONScripter::saveSaveFileData() {
	// Variable data
	writeVariables(0, script_h.global_variable_border);
	writeArrayVariable();

	// Textbox data
	writeFontinfo(name_font);
	writeFontinfo(sentence_font);
	writeWindowCtrl(wndCtrl);
	writeStr(decodeUTF16String(dlgCtrl.dialogueName).c_str());
	write32s(window_effect.effect);
	write32s(window_effect.duration);
	writeStr(window_effect.anim.image_name);

	// Command data
	writeStr(current_label_info->name);
	write32s(current_line);
	const char *buf = script_h.getAddressByLine(current_label_info->start_line + current_line);
	//sendToLog(LogLevel::Info, "save %d:%d\n", current_label_info->start_line, current_line);

	int32_t command = 0;
	if (!dlgCtrl.dialogueProcessingState.active) {
		while (buf != script_h.getCurrent()) {
			if (*buf == ':')
				command++;
			buf++;
		}
	}
	write32s(command);

	// AnimationInfo data
	writeAnimationInfo(cursor_info[0]);
	writeAnimationInfo(cursor_info[1]);
	writeAnimationInfo(sentence_font_info);
	writeStr(bg_info.file_name);

	for (auto &tachi : tachi_info) {
		writeAnimationInfo(tachi);
	}

	for (int i = 0; i < MAX_SPRITE_NUM; i++) {
		writeAnimationInfo(sprite_info[i]);
		writeAnimationInfo(sprite2_info[i]);
	}

	writeStr(btndef_info.image_name);

	write32s(static_cast<int32_t>(nontransitioningSprites.size()));
	for (auto &ai : nontransitioningSprites) {
		write8s(ai->type == SPRITE_LSP2);
		write32s(ai->id);
	}

	writeSpritesetInfo(spritesets);
	writeNestedInfo();
	writeGlobalFlags();
	writeSoundData();

	// Param data
	for (int i = 0; i < MAX_PARAM_NUM; i++) {
		writeParamData(prnum_info[i], false);
		writeParamData(bar_info[i], true);
	}
}

bool ONScripter::readSaveFileHeader(int no, SaveFileInfo *save_file_info) {
	char filename[16];
	std::snprintf(filename, sizeof(filename), "save%d.dat", no);
	if (loadFileIOBuf(filename)) {
		//sendToLog(LogLevel::Error, "can't open save file %s\n", filename);
		return false;
	}

	if (read32u() != SAVEFILE_MAGIC_NUMBER) {
		sendToLog(LogLevel::Error, "Save file has unsupport magic header.\n");
		return false;
	}

	int file_version = read8s() * 100;
	file_version += read8s();

	//sendToLog(LogLevel::Info, "Save file version is %d.%d\n", file_version/100, file_version%100);
	if (file_version > SAVEFILE_VERSION_MAJOR * 100 + SAVEFILE_VERSION_MINOR) {
		sendToLog(LogLevel::Error, "Save file is newer than %d.%d, please use the latest ONScripter-RU.\n", SAVEFILE_VERSION_MAJOR, SAVEFILE_VERSION_MINOR);
		return false;
	}

	if (file_version < SAVEFILE_VERSION_MAJOR * 100) {
		sendToLog(LogLevel::Error, "Save file is too old %d vs %d needed.\n", file_version, SAVEFILE_VERSION_MAJOR * 100);
		return false;
	}

	int8_t day    = read8s();
	int8_t month  = read8s();
	int16_t year  = read16s();
	int8_t hour   = read8s();
	int8_t minute = read8s();

	char *descr{nullptr};
	readStr(&descr);

	if (save_file_info) {
		save_file_info->day     = day;
		save_file_info->month   = month;
		save_file_info->year    = year;
		save_file_info->hour    = hour;
		save_file_info->minute  = minute;
		save_file_info->descr   = std::unique_ptr<char[]>(descr);
		save_file_info->version = file_version;
	} else {
		freearr(&descr);
	}

	return true;
}

void ONScripter::writeSaveFileHeader(const char *descr) {
	write32u(SAVEFILE_MAGIC_NUMBER);
	write8s(SAVEFILE_VERSION_MAJOR);
	write8s(SAVEFILE_VERSION_MINOR);

	time_t rawtime = time(nullptr);
	tm *timeinfo   = localtime(&rawtime);
	write8s(timeinfo->tm_mday);
	write8s(timeinfo->tm_mon + 1);
	write16s(timeinfo->tm_year + 1900);
	write8s(timeinfo->tm_hour);
	write8s(timeinfo->tm_min);

	writeStr(descr);
}

bool ONScripter::verifyChecksum() {
	auto prevPtr    = file_io_buf_ptr;
	auto dataLen    = file_io_read_len - sizeof(uint32_t);
	file_io_buf_ptr = dataLen;
	auto hash       = read32u();
	auto calcHash   = adler32_z(SAVEFILE_INIT_HASH, file_io_buf.data(), dataLen);
	file_io_buf_ptr = prevPtr;
	if (static_cast<uint32_t>(calcHash) != hash) {
		sendToLog(LogLevel::Error, "Save file is corrupted.\n");
		return false;
	}

	return true;
}

void ONScripter::writeChecksum() {
	auto calcHash = adler32_z(SAVEFILE_INIT_HASH, file_io_buf.data(), file_io_buf.size());
	write32u(static_cast<uint32_t>(calcHash));
}

int ONScripter::loadSaveFile(int no) {
	if (!readSaveFileHeader(no) || !verifyChecksum()) {
		return -1;
	}

	loadSaveFileData();

	if (file_io_read_len != file_io_buf_ptr + SAVEFILE_HASH_LENGH) {
		ons.errorAndExit("Unrecognised data was discovered in the save file");
	}

	return 0;
}

int ONScripter::saveSaveFile(int no, const char *savestr, bool no_error) {
	// make save data structure on memory
	if ((no < 0) || (saveon_flag && internal_saveon_flag)) {
		// Unsure if perfectly safe, but should be not bad
		if (skip_mode & SKIP_SUPERSKIP)
			return 0;

		file_io_buf.clear();
		saveSaveFileData();
		save_data_buf = file_io_buf;
	}

	if (no >= 0) {
		saveAll(no_error);

		file_io_buf.clear();
		writeSaveFileHeader(savestr);
		file_io_buf.insert(file_io_buf.end(), save_data_buf.begin(), save_data_buf.end());
		writeChecksum();

		char filename[16];
		std::snprintf(filename, sizeof(filename), "save%d.dat", no);

		if (saveFileIOBuf(filename)) {
			return -1;
		}
	}

	return 0;
}

bool ONScripter::readIniFile(const char *path, IniContainer &result) {
	char *fullpath = script_h.reader->completePath(path, FileType::File);
	FILE *fp       = nullptr;

	if (fullpath && (fp = FileIO::openFile(fullpath, "r")) != nullptr) {
		char ini_buf[256];
		std::string ini_sec;

		while (fgets(ini_buf, sizeof(ini_buf), fp)) {
			size_t c = 1;

			if (ini_buf[0] == '[') {
				while (ini_buf[c] != ']' && ini_buf[c] != '\0') c++;
				ini_sec = std::string(ini_buf + 1, c - 1);
				continue;
			} else if (ini_sec.empty()) {
				continue;
			}

			while (ini_buf[c] != '"' && ini_buf[c] != '\0') c++;
			std::string ini_key(ini_buf + 1, c - 1);

			c++;
			while (ini_buf[c] == ' ' || ini_buf[c] == '\t' || ini_buf[c] == '=') c++;

			if (ini_buf[c] != '"') {
				continue;
			} else {
				c++;
			}

			size_t d = c;
			while (ini_buf[c] != '"' && ini_buf[c] != '\0') c++;
			std::string reg_val(ini_buf + d, c - d);

			result[ini_sec][ini_key] = reg_val;
		}

		std::fclose(fp);
	}

	freearr(&fullpath);
	return fp;
}

bool ONScripter::readAdler32Hash(const char *path, uint32_t &adler) {
	// We should have relatively small files or it will lag badly

	if (loadFileIOBuf(path, false)) {
		return false;
	}

	adler = static_cast<uint32_t>(adler32_z(adler32_z(0, Z_NULL, 0), file_io_buf.data(), file_io_read_len));
	return true;
}

void ONScripter::saveReadLabels(const char *filename) {
	if (!script_h.save_path)
		return;

	file_io_buf.clear();
	int amount = 0;

	for (int write = 0; write < 2; write++) {
		if (write) {
			write32s(amount);
		}

		int sequence_size = 0;
		LabelInfo *lbl    = nullptr;
		for (auto it = script_h.logState.readLabels.begin(); it != script_h.logState.readLabels.end(); ++it) {
			if (*it && !sequence_size) {
				lbl = script_h.getLabelByIndex(static_cast<uint32_t>(it - script_h.logState.readLabels.begin()));
				sequence_size++;
			} else if (*it) {
				sequence_size++;
			} else if (lbl) {
				if (write) {
					writeStr(lbl->name);
					write32s(sequence_size);
				} else {
					amount++;
				}
				sequence_size = 0;
				lbl           = nullptr;
			}
		}
	}

	saveFileIOBuf(filename);
}

void ONScripter::loadReadLabels(const char *filename) {
	if (!script_h.save_path)
		return;

	if (loadFileIOBuf(filename)) {
		//sendToLog(LogLevel::Error, "can't open label file %s\n", filename);
		return;
	}

	uint32_t labels = read32s(), index = 0, sequence_size = 0;
	char *buf{nullptr};
	for (uint32_t i = 0; i < labels; i++) {
		readStr(&buf);
		index = script_h.getLabelIndex(script_h.lookupLabel(buf));
		for (sequence_size = read32s(); sequence_size > 0; sequence_size--, index++) {
			if (static_cast<size_t>(index) < script_h.logState.readLabels.size()) {
				script_h.logState.readLabels[index] = true;
			}
		}
	}
}
