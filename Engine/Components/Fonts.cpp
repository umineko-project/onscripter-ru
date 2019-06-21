/**
 *  Fonts.cpp
 *  ONScripter-RU
 *
 *  Low level font control code based on freetype.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Components/Fonts.hpp"
#include "Engine/Readers/Base.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Support/FileIO.hpp"

#include <sstream>

FontsController fonts;

bool FontsController::loadFont(Font &f, size_t i, bool user) {
	char *dir = user ? userfontdir : fontdir;
	if (dir[0] == '\0') {
		char *tmp = (*reader)->completePath(user ? "fonts/usr" : "fonts", FileType::Directory);
		if (!tmp) {
			// Font directory must exist, we no longer support root as a font directory.
			return false;
		}
		copystr(dir, tmp, sizeof(user ? userfontdir : fontdir));
		FileIO::terminatePath(dir, sizeof(user ? userfontdir : fontdir));
		freearr(&tmp);
	}

	const char *extensions[]{".ttf", ".otf"};
	bool found{false};
	size_t size{0};

	for (auto &ext : extensions) {
		char tmp[32]{};

		if (i == 0)
			std::snprintf(tmp, sizeof(tmp), "default%s", ext);
		else
			std::snprintf(tmp, sizeof(tmp), "font%zu%s", i, ext);

		if (FileIO::accessFile(tmp, dir, FileType::File, &size)) {
			size_t len = std::strlen(dir) + std::strlen(tmp) + 1;
			f.path = std::make_unique<char[]>(len);
			std::snprintf(f.path.get(), len, "%s%s", dir, tmp);
			found  = true;
		}
	}

	if (!found)
		return false;

	FILE *fp = FileIO::openFile(f.path.get(), "rb");

	if (!fp)
		return false;

	auto stream                = new FT_StreamRec{};
	stream->descriptor.pointer = fp;
	stream->size               = size;
	stream->read               = [](FT_Stream stream, unsigned long offset, unsigned char *buffer, unsigned long count) -> unsigned long {
		auto fp = static_cast<FILE *>(stream->descriptor.pointer);
		FileIO::seekFile(fp, offset, SEEK_SET);
		return std::fread(buffer, sizeof(uint8_t), count, fp);
	};
	stream->close = [](FT_Stream stream) -> void {
		std::fclose(static_cast<FILE *>(stream->descriptor.pointer));
		delete stream;
	};

	FT_Open_Args args{};
	args.flags  = FT_OPEN_STREAM;
	args.stream = stream;

	if (FT_Open_Face(freetype, &args, 0, &f.normal_face))
		return false;

	//Set normal_face as current face
	f.face = f.normal_face;

	//Time to load other typefaces
	FT_Long num = f.normal_face->num_faces;
	//sendToLog(LogLevel::Info, "Font %d with path %s has %d typefaces\n", i, fonts[i].path, num);

	if (num > 1) {
		//Start from 1, 0 is default
		for (FT_Long t = 1; t < num; t++) {
			FT_Face tmp_font_face;
			if (FT_New_Face(freetype, f.path.get(), t, &tmp_font_face)) {
				//sendToLog(LogLevel::Error, "Error at loading typface %d\n", t);
				FT_Done_Face(tmp_font_face);
				continue;
			}

			if (tmp_font_face->style_flags == (FT_STYLE_FLAG_ITALIC | FT_STYLE_FLAG_BOLD) && !f.hasInternalBoldItalicFace) {
				f.bold_italic_face          = tmp_font_face;
				f.hasInternalBoldItalicFace = true;
				//sendToLog(LogLevel::Info, "bold_italic_face loaded\n");
				continue;
			} else if (tmp_font_face->style_flags == FT_STYLE_FLAG_BOLD && !f.hasInternalBoldFace) {
				f.bold_face           = tmp_font_face;
				f.hasInternalBoldFace = true;
				//sendToLog(LogLevel::Info, "bold_face loaded\n");
				continue;
			} else if (tmp_font_face->style_flags == FT_STYLE_FLAG_ITALIC && !f.hasInternalItalicFace) {
				f.italic_face           = tmp_font_face;
				f.hasInternalItalicFace = true;
				//sendToLog(LogLevel::Info, "italic_face loaded\n");
				continue;
			}
			//Something unrelated
			FT_Done_Face(tmp_font_face);
		}
	}

	f.loaded = true;

	return true;
}

int FontsController::ownInit() {
	auto it = ons.ons_cfg_options.find("font-overrides");
	if (it != ons.ons_cfg_options.end())
		initFontOverrides(it->second);
	it = ons.ons_cfg_options.find("font-multiplier");
	if (it != ons.ons_cfg_options.end())
		initFontMultiplier(it->second);

	glyphStorageOptimisation = baseSizeMultipliers.empty() && presetSizeMultipliers.empty();

	//sendToLog(LogLevel::Info, "Initialising font system.\n");
	if (FT_Init_FreeType(&freetype))
		return -1;

	fonts_number = 0;

	for (size_t i = 0; i < 10; i++, fonts_number++) {
		if (!loadFont(fonts[i], i, false)) {
			if (i == 0)
				return -1; // default font must be loaded
			break;
		}

		//sendToLog(LogLevel::Info, "Path %i: %s\n", i, fonts[i].path);
	}

	for (size_t i = 0; i < 10; i++, user_fonts_number++) {
		if (!loadFont(user_fonts[i], i, true)) {
			break;
		}

		//sendToLog(LogLevel::Info, "Path %i: %s\n", i, fonts[i].path);
	}

	// Now need to take a basedir from default.ttf and use it as a font dir
	std::sprintf(fontdir, "%s", fonts[0].path.get());
	*(std::strrchr(fontdir, DELIMITER) + 1) = '\0';

	return 0;
}

void FontsController::initFontOverrides(const std::string &o) {
	std::stringstream overrides(o);

	while (static_cast<void>(overrides.peek()), !overrides.eof()) {
		bool base{false};
		unsigned int src_id{0};
		unsigned int dst_id{0};
		unsigned int preset_id{0};

		if (overrides.get() == 'b') {
			base = true;
		}
		char next = overrides.peek();

		if (!base && next >= '0' && next <= '9') {
			overrides >> preset_id;
			if (overrides.get() == ':') {
				next = overrides.peek();
			} else {
				// error, invalid sequence
				return;
			}
		}

		// get src_id
		if (next >= '0' && next <= '9') {
			overrides >> src_id;
			next = overrides.get();
			if (src_id > 9)
				return; // error, invalid id
		} else {
			// error, invalid sequence
			return;
		}
		if (next == ':') {
			next = overrides.peek();
		} else {
			// error, invalid sequence
			return;
		}
		// get dst_id
		if (next >= '0' && next <= '9') {
			overrides >> dst_id;
			overrides.get();
			if (dst_id > 9)
				return; // error, invalid id
		} else {
			// error, invalid sequence
			return;
		}

		if (base) {
			baseFontOverrides[src_id] = dst_id;
		} else {
			presetFontOverrides[preset_id][src_id] = dst_id;
		}
	}
}
void FontsController::initFontMultiplier(const std::string &m) {
	std::stringstream multipliers(m);

	while (static_cast<void>(multipliers.peek()), !multipliers.eof()) {
		bool base{false};
		unsigned int src_id{0};
		float mult{0};
		unsigned int preset_id{0};

		if (multipliers.get() == 'b') {
			base = true;
		}
		char next = multipliers.peek();

		if (!base && next >= '0' && next <= '9') {
			multipliers >> preset_id;
			if (multipliers.get() == ':') {
				next = multipliers.peek();
			} else {
				// error, invalid sequence
				return;
			}
		}

		// get src_id
		if (next >= '0' && next <= '9') {
			multipliers >> src_id;
			next = multipliers.get();
			if (src_id > 9)
				return; // error, invalid id
		} else {
			// error, invalid sequence
			return;
		}
		if (next == ':') {
			next = multipliers.peek();
		} else {
			// error, invalid sequence
			return;
		}
		// get mult
		if (next >= '0' && next <= '9') {
			multipliers >> mult;
			multipliers.get();
			if (mult <= 0 || mult > 10)
				return; // error, invalid multiplier
		} else {
			// error, invalid sequence
			return;
		}

		if (base) {
			baseSizeMultipliers[src_id] = mult;
		} else {
			presetSizeMultipliers[preset_id][src_id] = mult;
		}
	}
}

int FontsController::ownDeinit() {
	if (fonts_number > 0)
		FT_Done_FreeType(freetype);
	return 0;
}

Font &FontsController::getFont(unsigned int id, int preset_id) {
	if (preset_id >= 0) {
		auto m = presetFontOverrides.find(preset_id);
		if (m != presetFontOverrides.end()) {
			auto f = m->second.find(id);
			if (f != m->second.end() && f->second <= user_fonts_number) {
				return user_fonts[f->second];
			}
		}

	} else {
		auto f = baseFontOverrides.find(id);
		if (f != baseFontOverrides.end() && f->second <= user_fonts_number) {
			return user_fonts[f->second];
		}
	}
	return fonts[id];
}

float FontsController::getMultiplier(unsigned int id, int preset_id) {
	if (preset_id >= 0) {
		auto m = presetSizeMultipliers.find(preset_id);
		if (m != presetSizeMultipliers.end()) {
			auto f = m->second.find(id);
			if (f != m->second.end()) {
				return f->second;
			}
		}
	} else {
		auto f = baseSizeMultipliers.find(id);
		if (f != baseSizeMultipliers.end()) {
			return f->second;
		}
	}

	return 1.0;
}
