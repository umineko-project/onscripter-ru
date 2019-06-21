/**
 *  Animation.cpp
 *  ONScripter-RU
 *
 *  Methods to manipulate AnimationInfo.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Graphics/GPU.hpp"
#include "Support/FileIO.hpp"

#include <unistd.h>

int ONScripter::proceedAnimation() {
	int minimum_duration = -1;

	for (auto anim : sprites(SPRITE_TACHI | SPRITE_LSP | SPRITE_LSP2, true)) {
		if (anim->visible && anim->is_animatable) {
			minimum_duration = estimateNextDuration(anim, anim->pos, minimum_duration);
		}
		if (anim->old_ai && anim->old_ai->visible && anim->old_ai->is_animatable) {
			minimum_duration = estimateNextDuration(anim->old_ai, anim->old_ai->pos, minimum_duration, true);
		}
	}

//Mion - ogapee2009
#ifdef USE_LUA
	if (lua_handler.is_animatable && !script_h.isExternalScript()) {
		if (lua_handler.remaining_time == 0) {
			lua_handler.remaining_time = lua_handler.duration_time;
			if (minimum_duration == -1 ||
			    minimum_duration > lua_handler.remaining_time)
				minimum_duration = lua_handler.remaining_time;
			int lua_event_mode           = event_mode;
			int tmp_string_buffer_offset = string_buffer_offset;
			const char *current          = script_h.getCurrent();
			lua_handler.callback(LUAHandler::LUA_ANIMATION);
			script_h.setCurrent(current);
			readToken();
			string_buffer_offset = tmp_string_buffer_offset;
			event_mode           = lua_event_mode;
		} else if ((minimum_duration == -1) ||
		           (minimum_duration > lua_handler.remaining_time)) {
			minimum_duration = lua_handler.remaining_time;
		}
	}
#endif

	return minimum_duration;
}

int ONScripter::proceedCursorAnimation() {
	if (enable_custom_cursors)
		return -1;

	int minimum_duration = -1;
	AnimationInfo *anim  = nullptr;

	if (!textgosub_label && draw_cursor_flag &&
	    (clickstr_state == CLICK_WAIT || clickstr_state == CLICK_NEWPAGE)) {

		if (clickstr_state == CLICK_WAIT)
			anim = &cursor_info[CURSOR_WAIT_NO];
		else
			anim = &cursor_info[CURSOR_NEWPAGE_NO];

		if (anim->visible && anim->is_animatable) {
			GPU_Rect dst_rect = anim->pos;
			if (!anim->abs_flag) {
				dst_rect.x += sentence_font.x();
				dst_rect.y += sentence_font.y();
			}

			minimum_duration = estimateNextDuration(anim, dst_rect, minimum_duration);
		}

		if (anim->old_ai && anim->old_ai->visible && anim->old_ai->is_animatable) {
			GPU_Rect dst_rect = anim->old_ai->pos;
			if (!anim->old_ai->abs_flag) {
				dst_rect.x += sentence_font.x();
				dst_rect.y += sentence_font.y();
			}

			minimum_duration = estimateNextDuration(anim->old_ai, dst_rect, minimum_duration, true);
		}
	}

	return minimum_duration;
}

int ONScripter::estimateNextDuration(AnimationInfo *anim, GPU_Rect & /*rect*/, int minimum, bool old_ai) {
	if (anim->clock.expired()) {
		if (anim->trans_mode != AnimationInfo::TRANS_LAYER) {
			if ((minimum == -1) ||
			    (minimum > anim->getDuration(anim->current_cell)))
				minimum = anim->getDuration(anim->current_cell);
			if (anim->proceedAnimation()) {
				dirtySpriteRect(anim, old_ai);
			}
		} else if (anim->layer_no >= 0) {
			auto handler = getLayer<Layer>(anim->layer_no, false);
			//sendToLog(LogLevel::Info, "About to update AI %p, anim->clock.time() %d\n", anim, anim->clock.time());
			if (handler->update(old_ai))
				dirtySpriteRect(anim, old_ai);
			anim->clock.setCountdownNanos(anim->getDurationNanos(anim->current_cell));
			if ((minimum == -1) ||
			    (minimum > anim->getDuration(anim->current_cell)))
				minimum = anim->getDuration(anim->current_cell);
		}
	} else {
		if ((minimum == -1) || (minimum) > static_cast<int>(anim->clock.remaining()))
			minimum = anim->clock.remaining();
	}

	return minimum;
}

void ONScripter::advanceAIclocks(uint64_t ns) {
	for (int i = 0; i < 3; i++) {
		advanceSpecificAIclocks(ns, i, -1);
	}
	for (int i = MAX_SPRITE_NUM - 1; i >= 0; i--) { // why the hell backwards... stand upright ons writers
		advanceSpecificAIclocks(ns, i, 0);
		advanceSpecificAIclocks(ns, i, 1);
	}

//Mion - ogapee2009
#ifdef USE_LUA
	if (lua_handler.is_animatable && !script_h.isExternalScript())
		lua_handler.remaining_time -= (ns / 1000000);
#endif
}

// Can put this in AI if you want
void ONScripter::advanceSpecificAIclocks(uint64_t ns, int i, int type, bool old_ai) {
	AnimationInfo *ai = type == 0 ? &sprite_info[i] :
	                                type > 0 ? &sprite2_info[i] :
	                                           &tachi_info[i];

	if (old_ai) {
		if (!ai->old_ai) {
			errorAndExit("Asked to advance clocks for a non-existent old_ai");
			return; //dummy
		}
		ai = ai->old_ai;
	}

	if (ai->visible && ai->is_animatable) {
		ai->clock.tickNanos(ns);
		//sendToLog(LogLevel::Info, "Advanced clock for AI %p, anim->clock.time() %i, remaining() %i, expired() %i\n", anim, anim->clock.time(), anim->clock.remaining(), anim->clock.expired());
	}

	// update sprite camera positions
	// ***************** Is this broken now that we allowed camera clock updates for old_ais?? ***********************
	if (ai->camera.isMoving() && type >= 0) {
		dirtySpriteRect(i, type == 1);
		ai->camera.update(static_cast<unsigned int>(ns / 1000000));
		dirtySpriteRect(i, type == 1);
	}

	if (ai->spriteTransforms.warpAmplitude != 0)
		ai->spriteTransforms.warpClock.tickNanos(ns);

	if (ai->old_ai)
		advanceSpecificAIclocks(ns, i, type, true);
}

void ONScripter::setupAnimationInfo(AnimationInfo *anim, Fontinfo *info) {
	if (anim->gpu_image && !anim->stale_image) {
		anim->exists = true;
		return;
	}

	if (deferredLoadingEnabled && !info && (skip_mode & SKIP_SUPERSKIP) && anim->type == SPRITE_LSP2) {
		// Superskip check should be unnecessary actually...
		anim->exists          = true;
		anim->deferredLoading = true;
		return;
	}

	auto st = SDL_GetTicks();
	anim->deleteImage();
	anim->abs_flag = true;

	if (anim->trans_mode == AnimationInfo::TRANS_STRING) {
		Fontinfo f_info = info ? *info : sentence_font;

		while (f_info.styleStack.size() > 1) f_info.styleStack.pop();

		f_info.clear();

		if (anim->font_size_xy[0] >= 0) {
			f_info.top_xy[0]               = anim->orig_pos.x;
			f_info.top_xy[1]               = anim->orig_pos.y;
			f_info.changeStyle().font_size = anim->font_size_xy[0] > anim->font_size_xy[1] ? anim->font_size_xy[0] : anim->font_size_xy[1];
		}

		anim->pos.x = f_info.x();
		anim->pos.y = f_info.y();

		f_info.changeStyle().is_gradient = use_text_gradients_for_sprites;
		f_info.top_xy[0]                 = 0;
		f_info.top_xy[1]                 = 0;

		copyarr(f_info.buttonMultiplyColor, anim->color_list[0]);

		TextRenderingState state;
		uint16_t w, h;
		GPU_Rect clip{0, 0, 0, 0};
		state.dstClip = &clip;

		//TODO: make this configurable? vertical cells for buttons are less likely to exceed the texture limit
		anim->vertical_cells = true;

		for (int i = 0; i < anim->num_of_cells; i++) {
			//TODO: anim->skip_whitespace?

			if (i == 0) {
				dlgCtrl.prepareForRendering(anim->file_name, f_info, state, w, h);
				if (w == 0 || h == 0)
					break;

				if (!anim->vertical_cells)
					w *= anim->num_of_cells;
				else
					h *= anim->num_of_cells;

				if (anim->is_big_image) {
					anim->setBigImage(new GPUBigImage(w, h, 4));
					state.dst.bigImage = anim->big_image.get();
				} else {
					anim->setImage(gpu.createImage(w, h, 4));
					GPU_GetTarget(anim->gpu_image);
					state.dst.target = anim->gpu_image->target;
				}

				clip.w = anim->pos.w;
				clip.h = anim->pos.h;
			} else {
				// Shift letters
				if (!anim->vertical_cells) {
					state.offset.x += anim->pos.w;
					clip.x = anim->pos.w * i;
				} else {
					state.offset.y += anim->pos.h;
					clip.y = anim->pos.h * i;
				}
				// Update multiply colour
				for (auto &piece : state.getPieces(true))
					for (auto &fi : piece->fontInfos)
						copyarr(fi.buttonMultiplyColor, anim->color_list[i]);
			}

			dlgCtrl.render(state);
		}
	} else if (anim->trans_mode == AnimationInfo::TRANS_LAYER) {
		//pos w&h already screen-size
		anim->calculateImage(anim->pos.w, anim->pos.h);
		//anim->fill(0, 0, 0, 0);
	} else {
		async.loadImage(anim);
		// Wait in loop (like crEffect) until we are loaded
		// detect events from during image loading & resizing, but without any image refresh (esp. if trapping)
		int old_event_mode = event_mode;
		event_mode         = IDLE_EVENT_MODE;
		preventExit(true);
		while (SDL_SemWaitTimeout(async.loadImageQueue.resultsWaiting, 1) != 0)
			waitEvent(0);
		preventExit(false);
		event_mode = old_event_mode;
		buildGPUImage(*anim);
		freeRedundantSurfaces(*anim);
	}
	anim->stale_image     = false;
	anim->exists          = true;
	anim->deferredLoading = false;

	internal_slowdown_counter += SDL_GetTicks() - st;
}

void ONScripter::postSetupAnimationInfo(AnimationInfo *anim) {
	if (anim->type == SPRITE_LSP2) {
		anim->calcAffineMatrix(window.script_width, window.script_height);
		if (anim->visible)
			dirtySpriteRect(anim);
		if (anim->layer_no < 0)
			anim->is_animatable = false; //extended sprites don't animate unless they display layers
	}
}

void ONScripter::buildAIImage(AnimationInfo *anim) {
	bool has_alpha{false};
	bool allow_24_bpp{anim->trans_mode == AnimationInfo::TRANS_COPY};

	SDL_Surface *surface = loadImage(anim->file_name, &has_alpha, allow_24_bpp);
	if (!surface)
		return;

	bool using_24_bpp = surface->format->BitsPerPixel == 24;

	SDL_Surface *surface_m = nullptr;
	if (anim->trans_mode == AnimationInfo::TRANS_MASK)
		surface_m = loadImage(anim->mask_file_name);

	if (!using_24_bpp) {
		surface = anim->setupImageAlpha(surface, surface_m, has_alpha);
	}

	anim->setSurface(surface);

	if (surface_m)
		SDL_FreeSurface(surface_m);
}

bool ONScripter::treatAsSameImage(const AnimationInfo &anim1, const AnimationInfo &anim2) {
	// returns true if the AnimationInfo tags would create identical images
	if (&anim1 == &anim2)
		return true;

	if ((anim1.trans_mode != anim2.trans_mode) ||
	    //assume layers aren't identical
	    (anim1.trans_mode == AnimationInfo::TRANS_LAYER) ||
	    (anim1.is_animatable != anim2.is_animatable) ||
	    (anim1.num_of_cells != anim2.num_of_cells) ||
	    (anim1.vertical_cells != anim2.vertical_cells))
		return false;

	if (!equalstr(anim1.file_name, anim2.file_name) ||
	    !equalstr(anim1.mask_file_name, anim2.mask_file_name))
		return false;

	if (anim1.color != anim2.color ||
	    (anim1.trans_mode == AnimationInfo::TRANS_DIRECT &&
	     anim1.direct_color != anim2.direct_color))
		return false;

	if (anim1.trans_mode == AnimationInfo::TRANS_STRING) {
		for (int i = 0; i < anim1.num_of_cells; i++) {
			if (anim1.color_list[i] != anim2.color_list[i])
				return false;
		}
	}
	//by this point, they most likely create the same images
	return true;
}

void ONScripter::parseTaggedString(AnimationInfo *anim, bool is_mask) {
	if (anim->image_name == nullptr)
		return;

	AnimationInfo acopy;
	if (!anim->stale_image && anim->gpu_image)
		acopy.deepcopyNonImageFields(*anim); // a copy of the tag, for later comparison

	bool has = anim->has_z_order_override;
	anim->removeTag();
	if (preserve) {
		anim->has_z_order_override = has;
	}

	int i;
	const char *buffer = anim->image_name;

	anim->num_of_cells = 1;
	anim->current_cell = 0;
	anim->trans_mode   = trans_mode;
	//use COPY as default trans_mode for masks
	if (is_mask)
		anim->trans_mode = AnimationInfo::TRANS_COPY;

	if (buffer[0] == '*') {
		//Mion: it's a layer!

		anim->trans_mode = AnimationInfo::TRANS_LAYER;
		buffer++;
		anim->layer_no = getNumberFromBuffer(&buffer);

		auto tmp = getLayerInfo(anim->layer_no, false);

		if (tmp) {
			anim->pos.x = anim->pos.y = 0;
			anim->pos.w               = window.script_width;
			anim->pos.h               = window.script_height;
			tmp->handler->setSpriteInfo(sprite_info, anim);
			anim->duration_list    = new int[1];
			anim->duration_list[0] = tmp->interval;
			anim->is_animatable    = true;
			sendToLog(LogLevel::Info, "setup a sprite for layer %d\n", anim->layer_no);
		} else
			anim->layer_no = -1;
		return;
	}
	if (buffer[0] == ':') {
		while (*++buffer == ' ')
			;

		if (buffer[0] == 'a') {
			anim->trans_mode = AnimationInfo::TRANS_ALPHA;
			buffer++;
		} else if (buffer[0] == 'l') {
			anim->trans_mode = AnimationInfo::TRANS_TOPLEFT;
			buffer++;
		} else if (buffer[0] == 'r') {
			anim->trans_mode = AnimationInfo::TRANS_TOPRIGHT;
			buffer++;
		} else if (buffer[0] == 'c') {
			anim->trans_mode = AnimationInfo::TRANS_COPY;
			buffer++;
		} else if (buffer[0] == 'd') {
			anim->trans_mode    = AnimationInfo::TRANS_COPY;
			anim->blending_mode = BlendModeId::ADD;
			buffer++;
		} else if (buffer[0] == 'b') {
			anim->trans_mode    = AnimationInfo::TRANS_COPY;
			anim->blending_mode = BlendModeId::SUB;
			buffer++;
		} else if (buffer[0] == 'u') {
			anim->trans_mode    = AnimationInfo::TRANS_COPY;
			anim->blending_mode = BlendModeId::MUL;
			buffer++;
		} else if (buffer[0] == 's') {
			anim->trans_mode = AnimationInfo::TRANS_STRING;
			buffer++;
			anim->num_of_cells = 0;
			if (*buffer == '/') {
				buffer++;
				script_h.getNext();

				script_h.pushCurrent(buffer);
				anim->font_size_xy[0] = script_h.readInt();
				anim->font_size_xy[1] = script_h.readInt();
				//IMPORTED: from ONS (pitch param is optional)
				if (script_h.hasMoreArgs()) {
					script_h.readInt(); // dummy read for pitch
					if (script_h.hasMoreArgs()) {
						script_h.readInt(); // 0 ... normal, 1 ... no anti-aliasing, 2 ... Fukuro
					}
				}
				buffer = script_h.getNext();
				script_h.popCurrent();
			} else {
				anim->font_size_xy[0] = anim->font_size_xy[1] = sentence_font.style().font_size;
			}
			while (buffer[0] != '#' && buffer[0] != '\0') buffer++;
			i = 0;
			while (buffer[i] == '#') {
				anim->num_of_cells++;
				i += 7;
			}
			anim->color_list = new uchar3[anim->num_of_cells];
			for (i = 0; i < anim->num_of_cells; i++) {
				readColor(&anim->color_list[i], buffer);
				buffer += 7;
			}
		} else if (buffer[0] == 'm') {
			anim->trans_mode  = AnimationInfo::TRANS_MASK;
			const char *start = ++buffer;
			while (buffer[0] != ';' && buffer[0] != 0x0a && buffer[0] != '\0') buffer++;
			if (buffer[0] == ';')
				script_h.setStr(&anim->mask_file_name, start, buffer - start);
		} else if (buffer[0] == '#') {
			anim->trans_mode = AnimationInfo::TRANS_DIRECT;
			readColor(&anim->direct_color, buffer);
			buffer += 7;
		} else if (buffer[0] == '!') {
			anim->trans_mode = AnimationInfo::TRANS_PALETTE;
			buffer++;
			getNumberFromBuffer(&buffer); //palette number, which is now dead
		}

		if (anim->trans_mode != AnimationInfo::TRANS_STRING)
			while (buffer[0] != '/' && buffer[0] != ';' && buffer[0] != '\0') buffer++;
	}

	//IMPORTED: from ONS
	if (buffer[0] == '/' && anim->trans_mode != AnimationInfo::TRANS_STRING) {
		buffer++;
		anim->num_of_cells = getNumberFromBuffer(&buffer);
		if (anim->num_of_cells == 0) {
			sendToLog(LogLevel::Error, "ONScripter::parseTaggedString  The number of cells is 0\n");
			return;
		}

		anim->duration_list = new int[anim->num_of_cells];

		if (*buffer == ',') {
			buffer++;

			if (*buffer == '<') {
				buffer++;
				for (i = 0; i < anim->num_of_cells; i++) {
					anim->duration_list[i] = getNumberFromBuffer(&buffer);
					buffer++;
				}
			} else {
				anim->duration_list[0] = getNumberFromBuffer(&buffer);
				for (i = 1; i < anim->num_of_cells; i++)
					anim->duration_list[i] = anim->duration_list[0];
			}
			//anim->remaining_time = anim->duration_list[0];

			buffer++;
			anim->loop_mode = *buffer++ - '0'; // 3...no animation

			if (*buffer == ',') {
				buffer++;
				anim->vertical_cells = getNumberFromBuffer(&buffer);
			}
		} else {
			for (i = 0; i < anim->num_of_cells; i++)
				anim->duration_list[0] = 0;
			anim->loop_mode = 3; // 3...no animation
		}
		if (anim->loop_mode != 3)
			anim->is_animatable = true;

		while (buffer[0] != ';' && buffer[0] != '\0') buffer++;
	}

	if (buffer[0] == ';' && anim->trans_mode != AnimationInfo::TRANS_STRING)
		buffer++;

	if (anim->trans_mode == AnimationInfo::TRANS_STRING && buffer[0] == '$') {
		script_h.pushCurrent(buffer);
		script_h.setStr(&anim->file_name, script_h.readStr());
		script_h.popCurrent();
	} else {
		script_h.setStr(&anim->file_name, buffer);
		translatePathSlashes(anim->file_name);
	}

	anim->stale_image = (anim->stale_image || (anim->gpu_image == nullptr) ||
	                     !treatAsSameImage(*anim, acopy));
}

void ONScripter::drawSpritesetToGPUTarget(GPU_Target *target, SpritesetInfo *spriteset, GPU_Rect *clip, int rm) {
	GPU_Rect myClip{0, 0, static_cast<float>(target->w), static_cast<float>(target->h)};
	if (clip) {
		myClip = *clip;
		myClip.x += camera.center_pos.x;
		myClip.y += camera.center_pos.y;
	}

	//This is a D.S.T. clip
	/*myClip.x += spriteset->pos.x;
	myClip.y += spriteset->pos.y;*/

	bool blur     = spriteset->blur > 0;
	bool mask     = spriteset->maskSpriteNumber != -1;
	bool breakup  = spriteset->breakupFactor > 0;
	bool pixelate = spriteset->pixelateFactor > 0;
	bool warp     = spriteset->warpAmplitude != 0;
	bool any      = blur || mask || breakup || pixelate || warp;

	auto &ssim     = rm & REFRESH_BEFORESCENE_MODE ? spriteset->im : spriteset->imAfterscene;
	GPU_Image *src = ssim.image;
	PooledGPUImage toDraw;
	if (any) {
		if (blur)
			toDraw = gpu.getBlurredImage(ssim, spriteset->blur);
		if (mask) {
			GPUTransformableCanvasImage tmp(toDraw.image);
			toDraw = gpu.getMaskedImage(toDraw.image ? tmp : ssim, sprite_info[spriteset->maskSpriteNumber].oldNew(rm)->gpu_image);
		}
		if (breakup) {
			GPUTransformableCanvasImage tmp(toDraw.image);
			toDraw = gpu.getBrokenUpImage(toDraw.image ? tmp : ssim, {{BreakupType::SPRITESET, static_cast<int16_t>(spriteset->id)}}, spriteset->breakupFactor, spriteset->breakupDirectionFlagset, nullptr);
		}
		if (pixelate) {
			GPUTransformableCanvasImage tmp(toDraw.image);
			toDraw = gpu.getPixelatedImage((toDraw.image ? tmp : ssim), spriteset->pixelateFactor);
		}
		if (warp) {
			GPUTransformableCanvasImage tmp(toDraw.image);
			float secs = spriteset->warpClock.time() / 1000.0;
			toDraw     = gpu.getWarpedImage(toDraw.image ? tmp : ssim, secs, spriteset->warpAmplitude, spriteset->warpWaveLength, spriteset->warpSpeed);
		}
		src = toDraw.image;
	}

	if (spriteset->trans < 255) {
		GPU_SetRGBA(src, spriteset->trans, spriteset->trans, spriteset->trans, spriteset->trans);
	}
	float xpos = spriteset->pos.x + (spriteset->has_scale_center ? spriteset->scale_center_x : src->w / 2.0);
	float ypos = spriteset->pos.y + (spriteset->has_scale_center ? spriteset->scale_center_y : src->h / 2.0);

	gpu.copyGPUImage(src, nullptr, &myClip, target, xpos, ypos, spriteset->scale_x / 100.0, spriteset->scale_y / 100.0, -spriteset->rot, true);
	if (spriteset->trans < 255) {
		GPU_SetRGBA(src, 255, 255, 255, 255);
	}
}

void ONScripter::drawSpecialScrollable(GPU_Target *target, AnimationInfo *info, int refresh_mode, GPU_Rect *clip) {
	if (!info->scrollableInfo.isSpecialScrollable)
		return;

	// We might have scrolled since last draw, so we've got to update the mouse cursor position...
	// This breaks mouseover for some buttons sometimes somehow and is not needed anymore (because we check in dynprop.apply()
	//refreshButtonHoverState();

	GPU_Rect canvasPos = info->pos;
	canvasPos.x += camera.center_pos.x;
	canvasPos.y += camera.center_pos.y;
	GPU_Rect localClip = clip ? *clip : canvasPos;
	doClipping(&localClip, &canvasPos);
	GPU_SetClipRect(target, localClip);

	AnimationInfo::ScrollableInfo &si = info->scrollableInfo;
	StringTree &tree                  = dataTrees[si.elementTreeIndex];
	int scroll_y                      = info->scrollable.y;

	//sendToLog(LogLevel::Info, "Hovered: %d\n", si.hoveredElement);

	auto first = getScrollableElementsVisibleAt(&si, tree, scroll_y);
	for (auto it = first; it != tree.insertionOrder.end(); ++it) {
		auto elementIndex = it - tree.insertionOrder.begin();

		StringTree &elem = tree[*it];
		if (elem.has("log")) {
			bool read = script_h.logState.logEntryIndexToIsRead(std::stoi(elem["log"].value));
			if (!read)
				break;
		}

		float w = si.elementWidth ? si.elementWidth : info->pos.w;
		float h = si.elementHeight;
		GPU_Rect elemRect{0, 0, w, h};
		setRectForScrollableElement(&elem, elemRect);
		if (elemRect.y - scroll_y > info->pos.h) {
			// we're off the bottom of the visible area, break
			break;
		}

		auto spriteBg = elem.has("bg") ? &sprite2_info[std::stoi(elem["bg"].value)] : si.elementBackground;
		if (spriteBg)
			spriteBg = spriteBg->oldNew(refresh_mode);

		float yTop  = elemRect.y - scroll_y + camera.center_pos.y;
		float yBot  = yTop + elemRect.h;
		float xLeft = elemRect.x + camera.center_pos.x;

		// Draw divider before first element
		if (it == first && si.divider) {
			gpu.copyGPUImage(si.divider->oldNew(refresh_mode)->gpu_image, nullptr, &localClip, target, info->pos.x + camera.center_pos.x, info->pos.y + yTop - si.divider->pos.h);
		}

		// DEBUG: Draw debug highlight
		/*if (si.hoveredElement == elementIndex) {
            GPU_FlushBlitBuffer();
            GPU_Rect rect { info->pos.x+xLeft, info->pos.y+yTop, elemRect.w, elemRect.h };
            GPU_SetClipRect(target, rect);
            GPU_ClearRGBA(target, 255, 0, 0, 255);
            GPU_SetClipRect(target, localClip);
        }*/

		// Draw element background if any
		if (spriteBg) {
			GPU_Rect bg_rect{0, 0, spriteBg->pos.w, spriteBg->pos.h};
			if (spriteBg->num_of_cells > 1 && si.hoveredElement == elementIndex) {
				// May need to be expanded to allow for elements you can set into a state (e.g. "playing") and then move away from
				// e.g. selectedElement field (seems confuseable with hoveredElement lol)
				bg_rect.x += spriteBg->pos.w;
			}
			// Currently prints at 0,0 of element with no attempt to do proper padding
			gpu.copyGPUImage(spriteBg->gpu_image, &bg_rect, &localClip,
			                 target, info->pos.x + xLeft, info->pos.y + yTop,
			                 1, 1, 0, false);
		}

		// Draw the element itself
		if (elem.has("text") || elem.has("log")) {
			Fontinfo fi = sentence_font;
			fi.clear();
			int marginLeft = elem.has("textmarginwidth") ? std::stoi(elem["textmarginwidth"].value) :
			                                               elem.has("textmarginleft") ? std::stoi(elem["textmarginleft"].value) : si.textMarginLeft;
			int marginRight = elem.has("textmarginwidth") ? std::stoi(elem["textmarginwidth"].value) :
			                                                elem.has("textmarginright") ? std::stoi(elem["textmarginright"].value) : si.textMarginRight;
			int marginTop                = elem.has("textmargintop") ? std::stoi(elem["textmargintop"].value) : si.textMarginTop;
			fi.top_xy[0]                 = info->pos.x + xLeft + marginLeft;
			fi.top_xy[1]                 = info->pos.y + yTop + marginTop;
			fi.changeStyle().wrap_limit  = elemRect.w - (marginLeft + marginRight);
			fi.changeStyle().can_loghint = true;
			auto &buttonMultiplyColor    = elementIndex == si.hoveredElement ? si.hoverMultiplier : si.normalMultipler;
			auto &gradient               = elementIndex == si.hoveredElement ? si.hoverGradients : si.normalGradients;
			fi.buttonMultiplyColor       = buttonMultiplyColor;
			if (fi.style().is_gradient != gradient)
				fi.changeStyle().is_gradient = gradient;
			std::string &text = elem.has("log") ? script_h.logState.logEntryIndexToDialogueData(std::stoi(elem["log"].value)).text : elem["text"].value;
			char *txt         = const_cast<char *>(text.data());
			dlgCtrl.renderToTarget(target, &localClip, txt, &fi, false, si.tightlyFit);
		}

		// Draw divider after each element
		if (si.divider)
			gpu.copyGPUImage(si.divider->oldNew(refresh_mode)->gpu_image, nullptr, &localClip, target, info->pos.x + camera.center_pos.x, info->pos.y + yBot);
	}

	//sendToLog(LogLevel::Info, "scroll_y: %u\n", scroll_y);
	// all elements drawn
	// return
	GPU_UnsetClip(target);
}

// Probably belongs somewhere other than animation.cpp ... i think we will want a dedicated scrollable.cpp personally
// This method takes a specially scrollable AI and goes through its element tree,
// computing the y-position of the elements according to the value of their "height" key,
// and storing it in a newly computed "y" key which can be used by the draw function
void ONScripter::layoutSpecialScrollable(AnimationInfo *info) {
	if (!info->scrollableInfo.isSpecialScrollable)
		return;
	AnimationInfo::ScrollableInfo &si = info->scrollableInfo;
	StringTree &tree                  = dataTrees[si.elementTreeIndex];
	int dividerHeight                 = 0;
	int currentColumn                 = 0;
	if (si.divider)
		dividerHeight = si.divider->pos.h;
	int currentY = si.firstMargin + dividerHeight; // the top gets a divider too
	int currentX = 0;
	int height   = 0;

	assert(tree.insertionOrder.begin() + si.layoutedElements <= tree.insertionOrder.end());

	for (auto it = tree.insertionOrder.begin() + si.layoutedElements; it < tree.insertionOrder.end(); ++it, si.layoutedElements++) {
		auto &s = *it;

		StringTree &t = tree[s];
		if (t.has("log")) {
			bool read = script_h.logState.logEntryIndexToIsRead(std::stoi(t["log"].value));
			if (!read) {
				height = 0;
				break;
			}
		}

		height    = t.has("height") ? std::stoi(t["height"].value) : si.elementHeight;
		int width = t.has("width") ? std::stoi(t["width"].value) :
		                             si.elementWidth ? si.elementWidth :
		                                               info->pos.w;
		if (currentColumn > 0)
			t["x"].value = std::to_string(currentX);
		t["y"].value = std::to_string(currentY);
		if (si.columns > 1)
			t["col"].value = std::to_string(currentColumn);

		if (height == 0) {
			// autocalculate height
			int marginLeft = t.has("textmarginwidth") ? std::stoi(t["textmarginwidth"].value) :
			                                            t.has("textmarginleft") ? std::stoi(t["textmarginleft"].value) : si.textMarginLeft;
			int marginRight = t.has("textmarginwidth") ? std::stoi(t["textmarginwidth"].value) :
			                                             t.has("textmarginright") ? std::stoi(t["textmarginright"].value) : si.textMarginRight;
			calculateDynamicElementHeight(t, width - (marginLeft + marginRight), si.tightlyFit);
			height = std::stoi(t["height"].value);
		}

		// move on to next element position (if columns are not in use then currentColumn==0 always)
		if (it + 1 != tree.insertionOrder.end()) {
			currentColumn = (currentColumn + 1) % si.columns;
			if (currentColumn == 0) {
				currentY += height + dividerHeight;
				currentX = 0;
			} else {
				currentX += width + si.columnGap;
			}
		}
	}
	si.totalHeight = currentY + si.lastMargin + height;
	//tree.accept(StringTreePrinter());
}

void ONScripter::calculateDynamicElementHeight(StringTree &element, int width, int tightlyFit) {
	if (!element.has("text") && !element.has("log")) {
		element["height"].value = "0";
		return;
	}
	Fontinfo fi = sentence_font;
	fi.clear();
	fi.top_xy[0]                 = 0;
	fi.top_xy[1]                 = 0;
	fi.changeStyle().can_loghint = true;
	fi.changeStyle().wrap_limit  = width;
	std::string &text            = element.has("log") ? script_h.logState.logEntryIndexToDialogueData(std::stoi(element["log"].value)).text : element["text"].value;
	char *txt                    = const_cast<char *>(text.data());
	GPU_Rect bounds;
	dlgCtrl.renderToTarget(nullptr, &bounds, txt, &fi, false, tightlyFit); // dummy draw; get size
	element["height"].value = std::to_string(bounds.y + bounds.h);
}

void ONScripter::changeScrollableHoveredElement(AnimationInfo *info, Direction d) {
	AnimationInfo::ScrollableInfo &si       = info->scrollableInfo;
	StringTree &tree                        = dataTrees[si.elementTreeIndex];
	bool currentHoveredElemPartiallyVisible = false;
	long firstVisibleElemId = -1, lastVisibleElemId = -1;
	for (auto it = getScrollableElementsVisibleAt(&si, tree, info->scrollable.y + si.firstMargin); it != tree.insertionOrder.end(); ++it) {
		auto elemId = it - tree.insertionOrder.begin();
		if (firstVisibleElemId == -1)
			firstVisibleElemId = elemId;
		if (si.hoveredElement == elemId) {
			currentHoveredElemPartiallyVisible = true;
		}
		float w = si.elementWidth ? si.elementWidth : info->pos.w;
		float h = si.elementHeight;
		GPU_Rect elemRect{0, 0, w, h};
		setRectForScrollableElement(&tree[*it], elemRect);
		if (elemRect.y - info->scrollable.y >= info->pos.h - si.lastMargin) {
			// we're off the bottom of the visible area, break
			lastVisibleElemId = elemId - 1;
			break;
		}
	}

	// This fixes a bug in lookback when one hovers the last element and presses down key
	// Same thing done below
	long maxId = si.layoutedElements - 1; //tree.insertionOrder.size() - 1;
	if (lastVisibleElemId == -1)
		lastVisibleElemId = maxId;

	if (currentHoveredElemPartiallyVisible || (si.snapType != AnimationInfo::ScrollSnap::NONE &&
	                                           tree.getById(si.hoveredElement).has("y") && tree.getById(si.snappedElement).has("y") &&
	                                           std::stoi(tree.getById(si.hoveredElement)["y"].value) == std::stoi(tree.getById(si.snappedElement)["y"].value))) {

		switch (d) {
			case Direction::LEFT:
				if (si.hoveredElement - 1 >= 0)
					si.hoveredElement--;
				break;
			case Direction::RIGHT:
				if (si.hoveredElement + 1 <= maxId)
					si.hoveredElement++;
				break;
			case Direction::UP:
				if (si.hoveredElement - si.columns >= 0)
					si.hoveredElement -= si.columns;
				break;
			case Direction::DOWN:
				if (si.hoveredElement + si.columns <= maxId)
					si.hoveredElement += si.columns;
				break;
		}

		float w = si.elementWidth ? si.elementWidth : info->pos.w;
		float h = si.elementHeight;
		GPU_Rect elemRect{0, 0, w, h};
		int dividerH = si.divider ? si.divider->orig_pos.h : 0;
		setRectForScrollableElement(&tree.getById(si.hoveredElement), elemRect);

		if (elemRect.y + dividerH < info->scrollable.y + si.firstMargin) {
			snapScrollableToElement(info, si.hoveredElement, AnimationInfo::ScrollSnap::TOP);
			//sendToLog(LogLevel::Info, "Snapping up to %d\n", si.hoveredElement);
		}
		if (elemRect.y + elemRect.h + dividerH > info->scrollable.y + info->pos.h - si.lastMargin) {
			snapScrollableToElement(info, si.hoveredElement, AnimationInfo::ScrollSnap::BOTTOM);
			//sendToLog(LogLevel::Info, "Snapping down to %d\n", si.hoveredElement);
		} //FIXME: the use of info->scrollable.y here will probably create issues since it's being animated

	} else {
		//CHECKME: do these snaps need a condition?
		if (d == Direction::UP || d == Direction::LEFT) {
			si.hoveredElement = lastVisibleElemId;
			snapScrollableToElement(info, si.hoveredElement, AnimationInfo::ScrollSnap::BOTTOM);
		} else {
			si.hoveredElement = firstVisibleElemId;
			snapScrollableToElement(info, si.hoveredElement, AnimationInfo::ScrollSnap::TOP);
		}
	}
	fillCanvas(true, true); // temp
	flush(refreshMode());
}

void ONScripter::snapScrollableByOffset(AnimationInfo *info, int rowsDownwards) {
	if (!rowsDownwards)
		sendToLog(LogLevel::Error, "!? Asked to scroll a scrollable by 0 elements\n");
	AnimationInfo::ScrollableInfo &si = info->scrollableInfo;
	StringTree &tree                  = dataTrees[si.elementTreeIndex];
	long maxId                        = si.layoutedElements - 1; //tree.insertionOrder.size() - 1;
	bool alreadySnappedCorrectlyDown  = (rowsDownwards > 0 && si.snapType == AnimationInfo::ScrollSnap::BOTTOM);
	bool alreadySnappedCorrectlyUp    = (rowsDownwards < 0 && si.snapType == AnimationInfo::ScrollSnap::TOP);
	if (!alreadySnappedCorrectlyDown && !alreadySnappedCorrectlyUp) {
		// we need to find an appropriate element to begin the snap with
		long firstVisibleElemId = -1, lastVisibleElemId = -1;
		for (auto it = getScrollableElementsVisibleAt(&si, tree, info->scrollable.y + si.firstMargin); it != tree.insertionOrder.end(); ++it) {
			long elemId = it - tree.insertionOrder.begin();
			if (firstVisibleElemId == -1)
				firstVisibleElemId = elemId;
			float w = si.elementWidth ? si.elementWidth : info->pos.w;
			float h = si.elementHeight;
			GPU_Rect elemRect{0, 0, w, h};
			setRectForScrollableElement(&tree[*it], elemRect);
			if (elemRect.y - info->scrollable.y >= info->pos.h - si.lastMargin) {
				// we're off the bottom of the visible area, break
				lastVisibleElemId = elemId - 1;
				break;
			}
		}
		if (rowsDownwards < 0) {
			si.snappedElement = firstVisibleElemId;
			si.snapType       = AnimationInfo::ScrollSnap::TOP;
		} else {
			si.snappedElement = lastVisibleElemId;
			si.snapType       = AnimationInfo::ScrollSnap::BOTTOM;
		}
	}
	// ok, snap is set properly, now let's process this offset
	si.snappedElement += si.columns * rowsDownwards;
	if (si.snappedElement < 0)
		si.snappedElement = 0;
	if (si.snappedElement > maxId)
		si.snappedElement = maxId;
	snapScrollableToElement(info, si.snappedElement, si.snapType);
}

void ONScripter::snapScrollableToElement(AnimationInfo *info, long elementId, AnimationInfo::ScrollSnap snapType, bool instant) {
	if (snapType == AnimationInfo::ScrollSnap::NONE)
		sendToLog(LogLevel::Error, "!? Snap but don't snap? Think before you speak\n");
	if (!info->scrollableInfo.isSpecialScrollable)
		sendToLog(LogLevel::Error, "!? That isn't even a scrollable\n");
	AnimationInfo::ScrollableInfo &si = info->scrollableInfo;
	StringTree &tree                  = dataTrees[si.elementTreeIndex];
	si.snappedElement                 = elementId;
	si.snapType                       = snapType;
	float w                           = si.elementWidth ? si.elementWidth : info->pos.w;
	float h                           = si.elementHeight;
	GPU_Rect elemRect{0, 0, w, h};
	setRectForScrollableElement(&tree.getById(elementId), elemRect);
	int dividerH  = si.divider ? si.divider->orig_pos.h : 0;
	float dstYTop = snapType == AnimationInfo::ScrollSnap::TOP ? elemRect.y - dividerH - si.firstMargin : elemRect.y + elemRect.h - info->pos.h + si.lastMargin + dividerH;
	bool lsp2;
	int num = getAIno(info, false, lsp2);
	if (!instant)
		dynamicProperties.addSpriteProperty(info, num, lsp2, true, SPRITE_PROPERTY_SCROLLABLE_Y, dstYTop, 100, 1, true);
	else
		dynamicProperties.addSpriteProperty(info, num, lsp2, true, SPRITE_PROPERTY_SCROLLABLE_Y, dstYTop);
	//sendToLog(LogLevel::Info, "Setting dynamic property with destination %f (current pos %f)\n", dstYTop, info->scrollable.y);
}

// returns iterator to si->insertionOrder
std::vector<std::string>::iterator ONScripter::getScrollableElementsVisibleAt(AnimationInfo::ScrollableInfo *si, StringTree &tree, int y) {
	std::string dummy;
	// finds first entry in tree.insertionOrder that has "entry.y + entry.h >= y"
	// dummy value represents y (search value for lower_bound)
	auto res = std::lower_bound(tree.insertionOrder.begin(), tree.insertionOrder.end(), std::string{}, [&tree, &si, &y](const std::string &s1, const std::string &s2) {
		int y_b1, y_b2;
		if (s1.empty()) {
			y_b1 = y;
		} else {
			StringTree &t = tree[s1];
			if (!t.has("y"))
				return false;
			y_b1 = std::stoi(t["y"].value) + (t.has("height") ? std::stoi(t["height"].value) : si->elementHeight);
		}
		if (s2.empty()) {
			y_b2 = y;
		} else {
			StringTree &t2 = tree[s2];
			if (!t2.has("y"))
				return false;
			y_b2 = std::stoi(t2["y"].value) + (t2.has("height") ? std::stoi(t2["height"].value) : si->elementHeight);
		}
		return y_b1 < y_b2;
	});
	return res;
}

void ONScripter::setRectForScrollableElement(StringTree *elem, GPU_Rect &rect) {
	if (elem->has("x"))
		rect.x = std::stoi((*elem)["x"].value);
	if (elem->has("y"))
		rect.y = std::stoi((*elem)["y"].value);
	if (elem->has("width"))
		rect.w = std::stoi((*elem)["width"].value);
	if (elem->has("height"))
		rect.h = std::stoi((*elem)["height"].value);
}

// Script coords with scroll already taken into account, so y can be 0 to infinity.
void ONScripter::mouseOverSpecialScrollable(int aiSpriteNo, int x, int y) {
	//sendToLog(LogLevel::Info, "moused over with coords %d, %d\n", x, y);
	AnimationInfo *ai                 = &sprite_info[aiSpriteNo];
	AnimationInfo::ScrollableInfo *si = &ai->scrollableInfo;
	StringTree &tree                  = dataTrees[si->elementTreeIndex];
	for (auto it = getScrollableElementsVisibleAt(si, tree, y); it != tree.insertionOrder.end(); ++it) {
		StringTree &elem = tree[*it];
		float w          = si->elementWidth;
		float h          = si->elementHeight;
		GPU_Rect elemRect{0, 0, w, h};
		setRectForScrollableElement(&elem, elemRect);
		if (elemRect.y > y) {
			// Went too far down, found nothing
			si->mouseCursorIsOverHoveredElement = false;
			return;
		}
		if (x >= elemRect.x && x < elemRect.x + elemRect.w &&
		    y >= elemRect.y && y < elemRect.y + elemRect.h) {
			si->mouseCursorIsOverHoveredElement = true;
			// int newHover = std::stoi(*it);
			// stoi causes a crash with string keys. Using indices might be wrong but I cannot think of a particular issue atm.
			long newHover = it - tree.insertionOrder.begin();
			if (newHover != si->hoveredElement) {
				si->hoveredElement = newHover;
				dirtySpriteRect(aiSpriteNo, false);
				flush(refreshMode());
			}
			return;
		}
	}
	// Deliberately no code here for setting hoveredElement to -1 or something if we are over blank space.
	// This covers the case when we mouseover something, then nothing, and then try to use the gamepad.
	// We have to remember where we were somehow or gamepad can't continue.
	// So instead we do this:
	si->mouseCursorIsOverHoveredElement = false;
}

void ONScripter::drawBigImage(GPU_Target *target, AnimationInfo *info, int /*refresh_mode*/, GPU_Rect *clip, bool centre_coordinates) {
	GPU_Rect targetClip = clip ? *clip : GPU_Rect{-camera.center_pos.x, -camera.center_pos.y, static_cast<float>(window.canvas_width), static_cast<float>(window.canvas_height)};

	float scale_x     = info->scale_x / 100.0;
	float scale_y     = info->scale_y / 100.0;
	float bound_off_x = 0;
	float bound_off_y = 0;

	int cell_off_x = info->vertical_cells ? 0 : info->pos.w * info->current_cell;
	int cell_off_y = info->vertical_cells ? info->pos.h * info->current_cell : 0;

	GPU_Rect bounding_rect = info->bounding_rect;
	if (info->scrollable.h > 0) {
		cell_off_y += info->scrollable.y;
		bounding_rect.h = info->scrollable.h;
	}
	if (info->scrollable.w > 0) {
		cell_off_x += info->scrollable.x;
		bounding_rect.w = info->scrollable.w;
	}

	GPU_Image *sprite_transformation_image{nullptr};
	GPU_Rect sourceClip = info->pos;

	if (scale_x == 1 && scale_y == 1) {
		// sourceClip has script coordinates
		sourceClip.x = bounding_rect.x; // remove lsp2 specfics
		sourceClip.y = bounding_rect.y; // remove lsp2 specfics
		if (doClipping(&sourceClip, &targetClip))
			return;
		sourceClip.x -= bounding_rect.x; // switch to image coordinates
		sourceClip.y -= bounding_rect.y; // switch to image coordinates
		// Change the cell
		sourceClip.x += cell_off_x;
		sourceClip.y += cell_off_y;
	} else if (scale_x >= 1 && scale_y >= 1) {
		sprite_transformation_image = gpu.getCanvasImage();

		// we have script coordinates in bounding_rect, containing the area a scaled image covers
		// we have a relatively small temp image (smaller than BigImage) we need to fit our unscaled area in

		// Forget about clips for now
		sourceClip.x = cell_off_x;
		sourceClip.y = cell_off_y;

		GPU_Rect tmp = bounding_rect;

		// We are working with canvas then let's use canvas coordinates for lower calculations
		tmp.x += camera.center_pos.x;
		tmp.y += camera.center_pos.y;

		// Calculate visible offsets...

		// Firstly the negative area (top-left part)
		if (tmp.x < 0) {
			sourceClip.x = -tmp.x / scale_x;
			// Shift the width
			sourceClip.w -= sourceClip.x;
			tmp.w += tmp.x; // 0 -> offscreen
		} else {
			// If the image top-left edge is visible on the opposite
			bound_off_x = tmp.x / scale_x;
		}
		if (tmp.y < 0) {
			sourceClip.y = -tmp.y / scale_y;
			// Shift the neight
			sourceClip.h -= sourceClip.y;
			tmp.h += tmp.y; // 0 -> offscreen
		} else {
			// If the image top-left edge is visible on the opposite
			bound_off_y = tmp.y / scale_y;
		}

		// Secondly the positive area (bottom-right part)
		if (tmp.w > window.canvas_width)
			sourceClip.w -= (tmp.w - window.canvas_width) / scale_x;
		if (tmp.h > window.canvas_height)
			sourceClip.h -= (tmp.h - window.canvas_height) / scale_y;

		// Fix possible out of scope
		if (sourceClip.w < 0)
			sourceClip.w = 0;
		if (sourceClip.h < 0)
			sourceClip.h = 0;

		// At this point we know the area of the image we need to display

		// Now we have a max possible area we can display on the canvas image starting from the top-left of a sprite_transformation_image
	} else {
		ons.errorAndExit("Big images cannot be zoomed out!");
		return; //dummy
	}

	if (doClipping(&targetClip, &bounding_rect)) {
		if (sprite_transformation_image)
			gpu.giveCanvasImage(sprite_transformation_image);
		return;
	}

	// Switch to canvas (dst) coords
	targetClip.x += camera.center_pos.x;
	targetClip.y += camera.center_pos.y;

	gpu.pushBlendMode(info->blending_mode);

	bool allowDirectCopy{false};

	if (info->trans_mode == AnimationInfo::TRANS_COPY && // the sprite has no alpha
	    gpu.blend_mode.top() == BlendModeId::NORMAL &&   // no weird blending modes
	    info->trans >= 255)                              // must not be transparent at all
		allowDirectCopy = true;

	auto chunks = info->big_image->getImagesForArea(sourceClip);
	for (auto &chunk : chunks) {
		float x = chunk.second.x + chunk.first->w / 2.0;
		float y = chunk.second.y + chunk.first->h / 2.0;

		if (sprite_transformation_image) {
			GPU_SetBlending(chunk.first, false);

			gpu.copyGPUImage(chunk.first, nullptr, nullptr, sprite_transformation_image->target,
			                 x - sourceClip.x + bound_off_x,
			                 y - sourceClip.y + bound_off_y,
			                 1, 1, 0, centre_coordinates);

			GPU_SetBlending(chunk.first, true);
		} else {
			if (allowDirectCopy)
				GPU_SetBlending(chunk.first, false);
			if (info->trans <= 255)
				GPU_SetRGBA(chunk.first, info->trans, info->trans, info->trans, info->trans);
			gpu.copyGPUImage(chunk.first, nullptr, &targetClip, target,
			                 x + camera.center_pos.x + bounding_rect.x - cell_off_x,
			                 y + camera.center_pos.y + bounding_rect.y - cell_off_y,
			                 1, 1, 0, centre_coordinates);
			if (allowDirectCopy)
				GPU_SetBlending(chunk.first, true);
			if (info->trans <= 255)
				GPU_SetRGBA(chunk.first, 255, 255, 255, 255);
		}
	}

	if (sprite_transformation_image) {
		if (info->trans <= 255)
			GPU_SetRGBA(sprite_transformation_image, info->trans, info->trans, info->trans, info->trans);
		gpu.copyGPUImage(sprite_transformation_image, nullptr, &targetClip, target,
		                 sprite_transformation_image->w / 2.0 * scale_x, sprite_transformation_image->h / 2.0 * scale_y,
		                 scale_x, scale_y, 0, centre_coordinates);
		if (info->trans <= 255)
			GPU_SetRGBA(sprite_transformation_image, 255, 255, 255, 255);
	}

	gpu.popBlendMode();

	if (sprite_transformation_image)
		gpu.giveCanvasImage(sprite_transformation_image);
}

void ONScripter::drawToGPUTarget(GPU_Target *target, AnimationInfo *info, int refresh_mode, GPU_Rect *clip, bool centre_coordinates) {
	if (!target) {
		sendToLog(LogLevel::Error, "drawToGPUTarget has no proper target\n");
		return;
	}
	if (target->w != window.canvas_width || target->h != window.canvas_height) {
		sendToLog(LogLevel::Error, "drawToGPUTarget requires a canvas-sized target!\n");
	}

	// Don't draw sprites that have a parent independently (they are drawn as part of the drawToGPUTarget(tgt, parent, ...))
	if (info->parentImage.no != -1) {
		return;
	}

	GPU_Rect real_clip{0, 0, static_cast<float>(target->w), static_cast<float>(target->h)};
	if (clip) {
		real_clip = *clip;
		real_clip.x += camera.center_pos.x;
		real_clip.y += camera.center_pos.y;
	}

	GPU_Image *sprite_transformation_image = nullptr;
	GPU_Image *subimage_compositing_image  = nullptr;
	GPU_Image *src                         = nullptr;

	bool opacityTransform = info->darkenHue.r < 255 || info->darkenHue.g < 255 ||
	                        info->darkenHue.b < 255 || info->trans < 255;

	float coord_x = info->rot == 0 && !info->has_hotspot ? info->pos.x : info->rendering_center.x;
	float coord_y = info->rot == 0 && !info->has_hotspot ? info->pos.y : info->rendering_center.y;

	// Adjust by sprite-specific camera
	coord_x += info->camera.pos.x;
	coord_y += info->camera.pos.y;

	/* A paint at 0,0 paints to
	 ----------------------
	 |  here              |
	 |  *--------------   |
	 |  |  script_w/h |   |
	 |  ---------------   |
	 |        canvas_w/h  |
	 ----------------------
		instead of to the top left corner of the canvas.
		(The alternative is to switch everything to using centered coordinates or change all the positions in our script...) */
	coord_x += camera.center_pos.x;
	coord_y += camera.center_pos.y;

	bool needTransformationImage{true};
	BreakupID breakupID{{BreakupType::NONE, 0}};
	if (info->spriteTransforms.hasNoneExceptMaybeBreakup()) {
		// We might be able to get away without a transformation image, but let's consider breakup carefully first.
		if (info->spriteTransforms.breakupFactor == 0) {
			// No breakup either! Great! No transformation image required.
			needTransformationImage = false;
		} else {
			// OK, we have breakup... we might need a secondary image still.
			breakupID.id = info->id;
			if (!ons.new_breakup_implementation) {
				// It's old breakup. There's nothing we can do to optimize that.
				breakupID.type = BreakupType::SPRITE_CANVAS;
			} else {
				// New breakup. We only need a transformation image if it violates one of our conditions.
				bool breakupTightfits = info->rot == 0 && info->scale_x == 100 && info->scale_y == 100 && info->flip == 0 && info->layer_no == -1;
				if (!breakupTightfits) {
					// Support needs to be added into breakUpImage for us to deal with this kind of complication.
					// For now, this has to break up as BreakupType::SPRITE_CANVAS. :(
					breakupID.type = BreakupType::SPRITE_CANVAS;
				} else if (opacityTransform) {
					// It does have an opacity transform, but we can still kind of do something optimized with this.
					// We can use BreakupType::SPRITE_TIGHTFIT at least, though we still need to use a secondary texture.
					breakupID.type = BreakupType::SPRITE_TIGHTFIT;
				} else {
					// It's just a regular, very optimized new breakup. We can TIGHTFIT and we don't need anything special :)
					breakupID.type          = BreakupType::SPRITE_TIGHTFIT;
					needTransformationImage = false;
				}
			}
		}
	}

	if (needTransformationImage) {
		sprite_transformation_image = gpu.getCanvasImage();
	}

	// going to support sprite_transformation_image on special scrollables i think, should be no extra effort
	if (info->scrollableInfo.isSpecialScrollable) {
		if (sprite_transformation_image)
			errorAndExit("Cannot transform a SpecialScrollable");
		drawSpecialScrollable(target, info, refresh_mode, &real_clip);
		return;
	}
	if (info->is_big_image && info->big_image.get()) {
		if (sprite_transformation_image)
			errorAndExit("Cannot transform a BigImage");
		drawBigImage(target, info, refresh_mode, clip, centre_coordinates);
		return;
	}
	if (info->layer_no >= 0 && info->trans_mode == AnimationInfo::TRANS_LAYER) {
		auto handler      = getLayer<Layer>(info->layer_no, false);
		auto layer_target = target;
		auto mode         = handler->blendingMode(refresh_mode);
		if (sprite_transformation_image)
			layer_target = sprite_transformation_image->target;
		// TODO: Layers need some kind of support for flip at least. They may need to use a sprite_transformation_image if they have one of those properties set.
		if (mode != BlendModeId::NORMAL)
			gpu.pushBlendMode(mode);
		handler->refresh(layer_target, real_clip, coord_x, coord_y, centre_coordinates, refresh_mode,
		                 (info->flip & FLIP_HORIZONTALLY ? -1 : 1) * (info->scale_x ? info->scale_x / 100.0 : 1),
		                 (info->flip & FLIP_VERTICALLY ? -1 : 1) * (info->scale_y ? info->scale_y / 100.0 : 1));
		if (mode != BlendModeId::NORMAL)
			gpu.popBlendMode();
		if (!sprite_transformation_image)
			return;
	} else {
		if (!info->gpu_image)
			return;

		src = info->gpu_image;

		GPU_Rect clip_rect{0, 0, info->pos.w, info->pos.h};
		if (info->num_of_cells > 1 && info->current_cell != 0) {
			if (!info->vertical_cells)
				clip_rect.x += info->pos.w * info->current_cell;
			else
				clip_rect.y += info->pos.h * info->current_cell;
		}
		if (info->scrollable.h > 0) {
			clip_rect.h = clip_rect.h > info->scrollable.h ? info->scrollable.h : clip_rect.h;
			clip_rect.y = info->scrollable.y;
		}
		if (info->scrollable.w > 0) {
			clip_rect.w = clip_rect.w > info->scrollable.w ? info->scrollable.w : clip_rect.w;
			clip_rect.x = info->scrollable.x;
		}

		if (!info->childImages.empty()) {
			bool foundNonNullChild{false};
			// Copy all the child images onto it in order
			for (auto &zLevel : info->childImages) {
				int sprite_no        = zLevel.second.no;
				bool lsp2            = zLevel.second.lsp2;
				AnimationInfo *child = (lsp2 ? sprite2_info[sprite_no] : sprite_info[sprite_no]).oldNew(refresh_mode);
				if (!child->gpu_image)
					continue;
				if (!foundNonNullChild) {
					// First copy the parent image
					foundNonNullChild = true;
					// Limitation: The parent image cannot be larger than the canvas
					subimage_compositing_image = gpu.getCanvasImage();
					GPU_SetBlending(src, false);
					gpu.copyGPUImage(src, &clip_rect, nullptr, subimage_compositing_image->target, 0, 0, 1, 1, 0, false);
					GPU_SetBlending(src, true);
				}
				clip_rect = {0, 0, child->pos.w, child->pos.h};
				// Respect child image cells if they have them
				if (child->num_of_cells > 1 && child->current_cell != 0) {
					if (!child->vertical_cells)
						clip_rect.x += child->pos.w * child->current_cell;
					else
						clip_rect.y += child->pos.h * child->current_cell;
				}

				gpu.pushBlendMode(BlendModeId::NORMAL);
				gpu.copyGPUImage(child->gpu_image, &clip_rect, nullptr, subimage_compositing_image->target, child->pos.x, child->pos.y, 1, 1, 0, false);
				gpu.popBlendMode();
			}
			if (foundNonNullChild) {
				src       = subimage_compositing_image;
				clip_rect = {0, 0, info->pos.w, info->pos.h};
			}
		}

		if (!sprite_transformation_image) {
			gpu.pushBlendMode(info->blending_mode);

			// Blend with transparency, but only if we do not have further blending to do.
			if (opacityTransform) {
				GPU_SetRGBA(src, info->trans * info->darkenHue.r / 255, info->trans * info->darkenHue.g / 255,
				            info->trans * info->darkenHue.b / 255, info->trans);
			}
		}

		GPU_Target *dst    = sprite_transformation_image ? sprite_transformation_image->target : target;
		GPU_Rect *dst_clip = sprite_transformation_image ? nullptr : &real_clip;
		float scale_x      = (info->flip & FLIP_HORIZONTALLY ? -1 : 1) * (info->scale_x ? info->scale_x / 100.0 : 1);
		float scale_y      = (info->flip & FLIP_VERTICALLY ? -1 : 1) * (info->scale_y ? info->scale_y / 100.0 : 1);

		if (breakupID.type == BreakupType::SPRITE_TIGHTFIT) {
			//TODO:
			// * remove - half w/h, breakUpImage should work with centred coordinates
			// * add scale factor with flip support
			// * add clips
			gpu.breakUpImage(breakupID, src, &clip_rect, dst, info->spriteTransforms.breakupFactor,
			                 info->spriteTransforms.breakupDirectionFlagset, nullptr, coord_x - info->pos.w / 2,
			                 coord_y - info->pos.h / 2);
		} else {
			bool allowDirectCopy{false};

			if (sprite_transformation_image || (                                                      // We're blitting onto a completely transparent surface, so there should be no need to blend.
			                                       (info->trans_mode == AnimationInfo::TRANS_COPY) && // the sprite has no alpha
			                                       (std::remainder(info->rot, 90) == 0) &&            // rectangular form (angles that do not line up with pixel boundaries probably require alpha blending, right?)
			                                       (gpu.blend_mode.top() == BlendModeId::NORMAL) &&   // no weird blending modes
			                                       (!info->scale_x && !info->scale_y) &&              // scaling could result in missing pixel boundaries again
			                                       (info->trans >= 255)                               // must not be transparent at all
			                                       )) {
				allowDirectCopy = true;
			};

			if (allowDirectCopy)
				GPU_SetBlending(src, false);
			gpu.copyGPUImage(src, &clip_rect, dst_clip, dst, coord_x, coord_y, scale_x, scale_y,
			                 //ONScripter uses right-to-left angling system and sdl-gpu prefers left-to-right. I prefer sdl-gpu, but we are to follow the standards.
			                 -info->rot, centre_coordinates);
			if (allowDirectCopy)
				GPU_SetBlending(src, true);
		}

		if (subimage_compositing_image)
			gpu.giveCanvasImage(subimage_compositing_image);
	}

	if (sprite_transformation_image) {
		src = sprite_transformation_image;
		PooledGPUImage toDraw;

		if (info->spriteTransforms.blurFactor > 0) {
			GPUTransformableCanvasImage tmp(src);
			toDraw = gpu.getBlurredImage(tmp, info->spriteTransforms.blurFactor);
			src    = toDraw.image;
			// We (have) set a larger clip for this in dirtySpriteRect to ensure we are called with a large enough clip
		}
		if (info->spriteTransforms.negative1) {
			GPUTransformableCanvasImage tmp(src);
			toDraw = gpu.getNegativeImage(tmp);
			src    = toDraw.image;
		}
		if (info->spriteTransforms.sepia) {
			GPUTransformableCanvasImage tmp(src);
			toDraw = gpu.getSepiaImage(tmp);
			src    = toDraw.image;
		}
		if (info->spriteTransforms.greyscale) {
			GPUTransformableCanvasImage tmp(src);
			toDraw = gpu.getGreyscaleImage(tmp, info->darkenHue);
			src    = toDraw.image;
		}
		if (info->spriteTransforms.negative2) {
			GPUTransformableCanvasImage tmp(src);
			toDraw = gpu.getNegativeImage(tmp);
			src    = toDraw.image;
		}
		if (breakupID.type == BreakupType::SPRITE_CANVAS) {
			GPUTransformableCanvasImage tmp(src);
			toDraw = gpu.getBrokenUpImage(tmp, breakupID, info->spriteTransforms.breakupFactor,
			                              info->spriteTransforms.breakupDirectionFlagset, nullptr);
			src    = toDraw.image;
			// We (have) set a larger clip for this in dirtySpriteRect to ensure we are called with a large enough clip
		}
		if (info->spriteTransforms.warpAmplitude != 0) {
			GPUTransformableCanvasImage tmp(src);
			float secs = info->spriteTransforms.warpClock.time() / 1000.0;
			toDraw     = gpu.getWarpedImage(tmp, secs, info->spriteTransforms.warpAmplitude, info->spriteTransforms.warpWaveLength,
                                        info->spriteTransforms.warpSpeed);
			src        = toDraw.image;
			// We (have) set a larger clip for this in dirtySpriteRect to ensure we are called with a large enough clip
		}

		if (opacityTransform)
			GPU_SetRGBA(src, info->trans * info->darkenHue.r / 255,
			            info->trans * info->darkenHue.g / 255,
			            info->trans * info->darkenHue.b / 255, info->trans);

		if (info->layer_no >= 0 && info->trans_mode == AnimationInfo::TRANS_LAYER)
			gpu.pushBlendMode(getLayer<Layer>(info->layer_no, false)->blendingMode(refresh_mode));
		else
			gpu.pushBlendMode(info->blending_mode);

		gpu.copyGPUImage(src, nullptr, &real_clip, target, 0, 0, 1, 1, 0, false);
		gpu.giveCanvasImage(sprite_transformation_image);
	}

	gpu.popBlendMode();

	if (opacityTransform)
		GPU_SetRGBA(src, 255, 255, 255, 255);
}

void ONScripter::commitVisualState() {
	//sendToLog(LogLevel::Info, "commitVisualState\n");
	for (auto i : queueAnimationInfo) {
		//So that we don't break the new AIs by continuing to run wrong timed property changes
		if (i->old_ai && i->distinguish_from_old_ai) {
			//sendToLog(LogLevel::Info, "Terminating sprite properties\n");
			dynamicProperties.terminateSpriteProperties(i);
		}
		i->commitState();
		deinitBreakup({{BreakupType::SPRITE_TIGHTFIT, static_cast<int16_t>(i->id)}});
		deinitBreakup({{BreakupType::SPRITE_CANVAS, static_cast<int16_t>(i->id)}});
	}

	for (auto &i : spritesets) {
		commitSpriteset(&i.second);
	}

	if (layer_info)
		layer_info->commit();

	queueAnimationInfo.clear();

	monocro_flag[BeforeScene]  = monocro_flag[AfterScene];
	monocro_color[BeforeScene] = monocro_color[AfterScene];
	nega_mode[BeforeScene]     = nega_mode[AfterScene];
	blur_mode[BeforeScene]     = blur_mode[AfterScene];

	// We committed, so any changes made to the after scene must now be applied to the normal (before) scene
	before_dirty_rect_hud.add(dirty_rect_hud.bounding_box_script);
	before_dirty_rect_scene.add(dirty_rect_scene.bounding_box_script);
	dirty_rect_hud.clear();
	dirty_rect_scene.clear();
}

void ONScripter::backupState(AnimationInfo *info) {
	// Do not back up sprites with transitions disabled.
	// This enables HUD elements etc to move independently on the scene using properties without caring about what is happening ingame.
	if (nontransitioningSprites.count(info)) {
		return;
	}

	if (!info->old_ai) {
		info->backupState();
		queueAnimationInfo.push_back(info);
	}
}

void ONScripter::commitSpriteset(SpritesetInfo *si) {
	dynamicProperties.terminateSpritesetProperties(si);
	cleanSpritesetCache(si, true);
	cleanSpritesetCache(si, false);
	si->commit();
}

void ONScripter::stopCursorAnimation(int click) {
	if (enable_custom_cursors)
		return;

	int no;

	if (textgosub_label || !draw_cursor_flag)
		return;

	if (click == CLICK_WAIT)
		no = CURSOR_WAIT_NO;
	else if (click == CLICK_NEWPAGE)
		no = CURSOR_NEWPAGE_NO;
	else
		return;

	if (cursor_info[no].gpu_image == nullptr)
		return;

	GPU_Rect dst_rect = cursor_info[no].pos;

	if (!cursor_info[no].abs_flag) {
		dst_rect.x += sentence_font.x();
		dst_rect.y += sentence_font.y();
	}

	GPU_Rect empty_rect{0, 0, 0, 0};
	flushDirect(empty_rect, dst_rect, refreshMode());
}

void ONScripter::buildGPUImage(AnimationInfo &ai) {
	if (!ai.image_surface) {
		//sendToLog(LogLevel::Error, "No image_surface in buildGPUImage\n");
		return;
	}

	ai.big_image.reset();
	if (ai.gpu_image) {
		gpu.freeImage(ai.gpu_image);
		ai.gpu_image = nullptr;
	}

	if (ai.image_surface->w == 0 || ai.image_surface->h == 0) {
		SDL_FreeSurface(ai.image_surface);
		ai.image_surface = nullptr;
		return;
	}

	//sendToLog(LogLevel::Info, "Creating gpu image in buildGPUImage (%d,%d)@%d\n",ai.image_surface->w, ai.image_surface->h, ai.image_surface->format->BytesPerPixel);

	if (ai.is_big_image) {
		ai.big_image = std::make_shared<GPUBigImage>(ai.image_surface);
	} else {
#if !defined(IOS) && !defined(DROID) // There is some issue with loadGPUImageByChunks on iOS
		if (!(skip_mode & SKIP_SUPERSKIP))
			ai.gpu_image = gpu.loadGPUImageByChunks(ai.image_surface);
		else
#endif
			ai.gpu_image = gpu.copyImageFromSurface(ai.image_surface);

		GPU_GetTarget(ai.gpu_image);
		gpu.multiplyAlpha(ai.gpu_image);
	}
}

void ONScripter::freeRedundantSurfaces(AnimationInfo &ai) {
	// Our test for whether a surface is "redundant" is initially cautious for safety.
	// The main concern is buttons, which require the surface for proper click handling.
	// An LSP can be declared as a button a long time after it is initially created, so without a model rework,
	// we must be careful not to dispose any surface that has any chance of later becoming a button.

	if (!ai.image_surface) {
		// Can't free if it doesn't exist.
		return;
	}

	if (ai.image_surface->w < window.script_width || ai.image_surface->h < window.script_height) {
		// It's not a huge image and might later become a button.
		return;
	}
	if (ai.num_of_cells > 1) {
		// Suspicious buttony behavior!
		return;
	}

	// OK, surely now it's safe to free this thing.
	SDL_FreeSurface(ai.image_surface);
	ai.image_surface = nullptr;
}
