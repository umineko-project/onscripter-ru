/**
 *  DynamicProperty.cpp
 *  ONScripter-RU
 *
 *  Dynamic transition component support (e.g. animations).
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Components/DynamicProperty.hpp"
#include "Engine/Components/Window.hpp"
#include "Engine/Core/ONScripter.hpp"

DynamicPropertyController dynamicProperties;

/* ---------------- Dynamic properties for effects ---------------- */

void DynamicPropertyController::reset() {
	customProperties.clear();
	spriteProperties.clear();
	globalProperties.clear();
	spritesetProperties.clear();

	registeredProperties.clear();
	registeredPropertiesMap.clear();
}

void DynamicPropertyController::advanceNanos(uint64_t ns) {
	for (auto &map : customProperties) {
		if (map.second.empty())
			continue;
		DynamicCustomProperty &cp = map.second.front(); // Only advance the item at the front of the queue
		if (cp.clock.time() == 0)
			cp.begin();
		if (cp.duration != 0)
			cp.clock.tickNanos(ns);
	}
	for (auto &map : spriteProperties) {
		if (map.second.empty())
			continue;
		DynamicSpriteProperty &sp = map.second.front(); // Only advance the item at the front of the queue
		if (sp.clock.time() == 0)
			sp.begin();
		if (sp.duration != 0)
			sp.clock.tickNanos(ns);
	}
	for (auto &map : globalProperties) {
		if (map.second.empty())
			continue;
		DynamicGlobalProperty &gp = map.second.front(); // Only advance the item at the front of the queue
		if (gp.clock.time() == 0)
			gp.begin();
		if (gp.duration != 0)
			gp.clock.tickNanos(ns);
	}
	for (auto &map : spritesetProperties) {
		if (map.second.empty())
			continue;
		DynamicSpritesetProperty &ssp = map.second.front(); // Only advance the item at the front of the queue
		if (ssp.clock.time() == 0)
			ssp.begin();
		if (ssp.duration != 0)
			ssp.clock.tickNanos(ns);
	}
}

void DynamicPropertyController::advance(int ms) {
	uint64_t ns = ms;
	ns *= 1000000;
	advanceNanos(ns);
}

void DynamicPropertyController::apply() {
	bool applied_something = false;
	for (auto &map : customProperties) {
		while (!map.second.empty()) {
			DynamicCustomProperty &cp = map.second.front(); // Only apply the item at the front of the queue
			cp.apply();
			applied_something = true;
			if (!cp.endless && cp.clock.time() >= cp.duration) {
				map.second.pop_front(); // pop it and apply the next one (properly initializes its start value)
			} else
				break; // we applied once and couldn't finish and pop, so we're done with this property until the next advance
		}
	}
	for (auto &map : spriteProperties) {
		while (!map.second.empty()) {
			DynamicSpriteProperty &sp = map.second.front(); // Only apply the item at the front of the queue
			sp.apply();
			applied_something = true;
			if (!sp.endless && sp.clock.time() >= sp.duration) {
				map.second.pop_front(); // pop it and apply the next one (properly initializes its start value)
			} else
				break; // we applied once and couldn't finish and pop, so we're done with this property until the next advance
		}
	}
	for (auto &map : globalProperties) {
		while (!map.second.empty()) {
			DynamicGlobalProperty &gp = map.second.front(); // Only apply the item at the front of the queue
			gp.apply();
			applied_something = true;
			if (!gp.endless && gp.clock.time() >= gp.duration) {
				map.second.pop_front(); // pop it and apply the next one (properly initializes its start value)
			} else
				break; // we applied once and couldn't finish and pop, so we're done with this property until the next advance
		}
	}
	for (auto &map : spritesetProperties) {
		while (!map.second.empty()) {
			DynamicSpritesetProperty &ssp = map.second.front(); // Only apply the item at the front of the queue
			ssp.apply();
			applied_something = true;
			if (!ssp.endless && ssp.clock.time() >= ssp.duration) {
				map.second.pop_front(); // pop it and apply the next one (properly initializes its start value)
			} else
				break; // we applied once and couldn't finish and pop, so we're done with this property until the next advance
		}
	}
	if (applied_something) {
		// this probably should have been in the setValues instead of all the way out here.
		// I think it was actually causing bugs by missing out flushes on calls to addSpriteProperty among other things.
		ons.flush(REFRESH_NORMAL_MODE, nullptr, nullptr, false, false, false);
	}
}

void DynamicPropertyController::addCustomProperty(void *_ptr, bool _is_abs, int _property, int _value, int _duration, int _motion_equation, bool _is_override) {
	if (_is_override) {
		auto &cp = customProperties[std::make_pair(_ptr, _property)];
		if (!cp.empty()) {
			DynamicCustomProperty existing = cp.front();
			existing.apply();
			if (!_is_abs)
				_value += existing.getRemainingValue(); //implicit double to int
			cp.clear();                                 //e.g. halfway through a 0->100 prop we override with a +100, new prop will be 50->150
		}
	}
	DynamicCustomProperty cp = DynamicCustomProperty(this, _ptr, _is_abs, _property, _value, _duration, _motion_equation);
	if (cp.duration == 0) {
		cp.apply();
	} else
		customProperties[std::make_pair(_ptr, _property)].push_back(cp);
}

void DynamicPropertyController::addSpriteProperty(AnimationInfo *_ai, int _sprite_number, bool _is_lsp2, bool _is_abs, int _property, int _value, int _duration, int _motion_equation, bool _is_override) {

	if (_is_override) {
		auto &sps = spriteProperties[std::make_pair(_ai, _property)];
		if (!sps.empty()) {
			DynamicSpriteProperty existing = sps.front();
			existing.apply();
			if (!_is_abs)
				_value += existing.getRemainingValue(); //implicit double to int
			sps.clear();                                //e.g. halfway through a 0->100 prop we override with a +100, new prop will be 50->150
		}
	}
	// Note: Strictly not conforming to PS3. 0 duration properties are meant to stack and execute when they reach the head of the queue.
	// This matters in the case of attempting to use a 0 duration property while the property is animating.
	// We will not change this ourselves due to potentially breaking our old_ai logic without any benefit.
	DynamicSpriteProperty sp    = DynamicSpriteProperty(this, _ai, _sprite_number, _is_lsp2, _is_abs, _property, _value, _duration, _motion_equation);
	sp.for_distinguished_new_ai = _ai->old_ai && _ai->distinguish_from_old_ai;
	if (sp.duration == 0) {
		sp.apply();
		return;
	} // Apply it immediately
	spriteProperties[std::make_pair(_ai, _property)].push_back(sp);
}

void DynamicPropertyController::addGlobalProperty(bool _is_abs, int _property, int _value, int _duration, int _motion_equation, bool _is_override) {
	if (_duration == 0 && (_property == GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE || _property == GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE)) {
		// Prevents a bug caused by setting a property with an instant-property while it is in the middle of animating.
		// Reported by https://forum.umineko-project.org/viewtopic.php?f=4&t=151
		auto &gps = globalProperties[_property];
		if (!gps.empty()) {
			waitOnGlobalProperty(_property);
		}
	}
	if (_is_override) {
		auto &gps = globalProperties[_property];
		if (!gps.empty()) {
			DynamicGlobalProperty existing = gps.front();
			existing.apply();
			if (!_is_abs)
				_value += existing.getRemainingValue(); //implicit double to int
			gps.clear();                                //e.g. halfway through a 0->100 prop we override with a +100, new prop will be 50->150
		}
	}
	DynamicGlobalProperty gp = DynamicGlobalProperty(this, _is_abs, _property, _value, _duration, _motion_equation);
	if (gp.duration == 0) {
		gp.apply();
	} else
		globalProperties[_property].push_back(gp);
}

void DynamicPropertyController::addSpritesetProperty(int _spriteset_number, bool _is_abs, int _property, int _value, int _duration, int _motion_equation) {
	DynamicSpritesetProperty ssp = DynamicSpritesetProperty(this, _spriteset_number, _is_abs, _property, _value, _duration, _motion_equation);
	if (ssp.duration == 0) {
		ssp.apply();
	} else
		spritesetProperties[std::make_pair(_spriteset_number, _property)].push_back(ssp);
}

void DynamicPropertyController::terminateSpriteProperties(AnimationInfo *ai) {
	for (auto it = spriteProperties.begin(); it != spriteProperties.end();) {
		if (it->first.first != ai) {
			++it;
			continue; // Not our sprite
		}
		// Early terminate.
		bool noErase{false};
		for (auto &sp2 : it->second) {

			// Not sure if this is necessary or even non-harmful
			if (sp2.for_distinguished_new_ai) {
				sp2.for_distinguished_new_ai = false;
				++it;
				noErase = true;
				break;
			}
			sp2.endless = false;
			sp2.clock.tick(sp2.getRemainingDuration()); // tick the whole property change away
			sp2.apply();
		}
		if (!noErase) {
			it->second.clear();
			++it;
		}
	}
}

void DynamicPropertyController::terminateSpritesetProperties(SpritesetInfo *si) {
	for (auto it = spritesetProperties.begin(); it != spritesetProperties.end();) {
		if (it->first.first != si->id) {
			++it;
			continue; // Not our set
		}
		// Early terminate.
		for (auto &ss : it->second) {
			ss.endless = false;
			ss.clock.tick(ss.getRemainingDuration()); // tick the whole property change away
			ss.apply();
		}
		it->second.clear();
		++it;
	}
}

void DynamicPropertyController::waitOnCustomProperty(void *ptr, int property, int event_mode_addons) {
	auto pair = std::make_pair(ptr, property);
	//sendToLog(LogLevel::Info, "Going to wait on properties\n");
	waitOnPropertyGeneric(customProperties[pair], event_mode_addons);
	//sendToLog(LogLevel::Info, "Wait on properties done\n");
}

void DynamicPropertyController::waitOnSpriteProperty(AnimationInfo *ai, int property, int event_mode_addons) {
	auto pair = std::make_pair(ai, property);
	//sendToLog(LogLevel::Info, "Going to wait on properties\n");
	waitOnPropertyGeneric(spriteProperties[pair], event_mode_addons);
	//sendToLog(LogLevel::Info, "Wait on properties done\n");
}

void DynamicPropertyController::waitOnGlobalProperty(int property, int event_mode_addons) {
	waitOnPropertyGeneric(globalProperties[property], event_mode_addons);
}

void DynamicPropertyController::waitOnSpritesetProperty(int spriteset_number, int property, int event_mode_addons) {
	auto pair = std::make_pair(spriteset_number, property);
	waitOnPropertyGeneric(spritesetProperties[pair], event_mode_addons);
}

template <class T>
int DynamicPropertyController::getMaxRemainingDuration(std::deque<T> &props) {
	static_assert(std::is_base_of<DynamicProperty, T>::value, "getMaxRemainingDuration called with class that is not a (subclass of) DynamicProperty");
	int max = 0;
	for (auto &prop : props) {
		int v = prop.getRemainingDuration();
		if (v > max)
			max = v;
	}
	return max;
}

template <class T>
void DynamicPropertyController::waitOnPropertyGeneric(std::deque<T> &props, int event_mode_addons) {
	static_assert(std::is_base_of<DynamicProperty, T>::value, "waitOnPropertyGeneric called with class that is not a (subclass of) DynamicProperty");
	while (!props.empty() && !props.front().endless) {
		if ((ons.skip_mode & (ONScripter::SKIP_NORMAL | ONScripter::SKIP_TO_WAIT)) || ons.keyState.ctrl) {
			advance(getMaxRemainingDuration(props));
		}
		ons.event_mode = ONScripter::WAIT_TIMER_MODE | ONScripter::WAIT_SLEEP_MODE | event_mode_addons;
		ons.waitEvent(0);
	}
}

double DynamicPropertyController::DynamicSpriteProperty::getValue() {
	switch (property) {
		case SPRITE_PROPERTY_X_POSITION: return ai->orig_pos.x;
		case SPRITE_PROPERTY_Y_POSITION: return ai->orig_pos.y;
		case SPRITE_PROPERTY_ALPHA_MULTIPLIER: return ai->trans;
		case SPRITE_PROPERTY_RED_MULTIPLIER: return ai->darkenHue.r;
		case SPRITE_PROPERTY_GREEN_MULTIPLIER: return ai->darkenHue.g;
		case SPRITE_PROPERTY_BLUE_MULTIPLIER: return ai->darkenHue.b;
		case SPRITE_PROPERTY_SCALE_X: return ai->scale_x;
		case SPRITE_PROPERTY_SCALE_Y: return ai->scale_y;
		case SPRITE_PROPERTY_ROTATION_ANGLE: return ai->rot;
		case SPRITE_PROPERTY_BLUR: return ai->spriteTransforms.blurFactor;
		case SPRITE_PROPERTY_BREAKUP: return ai->spriteTransforms.breakupFactor;
		case SPRITE_PROPERTY_BREAKUP_DIRECTION: return ai->spriteTransforms.breakupDirectionFlagset;
		case SPRITE_PROPERTY_QUAKE_X_MULTIPLIER: return ai->camera.x_move.multiplier;
		case SPRITE_PROPERTY_QUAKE_X_AMPLITUDE: return ai->camera.x_move.getAmplitude();
		case SPRITE_PROPERTY_QUAKE_X_CYCLE_TIME: return ai->camera.x_move.cycleTime;
		case SPRITE_PROPERTY_QUAKE_Y_MULTIPLIER: return ai->camera.y_move.multiplier;
		case SPRITE_PROPERTY_QUAKE_Y_AMPLITUDE: return ai->camera.y_move.getAmplitude();
		case SPRITE_PROPERTY_QUAKE_Y_CYCLE_TIME: return ai->camera.y_move.cycleTime;
		case SPRITE_PROPERTY_WARP_AMPLITUDE: return ai->spriteTransforms.warpAmplitude;
		case SPRITE_PROPERTY_WARP_WAVELENGTH: return ai->spriteTransforms.warpWaveLength;
		case SPRITE_PROPERTY_WARP_SPEED: return ai->spriteTransforms.warpSpeed;
		case SPRITE_PROPERTY_SCROLLABLE_H: return ai->scrollable.h;
		case SPRITE_PROPERTY_SCROLLABLE_W: return ai->scrollable.w;
		case SPRITE_PROPERTY_SCROLLABLE_Y: return ai->scrollable.y;
		case SPRITE_PROPERTY_SCROLLABLE_X: return ai->scrollable.x;
		case SPRITE_PROPERTY_FLIP_MODE: return ai->flip;
		case SPRITE_PROPERTY_Z_ORDER: return ai->z_order_override;
		default: return 0;
	}
}

void DynamicPropertyController::DynamicSpriteProperty::setValue(double value) {

	//FIXME: This may be insufficient.
	if (ons.effect_current) {
		ons.backupState(ai);
	}

	if (is_lsp2) {
		if (!(ai->old_ai && ai->distinguish_from_old_ai))
			ons.dirtySpriteRect(sprite_number, true); // don't update new ai if it will be a 'different' sprite post-commit
		if (ai->old_ai && !for_distinguished_new_ai)
			ons.dirtySpriteRect(sprite_number, true, true);
	} else {
		if (!(ai->old_ai && ai->distinguish_from_old_ai))
			ons.dirtySpriteRect(sprite_number, false);
		if (ai->old_ai && !for_distinguished_new_ai)
			ons.dirtySpriteRect(sprite_number, false, true);
	}

	//sendToLog(LogLevel::Info, "property %d value %f old_ai %u distinguish %u\n",property,value,ai->old_ai!=nullptr,ai->distinguish_from_old_ai);
	AnimationInfo *curAi = ai;
	if (ai->old_ai && ai->distinguish_from_old_ai && !for_distinguished_new_ai)
		curAi = ai->old_ai; // only update old ai if it will be a 'different' sprite post-commit

	do {
		switch (property) {
			case SPRITE_PROPERTY_X_POSITION: curAi->orig_pos.x = value; break;
			case SPRITE_PROPERTY_Y_POSITION: curAi->orig_pos.y = value; break;
			case SPRITE_PROPERTY_ALPHA_MULTIPLIER: curAi->trans = value; break;
			case SPRITE_PROPERTY_RED_MULTIPLIER: curAi->darkenHue.r = value; break;
			case SPRITE_PROPERTY_GREEN_MULTIPLIER: curAi->darkenHue.g = value; break;
			case SPRITE_PROPERTY_BLUE_MULTIPLIER: curAi->darkenHue.b = value; break;
			case SPRITE_PROPERTY_BLUR: curAi->spriteTransforms.blurFactor = value; break;
			case SPRITE_PROPERTY_BREAKUP: curAi->spriteTransforms.breakupFactor = value; break;
			case SPRITE_PROPERTY_BREAKUP_DIRECTION: curAi->spriteTransforms.breakupDirectionFlagset = value; break;
			case SPRITE_PROPERTY_QUAKE_X_MULTIPLIER: curAi->camera.x_move.multiplier = value; break;
			case SPRITE_PROPERTY_QUAKE_X_AMPLITUDE: curAi->camera.x_move.setAmplitude(value); break;
			case SPRITE_PROPERTY_QUAKE_X_CYCLE_TIME: curAi->camera.x_move.cycleTime = value; break;
			case SPRITE_PROPERTY_QUAKE_Y_MULTIPLIER: curAi->camera.y_move.multiplier = value; break;
			case SPRITE_PROPERTY_QUAKE_Y_AMPLITUDE: curAi->camera.y_move.setAmplitude(value); break;
			case SPRITE_PROPERTY_QUAKE_Y_CYCLE_TIME: curAi->camera.y_move.cycleTime = value; break;
			case SPRITE_PROPERTY_WARP_AMPLITUDE: curAi->spriteTransforms.warpAmplitude = value; break;
			case SPRITE_PROPERTY_WARP_WAVELENGTH: curAi->spriteTransforms.warpWaveLength = value; break;
			case SPRITE_PROPERTY_WARP_SPEED: curAi->spriteTransforms.warpSpeed = value; break;
			case SPRITE_PROPERTY_FLIP_MODE: curAi->flip = value; break;
			case SPRITE_PROPERTY_SCROLLABLE_H:
				if (value < curAi->pos.h)
					curAi->scrollable.h = value;
				break;
			case SPRITE_PROPERTY_SCROLLABLE_W:
				if (value < curAi->pos.w)
					curAi->scrollable.w = value;
				break;
			case SPRITE_PROPERTY_SCROLLABLE_Y:
			case SPRITE_PROPERTY_SCROLLABLE_X: {
				bool vertical = property == SPRITE_PROPERTY_SCROLLABLE_Y;
				if (vertical) {
					int maxH = curAi->scrollableInfo.isSpecialScrollable ? curAi->scrollableInfo.totalHeight : curAi->pos.h;
					// No need to scroll when we have nothing to scroll
					if (maxH <= curAi->scrollable.h)
						break;
					if (value > 0)
						curAi->scrollable.y = value + curAi->scrollable.h > maxH ? maxH - curAi->scrollable.h : value;
					else
						curAi->scrollable.y = 0;

					// Refresh scrollbar area as well
					if (curAi->scrollableInfo.scrollbar) {
						bool lsp2 = false;
						int num   = ons.getAIno(curAi->scrollableInfo.scrollbar, false, lsp2);
						ons.dirtySpriteRect(num, lsp2);
						curAi->scrollableInfo.scrollbar->orig_pos.y = curAi->scrollableInfo.scrollbarTop +
						                                              (curAi->scrollable.y / (curAi->scrollableInfo.totalHeight - curAi->scrollable.h)) * curAi->scrollableInfo.scrollbarHeight;
						ons.UpdateAnimPosXY(curAi->scrollableInfo.scrollbar);
						ons.dirtySpriteRect(num, lsp2);
					}
				} else {
					int maxW = curAi->pos.w;
					// No need to scroll when we have nothing to scroll
					if (maxW <= curAi->scrollable.w)
						break;
					if (value > 0)
						curAi->scrollable.x = value + curAi->scrollable.w > maxW ? maxW - curAi->scrollable.w : value;
					else
						curAi->scrollable.x = 0;
				}

				if (curAi->scrollableInfo.isSpecialScrollable && ons.controlMode == ONScripter::ControlMode::Mouse) {
					ons.refreshButtonHoverState(true);
				}

				break;
			}
			case SPRITE_PROPERTY_Z_ORDER:
				curAi->has_z_order_override = true;
				curAi->z_order_override     = value;
				//sendToLog(LogLevel::Info, "order %d (%d)\n", curAi->id, value);
				break;
			default: // LSP2 properties:
				switch (property) {
					case SPRITE_PROPERTY_SCALE_X: curAi->scale_x = value; break;
					case SPRITE_PROPERTY_SCALE_Y: curAi->scale_y = value; break;
					case SPRITE_PROPERTY_ROTATION_ANGLE: curAi->rot = value; break;
					default:
						ons.errorAndExit("Unknown dynamic property specified.");
				}
				if (!is_lsp2)
					ons.errorAndExit("Make sure to use sprite_property2 for dynamic properties of lsp2s.");
		}

		ons.UpdateAnimPosXY(curAi);

		if (curAi->old_ai && !for_distinguished_new_ai)
			curAi = ai->old_ai;
		else
			curAi = nullptr;
	} while (curAi);

	if (is_lsp2) {
		ai->calcAffineMatrix(window.script_width, window.script_height);
		if (ai->old_ai && !for_distinguished_new_ai)
			ai->old_ai->calcAffineMatrix(window.script_width, window.script_height);
		ons.dirtySpriteRect(sprite_number, true);
	} else {
		ons.dirtySpriteRect(sprite_number, false);
	}
	ons.flush(REFRESH_NORMAL_MODE, nullptr, nullptr, false, false, false);
}

double DynamicPropertyController::DynamicGlobalProperty::getValue() {
	switch (property) {
		case GLOBAL_PROPERTY_QUAKE_X_MULTIPLIER: return ons.camera.x_move.multiplier;
		case GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE: return ons.camera.x_move.getAmplitude();
		case GLOBAL_PROPERTY_QUAKE_X_CYCLE_TIME: return ons.camera.x_move.cycleTime;
		case GLOBAL_PROPERTY_QUAKE_Y_MULTIPLIER: return ons.camera.y_move.multiplier;
		case GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE: return ons.camera.y_move.getAmplitude();
		case GLOBAL_PROPERTY_QUAKE_Y_CYCLE_TIME: return ons.camera.y_move.cycleTime;
		case GLOBAL_PROPERTY_ONION_ALPHA: return ons.onionAlphaFactor;
		case GLOBAL_PROPERTY_ONION_SCALE: return ons.onionAlphaScale;
		case GLOBAL_PROPERTY_BGM_CHANNEL_VOLUME: return ons.music_volume;
		case GLOBAL_PROPERTY_WARP_SPEED: return ons.warpSpeed;
		case GLOBAL_PROPERTY_WARP_WAVELENGTH: return ons.warpWaveLength;
		case GLOBAL_PROPERTY_WARP_AMPLITUDE: return ons.warpAmplitude;
		case GLOBAL_PROPERTY_TEXTBOX_EXTENSION: return wndCtrl.extension;
		case GLOBAL_PROPERTY_BLUR: return ons.blur_mode[ONScripter::BeforeScene];
		case GLOBAL_PROPERTY_CAMERA_X: return ons.camera.offset_pos.x;
		case GLOBAL_PROPERTY_CAMERA_Y: return ons.camera.offset_pos.y;
	}
	if (property & GLOBAL_PROPERTY_MIX_CHANNEL_VOLUME) {
		uint32_t ch = property - GLOBAL_PROPERTY_MIX_CHANNEL_VOLUME;
		return ons.channelvolumes[ch];
	}
	return 0;
}

void DynamicPropertyController::DynamicGlobalProperty::setValue(double value) {
	switch (property) {
		case GLOBAL_PROPERTY_QUAKE_X_MULTIPLIER: ons.camera.x_move.multiplier = value; break;
		case GLOBAL_PROPERTY_QUAKE_X_AMPLITUDE: ons.camera.x_move.setAmplitude(value); break;
		case GLOBAL_PROPERTY_QUAKE_X_CYCLE_TIME: ons.camera.x_move.cycleTime = value; break;
		case GLOBAL_PROPERTY_QUAKE_Y_MULTIPLIER: ons.camera.y_move.multiplier = value; break;
		case GLOBAL_PROPERTY_QUAKE_Y_AMPLITUDE: ons.camera.y_move.setAmplitude(value); break;
		case GLOBAL_PROPERTY_QUAKE_Y_CYCLE_TIME: ons.camera.y_move.cycleTime = value; break;
		case GLOBAL_PROPERTY_ONION_ALPHA: ons.onionAlphaFactor = value; break;
		case GLOBAL_PROPERTY_ONION_SCALE: ons.onionAlphaScale = value; break;
		case GLOBAL_PROPERTY_BGM_CHANNEL_VOLUME:
			ons.setCurMusicVolume(value);
			if (value < 0) {
				if (value == -1)
					ons.stopBGM(false);
				value = 0;
			}
			ons.music_volume = value;
			break;
		case GLOBAL_PROPERTY_WARP_SPEED: ons.warpSpeed = value; break;
		case GLOBAL_PROPERTY_WARP_WAVELENGTH: ons.warpWaveLength = value; break;
		case GLOBAL_PROPERTY_WARP_AMPLITUDE: ons.warpAmplitude = value; break;
		case GLOBAL_PROPERTY_TEXTBOX_EXTENSION: wndCtrl.extension = value; break;
		case GLOBAL_PROPERTY_BLUR:
			ons.blur_mode[ONScripter::AfterScene] = ons.blur_mode[ONScripter::BeforeScene] = value;
			ons.fillCanvas(true, true);
			break;
		case GLOBAL_PROPERTY_CAMERA_X:
			ons.camera.offset_pos.x = value;
			ons.fillCanvas(true, true);
			break;
		case GLOBAL_PROPERTY_CAMERA_Y:
			ons.camera.offset_pos.y = value;
			ons.fillCanvas(true, true);
			break;
	}
	if (property & GLOBAL_PROPERTY_MIX_CHANNEL_VOLUME) {
		uint32_t ch = ons.validChannel(property - GLOBAL_PROPERTY_MIX_CHANNEL_VOLUME);
		if (value < 0) {
			if (value == -1)
				ons.stopDWAVE(ch);
			value = 0;
		}
		ons.setVolume(ch, ons.validVolume(value), ons.volume_on_flag);
	}
	/* May be missing required flush?
     * ons.flush(ONScripter::REFRESH_NORMAL_MODE, nullptr, nullptr, false, false, false); */
}

double DynamicPropertyController::DynamicSpritesetProperty::getValue() {
	switch (property) {
		case SPRITESET_PROPERTY_X_POSITION: return ons.spritesets[spriteset_number].pos.x;
		case SPRITESET_PROPERTY_Y_POSITION: return ons.spritesets[spriteset_number].pos.y;
		case SPRITESET_PROPERTY_ALPHA: return ons.spritesets[spriteset_number].trans;
		case SPRITESET_PROPERTY_BLUR: return ons.spritesets[spriteset_number].blur;
		case SPRITESET_PROPERTY_BREAKUP: return ons.spritesets[spriteset_number].breakupFactor;
		case SPRITESET_PROPERTY_BREAKUP_DIRECTION: return ons.spritesets[spriteset_number].breakupFactor ? 1 : 2;
		case SPRITESET_PROPERTY_PIXELATE: return ons.spritesets[spriteset_number].pixelateFactor;
		case SPRITESET_PROPERTY_WARP_AMPLITUDE: return ons.spritesets[spriteset_number].warpAmplitude;
		case SPRITESET_PROPERTY_WARP_WAVELENGTH: return ons.spritesets[spriteset_number].warpWaveLength;
		case SPRITESET_PROPERTY_WARP_SPEED: return ons.spritesets[spriteset_number].warpSpeed;
		case SPRITESET_PROPERTY_FLIP_MODE: return ons.spritesets[spriteset_number].flip;
		case SPRITESET_PROPERTY_CENTRE_X: return ons.spritesets[spriteset_number].scale_center_x;
		case SPRITESET_PROPERTY_CENTRE_Y: return ons.spritesets[spriteset_number].scale_center_y;
		case SPRITESET_PROPERTY_SCALE_X: return ons.spritesets[spriteset_number].scale_x;
		case SPRITESET_PROPERTY_SCALE_Y: return ons.spritesets[spriteset_number].scale_y;
		case SPRITESET_PROPERTY_ROTATION_ANGLE: return ons.spritesets[spriteset_number].rot;
		default: return 0;
	}
}

void DynamicPropertyController::DynamicSpritesetProperty::setValue(double value) {
	switch (property) {
		case SPRITESET_PROPERTY_X_POSITION: ons.spritesets[spriteset_number].pos.x = value; break;
		case SPRITESET_PROPERTY_Y_POSITION: ons.spritesets[spriteset_number].pos.y = value; break;
		case SPRITESET_PROPERTY_ALPHA: ons.spritesets[spriteset_number].trans = value; break;
		case SPRITESET_PROPERTY_BLUR: ons.spritesets[spriteset_number].blur = value; break;
		case SPRITESET_PROPERTY_BREAKUP: ons.spritesets[spriteset_number].breakupFactor = value; break;
		case SPRITESET_PROPERTY_BREAKUP_DIRECTION: ons.spritesets[spriteset_number].breakupDirectionFlagset = value; break;
		case SPRITESET_PROPERTY_PIXELATE: ons.spritesets[spriteset_number].pixelateFactor = value; break;
		case SPRITESET_PROPERTY_WARP_AMPLITUDE: ons.spritesets[spriteset_number].warpAmplitude = value; break;
		case SPRITESET_PROPERTY_WARP_WAVELENGTH: ons.spritesets[spriteset_number].warpWaveLength = value; break;
		case SPRITESET_PROPERTY_WARP_SPEED: ons.spritesets[spriteset_number].warpSpeed = value; break;
		case SPRITESET_PROPERTY_FLIP_MODE: ons.spritesets[spriteset_number].flip = value; break;
		case SPRITESET_PROPERTY_CENTRE_X: ons.spritesets[spriteset_number].scale_center_x = value; break;
		case SPRITESET_PROPERTY_CENTRE_Y: ons.spritesets[spriteset_number].scale_center_y = value; break;
		case SPRITESET_PROPERTY_SCALE_X: ons.spritesets[spriteset_number].scale_x = value; break;
		case SPRITESET_PROPERTY_SCALE_Y: ons.spritesets[spriteset_number].scale_y = value; break;
		case SPRITESET_PROPERTY_ROTATION_ANGLE: ons.spritesets[spriteset_number].rot = value; break;
	}
	//We may be in REFRESH_BEFORESCENE_MODE (addSpritesetProperty), but non-0-duration properties imply both before/after changes.
	ons.dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
	ons.before_dirty_rect_scene.fill(window.canvas_width, window.canvas_height);
	ons.flush(REFRESH_NORMAL_MODE, nullptr, nullptr, false, false, false);
}

void DynamicPropertyController::DynamicProperty::apply() {
	if (clock.time() == 0) {
		begin();
	}
	//sendToLog(LogLevel::Info, "apply property for %i/%i of the way through %i -> %i\n", counter, duration, start_value, end_value);
	setValue(getInterpolatedValue());
}

void DynamicPropertyController::DynamicProperty::begin() {
	start_value = getValue();
	if (!is_abs) {
		end_value = start_value + end_value;
	}
	if (motion_equation == MOTION_EQUATION_CONSTANT_ROTATE_SPEED) {
		// passed duration is in fact degrees per second; let's make it into an actual duration
		duration = std::abs((1000) * (end_value - start_value) / static_cast<double>(duration)); // ms = (ms/sec)*(deg) / (deg/sec)
	}
	if(motion_equation == MOTION_EQUATION_COSINE_WAVE) {
		endless = true;
	}
	clock.tick(1); // Tiny hack; prevent begin from being executed more than once by immediately advancing 1ms. Creates a very slight timing error.
	               // (Could also have fixed this by making sure that, when a property P1 finishes and is popped, to advance the next property P2 by the remaining time before applying P2 --
	               // but this is more complicated since P2 may have 0ms left to advance, in a corner case)
	               // (Could also introduce a "hasBegun" flag.)
}

double DynamicPropertyController::DynamicProperty::getInterpolatedValue() {
	//sendToLog(LogLevel::Info, "getInterpolatedValue for %i/%i of the way through %i -> %i\n", counter, duration, start_value, end_value);
	if (!endless && clock.time() >= duration)
		return end_value;
	if (clock.time() == 0)
		return start_value;
	double t = static_cast<double>(clock.time()) / static_cast<double>(duration);
	double new_t;
	switch (motion_equation) {
		case MOTION_EQUATION_SMOOTH:
			new_t = std::sin(M_PI * t / 2);
			new_t *= new_t;
			break;
		case MOTION_EQUATION_SPEEDUP:
			new_t = 1 - (alpha_f * (1 - t) + (3 - 2 * alpha_f) * std::pow((1 - t), 2) + (alpha_f - 2) * std::pow((1 - t), 3));
			break;
		case MOTION_EQUATION_SLOWDOWN:
			//this equation f(t) is the cubic polynomial satisfying f(0)=0, f(1)=1, f'(0)=alpha_f, f'(1)=0
			new_t = alpha_f * t + (3 - 2 * alpha_f) * std::pow(t, 2) + (alpha_f - 2) * std::pow(t, 3);
			break;
		case MOTION_EQUATION_COSINE_WAVE:
			new_t = 0.5 - (std::cos(2*M_PI*t)) / 2;
			break;
		case MOTION_EQUATION_LINEAR:
		case MOTION_EQUATION_CONSTANT_ROTATE_SPEED:
		default:
			new_t = t;
	}
	return (end_value * (new_t)) + (start_value * (1.0 - new_t));
}

int DynamicPropertyController::DynamicProperty::getRemainingDuration() {
	int remaining = duration - clock.time();
	return remaining > 0 ? remaining : 0;
}

double DynamicPropertyController::DynamicProperty::getRemainingValue() {
	return end_value - getValue();
}
