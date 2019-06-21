/**
 *  Effect.cpp
 *  ONScripter-RU
 *
 *  Effect executer core code.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

/*
 *  Uses emulation of Takashi Toyama's "cascade.dll", "whirl.dll",
 *  "trvswave.dll", and "breakup.dll" NScripter plugin effects.
 */

#include "Engine/Core/ONScripter.hpp"

static char *dll = nullptr, *params = nullptr; //for dll-based effects

bool ONScripter::constantRefreshEffect(EffectLink *effect, bool clear_dirty_rect_when_done, bool async, int refresh_mode_src, int refresh_mode_dst) {

	if (effect->effect == 0)
		return true; // true: go home; no effect performed or scheduled (it failed)

	if (effect->effect == 1 && (skip_mode & SKIP_SUPERSKIP)) {
		if (!(refresh_mode_src & REFRESH_BEFORESCENE_MODE) || !(refresh_mode_dst & REFRESH_BEFORESCENE_MODE))
			commitVisualState();
		return false; // No need to bother
	}

	//sendToLog(LogLevel::Info, "constantRefreshEffect start\n");

	if (effect->effect == 15 || effect->effect == 18) {
		if (!effect->anim.gpu_image) {
			parseTaggedString(&effect->anim, true);
			setupAnimationInfo(&effect->anim);
			static int calls = 0;
			calls++;
			if (!effect->anim.gpu_image) {
				sendToLog(LogLevel::Error, "constantRefreshEffect setupAnimationInfo failed on call %d\n", calls);
			}
		}
	}

	// Save into global state for performing during constant refresh.
	effect_current          = effect;
	effect_set              = false;
	effect_rect_cleanup     = clear_dirty_rect_when_done;
	effect_refresh_mode_src = refresh_mode_src;
	effect_refresh_mode_dst = refresh_mode_dst;

	if (!async) {
		event_mode = IDLE_EVENT_MODE;
		while (effect_current) {
			waitEvent(0);
		}
	}

	//sendToLog(LogLevel::Info, "constantRefreshEffect return\n");

	return false; // false: effect complete or scheduled
}

bool ONScripter::setEffect() {
	/*	sendToLog(LogLevel::Info, "setEffect. effect_no %i, dirty_rect_hud.bounding_box xywh: %i %i %i %i\n",
			effect->effect,
			dirty_rect_hud.bounding_box.x, dirty_rect_hud.bounding_box.y, dirty_rect_hud.bounding_box.w, dirty_rect_hud.bounding_box.h);
	
	sendToLog(LogLevel::Info, "setEffect. effect_no %i, dirty_rect_scene.bounding_box xywh: %i %i %i %i\n",
			effect->effect,
			dirty_rect_scene.bounding_box.x, dirty_rect_scene.bounding_box.y, dirty_rect_scene.bounding_box.w, dirty_rect_scene.bounding_box.h);
*/

	EffectLink *effect       = effect_current;
	int refresh_mode_dst     = effect_refresh_mode_dst;

	if (effect->effect == 0)
		return true;

	int effect_no = effect->effect;

	if (refresh_mode_dst == -1)
		refresh_mode_dst = refreshMode();

	if (effect_dst_gpu == nullptr) {
		assert(hud_effect_dst_gpu == nullptr && combined_effect_dst_gpu == nullptr);
		effect_dst_gpu = gpu.getCanvasImage();
		hud_effect_dst_gpu = gpu.getCanvasImage();
		combined_effect_dst_gpu = gpu.getScriptImage();
	}

	if (pre_screen_gpu == nullptr)
		pre_screen_gpu = gpu.getScriptImage();

	//Copy old data to _dst in case we don't update the whole screen
	gpu.copyGPUImage(accumulation_gpu, nullptr, nullptr, effect_dst_gpu->target);
	gpu.copyGPUImage(hud_gpu, nullptr, nullptr, hud_effect_dst_gpu->target);

	//All these commands may be called from CR and mergeForEffect calls refresh*To afterwards.
	//If we don't provide CR_MODE we will never get proper combined_*gpu for refreshMode()
	if (effect_no == 1) {
		mergeForEffect(combined_effect_dst_gpu,
		               &dirty_rect_scene.bounding_box_script,
		               &dirty_rect_hud.bounding_box_script,
		               refresh_mode_dst | CONSTANT_REFRESH_MODE);
	} else {
		// Allocate src images for transitional effects!
		if (effect_src_gpu == nullptr) {
			assert(hud_effect_src_gpu == nullptr && combined_effect_src_gpu == nullptr);
			effect_src_gpu = gpu.getCanvasImage();
			hud_effect_src_gpu = gpu.getCanvasImage();
			combined_effect_src_gpu = gpu.getScriptImage();
		}

		gpu.copyGPUImage(accumulation_gpu, nullptr, nullptr, effect_src_gpu->target);
		gpu.copyGPUImage(hud_gpu, nullptr, nullptr, hud_effect_src_gpu->target);
		mergeForEffect(combined_effect_src_gpu, nullptr, nullptr, REFRESH_NONE_MODE);
		mergeForEffect(combined_effect_dst_gpu, nullptr, nullptr, refresh_mode_dst | CONSTANT_REFRESH_MODE);
	}

	effect_counter       = 0;
	effect_previous_time = SDL_GetTicks();
	effect_duration      = effect->duration;
	effect_first_time    = true;

	if (keyState.ctrl || skip_mode & SKIP_NORMAL) {
		// shorten the duration of effects while skipping
		if (effect_cut_flag) {
			effect_duration = 0;
			return false; //don't parse effects if effectcut skip
		}
		if (effect_duration > 100) {
			effect_duration = effect_duration / 10;
		} else if (effect_duration > 10) {
			effect_duration = 10;
		} else {
			effect_duration = 1;
		}
	} else if (effectspeed == EFFECTSPEED_INSTANT) {
		effect_duration = 0;
		return false; //don't parse effects if instant speed
	} else if (effectspeed == EFFECTSPEED_QUICKER) {
		effect_duration = effect_duration / 2;
		if (effect_duration <= 0)
			effect_duration = 1;
	}

	/* Load mask image */
	if (effect_no == 15 || effect_no == 18) {
		if (!effect->anim.gpu_image) {
			sendToLog(LogLevel::Error, "We should have a gpu_image here built by constantRefreshEffect. This is madness...\n");
		}
	}
	if (effect_no == 11 || effect_no == 12 || effect_no == 13 || effect_no == 14 ||
	    effect_no == 16 || effect_no == 17) {
		fillCanvas();
	}

	dll = params = nullptr;
	if (effect_no == 99) { // dll-based
		dll = effect->anim.image_name;
		if (dll != nullptr) { //just in case no dll is given
			if (debug_level > 0)
				sendToLog(LogLevel::Info, "dll effect: Got dll/params '%s'\n", dll);

			params = dll;
			while (*params != 0 && *params != '/') params++;
			if (*params == '/')
				params++;
			fillCanvas();
		}
	}
	return false;
}

void ONScripter::mergeForEffect(GPU_Image *dst, GPU_Rect *scene_rect, GPU_Rect *hud_rect, int refresh_mode) {
	if (dst != combined_effect_src_gpu && dst != combined_effect_dst_gpu)
		throw std::runtime_error("unexpected GPU_Image used for merge");

	GPU_Rect full_rect = full_script_clip;
	if (!scene_rect)
		scene_rect = &full_rect;
	if (!hud_rect)
		hud_rect = &full_rect;

	//sendToLog(LogLevel::Info, "mergeForEffect with dst %d\n", dst==combined_effect_dst_gpu);

	if (dst == combined_effect_src_gpu) {
		combineWithCamera(effect_src_gpu, hud_effect_src_gpu, combined_effect_src_gpu->target, *scene_rect, *hud_rect, refresh_mode);
	} else {
		combineWithCamera(effect_dst_gpu, hud_effect_dst_gpu, combined_effect_dst_gpu->target, *scene_rect, *hud_rect, refresh_mode);
	}
}

bool ONScripter::doEffect() {
	EffectLink *effect   = effect_current;
	int refresh_mode_src = effect_refresh_mode_src;
	int refresh_mode_dst = effect_refresh_mode_dst;

	if (refresh_mode_src == -1)
		refresh_mode_src = refreshMode() | REFRESH_BEFORESCENE_MODE;
	if (refresh_mode_dst == -1)
		refresh_mode_dst = refreshMode();
	bool no_commit = (refresh_mode_src & REFRESH_BEFORESCENE_MODE) &&
	                 (refresh_mode_dst & REFRESH_BEFORESCENE_MODE);

	int start_time       = SDL_GetTicks();
	int timer_resolution = start_time - effect_previous_time;
	effect_previous_time = start_time;

	int effect_no = effect->effect;
	if (effect_first_time) {
		if ((effect_cut_flag && (keyState.ctrl || skip_mode & SKIP_NORMAL)) ||
		    effectspeed == EFFECTSPEED_INSTANT)
			effect_no = 1;
	}

	/* ---------------------------------------- */
	/* Execute effect */
	if (debug_level > 1 && effect_first_time)
		sendToLog(LogLevel::Info, "Effect number %d, %d ms\n", effect_no, effect_duration);

	bool not_implemented = false;
	switch (effect_no) {
		case 0: // Instant display
		case 1: // Instant display
			break;

		default:
			//not_implemented = true;
			if (effect_first_time) {
				std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
				              "effect No. %d not implemented; substituting crossfade",
				              effect_no);
				errorAndCont(script_h.errbuf);
			}
			/* fall through */

		case 10: // Cross fade
			effectBlendToCombinedImage(nullptr, ALPHA_BLEND_CONST, 256 * effect_counter / effect_duration, pre_screen_gpu);
			break;

		case 15: // Fade with mask
			effectBlendToCombinedImage(effect->anim.gpu_image, ALPHA_BLEND_FADE_MASK, 256 * effect_counter / effect_duration, pre_screen_gpu);
			break;

		case 18: // Cross fade with mask
			effectBlendToCombinedImage(effect->anim.gpu_image, ALPHA_BLEND_CROSSFADE_MASK, 256 * effect_counter * 2 / effect_duration, pre_screen_gpu);
			break;

		case 99: // dll-based
			if (dll != nullptr) {
				if (!std::strncmp(dll, "whirl.dll", std::strlen("whirl.dll"))) {
					effectWhirl(params, effect_duration);
				} else if (!std::strncmp(dll, "trvswave.dll", std::strlen("trvswave.dll"))) {
					effectTrvswave(params, effect_duration);
				} else if (!std::strncmp(dll, "breakup.dll", std::strlen("breakup.dll"))) {
					effectBreakupParser(params, refresh_mode_src, refresh_mode_dst);
				} else if (!std::strncmp(dll, "glass.dll", std::strlen("glass.dll"))) {
					if (new_glass_smash_implementation)
						effectBrokenGlassParser(params, refresh_mode_src, refresh_mode_dst);
					else
						effectTrvswave(params, effect_duration);
				} else {
					not_implemented = true;
					if (effect_first_time) {
						std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
						              "dll effect '%s' (%d) not implemented; substituting crossfade",
						              dll, effect_no);
						errorAndCont(script_h.errbuf);
					}
				}
			} else { //just in case no dll is given
				not_implemented = true;
				if (effect_first_time) {
					std::snprintf(script_h.errbuf, MAX_ERRBUF_LEN,
					              "no dll provided for effect %d; substituting crossfade",
					              effect_no);
					errorAndCont(script_h.errbuf);
				}
			}
			if (not_implemented) {
				// do crossfade
				effectBlendGPU(nullptr, ALPHA_BLEND_CONST, 256 * effect_counter / effect_duration, &dirty_rect_scene.bounding_box_script);
			}
			break;
	}

	if (debug_level > 1)
		sendToLog(LogLevel::Info, "\teffect count %d / dur %d\n", effect_counter, effect_duration);

	effect_counter += timer_resolution;
	effect_first_time = false;

	if (effect_counter < effect_duration && effect_no != 1) {
		if (effectskip_flag && skip_effect && skip_enabled) {
			effect_counter = effect_duration;
		}
		return true;
	}

	// last call
	gpu.copyGPUImage(effect_dst_gpu, nullptr, nullptr, accumulation_gpu->target);
	gpu.clearWholeTarget(hud_gpu->target);
	gpu.copyGPUImage(hud_effect_dst_gpu, nullptr, nullptr, hud_gpu->target);

	if (!no_commit)
		commitVisualState();

	pre_screen_render = false;
	if (pre_screen_gpu) {
		gpu.giveScriptImage(pre_screen_gpu);
		pre_screen_gpu = nullptr;
	}

	if (effect_no > 1)
		fillCanvas(false, true); //formerly true, false (creates #110)

	// free upon next effect
	gpu.giveCanvasImage(effect_dst_gpu);
	gpu.giveCanvasImage(hud_effect_dst_gpu);
	gpu.giveScriptImage(combined_effect_dst_gpu);
	effect_dst_gpu = nullptr;
	hud_effect_dst_gpu = nullptr;
	combined_effect_dst_gpu = nullptr;

	if (effect_src_gpu != nullptr && hud_effect_src_gpu != nullptr && combined_effect_src_gpu != nullptr) {
		gpu.giveCanvasImage(effect_src_gpu);
		gpu.giveCanvasImage(hud_effect_src_gpu);
		gpu.giveScriptImage(combined_effect_src_gpu);
		effect_src_gpu = nullptr;
		hud_effect_src_gpu = nullptr;
		combined_effect_src_gpu = nullptr;
	}

	if (effect_no == 1)
		effect_counter = 0;
	else if (effect_no == 99 && dll != nullptr)
		dll = params = nullptr;

	return false;
}

void ONScripter::sendToPreScreen(bool refreshSrc, std::function<PooledGPUImage(GPUTransformableCanvasImage &)> applyTransform, int refresh_mode_src, int refresh_mode_dst) {
	if (refreshSrc || camera.has_moved || !before_dirty_rect_scene.isEmpty() || !before_dirty_rect_hud.isEmpty()) {
		int rm = refresh_mode_src | CONSTANT_REFRESH_MODE;
		combineWithCamera(effect_src_gpu, hud_effect_src_gpu, combined_effect_src_gpu->target,
		                  before_dirty_rect_scene.bounding_box_script, before_dirty_rect_hud.bounding_box_script, rm);
	}

	if (!refreshSrc || camera.has_moved || !dirty_rect_scene.isEmpty() || !dirty_rect_hud.isEmpty()) {
		int rm = refresh_mode_dst | CONSTANT_REFRESH_MODE;
		combineWithCamera(effect_dst_gpu, hud_effect_dst_gpu, combined_effect_dst_gpu->target,
		                  dirty_rect_scene.bounding_box_script, dirty_rect_hud.bounding_box_script, rm);
	}

	GPU_Image *lower = refreshSrc ? combined_effect_dst_gpu : combined_effect_src_gpu;
	GPU_Image *upper = refreshSrc ? combined_effect_src_gpu : combined_effect_dst_gpu;

	GPUTransformableCanvasImage transform(upper);
	PooledGPUImage result = applyTransform(transform);

	gpu.clearWholeTarget(upper->target);
	GPU_SetBlending(upper, false);
	gpu.copyGPUImage(result.image, nullptr, nullptr, upper->target);
	GPU_SetBlending(upper, true);

	pre_screen_render = true;
	if (pre_screen_gpu == nullptr)
		pre_screen_gpu = gpu.getScriptImage();

	GPU_SetBlending(lower, false);
	gpu.copyGPUImage(lower, nullptr, nullptr, pre_screen_gpu->target); //unchanged surface first
	GPU_SetBlending(lower, true);
	gpu.copyGPUImage(upper, nullptr, nullptr, pre_screen_gpu->target); //then the changed one
}

void ONScripter::effectBreakupParser(const char *params, int refresh_mode_src, int refresh_mode_dst) {
	bool refreshSrc  = params[2] != 'p' && params[2] != 'P';
	int breakupValue = refreshSrc ? 1000 * effect_counter / effect_duration : 1000 - (1000 * effect_counter / effect_duration);

	sendToPreScreen(refreshSrc, [breakupValue, params](GPUTransformableCanvasImage &transform) {
		return gpu.getBrokenUpImage(transform, {{BreakupType::GLOBAL, 0}}, breakupValue,
		                            BREAKUP_MODE_LEFT, params);
	},
	                refresh_mode_src, refresh_mode_dst);
}

void ONScripter::effectBrokenGlassParser(const char *params, int refresh_mode_src, int refresh_mode_dst) {
	int smashFactor = 1000 * effect_counter / effect_duration;

	// Reset per each effect
	if (effect_first_time) {
		glassSmashData.smashParameter = script_h.parseInt(&params);
		if (glassSmashData.smashParameter == 0)
			glassSmashData.smashParameter = GlassSmashData::DefaultParameter;

		glassSmashData.initialised = false;
	}

	sendToPreScreen(true, [smashFactor](GPUTransformableCanvasImage &transform) {
		return gpu.getGlassSmashedImage(transform, smashFactor);
	},
	                refresh_mode_src, refresh_mode_dst);
}
