/**
 *  Text.cpp
 *  ONScripter-RU
 *
 *  Text parser and tag converter.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Async.hpp"
#include "Engine/Components/Fonts.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Support/Unicode.hpp"

// Should probably use regex
bool ONScripter::isAlphanumeric(char16_t codepoint) {
	if (codepoint >= u'a' && codepoint <= u'z')
		return true;
	if (codepoint >= u'A' && codepoint <= u'Z')
		return true;
	if (codepoint >= u'0' && codepoint <= u'9')
		return true;
	if (codepoint >= u'А' && codepoint <= u'я')
		return true;
	if (codepoint == u'Ё' || codepoint == u'ё')
		return true;
	return false;
}

void ONScripter::processSpecialCharacters(std::u16string &text, Fontinfo &info, Fontinfo::InlineOverrides &io) {
	bool modifiedString;
	do {
		modifiedString = false;
		modifiedString |= processTransformedCharacterSequence(text, info);
		modifiedString |= processSmartQuote(text, info);
		modifiedString |= processInlineCommand(text, info, io);
		modifiedString |= processHashColor(text, info);
		modifiedString |= processIgnored(text, info);
	} while (modifiedString);
}

bool ONScripter::processTransformedCharacterSequence(std::u16string &string, Fontinfo & /*info*/) {
	bool modified = false;
	while (true) {
		if (string.empty())
			return modified;
		if (string[0] == u'`') {
			string.erase(0, 1);
			modified = true;
			continue;
		}
		if (ons.isAlphanumeric(string[0]) || string[0] == u'`' || string[0] == u'‐' || string[0] == u'*') {
			// Look for "..." after the start of the string (plus any number of additional "..."s)
			auto foundNonDot = string.find_first_not_of(u'.', 1);
			if (foundNonDot != std::u16string::npos && foundNonDot >= 4 && (foundNonDot % 3 == 1)) {
				auto afterDots                    = string[foundNonDot];
				const std::u16string permissibles = u"*{‘“";
				bool splittable                   = ons.isAlphanumeric(afterDots) || permissibles.find(afterDots) != std::u16string::npos;
				if (splittable) {
					for (auto it = string.begin() + (foundNonDot); it != string.begin() + 1; it -= 3) {
						it = string.insert(it, ZeroWidthSpace);
					}
					modified = true;
					continue;
				}
			}
		}
		if (string[0] == u'{') {
			if (string.compare(1, 2, u"n}") == 0) {
				string.replace(0, 3, 1, NewLine);
				modified = true;
				continue;
			}
			if (string.compare(1, 2, u"0}") == 0) {
				string.replace(0, 3, 1, ZeroWidthSpace);
				modified = true;
				continue;
			}
			if (string.compare(1, 3, u"qt}") == 0) {
				string.replace(0, 4, 1, NormalQuote);
				modified = true;
				continue;
			}
			if (string.compare(1, 3, u"ob}") == 0) {
				string.replace(0, 4, 1, OpeningCurlyBrace);
				modified = true;
				continue;
			}
			if (string.compare(1, 3, u"eb}") == 0) {
				string.replace(0, 4, 1, ClosingCurlyBrace);
				modified = true;
				continue;
			}
			if (string.compare(1, 3, u"os}") == 0) {
				string.replace(0, 4, 1, OpeningSquareBrace);
				modified = true;
				continue;
			}
			if (string.compare(1, 3, u"es}") == 0) {
				string.replace(0, 4, 1, ClosingSquareBrace);
				modified = true;
				continue;
			}
			if (string.compare(1, 2, u"-}") == 0) {
				string.replace(0, 3, 1, SoftHyphen);
				modified = true;
				continue;
			}
		}
		// Replace horizontal bar with em-dash
		if (string.compare(0, 1, u"―") == 0) {
			string.replace(0, 1, 1, EmDash);
			modified = true;
			continue;
		}
		// Replace em-dash quote with nobr guard
		if (string.compare(0, 2, u"—\"") == 0 && string.compare(0, 3, u"—\"}") != 0) {
			// FIXME: Isn't this dangerous due to incorrectly nested style stack pops?
			string.replace(0, 2, u"{nobr:—\"}");
			modified = true;
			continue;
		}
		if (string.length() >= 3 && string[1] == u'*') {
			auto prev = string.at(0);
			auto next = string.at(2);
			if (ons.isAlphanumeric(prev) && ons.isAlphanumeric(next)) {
				string.replace(1, 1, 1, LinebreakableAsterisk);
				modified = true;
			}
		}
		break;
	}
	return modified;
}

bool ONScripter::processSmartQuote(std::u16string &string, Fontinfo &info) {
	if (string.empty())
		return false;
	uint32_t codepoint  = static_cast<uint32_t>(string.at(0));
	auto &lastCodepoint = info.layoutData.last_printed_codepoint;

	// Smart quote support
	// b indicates we're in ru single-quote parsing mode
	bool b = !info.smart_single_quotes_represented_by_dumb_double && codepoint == '\'';
	if (info.smart_quotes && (b || codepoint == '"')) {
		uint32_t &oq  = b ? info.opening_single_quote : info.opening_double_quote;
		uint32_t &cq  = b ? info.closing_single_quote : info.closing_double_quote;
		const int &qn = b ? info.style().opened_single_quotes : info.style().opened_double_quotes;
		if (lastCodepoint == oq || (b && lastCodepoint == info.opening_double_quote)) {
			// “" becomes ““ (handles cases like ""this"".)
			// for RU: also, “' becomes “‘ (not “’)
			info.styleStack.push(info.style());
			int &qnNew = b ? info.changeStyle().opened_single_quotes : info.changeStyle().opened_double_quotes;
			qnNew++;
			codepoint = oq;
			string.replace(0, 1, 1, codepoint);
			return true;
		}
		if (lastCodepoint == ' ' || lastCodepoint == 0) {
			// a space "then quotes
			if (qn == 0 || !info.smart_single_quotes_represented_by_dumb_double) {
				// becomes “ when there aren't any double quotes open yet
				// for RU: becomes the appropriate opening quote here and now regardless of number context
				info.styleStack.push(info.style());
				int &qnNew = b ? info.changeStyle().opened_single_quotes : info.changeStyle().opened_double_quotes;
				qnNew++;
				codepoint = oq;
				string.replace(0, 1, 1, codepoint);
				return true;
			}
			// if there are double quotes already open, it becomes ‘
			info.styleStack.push(info.style());
			info.changeStyle().opened_single_quotes++;
			codepoint = info.opening_single_quote;
			string.replace(0, 1, 1, codepoint);
			return true;
		}
		if (info.smart_single_quotes_represented_by_dumb_double && info.style().opened_single_quotes > 0) {
			// if we are already two or more levels deep -- “in a case like ‘this"
			// then make a single closing quote
			// RU does not enter this block
			if (info.styleStack.size() > 1) {
				info.styleStack.pop();
				info.fontInfoChanged = true;
			}
			codepoint = info.closing_single_quote;
			string.replace(0, 1, 1, codepoint);
			return true;
		}
		// If there is no single quote open, and we do this" or this.", then close a double quote.
		// for RU: we get whichever closing quote we asked for.
		// Warning! If RU attempts an apostrophe, it will be treated as an unmatched closing quote.
		if (info.styleStack.size() > 1) {
			info.styleStack.pop();
			info.fontInfoChanged = true;
		}
		codepoint = cq;
		string.replace(0, 1, 1, codepoint);
		return true;
	}
	if (info.smart_quotes && codepoint == '\'') {
		codepoint = info.apostrophe;
		string.replace(0, 1, 1, codepoint);
		return true;
	}
	return false;
}

bool ONScripter::processInlineCommand(std::u16string &string, Fontinfo &info, Fontinfo::InlineOverrides &io) {
	if (string.empty())
		return false;
	bool modified      = false;
	uint32_t codepoint = static_cast<uint32_t>(string.at(0));
	if (codepoint == '}') {
		if (info.styleStack.size() > 1) {
			// we need to give character layouting an opportunity to layout this ruby now it's complete
			auto inruby = !info.style().ruby_text.empty();
			info.styleStack.pop();
			info.fontInfoChanged = true;
			string.erase(0, 1);
			if (inruby && info.style().ruby_text.empty())
				string.insert(0, 1, NoOp);
			io |= info.style().inlineOverrides;
			modified = true;
		}
		return modified;
	}

	if (codepoint != '{')
		return modified;

	info.styleStack.push(info.style());
	string.erase(0, 1);

	// read the name
	auto specialCharPos = string.find_first_not_of(u"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
	if (specialCharPos == std::u16string::npos) {
		sendToLog(LogLevel::Warn, "Inline command tag not closed\n");
		return true;
	}

	auto specialChar           = string.at(specialCharPos);
	std::u16string commandName = string.substr(0, specialCharPos);
	string.erase(0, specialCharPos);

	// read the parameter if any
	std::u16string param;
	if (specialChar != '}') {
		string.erase(0, 1);
		auto endParamCharPos          = string.find_first_of(specialChar);
		auto closeBracePos            = string.find_first_of(u"{}");
		bool endParamBeforeCloseBrace = endParamCharPos != std::u16string::npos &&
		                                (closeBracePos == std::u16string::npos ||
		                                 endParamCharPos < closeBracePos);
		if (endParamBeforeCloseBrace) {
			param = string.substr(0, endParamCharPos);
			string.erase(0, endParamCharPos + 1);
		}
	}

	// This command is meant to be ignored
	if (info.style().ignore_text) {
		return true;
	}

	// pass 'em in
	std::string cname{decodeUTF16String(commandName)};
	std::string cparam{decodeUTF16String(param)};
	bool wantsLinebreak = executeInlineTextCommand(cname, cparam, info);
	io |= info.style().inlineOverrides;

	// Encapsulate this in a fn if it gets any larger
	if (wantsLinebreak) {
		// Allow linebreaking before ruby (also passes control back to caller of processSpecialCharacters for correct handling of ruby prefontinfo)
		string.insert(0, u"{0}");
	}
	return true;
}

bool ONScripter::processHashColor(std::u16string &string, Fontinfo &info) {
	if (string.length() < 7)
		return false;
	if (string[0] != u'#')
		return false;
	if (string.find_first_not_of(u"0123456789abcdefABCDEF", 1, 6) != std::string::npos)
		return false;
	std::string value   = decodeUTF16String(string.substr(1, 6));
	std::string command = "color";
	string.erase(0, 7);
	executeInlineTextCommand(command, value, info);
	return true;
}

bool ONScripter::executeInlineTextCommand(std::string &command, std::string &param, Fontinfo &info) {
	// Currently case-sensitive.

	auto processItalic = [](std::string & /* command */, std::string & /* param */, Fontinfo &info) {
		info.changeStyle().is_bold   = false;
		info.changeStyle().is_italic = true;
		return false;
	};

	auto processBold = [](std::string & /* command */, std::string & /* param */, Fontinfo &info) {
		info.changeStyle().is_bold   = true;
		info.changeStyle().is_italic = false;
		return false;
	};

	auto processBoldItalic = [](std::string & /* command */, std::string & /* param */, Fontinfo &info) {
		info.changeStyle().is_bold   = true;
		info.changeStyle().is_italic = true;
		return false;
	};

	auto processUnderline = [](std::string & /* command */, std::string & /* param */, Fontinfo &info) {
		info.changeStyle().is_underline = true;
		return false;
	};

	auto processGradient = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		bool enable                    = param == "1" || param == "on" || param == "yes" || param == "y";
		info.changeStyle().is_gradient = enable;
		return false;
	};

	auto processLeft = [](std::string & /* command */, std::string & /* param */, Fontinfo & /* info */) {
		//FIXME: implement
		return false;
	};

	auto processCentre = [](std::string & /* command */, std::string & /* param */, Fontinfo &info) {
		info.changeStyle().inlineOverrides.is_centered.set(true); //TODO: replace by some kind of alignment enum.
		return false;
	};

	auto processRight = [](std::string & /* command */, std::string & /* param */, Fontinfo & /* info */) {
		//FIXME: implement
		return false;
	};

	auto processAlignment = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		if (param[0] == 'c')
			info.changeStyle().inlineOverrides.is_centered.set(true);
		//TODO: support others?
		return false;
	};

	auto processFit = [](std::string & /* command */, std::string & /* param */, Fontinfo &info) {
		info.changeStyle().inlineOverrides.is_fitted.set(true);
		return false;
	};

	auto processNobreak = [](std::string & /* command */, std::string & /* param */, Fontinfo &info) {
		info.changeStyle().no_break = true;
		return false;
	};

	auto processFont = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		info.changeCurrentFont(std::stoi(param), info.changeStyle().preset_id);
		return false;
	};

	auto processBorder = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		int paramInt                    = std::stoi(param);
		info.changeStyle().is_border    = paramInt;
		info.changeStyle().border_width = paramInt * 25;
		return false;
	};

	auto processShadow = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		auto commaLoc = param.find_first_of(',');
		if (commaLoc == std::string::npos)
			return false;
		int shadowX                           = std::stoi(param.substr(0, commaLoc));
		int shadowY                           = std::stoi(param.substr(commaLoc + 1, param.length() - (commaLoc + 1)));
		info.changeStyle().is_shadow          = shadowX || shadowY;
		info.changeStyle().shadow_distance[0] = shadowX;
		info.changeStyle().shadow_distance[1] = shadowY;
		return false;
	};

	auto processYesNo = [this](std::string &command, std::string &param, Fontinfo &info) {
		size_t idx = std::stoi(param);
		// if conditions is set to 1 and command is n, we want to ignore, and vice versa
		if (conditions[idx] != (command == "y")) {
			info.changeStyle().ignore_text = true;
		}
		return false;
	};

	auto processPreset = [this](std::string & /* command */, std::string &param, Fontinfo &info) {
		int paramInt = std::stoi(param);
		auto pr      = presets.find(paramInt);
		if (pr != presets.end())
			info.changeStyle() = pr->second;
		return false;
	};

	auto processFontSize = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		info.changeStyle().font_size = std::stoi(param);
		return false;
	};

	auto processFontSizePercent = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		info.changeStyle().font_size = (info.style().font_size * std::stoi(param)) / 100;
		return false;
	};

	auto processCharacterSpacing = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		info.changeStyle().character_spacing = std::stoi(param);
		return false;
	};

	auto processRuby = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		info.changeStyle().ruby_text = param;
		return true;
	};

	auto processLoghint = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		if (info.changeStyle().can_loghint)
			info.changeStyle().ruby_text = param;
		return true;
	};

	auto processWidth = [](std::string & /* command */, std::string &param, Fontinfo &info) {
		info.changeStyle().inlineOverrides.wrap_limit.set(std::stoi(param));
		return false;
	};

	auto processParallel = [](std::string & /* command */, std::string & /* param */, Fontinfo &info) {
		info.changeStyle().inlineOverrides.startsNewRun.set(true); // no real reason this needs to be an override... could have put it directly in fontInfo...
		return false;
	};

	auto processColour = [this](std::string & /* command */, std::string &param, Fontinfo &info) {
		if (param.length() != 6)
			return false;
		std::string colorString = "#" + param;
		readColor(&info.changeStyle().color, colorString.data());
		return false;
	};

	auto processShadowColour = [this](std::string & /* command */, std::string &param, Fontinfo &info) {
		if (param.length() != 6)
			return false;
		std::string colorString = "#" + param;
		readColor(&info.changeStyle().shadow_color, colorString.data());
		return false;
	};

	auto processBorderColour = [this](std::string & /* command */, std::string &param, Fontinfo &info) {
		if (param.length() != 6)
			return false;
		std::string colorString = "#" + param;
		readColor(&info.changeStyle().border_color, colorString.data());
		return false;
	};

	if (processFuncs.empty()) {
		processFuncs["italic"] = processFuncs["i"] = processItalic;
		processFuncs["bold"] = processFuncs["b"] = processBold;
		processFuncs["bolditalic"] = processFuncs["x"] = processBoldItalic;
		processFuncs["underline"] = processFuncs["u"] = processUnderline;

		processFuncs["gradient"] = processFuncs["g"] = processGradient;

		processFuncs["left"] = processFuncs["al"] = processLeft;
		processFuncs["center"] = processFuncs["centre"] = processFuncs["ac"] = processCentre;
		processFuncs["right"] = processFuncs["ar"] = processRight;
		processFuncs["alignment"] = processFuncs["a"] = processAlignment;

		processFuncs["fit"] = processFuncs["j"] = processFit;
		processFuncs["nobreak"] = processFuncs["nobr"] = processNobreak;

		processFuncs["font"] = processFuncs["f"] = processFont;

		processFuncs["border"] = processFuncs["borderwidth"] = processFuncs["o"] = processBorder;
		processFuncs["shadow"] = processFuncs["shadowdistance"] = processFuncs["s"] = processShadow;

		processFuncs["y"] = processFuncs["n"] = processYesNo;
		processFuncs["preset"] = processFuncs["p"] = processPreset;

		processFuncs["fontsize"] = processFuncs["fontsizeabsolute"] = processFuncs["size"] = processFuncs["d"] = processFontSize;
		processFuncs["fontsizepercent"] = processFuncs["fontsizepc"] = processFuncs["sizepercent"] = processFuncs["sizepc"] = processFuncs["e"] = processFontSizePercent;
		processFuncs["characterspacing"] = processFuncs["charspacing"] = processFuncs["m"] = processCharacterSpacing;

		processFuncs["ruby"] = processFuncs["h"] = processRuby;
		processFuncs["loghint"] = processFuncs["l"] = processLoghint;

		processFuncs["width"] = processFuncs["w"] = processWidth;
		processFuncs["parallel"] = processFuncs["t"] = processParallel;

		processFuncs["color"] = processFuncs["colour"] = processFuncs["c"] = processColour;
		processFuncs["shadowcolor"] = processFuncs["shadowcolour"] = processFuncs["v"] = processShadowColour;
		processFuncs["bordercolor"] = processFuncs["bordercolour"] = processFuncs["r"] = processBorderColour;
	}

	auto cmd = processFuncs.find(command);
	if (cmd != processFuncs.end())
		return cmd->second(command, param, info);
	return false;
}

bool ONScripter::processIgnored(std::u16string &string, Fontinfo &info) {
	if (info.style().ignore_text) {
		auto cmdLoc = string.find_first_of(u"{}");
		if (cmdLoc != std::string::npos) {
			string.erase(0, cmdLoc);
			return true;
		}
	}
	return false;
}

void ONScripter::resetGlyphCache() {
	if (!use_text_atlas) {
		throw std::runtime_error("Attempted to reset disabled text atlas");
	}

	sendToLog(LogLevel::Warn, "Resetting glyph cache will cause degraded performance!\n");
	glyphAtlas.reset();
	auto sz = glyphCache.size();
	glyphCache.resize(0);
	glyphCache.resize(sz);
}

void ONScripter::renderGlyphValues(const GlyphValues &values, GPU_Rect *dst_clip, TextRenderingState::TextRenderingDst dst, float x, float y, float r, bool render_border, int alpha) {
	GPU_Image *coloured_glyph{nullptr};
	GPU_Rect *src_rect{nullptr};
	if ((!render_border && values.glyph_pos.has()) || (render_border && values.border_pos.has())) {
		// new approach goes here
		coloured_glyph = glyphAtlas.atlas;
		src_rect       = render_border ? &values.border_pos.get() : &values.glyph_pos.get();
	} else {
		coloured_glyph = render_border ? values.border_gpu : values.glyph_gpu;
	}

	if (coloured_glyph) {
		x += r * (src_rect ? src_rect->w : coloured_glyph->w) / 2.0;
		y += 1 * (src_rect ? src_rect->h : coloured_glyph->h) / 2.0;
		if (alpha < 255) {
			GPU_SetRGBA(coloured_glyph, alpha, alpha, alpha, alpha);
		}
		if (dst.target) {
			gpu.copyGPUImage(coloured_glyph, src_rect, dst_clip, dst.target, x, y, r, 1, 0, true);
		} else {
			if (r == 1)
				gpu.copyGPUImage(coloured_glyph, src_rect, dst_clip, dst.bigImage, x, y);
			else
				errorAndExit("BigImages do not support scaled text at this moment!");
		}
		if (alpha < 255) {
			GPU_SetRGBA(coloured_glyph, 255, 255, 255, 255);
		}
	}
}

/* const: you may not alter the properties of the returned GlyphValues, because that would change our nice cached version */
const GlyphValues *ONScripter::renderUnicodeGlyph(Font *font, GlyphParams *key) {
	static SDL_Color fcol = {0xff, 0xff, 0xff, 0xff}, bcol = {0, 0, 0, 0};

	GlyphParams k = *key;

	GlyphValues *glyph;
	try {
		glyph = glyphCache.get(k);
	} catch (int) {
		// No coloured glyph found... we'll have to get an uncolored one and color it.
		// First let's see if there's an uncolored one already in the cache.
		GlyphParams uncolored = k;
		uncolored.is_colored  = false;
		GlyphValues *uncolored_glyph;
		try {
			uncolored_glyph = glyphCache.get(uncolored);
		} catch (int) {
			// No uncoloured one in the cache either. Looks like we gotta render it from FT. (Then put it in the cache for later use.)
			uncolored_glyph = font->renderGlyph(&uncolored, fcol, bcol);
			if (uncolored_glyph->buildGPUImages(use_text_atlas ? &glyphAtlas : nullptr)) {
				glyphCache.set(uncolored, uncolored_glyph);
			} else {
				delete uncolored_glyph;
				resetGlyphCache();
				return renderUnicodeGlyph(font, key);
			}
		}
		// OK, so we have the uncolored glyph one way or another... now let's paint it
		// (but not if we are asked to paint it black)
		bool black_glyph  = k.glyph_color.r == 0 && k.glyph_color.g == 0 && k.glyph_color.b == 0;
		bool black_border = k.border_color.r == 0 && k.border_color.g == 0 && k.border_color.b == 0;
		if (black_glyph && black_border) {
			return uncolored_glyph;
		}
		bool should_set = true;
		glyph           = new GlyphValues(*uncolored_glyph); // so we don't ruin the uncolored one in the cache (prevents trying to recolor an already colored glyph)
		if (!black_glyph)
			should_set = colorGlyph(key, glyph, &k.glyph_color, false, use_text_atlas ? &glyphAtlas : nullptr); // Color the glyph
		if (!black_border && should_set)
			should_set = colorGlyph(key, glyph, &k.border_color, true, use_text_atlas ? &glyphAtlas : nullptr); // Color the border
		if (should_set) {
			glyphCache.set(k, glyph); // Store the colored glyph in the cache so we don't need to color it repeatedly.
		} else {
			delete glyph;
			resetGlyphCache();
			return renderUnicodeGlyph(font, key);
		}
	}

	return glyph;
}

void ONScripter::enterTextDisplayMode() {
	if (saveon_flag && internal_saveon_flag) {
		saveSaveFile(-1);
		internal_saveon_flag = false;
	}

	did_leavetext = false;

	if (wndCtrl.usingDynamicTextWindow) {
		// When we are using a normal window textbox area is static, the only possible change is setwindow-based and
		// we can always add a new rect to the dirty_rect (which actually happens in those commands).
		// That's why enterTextDisplay is optimised not to refresh anything if we are already in text mode.
		// Dynamic window is not like that. We no longer know its previous dimensions when it dlgCtrl is deactivated,
		// which means that we have to cleanup a bigger area to avoid issues (i. e. script area).
		// This is done here, because earlier may well collide with pretext actions.
		// This is not done in texec3, because texec3 is a logical command that cleans the text out.
		before_dirty_rect_hud.add({0, 0, static_cast<float>(text_gpu->w), static_cast<float>(text_gpu->h)});
		dirty_rect_hud.add({0, 0, static_cast<float>(text_gpu->w), static_cast<float>(text_gpu->h)});
	}

	if (!(display_mode & DISPLAY_MODE_TEXT)) {

		display_mode = DISPLAY_MODE_TEXT;

		if (!wndCtrl.usingDynamicTextWindow)
			addTextWindowClip(before_dirty_rect_hud);

		// Unsure if perfectly safe...
		if (!(skip_mode & SKIP_SUPERSKIP)) {
			if (constantRefreshEffect(&window_effect, false, false,
			                          REFRESH_BEFORESCENE_MODE | REFRESH_NORMAL_MODE,     // refresh from no window (on beforescene)
			                          REFRESH_BEFORESCENE_MODE | refresh_window_text_mode // to window              (on beforescene)
			                          ))
				return;
		}
	} else if (wndCtrl.usingDynamicTextWindow) {
		// This will make sure we are refreshing what we need to
		flush(refresh_window_text_mode);
	}
}

void ONScripter::leaveTextDisplayMode(bool force_leave_flag, bool perform_effect) {

	//sendToLog(LogLevel::Info, "leaveTextDisplayMode(%u)\n", force_leave_flag);

	//ons-en feature: when in certain skip modes, don't actually leave
	//text display mode unless forced to (but say you did)
	if (!force_leave_flag && (skip_mode & (SKIP_NORMAL) || keyState.ctrl)) {
		did_leavetext = true;
		return;
	}
	if (force_leave_flag)
		did_leavetext = false;

	if (!did_leavetext && (display_mode & DISPLAY_MODE_TEXT) &&
	    (force_leave_flag || (erase_text_window_mode != 0))) {

		//sendToLog(LogLevel::Info, "leaveTextDisplayMode(%u) body\n", force_leave_flag);

		addTextWindowClip(dirty_rect_hud);

		display_mode = DISPLAY_MODE_NORMAL;

		// Unsure if perfectly safe...
		if (perform_effect && !(skip_mode & SKIP_SUPERSKIP)) {
			if (constantRefreshEffect(&window_effect, false, false,
			                          REFRESH_BEFORESCENE_MODE | refresh_window_text_mode, // refresh from window (on beforescene)
			                          REFRESH_BEFORESCENE_MODE | REFRESH_NORMAL_MODE       // to no window        (on beforescene)
			                          ))
				return;
		}
	}

	display_mode |= DISPLAY_MODE_UPDATED;
}

void ONScripter::renderDynamicTextWindow(GPU_Target *target, GPU_Rect *canvas_clip_dst, int refresh_mode, bool useCamera) {
	// The normal, non-dynamic way
	// drawToGPUTarget(target, sentence_font_info.oldNew(refresh_mode), refresh_mode, clip_dst);

	AnimationInfo *info = sentence_font_info.oldNew(refresh_mode);
	GPU_Image *src      = info->gpu_image;
	if (!src)
		return;

	auto blits = wndCtrl.getRegions();
	for (auto blit : blits) {
		GPU_Rect clip_src = blit.src;
		GPU_Rect real_dst = blit.dst;
		if (useCamera) {
			real_dst.x += camera.center_pos.x;
			real_dst.y += camera.center_pos.y;
		}
		float coord_x = real_dst.x + (real_dst.w / 2.0);
		float coord_y = real_dst.y + (real_dst.h / 2.0);
		float wResize = 1;
		float hResize = 1;
		if (real_dst.w > clip_src.w && clip_src.w > 0) {
			wResize = real_dst.w / clip_src.w;
		}
		if (real_dst.h > clip_src.h && clip_src.h > 0) {
			hResize = real_dst.h / clip_src.h;
		}
		if (canvas_clip_dst) {
			if (doClipping(&real_dst, canvas_clip_dst))
				continue;
		}
		gpu.copyGPUImage(src, &clip_src, &real_dst, target, coord_x, coord_y, wResize, hResize, 0, true);
	}
}

bool ONScripter::doClickEnd() {
	draw_cursor_flag          = true;
	internal_slowdown_counter = 0;

	if (!((skip_mode & SKIP_TO_EOL) && clickskippage_flag))
		skip_mode &= ~(SKIP_TO_WAIT | SKIP_TO_EOL);

	if (automode_flag) {
		event_mode = WAIT_TEXT_MODE | WAIT_INPUT_MODE |
		             WAIT_VOICE_MODE | WAIT_TIMER_MODE;
		if (automode_time < 0)
			waitEvent(-automode_time * dlgCtrl.dialogueRenderState.clickPartCharacterCount());
		else
			waitEvent(automode_time);
	} else if (autoclick_time > 0) {
		event_mode = WAIT_SLEEP_MODE | WAIT_TIMER_MODE;
		waitEvent(autoclick_time);
	} else {
		event_mode = WAIT_TEXT_MODE | WAIT_INPUT_MODE | WAIT_TIMER_MODE;
		waitEvent(-1);
	}

	draw_cursor_flag = false;

	// previously waitEvent was returning a result
	return false;
}

// "allowed" seems a far better name than the ambiguous "enabled"
bool ONScripter::skipIsAllowed() {
	if (!skip_enabled)
		return false;
	return skip_unread || !script_h.logState.unreadDialogue;
}

bool ONScripter::clickWait() {
	int tmp_skip = skip_mode;
	skip_mode &= ~(SKIP_TO_WAIT | SKIP_TO_EOL);
	internal_slowdown_counter = 0;

	flush(refreshMode());

	//Mion: apparently NScr doesn't call textgosub on clickwaits
	// while in skip mode (but does call it on pagewaits)
	// ^ We don't care what NScr does, its nonsense causes us bugs :D
	if (((skip_mode & (SKIP_NORMAL)) ||
	     ((tmp_skip & SKIP_TO_EOL) && clickskippage_flag) ||
	     keyState.ctrl) &&
	    !textgosub_label) {
		skip_mode      = tmp_skip;
		clickstr_state = CLICK_NONE;
		//if (textgosub_label && (script_h.getNext()[0] != 0x0a))
		//    new_line_skip_flag = true;
		event_mode = IDLE_EVENT_MODE;
		waitEvent(0);
	} else {

		keyState.pressedFlag = false;

		if (textgosub_label) {
			if ((tmp_skip & SKIP_TO_EOL) && clickskippage_flag)
				skip_mode = tmp_skip;
			saveoffCommand();
			clickstr_state = CLICK_NONE;

			const char *next = script_h.getNext();
			if (*next == 0x0a) {
				textgosub_clickstr_state = CLICK_WAITEOL;
			} else {
				new_line_skip_flag       = true;
				textgosub_clickstr_state = CLICK_WAIT;
			}
			if ((skip_mode & SKIP_NORMAL || keyState.ctrl) && skipgosub_label)
				gosubReal(skipgosub_label, next, true);
			else
				gosubReal(textgosub_label, next, true);

			return false;
		}

		clickstr_state = CLICK_WAIT;
		if (doClickEnd())
			return false;

		clickstr_state       = CLICK_NONE;
		keyState.pressedFlag = false;
	}

	return true;
}

bool ONScripter::clickNewPage() {
	skip_mode &= ~(SKIP_TO_WAIT | SKIP_TO_EOL);

	flush(refreshMode());
	clickstr_state = CLICK_NEWPAGE;

	bool skipping{skip_mode & SKIP_NORMAL || keyState.ctrl};

	if (skipping && !textgosub_label) {
		clickstr_state = CLICK_NONE;

		event_mode = IDLE_EVENT_MODE;
		waitEvent(0);
	} else {
		keyState.pressedFlag = false;

		if (textgosub_label) {
			saveoffCommand();
			clickstr_state = CLICK_NONE;

			const char *next         = script_h.getNext();
			textgosub_clickstr_state = CLICK_NEWPAGE;

			if (skipping && skipgosub_label)
				gosubReal(skipgosub_label, next, true);
			else
				gosubReal(textgosub_label, next, true);

			return false;
		}

		if (doClickEnd())
			return false;
	}

	newPage(true);
	clickstr_state       = CLICK_NONE;
	keyState.pressedFlag = false;

	return true;
}

int ONScripter::textCommand() {
	if (saveon_flag && internal_saveon_flag) {
		saveSaveFile(-1);
		internal_saveon_flag = false;
	}

	if (dlgCtrl.dialogueProcessingState.active) {

		script_h.popStringBuffer();

		if (pretextgosub_label && !dlgCtrl.dialogueProcessingState.pretextHasBeenToldToRunOnce) {
			// even in new model we want to handle pretext before allowing dialogueCommand / textCommand to complete, right?
			gosubReal(pretextgosub_label, dlgCtrl.dialogue_pos, true);
			dlgCtrl.dialogueProcessingState.pretextHasBeenToldToRunOnce = true;
			return RET_CONTINUE;
		}

		// feel like it's ok to let these two complete now instead of in CR, too
		if (!dlgCtrl.dialogueProcessingState.layoutDone) {
			dlgCtrl.layoutDialogue();
		}

		if (!page_enter_status) {
			refresh_window_text_mode = REFRESH_NORMAL_MODE | REFRESH_WINDOW_MODE | REFRESH_TEXT_MODE;
			enterTextDisplayMode();
			page_enter_status = 1;
		}

		dlgCtrl.dialogueProcessingState.readyToRun = true;

		//sendToLog(LogLevel::Info, "Start of dialogue dialogue event\n");
		dlgCtrl.events.emplace_get().firstCall = true;

		LabelInfo *label = current_label_info;
		if (!callStack.empty())
			label = callStack.front().label;
		auto id                                  = script_h.getLabelIndex(label);
		script_h.logState.currDialogueLabelIndex = id;
		script_h.logState.unreadDialogue         = !script_h.logState.readLabels[id];

	} else {
		errorAndExit("dlgCtrl is inactive but textCommand was called");
	}
	return RET_CONTINUE;
}

void ONScripter::displayDialogue() {
	if (skip_mode) {
		//sendToLog(LogLevel::Info, "skip-mode display dialogue event\n");
		dlgCtrl.events.emplace();
		for (const auto &a : fetchConstantRefreshActions<DialogueController::TextRenderingMonitorAction>()) {
			auto act = dynamic_cast<DialogueController::TextRenderingMonitorAction *>(a.get());
			act->lastCompletedSegment++;
		}
		return;
	}
	dlgCtrl.timeCurrentDialogueSegment();
	dlgCtrl.dialogueIsRendering = true;
	auto segAct                 = DialogueController::SegmentRenderingAction::create();
	segAct->segment             = dlgCtrl.dialogueRenderState.segmentIndex;
	Lock lock(&ons.registeredCRActions);
	registeredCRActions.emplace_back(segAct); // renders segment to completion
}

int ONScripter::getCharacterPreDisplayDelay(char16_t /*codepoint*/, int /*speed*/) {
	return 0;
}

int ONScripter::getCharacterPostDisplayDelay(char16_t codepoint, int speed) {
	int base = 20;
	uint32_t codepoint_u = codepoint;
	if (codepoint == u'⅓')
		base = 13; // special character indicating the delay for a terminating punctuation which will be followed by another
	else if (codepoint == ',')
		base = 100;
	else if (codepoint == ';' || codepoint == ':' || codepoint == u'—')
		base = 145;
	else if (codepoint == '.' || codepoint == '?' || codepoint == '!')
		base = 170;
	else if (codepoint_u >= CnBegin && codepoint_u <= CnEnd)
		base = 60;
	else if (codepoint_u >= JpBegin && codepoint_u <= JpEnd)
		base = 60;
	return base - (base * speed) / 10;
}

int ONScripter::unpackInlineCall(const char *cmd, int &val) {
	assert(cmd[0] == '!');

	std::string num;
	for (int i = 0; cmd[2 + i] >= '0' && cmd[2 + i] <= '9'; i++) num += cmd[2 + i];
	val = std::stoi(num);

	switch (cmd[1]) {
		case 'w':
			return 0;
		case 'd':
			return 1;
		default:
			ons.errorAndExit("This command cannot not be executed from here"); // for !s and friends
			return -1;                                                         //dummy
	}
}

int ONScripter::executeSingleCommandFromTreeNode(StringTree &command_node) {

	int res = RET_NO_READ;

	std::string &cmd = command_node[0].value;

	for (int i = 1; command_node.has(i); i++) {
		variableQueue.push(command_node.getById(i).value);
	}

	if (cmd.length() >= sizeof(script_h.current_cmd)) {
		errorAndExit("command buffer overflow");
	}

	if (isBuiltInCommand(cmd.c_str())) {
		setVariableQueue(true, cmd);
		// We need to backup & restore SH Data here (following command may kill string_buffer)
		ScriptHandler::ScriptLoanStorable storable{script_h.getScriptStateData()};
		evaluateBuiltInCommand(cmd.c_str());
		script_h.swapScriptStateData(storable);
		setVariableQueue(false);
	} else {
		inVariableQueueSubroutine = true;

		// The caller of tree_exec function should give us proper reexecution position (its start point)
		assert(currentCommandPosition.has());
		script_h.setCurrent(ons.currentCommandPosition.get());

		res = ScriptParser::evaluateCommand(cmd.c_str(), false);
	}

	return res;
}

const char *ONScripter::getFontPath(int i, bool /*fallback*/) {
	const char *path = sentence_font.getFontPath(i);
	if (!path)
		path = sentence_font.getFontPath(0);
	return path;
}

void ONScripter::addTextWindowClip(DirtyRect &rect) {
	if (wndCtrl.usingDynamicTextWindow) {
		// This represents the whole text window, when it is current and active
		// dlgCtrl.dialogueProcessingState.layoutDone == false means we are in pretext
		// pretext is better to think that we are still using a previous window (which is text_gpu)
		// this will avoid possible glitches if it tries to do anything with it
		if (dlgCtrl.dialogueProcessingState.active && dlgCtrl.dialogueProcessingState.layoutDone) {
			auto blits = wndCtrl.getRegions();
			for (auto &b : blits) rect.add(b.dst);
			// At this step we only have text_gpu & window_gpu left, it is guaranteed that text window is no bigger
		} else {
			rect.add({0, 0, static_cast<float>(text_gpu->w), static_cast<float>(text_gpu->h)});
		}
	} else {
		rect.add(sentence_font_info.pos);
	}
}
