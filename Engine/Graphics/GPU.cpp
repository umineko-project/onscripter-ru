/**
 *  GPU.cpp
 *  ONScripter-RU
 *
 *  Contains higher level GPU abstraction.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Graphics/GPU.hpp"
#include "Engine/Graphics/Common.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Window.hpp"
#include "Resources/Support/Resources.hpp"
#include "Resources/Support/Version.hpp"

#include <stdexcept>
#include <string>
#include <array>

GPUController gpu;

int GPUController::ownInit() {
	GPU_InitFlagEnum gpuFlags = GPU_DEFAULT_INIT_FLAGS;

#ifdef WIN32
	int swap_interval{1};
	if (ons.ons_cfg_options.count("try-late-swap") > 0)
		swap_interval = -1;
#else
	int swap_interval{-1};
	if (ons.ons_cfg_options.count("force-vsync") > 0)
		swap_interval = 1;
#endif

	if (swap_interval == 1)
		gpuFlags |= GPU_INIT_ENABLE_VSYNC;
	auto it = ons.ons_cfg_options.find("texture-upload");
	if (it != ons.ons_cfg_options.end() && it->second == "ramcopy")
		gpuFlags |= GPU_INIT_USE_COPY_TEXTURE_UPLOAD_FALLBACK;
	else
		gpuFlags |= GPU_INIT_USE_ROW_BY_ROW_TEXTURE_UPLOAD_FALLBACK;
	GPU_SetPreInitFlags(gpuFlags);

	globalImagePool.init();
	canvasImagePool.size = SDL_Point{window.canvas_width, window.canvas_height};
	scriptImagePool.size = SDL_Point{window.script_width, window.script_height};

	return 0;
}

GPU_Target *GPUController::rendererInitWithInfo(GPURendererInfo &info, uint16_t w, uint16_t h, GPU_WindowFlagEnum SDL_flags) {
	current_renderer = &info;
	sendToLog(LogLevel::Info, "Trying to initialise %s renderer\n", current_renderer->name);

	auto renderer_id = (this->*current_renderer->makeRendererId)();
	auto screen      = GPU_InitRendererByID(renderer_id, w, h, SDL_flags);

	if (screen) {
		// Set renderer-specific flags if any
		(this->*current_renderer->initRendererFlags)();

		// This fixes some blur on Windows (at least on SDL2)
		GPU_SetWindowResolution(w, h);

		createShadersFromResources();

		max_texture = (this->*current_renderer->getMaxTextureSize)();

		auto it = ons.ons_cfg_options.find("texlimit");
		if (it != ons.ons_cfg_options.end())
			max_texture = std::stoi(it->second);

		sendToLog(LogLevel::Info, "Maximum texture size (%s) is %d\n",
		          (it == ons.ons_cfg_options.end() ? "set automatically" : "provided by user"),
		          max_texture);

		it = ons.ons_cfg_options.find("chunklimit");
		if (it != ons.ons_cfg_options.end())
			max_chunk = std::stoi(it->second);

		sendToLog(LogLevel::Info, "Maximum texture chunk size (%s) is %d\n",
		          (it == ons.ons_cfg_options.end() ? "set automatically" : "provided by user"),
		          max_chunk);

		if (w != window.script_width || h != window.script_height)
			GPU_SetVirtualResolution(screen, window.script_width, window.script_height);
		window.setMainTarget(screen);

		//TODO: put this to some better place
		canvasImagePool.addImages(2);
		scriptImagePool.addImages(1);

		return screen;
	}

	current_renderer = nullptr;
	return nullptr;
}

GPU_Target *GPUController::rendererInit(GPU_WindowFlagEnum SDL_flags) {
	GPU_SetDebugLevel(GPU_DEBUG_LEVEL_MAX);

	auto it = ons.ons_cfg_options.find("render-self");
	if (it != ons.ons_cfg_options.end()) {
		if (it->second == "yes")
			render_to_self = 1;
		else if (it->second == "no")
			render_to_self = 0;
	}

	std::string blacklisted, preferred;
	it = ons.ons_cfg_options.find("renderer-blacklist");
	if (it != ons.ons_cfg_options.end())
		blacklisted = it->second;

	it = ons.ons_cfg_options.find("prefer-renderer");
	if (it != ons.ons_cfg_options.end())
		preferred = it->second;

	size_t rendererPasses = 1 + !preferred.empty();

	int w = 0, h = 0;
	window.getWindowSize(w, h);

	for (size_t i = 0; i < rendererPasses; i++) {
		for (auto &renderer : renderers) {
			if (blacklisted.find(renderer.name) != std::string::npos) {
				sendToLog(LogLevel::Info, "Skipping blacklisted %s renderer\n", renderer.name);
				continue;
			}

			if (!preferred.empty() && preferred != renderer.name)
				continue;

			auto screen = rendererInitWithInfo(renderer, w, h, SDL_flags);
			if (screen)
				return screen;
		}

		if (rendererPasses > 1) {
			std::string msg = "Cannot use preferred renderer " + preferred + "! Will try other available renderers now.";
			sendToLog(LogLevel::Warn, "%s\n", msg.c_str());
			window.showSimpleMessageBox(SDL_MESSAGEBOX_WARNING, VERSION_STR1, msg.c_str());
			preferred.clear();
		}
	}

	std::snprintf(ons.script_h.errbuf, MAX_ERRBUF_LEN, "Couldn't init OpenGL with %dx%d resolution", w, h);
	ons.errorAndExit(ons.script_h.errbuf, SDL_GetError(), "Init Error", true);

	return nullptr;
}

int GPUController::ownDeinit() {
	globalImagePool.clear();
	return 0;
}

void GPUController::setVirtualResolution(unsigned int width, unsigned int height) {
	auto &context               = GPU_GetCurrentRenderer()->current_context_target;
	unsigned int current_width  = context->w;
	unsigned int current_height = context->h;

	if (current_width != width || current_height != height)
		GPU_SetVirtualResolution(ons.screen_target, width, height);
}

void GPUController::pushBlendMode(BlendModeId mode) {
	blend_mode.push(mode);
}

void GPUController::popBlendMode() {
	if (blend_mode.empty())
		throw std::runtime_error("cannot pop blend_mode");
	else if (blend_mode.size() == 1)
		sendToLog(LogLevel::Warn, "popping last blend_mode, you were warned\n");

	blend_mode.pop();
}

void GPUController::setBlendMode(GPU_Image *image) {
#if 0
	// We perform these checks earlier
	if (!image)
		throw std::runtime_error("cannot set blend_mode for null image");
	
	if (blend_mode.empty())
		throw std::runtime_error("cannot get anything from blend_mode");
#endif

	// This is not safe but using and catching an exception seems to cause performance penalties on droid.
	image->blend_mode = BLEND_MODES[static_cast<size_t>(blend_mode.top())];
}

void GPUController::createShadersFromResources() {
	// Create default vertex shader
	createShader("defaultVertex.vert");
	if (shaders["defaultVertex.vert"] == 0) {
		sendToLog(LogLevel::Error, "Default vertex shader compilation failed.\nError follows\n");
		sendToLog(LogLevel::Error, "----------------------------------------\n");
		sendToLog(LogLevel::Error, "%s\n", GPU_GetShaderMessage());
		sendToLog(LogLevel::Error, "----------------------------------------\n");
		ons.errorAndExit("No default vertex shader!");
	}
	for (auto r = getResourceList(); r->buffer; ++r) {
		createShader(r->filename);
	}
	for (auto r = getResourceList(); r->buffer; ++r) {
		try {
			if (shaders.count(r->filename) > 0 && getShaderTypeByExtension(r->filename) == GPU_FRAGMENT_SHADER &&
			    isStandaloneShader(reinterpret_cast<const char *>(r->buffer))) {
				std::vector<uint32_t> linksWith = findAllLinkTargets(reinterpret_cast<const char *>(r->buffer), r->size);
				if (linksWith.empty()) {
					createProgramFromShaders(r->filename, r->filename, "defaultVertex.vert"); // Also make a default program that we can use, with the shader filename
				} else {
					linksWith.push_back(shaders.at("defaultVertex.vert"));
					linksWith.push_back(shaders.at(r->filename));
					createProgramFromShaders(r->filename, linksWith);
				}
			}
		} catch (std::invalid_argument &) {
			continue; // Only link fragment shaders
		}
	}
}

bool GPUController::isStandaloneShader(const char *text) {
	// We use this notation instead of normal #pragma because ANGLE 43 and 44 boil with an error on them :/
	return !std::strstr(text, "//PRAGMA: ONS_RU not_standalone");
}

std::vector<uint32_t> GPUController::findAllLinkTargets(const char *text, size_t len) {
	size_t pragmaLen = std::strlen("//PRAGMA: ONS_RU import ");
	std::vector<uint32_t> targets;
	while (true) {
		const char *pos = std::strstr(text, "//PRAGMA: ONS_RU import ");
		if (!pos)
			break;
		pos += pragmaLen;
		if (pos >= text + len)
			break;
		const char *end = pos;
		while (*end != ' ' && *end != '\n')
			end++;
		text = end;

		try {
			targets.push_back(shaders.at(std::string(pos, end - pos)));
		} catch (const std::out_of_range &) {
			ons.errorAndExit("Trying to import a non-existent shader");
		}
	}
	return targets;
}

void GPUController::createShader(const char *filename) {
	// Check to see if it's already created; if so do nothing
	if (gpu.shaders.count(std::string(filename)) > 0) {
		return;
	}

	// Make the shader of appropriate type according to file extension (does not read from file yet)
	GPU_ShaderEnum shaderType;
	uint32_t shader;
	SDL_RWops *shaderData;
	try {
		shaderType = gpu.getShaderTypeByExtension(filename);
	} catch (std::invalid_argument &) {
		return; // we can't figure out what shader type it is, so we can't make the shader.
	}

	sendToLog(LogLevel::Info, "Compiling shader: %s\n", filename);

	// Now we need to find our file. Look for the shader code in binary resources.
	const InternalResource *r = getResource(filename, current_renderer->mobile);

	if (r && r->size != 0) {
		shaderData = SDL_RWFromConstMem(static_cast<const void *>(r->buffer), static_cast<int>(r->size));
		shader     = GPU_CompileShader_RW(shaderType, shaderData, false);
		shaderData->close(shaderData);
	} else { // We couldn't find it in the binary, better try searching external paths
		shaderData = SDL_RWFromFile(filename, "rb");
		if (!shaderData)
			return;
		shader = GPU_CompileShader_RW(shaderType, shaderData, false);
		shaderData->close(shaderData);
	}

	// Let's just check that the shader compiled properly.
	if (shader == 0) {
		sendToLog(LogLevel::Error, "Shader compilation failed. Error follows\n");
		sendToLog(LogLevel::Error, "----------------------------------------\n");
		sendToLog(LogLevel::Error, "%s\n", GPU_GetShaderMessage());
		sendToLog(LogLevel::Error, "----------------------------------------\n");
		return;
	}
	// OK, found and compiled the shader. Add it to the shaders list!
	gpu.shaders[filename] = shader;
}

void GPUController::createProgramFromShaders(const char *programAlias, const char *frag, const char *vert) {
	uint32_t p = GPU_LinkShaders(gpu.shaders[frag], gpu.shaders[vert]);
	linkProgram(programAlias, p);
}

void GPUController::createProgramFromShaders(const char *programAlias, std::vector<uint32_t> &targets) {
	uint32_t p = GPU_LinkManyShaders(targets.data(), static_cast<int>(targets.size()));
	linkProgram(programAlias, p);
}

void GPUController::linkProgram(const char *programAlias, uint32_t prog) {
	if (prog == 0 || GPU_LinkShaderProgram(prog) == 0) {
		sendToLog(LogLevel::Error, "Shader linking failed. Error follows\n");
		sendToLog(LogLevel::Error, "----------------------------------------\n");
		sendToLog(LogLevel::Error, "%s\n", GPU_GetShaderMessage());
		sendToLog(LogLevel::Error, "----------------------------------------\n");
		return;
	}
	gpu.programs[programAlias] = prog;
}

GPU_ShaderEnum GPUController::getShaderTypeByExtension(const char *filename) {
	std::string myString(filename);
	size_t pos     = myString.find_last_of('.');
	auto extension = myString.substr(pos + 1);

	if (extension == "frag")
		return GPU_FRAGMENT_SHADER;
	if (extension == "vert")
		return GPU_VERTEX_SHADER;
	throw std::invalid_argument("Not a shader");
}

void GPUController::bindImageToSlot(GPU_Image *image, int slot_number) {
	int32_t texLoc;
	switch (slot_number) {
		case 0: texLoc = getUniformLoc("tex"); break;
		case 1: texLoc = getUniformLoc("tex1"); break;
		case 2: texLoc = getUniformLoc("tex2"); break;
		case 3: texLoc = getUniformLoc("tex3"); break;
		default: return;
	}
	GPU_SetShaderImage(image, texLoc, slot_number);
}

void GPUController::enter3dMode() {
	GPU_MatrixMode(0, GPU_MODEL);
	GPU_PushMatrix();
	GPU_LoadIdentity();
	GPU_MatrixMode(0, GPU_PROJECTION);
	GPU_PushMatrix();
	GPU_LoadIdentity();

	// l/r, t/b params are so to mirror the picture properly
	// near/far params behave differently to glFrustum but seem to somehow work
	// default camera z position is -10
	// Starting with SDL-gpu r707 the default dims are -100~100 instead of -1~1:
	// https://github.com/grimfang4/sdl-gpu/commit/4bd208f016316f2a17c65be456f5715ffb2ff320
	GPU_Frustum(100, -100, 100, -100, -1, 0);
}

void GPUController::exit3dMode() {
	GPU_MatrixMode(0, GPU_MODEL);
	GPU_PopMatrix();
	GPU_MatrixMode(0, GPU_PROJECTION);
	GPU_PopMatrix();
}

void GPUController::setShaderProgram(const char *programAlias) {
	/* Flush before setting */
	GPU_FlushBlitBuffer();

	auto p = programs.find(std::string(programAlias));
	if (p == programs.end()) {
		sendToLog(LogLevel::Error, "Shader program '%s' not found. Using fixed pipeline.\n", programAlias);
		unsetShaderProgram(); // No shader found.
		return;
	}

	currentProgram              = p->second;
	GPU_ShaderBlock shaderBlock = GPU_LoadShaderBlock(currentProgram, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
	GPU_ActivateShaderProgram(currentProgram, &shaderBlock);
	//sendToLog(LogLevel::Info, "current_program %d %d\n",currentProgram,GPU_GetContextTarget()->context->current_shader_program);
	//sendToLog(LogLevel::Info, "shaderBlock %d %d %d %d\n",shaderBlock.color_loc,shaderBlock.position_loc,shaderBlock.texcoord_loc,shaderBlock.modelViewProjection_loc);
}

void GPUController::unsetShaderProgram() {
	/* Flush before unsetting */
	//GPU_FlushBlitBuffer();
	currentProgram = 0;
	GPU_DeactivateShaderProgram();
	//glFlush(); // if we still hit problems with uncommitted shader state, glFinish() is also an option
}

int32_t GPUController::getUniformLoc(const char *name) {
	auto currShader = uniformLocations.find(currentProgram);
	if (currShader != uniformLocations.end()) {
		auto currVar = currShader->second.find(name);
		if (currVar != currShader->second.end()) {
			return currVar->second;
		}
	}
	return uniformLocations[currentProgram][name] = GPU_GetUniformLocation(currentProgram, name);
}

void GPUController::setShaderVar(const char *name, int value) {
	GPU_SetUniformi(getUniformLoc(name), value);
}

void GPUController::setShaderVar(const char *name, float value) {
	GPU_SetUniformf(getUniformLoc(name), value);
}

void GPUController::setShaderVar(const char *name, float value1, float value2) {
	float values[2]{value1, value2};
	GPU_SetUniformfv(getUniformLoc(name), 2, 1, values);
}

void GPUController::setShaderVar(const char *name, const SDL_Color &color) {
	float colour[4]{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
	GPU_SetUniformfv(getUniformLoc(name), 4, 1, colour);
}

void GPUController::multiplyAlpha(GPU_Image *image, GPU_Rect *dst_clip) {
	if (image->format == GPU_FORMAT_RGB)
		return;

	GPU_GetTarget(image);

	if (render_to_self) {
		setShaderProgram("multiplyAlpha.frag");
		GPU_SetBlending(image, false);
		gpu.copyGPUImage(image, nullptr, dst_clip, image->target);
		unsetShaderProgram();
		GPU_SetBlending(image, true);
	} else {
		auto tmp = gpu.createImage(image->w, image->h, 4);
		GPU_GetTarget(tmp);

		setShaderProgram("multiplyAlpha.frag");
		GPU_SetBlending(image, false);
		GPU_SetBlending(tmp, false);
		gpu.copyGPUImage(image, nullptr, nullptr, tmp->target);
		unsetShaderProgram();
		gpu.copyGPUImage(tmp, nullptr, dst_clip, image->target);
		GPU_SetBlending(tmp, true);
		GPU_SetBlending(image, true);

		gpu.freeImage(tmp);
	}
}

void GPUController::mergeAlpha(GPU_Image *image, GPU_Rect *imageRect, GPU_Image *mask, GPU_Rect *maskRect, SDL_Surface *src) {
	GPU_GetTarget(image);
	GPU_GetTarget(mask);

	GPU_Image *tmp{nullptr};
	if (render_to_self) {
		tmp = image;
	} else {
		tmp = gpu.createImage(image->w, image->h, 4);
		GPU_GetTarget(tmp);
	}

	gpu.updateImage(tmp, nullptr, src, imageRect, true);
	//Optimisation: set to true if causes bugs
	gpu.updateImage(mask, nullptr, src, maskRect, false);

	gpu.setShaderProgram("mergeAlpha.frag");
	gpu.bindImageToSlot(tmp, 0);
	gpu.bindImageToSlot(mask, 1);

	GPU_SetBlending(tmp, false);
	gpu.copyGPUImage(tmp, nullptr, nullptr, image->target);
	unsetShaderProgram();
	GPU_SetBlending(tmp, true);

	if (!render_to_self)
		gpu.freeImage(tmp);
}

void GPUController::copyGPUImage(GPU_Image *img, GPU_Rect *src_rect, GPU_Rect *clip_rect, GPU_Target *target, float x, float y, float ratio_x, float ratio_y, float angle, bool centre_coordinates) {
	//printClock("copyGPUImage");

	if (!target) {
		ons.errorAndExit("copyGPUImage has null target");
		return; //dummy
	}

	if (window.getFullscreenFix() && target == ons.screen_target) {
		// Ignore this flush, screen_target is not allowed to be modified during window mode change
		return;
	}

	if (clip_rect && (clip_rect->w == 0 || clip_rect->h == 0))
		return;

#ifdef IOS
	// On iOS performing an intermediate image copy does not guarantee maintaing the same quality
	// This particularly affects accumulation_gpu -> effect_dst_gpu and vice versa
	// It might be a little inefficient but safe to assume that copying an image to an intermediate image must be performed with NEAREST
	bool directCopy = !clip_rect && !src_rect && x == 0 && y == 0 && img->w == target->w && img->h == target->h && ratio_x == 1 && ratio_y == 1 && angle == 0;
	if (directCopy)
		GPU_SetImageFilter(img, GPU_FILTER_NEAREST);
#endif

	if (!centre_coordinates) {
		float dx, dy;

		if (src_rect != nullptr) {
			dx = src_rect->w;
			dy = src_rect->h;
		} else {
			dx = img->w;
			dy = img->h;
		}

		x += dx / 2.0;
		y += dy / 2.0;
	}

	if (target == ons.screen_target)
		window.translateRendering(x, y, clip_rect);

	if (clip_rect && !(target->use_clip_rect && target->clip_rect.x == clip_rect->x && target->clip_rect.y == clip_rect->y && target->clip_rect.w == clip_rect->w && target->clip_rect.h == clip_rect->h)) {
		GPU_SetClipRect(target, *clip_rect);
	}
	if (!clip_rect && target->use_clip_rect) {
		GPU_UnsetClip(target);
	}

	setBlendMode(img);

	if ((ratio_x != 1 || ratio_y != 1) && angle != 0) {
		GPU_BlitTransform(img, src_rect, target, x, y, angle, ratio_x, ratio_y);
	} else if (ratio_x != 1 || ratio_y != 1) {
		GPU_BlitScale(img, src_rect, target, x, y, ratio_x, ratio_y);
	} else if (angle != 0) {
		GPU_BlitRotate(img, src_rect, target, x, y, angle);
	} else {
		GPU_Blit(img, src_rect, target, x, y);
	}

#ifdef IOS
	if (directCopy)
		GPU_SetImageFilter(img, GPU_FILTER_LINEAR);
#endif

	// printClock("end copyGPUImage");
}

void GPUController::copyGPUImage(GPU_Image *img, GPU_Rect *src_rect, GPU_Rect *clip_rect, GPUBigImage *bigImage, float x, float y) {

	if (!bigImage) {
		ons.errorAndExit("copyGPUImage has null bigImage");
		return; //dummy
	}

	GPU_Rect dst_clip{0, 0, static_cast<float>(bigImage->w), static_cast<float>(bigImage->h)};
	if (clip_rect)
		doClipping(&dst_clip, clip_rect);

	auto images = bigImage->getImagesForArea(dst_clip);

	dst_clip.x = dst_clip.y = 0;

	float off_x = src_rect ? src_rect->w / 2.0 : img->w / 2.0;
	float off_y = src_rect ? src_rect->h / 2.0 : img->h / 2.0;

	for (auto &image : images) {
		// Blit time...
		GPU_Target *target = image.first->target;
		dst_clip.w         = image.second.w;
		dst_clip.h         = image.second.h;
		copyGPUImage(img, src_rect, nullptr, target, x - image.second.x - off_x, y - image.second.y - off_y);
	}
}

void GPUController::updateImage(GPU_Image *image, const GPU_Rect *image_rect, SDL_Surface *surface, const GPU_Rect *surface_rect, bool finish) {
	if (finish)
		(this->*current_renderer->syncRendererState)();
	GPU_UpdateImage(image, image_rect, surface, surface_rect);
}

void GPUController::convertNV12ToRGB(GPU_Image *image, GPU_Image **imgs, GPU_Rect &rect, uint8_t *planes[4], int *linesizes, bool masked) {

	// 2 planes: Y, UV

	GPU_Rect realPlane = rect;
	if (masked)
		realPlane.h *= 2;

	GPU_UpdateImageBytes(imgs[0], &realPlane, planes[0], linesizes[0]);

	realPlane.h /= 2;

	GPU_UpdateImageBytes(imgs[1], &realPlane, planes[1], linesizes[1]);

	setShaderProgram("colourConversion.frag");

	setShaderVar("conversionType", 0);
	setShaderVar("maskHeight", static_cast<int>(masked ? rect.h : 0));
	setShaderVar("dimensions", rect.w, rect.h);

	GPU_SetBlending(imgs[0], false);
	GPU_SetBlending(imgs[1], false);
	GPU_SetBlending(image, false);

	//bindImageToSlot(imgs[0], 0); redundant
	bindImageToSlot(imgs[1], 1);
	//bindImageToSlot(imgs[1], 2); redundant
	//bindImageToSlot(imgs[1], 3);

	copyGPUImage(imgs[0], nullptr, nullptr, image->target);
	unsetShaderProgram();

	GPU_SetBlending(imgs[0], true);
	GPU_SetBlending(imgs[1], true);
	GPU_SetBlending(image, true);
}

void GPUController::convertYUVToRGB(GPU_Image *image, GPU_Image **imgs, GPU_Rect &rect, uint8_t *planes[4], int *linesizes, bool masked) {

	// 3 planes: Y, U, V

	GPU_Rect realPlane = rect;
	if (masked)
		realPlane.h *= 2;

	GPU_UpdateImageBytes(imgs[0], &realPlane, planes[0], linesizes[0]);

	realPlane.h /= 2;

	GPU_UpdateImageBytes(imgs[1], &realPlane, planes[1], linesizes[1]);
	GPU_UpdateImageBytes(imgs[2], &realPlane, planes[2], linesizes[2]);

	setShaderProgram("colourConversion.frag");

	setShaderVar("conversionType", 1);
	setShaderVar("maskHeight", static_cast<int>(masked ? rect.h : 0));
	setShaderVar("dimensions", rect.w, rect.h);

	GPU_SetBlending(imgs[0], false);
	GPU_SetBlending(imgs[1], false);
	GPU_SetBlending(imgs[2], false);
	GPU_SetBlending(image, false);

	//bindImageToSlot(imgs[0], 0); redundant
	bindImageToSlot(imgs[1], 1);
	bindImageToSlot(imgs[2], 2);
	//bindImageToSlot(imgs[1], 3);

	copyGPUImage(imgs[0], nullptr, nullptr, image->target);

	unsetShaderProgram();

	GPU_SetBlending(imgs[0], true);
	GPU_SetBlending(imgs[1], true);
	GPU_SetBlending(imgs[2], true);
	GPU_SetBlending(image, true);
}

void GPUController::simulateRead(GPU_Image *image) {
	if (simulate_reads) {
		// Uncomment to see the actual bug.
		// Basically VMware drivers seem to apply various texture writes on read attempt, and sometimes fail to
		// detect them. As a result our atlas often misses glyphs, which appear in pairs and trinities overwriting
		// other ones and messing everything up.
		// A discovered workaround is to perform a blend to self, and this is what the code below does.

		/*
		static size_t simulated_reads;
		std::string str = "C:/atlas/atlas" + std::to_string(simulated_reads) + ".png";
		simulated_reads++;
		GPU_FlushBlitBuffer(); glFlush(); glFinish();
		GPU_SaveImage(image, str.c_str(), GPU_FILE_AUTO);
		*/

		if (ons.tmp_image) {
			if (ons.tmp_image->w == image->w && ons.tmp_image->h == image->h && ons.tmp_image->format == image->format) {
				gpu.clear(ons.tmp_image->target);
				GPU_SetBlending(image, false);
				copyGPUImage(image, nullptr, nullptr, ons.tmp_image->target);
				GPU_SetBlending(image, true);
			} else {
				gpu.freeImage(ons.tmp_image);
				ons.tmp_image = nullptr;
			}
		}

		if (!ons.tmp_image) {
			ons.tmp_image = gpu.copyImage(image);
			GPU_GetTarget(ons.tmp_image);
		}

		// This is the the actual line fixing things.
		// Intentionally not guarded with render_to_self (simulate-reads & no-render-self are incompatible).
		copyGPUImage(image, nullptr, nullptr, image->target);
		gpu.clear(image->target);
		GPU_SetBlending(ons.tmp_image, false);
		copyGPUImage(ons.tmp_image, nullptr, nullptr, image->target);
		GPU_SetBlending(ons.tmp_image, true);
	}
}

GPU_Image *GPUController::loadGPUImageByChunks(SDL_Surface *s, GPU_Rect *r) {
	auto &loader = ons.imageLoader = GPUImageChunkLoader();
	loader.src                     = s;
	loader.src_area                = r;

	auto w     = r ? r->w : s->w;
	auto h     = r ? r->h : s->h;
	auto bpp   = s->format->BytesPerPixel;
	loader.dst = gpu.createImage(w, h, bpp);

	auto pixels = max_chunk / bpp;

	if (GPUImageChunkLoader::MinimumChunkDim * w <= pixels)
		loader.chunkWidth = w;
	else
		loader.chunkWidth = pixels / h;

	loader.chunkHeight = pixels / w;

	auto mask          = GPUImageChunkLoader::MinimumChunkDim - 1;
	loader.chunkWidth  = (loader.chunkWidth + mask) & ~mask;
	loader.chunkHeight = (loader.chunkHeight + mask) & ~mask;

	GPU_GetTarget(loader.dst);
	bool finish{true};
	ons.preventExit(true);
	while (!loader.isLoaded) {
		loader.loadChunk(finish); // this is the wrong place for this, right? it doesn't work with the new cr model we want
		ons.event_mode = ONScripter::IDLE_EVENT_MODE;
		ons.waitEvent(0, true);
		finish = false;
	}
	ons.preventExit(false);
	return ons.imageLoader.dst;

	// This approach relies on mainThreadDowntimeProcessing to load the picture
	// The basic difference is what has a higher priority: fps or image loading speed
	/*ons.imageLoader = GPUImageChunkLoader();
	ons.imageLoader.src = s;
	ons.imageLoader.dst = gpu.createImage(s->w, s->h, s->format->BytesPerPixel);
	GPU_GetTarget(ons.imageLoader.dst);
	ons.imageLoader.isActive = true;
	while (!ons.imageLoader.isLoaded) {
		ons.waitEvent(0);
	}
	return ons.imageLoader.dst;*/
}

void GPUController::clearWholeTarget(GPU_Target *target, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	if (target->use_clip_rect && (target->clip_rect.x != 0 || target->clip_rect.y != 0 || target->clip_rect.w != target->w || target->clip_rect.h != target->h)) {
		GPU_UnsetClip(target);
	}
	gpu.clear(target, r, g, b, a);
}

// ***********************************
// **      GPUImageChunkLoader      **
// ***********************************

void GPUImageChunkLoader::loadChunk(bool finish) {
	if (isLoaded)
		return;

	float x_start = src_area ? src_area->x : 0;
	float y_start = src_area ? src_area->y : 0;
	float x_off   = chunkWidth * x;
	float y_off   = chunkHeight * y;

	int w = src_area ? src_area->w : src->w;
	int h = src_area ? src_area->h : src->h;

	GPU_Rect srcLoad{x_start + x_off, y_start + y_off, static_cast<float>(chunkWidth), static_cast<float>(chunkHeight)};
	int xovershoot = x_off + srcLoad.w - w;
	int yovershoot = y_off + srcLoad.h - h;
	if (xovershoot > 0)
		srcLoad.w -= xovershoot;
	if (yovershoot > 0)
		srcLoad.h -= yovershoot;

	GPU_Rect dstLoad{x_off, y_off, srcLoad.w, srcLoad.h};

	gpu.updateImage(dst, &dstLoad, src, &srcLoad, finish);

	x++;
	x_off += chunkWidth;
	if (x_off >= w) {
		x = 0;
		y++;
		y_off += chunkHeight;
	}
	if (y_off >= h)
		isLoaded = true;
}

// ***********************************
// **  GPUTransformableCanvasImage  **
// ***********************************

void GPUTransformableCanvasImage::setImage(GPU_Image *_canvas) {
	clearImage();
	image = _canvas;
}

void GPUTransformableCanvasImage::clearImage() {
	gpu.clearImage(this);
}

void GPUController::clearImage(GPUTransformableCanvasImage *im) {
	im->pooledDownscaledImages.clear();
	im->image = nullptr;
}

PooledGPUImage GPUController::getBlurredImage(GPUTransformableCanvasImage &im, int blurFactor) {
	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	if (!im.image) {
		sendToLog(LogLevel::Error, "No image to blur!\n");
		return newImage;
	}

	if (blurFactor == 0) {
		gpu.copyGPUImage(im.image, nullptr, nullptr, newImage.image->target);
		return newImage;
	}

	blurFactor *= 1.4f; // adjustment to bring more in line with ps3 blur strength

	SDL_Point size{window.canvas_width, window.canvas_height};

	float sizeDivisor = 2.0f;
	// for efficiency, and support of larger blur sizes, might want to dynamically change that divisor
	// based on the blurFactor, but "sewing together the borders" is a little hard so I will leave it for now

	size.x /= sizeDivisor;
	size.y /= sizeDivisor;

	GPU_Image *src = nullptr;
	if (!im.pooledDownscaledImages.count(size)) {
		im.pooledDownscaledImages.emplace(size, getPooledImage(size.x, size.y));
		src = im.pooledDownscaledImages.at(size).image;
		copyGPUImage(im.image, nullptr, nullptr, src->target, src->w / 2.0f, src->h / 2.0f, 1.0f / sizeDivisor, 1.0f / sizeDivisor, 0, true);
	} else {
		src = im.pooledDownscaledImages.at(size).image;
	}

	PooledGPUImage myImg  = getPooledImage(size.x, size.y);
	PooledGPUImage myImgH = getPooledImage(size.x, size.y);

	GPU_SetBlending(src, false);
	GPU_SetBlending(myImg.image, false);
	GPU_SetBlending(myImgH.image, false);

	copyGPUImage(src, nullptr, nullptr, myImg.image->target);

	setShaderProgram("blurH.frag");
	setShaderVar("sigma", blurFactor / 1000.0f);
	setShaderVar("blurSize", 1.0f / (myImg.image->w));
	copyGPUImage(myImg.image, nullptr, nullptr, myImgH.image->target);

	setShaderProgram("blurV.frag");
	setShaderVar("sigma", blurFactor / 1000.0f);
	setShaderVar("blurSize", 1.0f / (myImgH.image->h));
	copyGPUImage(myImgH.image, nullptr, nullptr, newImage.image->target, newImage.image->w / 2.0f, newImage.image->h / 2.0f, sizeDivisor, sizeDivisor, 0, true);
	unsetShaderProgram();

	GPU_SetBlending(src, true);
	GPU_SetBlending(myImg.image, true);
	GPU_SetBlending(myImgH.image, true);

	return newImage;
}

PooledGPUImage GPUController::getMaskedImage(GPUTransformableCanvasImage &im, GPU_Image *mask) {
	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	if (!im.image) {
		sendToLog(LogLevel::Error, "No image to mask!\n");
		return newImage;
	}

	if (!mask) {
		copyGPUImage(im.image, nullptr, nullptr, newImage.image->target);
		return newImage;
	}

	setShaderProgram("cropByMask.frag");
	bindImageToSlot(im.image, 0);
	bindImageToSlot(mask, 1);
	//GPU_SetImageFilter(mask, GPU_LINEAR);
	GPU_SetBlending(im.image, false);
	copyGPUImage(im.image, nullptr, nullptr, newImage.image->target, 0, 0);
	unsetShaderProgram();
	GPU_SetBlending(im.image, true);

	return newImage;
}

static void paramsToBreakupDirectionFlagset(const char *params, int &breakupDirectionFlagset) {
	if (params) {
		breakupDirectionFlagset = 0;
		if (params[0] == 'l')
			breakupDirectionFlagset |= BREAKUP_MODE_LOWER;
		if (params[1] == 'l')
			breakupDirectionFlagset |= BREAKUP_MODE_LEFT;
		if ((params[2] >= 'A') && (params[2] <= 'Z'))
			breakupDirectionFlagset |= BREAKUP_MODE_JUMBLE;
	}
}

void GPUController::breakUpImage(BreakupID id, GPU_Image *src, GPU_Rect *src_rect, GPU_Target *target,
                                 int breakupFactor,
                                 int breakupDirectionFlagset, const char *params, float dstX, float dstY) {
	if (!src) {
		sendToLog(LogLevel::Error, "No image to break up!\n");
		return;
	}

	if (breakupFactor == 0) {
		copyGPUImage(src, nullptr, nullptr, target, dstX, dstY);
		return;
	}

	paramsToBreakupDirectionFlagset(params, breakupDirectionFlagset);

	if (ons.breakupInitRequired(id)) {
		ons.initBreakup(id, src, src_rect);
		if (ons.new_breakup_implementation) {
			ons.breakupData[id].blitter.set(createTriangleBlitter(src, target)); // eh, do it out here...
		}
	}

	ONScripter::BreakupData &data = ons.breakupData[id];
	BreakupCell *myCells          = data.breakup_cells.data();
	if (ons.new_breakup_implementation) {
		bool largeImage = src_rect ? (src_rect->w >= window.script_width && src_rect->h >= window.script_height) :
		                             (src->w >= window.script_width && src->h >= window.script_height);
		if (!largeImage)
			setShaderProgram("alphaOutsideTextures.frag");
		TriangleBlitter &blitter = data.blitter.get();

		blitter.updateTargets(src, target);

		ons.oncePerBreakupEffectBreakupSetup(id, breakupDirectionFlagset, data.numCellsX, data.numCellsY); // will exit if nothing to do
		ons.effectBreakupNew(id, breakupFactor);
		drawUnbrokenBreakupRegions(id, dstX, dstY);

		for (int n = 0; n < data.numCellsX * data.numCellsY; ++n) {
			auto &cell = myCells[n];
			float x{cell.cell_x * static_cast<float>(data.cellFactor)};
			float y{cell.cell_y * static_cast<float>(data.cellFactor)};
			if (cell.diagonal > data.maxDiagonalToContainBrokenCells) {
				break;
			}
			if (cell.resizeFactor > 0) {
				blitter.useFewerTriangles(cell.resizeFactor < 0.15f);
				blitter.copyCircle(x, y, 12, x + cell.disp_x + dstX, y + cell.disp_y + dstY, cell.resizeFactor);
			}
		}
		blitter.finish();
		if (!largeImage)
			unsetShaderProgram();
	} else {
		ons.oncePerFrameBreakupSetup(id, breakupDirectionFlagset, data.numCellsX, data.numCellsY);
		ons.effectBreakupOld(id, breakupFactor);

		setShaderProgram("breakup.frag");
		bindImageToSlot(ons.breakup_cellforms_gpu, 1);
		bindImageToSlot(ons.breakup_cellform_index_grid, 2);
		setShaderVar("tilesX", data.wInCellsFloat);
		setShaderVar("tilesY", data.hInCellsFloat);
		setShaderVar("breakupCellforms", BREAKUP_CELLFORMS);
		//GPU_SetBlending(im.image,0); // commented; blending needs to remain enabled so blank space in circles doesn't overwrite other circles
		//std::fprintf(stderr, "Breakup state: ");
		for (int n = 0; n < data.numCellsX * data.numCellsY; ++n) {
			//int s = myCells[n].state;
			//if (s<300) std::fprintf(stderr, "%u,%u;", myCells[n].cell_x, myCells[n].cell_y);
			GPU_Rect rect{static_cast<float>(myCells[n].cell_x) * BREAKUP_CELLWIDTH, static_cast<float>(myCells[n].cell_y) * BREAKUP_CELLWIDTH, BREAKUP_CELLWIDTH, BREAKUP_CELLWIDTH};
			if (myCells[n].radius > 0)
				copyGPUImage(src, &rect, nullptr, target,
				             rect.x + myCells[n].disp_x,
				             rect.y + myCells[n].disp_y);
		}
		//std::fprintf(stderr, "end\n");
		unsetShaderProgram();
		//GPU_SetBlending(im.image,1); // commented; blending needs to remain enabled so blank space in circles doesn't overwrite other circles
	}
}

void GPUController::drawUnbrokenBreakupRegions(BreakupID id, float dstX, float dstY) {
	ONScripter::BreakupData &data = ons.breakupData[id];
	BreakupCell *myCells          = data.breakup_cells.data();

	int numCellsX        = data.numCellsX;
	int numCellsY        = data.numCellsY;
	int maxX             = numCellsX - 1;
	int maxDiagonalIndex = numCellsX + numCellsY - 2;

	float f = static_cast<float>(data.cellFactor);

	BreakupCell **diagonals = data.diagonals.data();
	//BreakupCell &firstCell = myCells[0]; // first to disappear (last to appear)
	BreakupCell &lastCell = myCells[numCellsX * numCellsY - 1]; // first to appear (last to disappear)

	if (data.maxDiagonalToContainBrokenCells + 1 >= maxDiagonalIndex) {
		// Nothing is locked in place yet, or, only one cell is (can't make a triangle from that -- zero area)
		return;
	}

	BreakupCell *firstOnDiagonal = diagonals[data.maxDiagonalToContainBrokenCells];
	BreakupCell *lastOnDiagonal  = ((diagonals[data.maxDiagonalToContainBrokenCells + 1]) - 1);
	std::array<BreakupCell *, 2> diagonalCells{{firstOnDiagonal, lastOnDiagonal}};
	for (BreakupCell *cell : diagonalCells) {
		if (cell->cell_x != lastCell.cell_x && cell->cell_y != lastCell.cell_y) {
			// must draw this triangle (cell, lastCell, shared corner)
			float sharedCornerX, sharedCornerY;
			if (cell->cell_x == 0 || cell->cell_x == maxX) {
				sharedCornerX = cell->cell_x;
				sharedCornerY = lastCell.cell_y;
			} else {
				sharedCornerX = lastCell.cell_x;
				sharedCornerY = cell->cell_y;
			}
			data.blitter.get().copyTriangle(
			    cell->cell_x * f, cell->cell_y * f,
			    lastCell.cell_x * f, lastCell.cell_y * f,
			    sharedCornerX * f, sharedCornerY * f,
			    dstX, dstY);
		}
	}
	// must draw the triangle (lastOnDiagonal, firstOnDiagonal, lastCell)
	data.blitter.get().copyTriangle(
	    lastOnDiagonal->cell_x * f, lastOnDiagonal->cell_y * f,
	    firstOnDiagonal->cell_x * f, firstOnDiagonal->cell_y * f,
	    lastCell.cell_x * f, lastCell.cell_y * f,
	    dstX, dstY);
}

PooledGPUImage GPUController::getBrokenUpImage(GPUTransformableCanvasImage &im, BreakupID id,
                                               int breakupFactor, int breakupDirectionFlagset,
                                               const char *params) {
	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	breakUpImage(id, im.image, nullptr, newImage.image->target, breakupFactor, breakupDirectionFlagset, params, 0, 0);

	return newImage;
}

void GPUController::glassSmashImage(GPU_Image *src, GPU_Target *target, int smashFactor) {
	if (!src) {
		sendToLog(LogLevel::Error, "No image to glass smash!\n");
		return;
	}

	constexpr int dotWidth{ONScripter::GlassSmashData::DotWidth};
	constexpr int dotHeight{ONScripter::GlassSmashData::DotHeight};
	constexpr int rectWidth{ONScripter::GlassSmashData::RectWidth};
	constexpr int rectHeight{ONScripter::GlassSmashData::RectHeight};
	constexpr int triangleNum{ONScripter::GlassSmashData::TriangleNum};

	auto &data   = ons.glassSmashData;
	auto &points = data.points;
	auto &seeds  = data.triangleSeeds;

	if (!data.initialised) {
		data.blitter.set(createTriangleBlitter(src, target));

		// Effect-specific data
		float xSep = window.canvas_width / static_cast<float>(rectWidth);
		float ySep = window.canvas_height / static_cast<float>(rectHeight);

		// Set up the dot grid
		for (int x = 0; x < dotWidth; x++) {
			for (int y = 0; y < dotHeight; y++) {
				std::pair<float, float> p{x * xSep, y * ySep};

				int i = x * dotWidth + y;

				// Setup seed per triangle
				int *s = &seeds[i * 2 % triangleNum];
				s[0]   = (std::rand() % 1000) - 500;
				s[1]   = (std::rand() % 1000) - 500;

				// Jiggle the dots that are belonging within rect (not outside)
				// (Jiggle factor is small enough to avoid the horizontal, vertical, and positive diagonal gridlines from crossing)
				if (x > 0 && y > 0 && x < rectWidth && y < rectHeight) {
					p.first += s[0] * xSep / (3.5 * 500.0);
					p.second += s[1] * ySep / (3.5 * 500.0);
				}

				points[i] = p;
			}
		}

		data.initialised = true;
	}

	// Regarding shine we probably want to draw it at gl_FragCoord coordinates when we have underlying
	// triangle data. Z should be decreased proportionally to triangle z

	enter3dMode();
	setShaderProgram("glassSmash.frag");

	// Looks somewhat right but it probably is not log in ps3
	float opacity = 0.05 / log(0.0001122 * smashFactor + 0.85) + 1.3;
	setShaderVar("alpha", opacity);

	auto &blitter = data.blitter.get();
	blitter.updateTargets(src, target);

	constexpr int xHalf = rectWidth / 2;
	constexpr int yHalf = rectHeight / 2;

	// The whole triangle should spin like 1-3 times, which means the output is
	// supposed to be 0 ~ (2*M_PI,6*M_PI)
	auto getAngle = [](int seed, int factor) {
		int dir   = std::signbit(seed) ? -1 : 1;
		float end = dir * (2 * M_PI + 4 * M_PI * (std::abs(seed) / 500.0));
		float t   = 0.15 / -log(0.0008632 * factor) - 0.02;
		return t * end;
	};

	float zParam = (smashFactor - 500) / 500.0;
	float zState = 1.0 - 1.42607 * std::sin(smashFactor / 1000.0);

	for (int y = 0; y < rectHeight; y++) {
		// 0 ~ 16 * 0 ~ 40 -> 0 ~ 640
		float yState = (y - yHalf) * (y - yHalf) * smashFactor / 25.0;
		float yParam = yState * zParam * zParam + yState * zParam - 100;

		for (int x = 0; x < rectWidth; x++) {
			float xState = (x - xHalf) * (x - xHalf) * smashFactor / 250.0;

			auto &tl = points[x * dotWidth + y];
			auto &bl = points[x * dotWidth + (y + 1)];
			auto &tr = points[(x + 1) * dotWidth + y];
			auto &br = points[(x + 1) * dotWidth + (y + 1)];

			int triangleIndex = (x * rectWidth + y) * 2;

			// Is it worth trying to manipulate row and pitch (will probably also require different z centres)?
			blitter.copyTriangle(tl.first, tl.second, tr.first, tr.second, bl.first, bl.second,
			                     xState, yParam, zState - x * y * 0.013, getAngle(seeds[triangleIndex], smashFactor), 0, 0);
			blitter.copyTriangle(bl.first, bl.second, tr.first, tr.second, br.first, br.second,
			                     xState, yParam, zState - x * y * 0.013, getAngle(seeds[triangleIndex + 1], smashFactor), 0, 0);
		}
	}

	blitter.finish();
	unsetShaderProgram();
	exit3dMode();
}

PooledGPUImage GPUController::getGlassSmashedImage(GPUTransformableCanvasImage &im, int smashFactor) {

	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	glassSmashImage(im.image, newImage.image->target, smashFactor);

	return newImage;
}

void TriangleBlitter::addTriangle(
    float s1, float t1, float s2, float t2, float s3, float t3,
    float x1, float y1, float z1, float x2, float y2, float z2,
    float x3, float y3, float z3) {
	if (verticesInVertexBuffer + 3 > maxVertices || verticesInIndexBuffer + 3 > maxIndices) {
		finish();
	}

	auto myVertices = vertices.data();
	auto myIndices  = indices.data();

	setTexturedVertex(myVertices, myIndices, s1, t1, x1, y1, z1);
	setTexturedVertex(myVertices, myIndices, s2, t2, x2, y2, z2);
	setTexturedVertex(myVertices, myIndices, s3, t3, x3, y3, z3);
}

void TriangleBlitter::rotateCoordinates(
    float coords[3][3],
    float centerX, float centerY, float centerZ,
    float yaw, float pitch, float roll) {
	float CA = std::cos(yaw);
	float CE = std::cos(pitch);
	float CR = std::cos(roll);

	float SA = std::sin(yaw);
	float SE = std::sin(pitch);
	float SR = std::sin(roll);

	float m[4][3];

	m[0][0] = CA * CE;                // a
	m[0][1] = CA * SE * SR - SA * CR; // b
	m[0][2] = CA * SE * CR + SA * SR; // c

	m[1][0] = SA * CE;                // d
	m[1][1] = CA * CR + SA * SE * SR; // e
	m[1][2] = SA * SE * CR - CA * SR; // f

	m[2][0] = -SE;     // g
	m[2][1] = CE * SR; // h
	m[2][2] = CE * CR; // i

	m[3][0] = centerX - m[0][0] * centerX - m[1][0] * centerY - m[2][0] * centerZ;
	m[3][1] = centerY - m[0][1] * centerX - m[1][1] * centerY - m[2][1] * centerZ;
	m[3][2] = centerZ - m[0][2] * centerX - m[1][2] * centerY - m[2][2] * centerZ;

	coords[0][0] = coords[0][0] * m[0][0] + coords[0][1] * m[1][0] + coords[0][2] * m[2][0] + m[3][0];
	coords[0][1] = coords[0][0] * m[0][1] + coords[0][1] * m[1][1] + coords[0][2] * m[2][1] + m[3][1];
	coords[0][2] = coords[0][0] * m[0][2] + coords[0][1] * m[1][2] + coords[0][2] * m[2][2] + m[3][2];

	coords[1][0] = coords[1][0] * m[0][0] + coords[1][1] * m[1][0] + coords[1][2] * m[2][0] + m[3][0];
	coords[1][1] = coords[1][0] * m[0][1] + coords[1][1] * m[1][1] + coords[1][2] * m[2][1] + m[3][1];
	coords[1][2] = coords[1][0] * m[0][2] + coords[1][1] * m[1][2] + coords[1][2] * m[2][2] + m[3][2];

	coords[2][0] = coords[2][0] * m[0][0] + coords[2][1] * m[1][0] + coords[2][2] * m[2][0] + m[3][0];
	coords[2][1] = coords[2][0] * m[0][1] + coords[2][1] * m[1][1] + coords[2][2] * m[2][1] + m[3][1];
	coords[2][2] = coords[2][0] * m[0][2] + coords[2][1] * m[1][2] + coords[2][2] * m[2][2] + m[3][2];
}

void TriangleBlitter::addEllipse(float s, float t, float radius_s, float radius_t, float x, float y, float radius_x, float radius_y) {
	int degrees = 0; // I don't think we need to make this flexible
	float rot_x = std::cos(degrees * M_PI / 180);
	float rot_y = std::sin(degrees * M_PI / 180);
	float dt    = (1.25f / std::sqrt(radius_x > radius_y ? radius_x : radius_y)); // s = rA, so dA = ds/r.  ds of 1.25*std::sqrt(radius) is good, use A in degrees.

	int numSegments;
	if (fewerTriangles) {
		//dt = dt < 1.0f ? 1.0f : dt;
		numSegments = 2;
		// Feels like the safest minimum number I can go with for now.
		// Using only a single large triangle for this ellipse may have some issues related to exceeding the neighboring cell area for breakup.
	} else {
		numSegments = 2 * M_PI / dt + 1;
	}

	if (verticesInVertexBuffer + (3 + numSegments - 2) > maxVertices) {
		finish();
	}
	if (verticesInIndexBuffer + (3 + (numSegments - 2) * 3 + 3) > maxIndices) {
		finish();
	}

	int start = verticesInVertexBuffer;

	auto myVertices = vertices.data();
	auto myIndices  = indices.data();

	if (numSegments == 2) {
		// segment one
		setTexturedVertex(myVertices, myIndices, s + radius_s, t + radius_t, x + radius_x, y + radius_y); // top right
		setTexturedVertex(myVertices, myIndices, s - radius_s, t - radius_t, x - radius_x, y - radius_y); // bottom left
		setTexturedVertex(myVertices, myIndices, s - radius_s, t + radius_t, x - radius_x, y + radius_y); // top left
		// segment two
		setIndexedVertex(myIndices, start);                                                               // top right
		setIndexedVertex(myIndices, start + 1);                                                           // bottom left
		setTexturedVertex(myVertices, myIndices, s + radius_s, t - radius_t, x + radius_x, y - radius_y); // bottom right
		return;
	}

	setTexturedVertex(myVertices, nullptr, s, t, x, y); // Center
	float cos_rads = std::cos(dt);
	float sin_rads = std::sin(dt);
	float dx, dy, s_transpose, t_transpose, x_transpose, y_transpose;
	dx          = 1.0f;
	dy          = 0.0f;
	s_transpose = rot_x * radius_s * dx - rot_y * radius_t * dy;
	t_transpose = rot_y * radius_s * dx + rot_x * radius_t * dy;
	x_transpose = rot_x * radius_x * dx - rot_y * radius_y * dy;
	y_transpose = rot_y * radius_x * dx + rot_x * radius_y * dy;
	setTexturedVertex(myVertices, nullptr, s + s_transpose, t + t_transpose, x + x_transpose, y + y_transpose);

	for (int i = 1; i < numSegments; i++) {
		float tempx = cos_rads * dx - sin_rads * dy;
		dy          = sin_rads * dx + cos_rads * dy;
		dx          = tempx;
		s_transpose = rot_x * radius_s * dx - rot_y * radius_t * dy;
		t_transpose = rot_y * radius_s * dx + rot_x * radius_t * dy;
		x_transpose = rot_x * radius_x * dx - rot_y * radius_y * dy;
		y_transpose = rot_y * radius_x * dx + rot_x * radius_y * dy;
		setIndexedVertex(myIndices, start + 0); // center
		setIndexedVertex(myIndices, start + i); // previous point
		setTexturedVertex(myVertices, myIndices, s + s_transpose, t + t_transpose, x + x_transpose, y + y_transpose);
	}

	setIndexedVertex(myIndices, start + 0);           // center
	setIndexedVertex(myIndices, start + numSegments); // last point
	setIndexedVertex(myIndices, start + 1);           // first point
}

void TriangleBlitter::finish() {
	if (!gpu.triangle_blit_flush) {
		GPU_TriangleBatch(image, target, verticesInVertexBuffer, vertices.data(), verticesInIndexBuffer, indices.data(), dataStructure);
	} else {
		// First finish does not need to be here, could be done once per breakup, however it seems to produce better performance
		GPU_FlushBlitBuffer();
		(gpu.*gpu.current_renderer->syncRendererState)();
		GPU_TriangleBatch(image, target, verticesInVertexBuffer, vertices.data(), verticesInIndexBuffer, indices.data(), dataStructure);
		GPU_FlushBlitBuffer();
		(gpu.*gpu.current_renderer->syncRendererState)();
	}
	verticesInVertexBuffer = 0;
	verticesInIndexBuffer  = 0; // this is all we need for clear
}

PooledGPUImage GPUController::getWarpedImage(GPUTransformableCanvasImage &im, float animationClock, float amplitude, float waveLength, float speed) {
	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	if (!im.image) {
		sendToLog(LogLevel::Error, "No image to warp!\n");
		return newImage;
	}

	if (amplitude == 0 || waveLength == 0) {
		copyGPUImage(im.image, nullptr, nullptr, newImage.image->target);
		return newImage;
	}

	setShaderProgram("effectWarp.frag");
	GPU_SetBlending(im.image, false);
	setShaderVar("animationClock", animationClock);
	setShaderVar("amplitude", amplitude);
	setShaderVar("wavelength", waveLength);
	setShaderVar("speed", speed);
	setShaderVar("cx", im.image->texture_w / static_cast<float>(im.image->w));
	setShaderVar("cy", im.image->texture_h / static_cast<float>(im.image->h));
	copyGPUImage(im.image, nullptr, nullptr, newImage.image->target, 0, 0);
	unsetShaderProgram();
	GPU_SetBlending(im.image, true);

	return newImage;
}

PooledGPUImage GPUController::getGreyscaleImage(GPUTransformableCanvasImage &im, const SDL_Color &color) {
	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	if (!im.image) {
		sendToLog(LogLevel::Error, "No image to turn to greyscale!\n");
		return newImage;
	}

	setShaderProgram("colorModification.frag");
	bindImageToSlot(im.image, 0);
	GPU_SetBlending(im.image, false);
	setShaderVar("modificationType", 4);
	setShaderVar("greyscaleHue", color);
	copyGPUImage(im.image, nullptr, nullptr, newImage.image->target, 0, 0);
	unsetShaderProgram();
	GPU_SetBlending(im.image, true);

	return newImage;
}

PooledGPUImage GPUController::getSepiaImage(GPUTransformableCanvasImage &im) {
	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	if (!im.image) {
		sendToLog(LogLevel::Error, "No image to turn to sepia!\n");
		return newImage;
	}

	setShaderProgram("colorModification.frag");
	bindImageToSlot(im.image, 0);
	GPU_SetBlending(im.image, false);
	setShaderVar("modificationType", 1);
	copyGPUImage(im.image, nullptr, nullptr, newImage.image->target, 0, 0);
	unsetShaderProgram();
	GPU_SetBlending(im.image, true);

	return newImage;
}

PooledGPUImage GPUController::getNegativeImage(GPUTransformableCanvasImage &im) {
	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	if (!im.image) {
		sendToLog(LogLevel::Error, "No image to turn to negative!\n");
		return newImage;
	}

	setShaderProgram("colorModification.frag");
	bindImageToSlot(im.image, 0);
	GPU_SetBlending(im.image, false);
	setShaderVar("modificationType", 5);
	copyGPUImage(im.image, nullptr, nullptr, newImage.image->target, 0, 0);
	unsetShaderProgram();
	GPU_SetBlending(im.image, true);

	return newImage;
}

PooledGPUImage GPUController::getPixelatedImage(GPUTransformableCanvasImage &im, int factor) {
	PooledGPUImage newImage = getPooledImage(window.canvas_width, window.canvas_height);

	if (!im.image) {
		sendToLog(LogLevel::Error, "No image to pixelate!\n");
		return newImage;
	}

	setShaderProgram("pixelate.frag");
	bindImageToSlot(im.image, 0);
	GPU_SetBlending(im.image, false);
	setShaderVar("width", im.image->texture_w);
	setShaderVar("height", im.image->texture_h);
	setShaderVar("factor", factor);
	copyGPUImage(im.image, nullptr, nullptr, newImage.image->target, 0, 0);
	unsetShaderProgram();
	GPU_SetBlending(im.image, true);

	return newImage;
}

// ************************
// **  TempGPUImagePool  **
// ************************

PooledGPUImage GPUController::getPooledImage(int w, int h) {
	if ((w < 0 && h < 0) || (w == window.canvas_width && h == window.canvas_height))
		return PooledGPUImage(&canvasImagePool);
	else if (w == window.script_width && h == window.script_height)
		return PooledGPUImage(&scriptImagePool);

	SDL_Point size{w, h};
	if (!typedImagePools.count(size)) {
		typedImagePools[size].size = size; // make sure size member is properly initialized
	}
	TempGPUImagePool *pool = &typedImagePools[size];
	return PooledGPUImage(pool);
}

GPU_Image *TempGPUImagePool::getImage() {
	// Look for unused temp image
	auto i = std::find_if(pool.begin(), pool.end(), [](const std::unordered_map<GPU_Image *, bool>::value_type &e) { return !e.second; });
	// If we found one, return that, otherwise make a new one
	GPU_Image *r;
	if (i != pool.end()) {
		r = i->first;
		gpu.clearWholeTarget(r->target);
	} else {
		r = gpu.createImage(size.x, size.y, 4);
		GPU_GetTarget(r);
		gpu.clearWholeTarget(r->target);
	}
	pool[r] = true;
	return r;
}

void TempGPUImagePool::giveImage(GPU_Image *im) {
	pool[im] = false;
	gpu.clearWholeTarget(im->target);
}

void TempGPUImagePool::addImages(int n) {
	GPU_Image *im;
	for (int i = 0; i < n; i++) {
		im       = gpu.createImage(size.x, size.y, 4);
		pool[im] = false;
		GPU_GetTarget(im);
		gpu.clearWholeTarget(im->target);
	}
}

void TempGPUImagePool::clearUnused(bool require_empty) {
	auto entry = pool.begin();
	while (entry != pool.end()) {
		if (!entry->second) {
			gpu.freeImage(entry->first);
			entry = pool.erase(entry);
		} else {
			++entry;
		}
	}

	if (require_empty && !pool.empty()) {
		throw std::runtime_error("Failed to cleanup TempGPUImagePool on request");
	}
}

GPU_Image *CombinedImagePool::get(int w, int h, int channels, bool store) {
	GPU_FormatEnum format;

	switch (channels) {
		case 1:
			format = GPU_FORMAT_LUMINANCE;
			break;
		case 2:
			format = GPU_FORMAT_LUMINANCE_ALPHA;
			break;
		case 4:
			format = GPU_FORMAT_RGBA;
			break;
		default:
			format = GPU_FORMAT_RGB;
	}

	// Clear the toDo
	if (GPU_FORMAT_RGBA == format) {
		SDL_AtomicLock(&access);
		auto itR = toDo.begin();
		while (itR != toDo.end()) {
			if ((*itR).w == w && (*itR).h == h) {
				itR = toDo.erase(itR);
			} else {
				++itR;
			}
		}
		SDL_AtomicUnlock(&access);
	}

	auto itI = requested.begin();
	while (itI != requested.end()) {
		if ((*itI)->w == w && (*itI)->h == h && (*itI)->format == format) {
			GPU_Image *ret = *itI;
			if (!store)
				requested.erase(itI);
			return ret;
		}
		++itI;
	}

	GPUImageDiff diff;
	diff.w      = w;
	diff.h      = h;
	diff.format = format;

	std::shared_ptr<Wrapped_GPU_Image> res = existent.get(diff);
	if (res) {
		GPU_Image *img = res->img;
		res->img       = nullptr;
		existent.remove(diff);
		if (store) {
			requested.push_back(img);
		} else {
			GPU_GetTarget(img);
			gpu.clearWholeTarget(img->target);
		}
		return img;
	}

	GPU_Image *img = GPU_CreateImage(w, h, format);
	if (store)
		requested.push_back(img);

	return img;
}

bool CombinedImagePool::generate() {
	SDL_AtomicLock(&access);
	if (!toDo.empty()) {
		int w = toDo[0].w, h = toDo[0].h;
		SDL_AtomicUnlock(&access);
		gpu.createImage(w, h, 4, true);
		return true;
	}
	SDL_AtomicUnlock(&access);
	return false;
}

// ***********************************
// **  GPUBigImage  **
// ***********************************

void GPUBigImage::create(SDL_Surface *surface) {
	auto s = gpu.max_texture;
	GPU_Rect tmp{0, 0, 0, 0};
	while (tmp.y < h) {
		tmp.w = w - tmp.x > s ? s : w - tmp.x;
		tmp.h = h - tmp.y > s ? s : h - tmp.y;

		GPU_Image *chunk{nullptr};
		if (surface) {
#if !defined(IOS) && !defined(DROID) // There is some issue with loadGPUImageByChunks on iOS
			if (!(ons.skip_mode & ONScripter::SKIP_SUPERSKIP)) {
				chunk = gpu.loadGPUImageByChunks(surface, &tmp);
			} else
#endif
			{
				chunk = gpu.createImage(tmp.w, tmp.h, channels);
				GPU_GetTarget(chunk);
				gpu.updateImage(chunk, nullptr, surface, &tmp);
			}
			gpu.multiplyAlpha(chunk);
		} else {
			chunk = gpu.createImage(tmp.w, tmp.h, channels);
			GPU_GetTarget(chunk);
		}
		images.emplace_back(chunk);

		tmp.x += s;

		if (tmp.x >= w) {
			tmp.x = 0;
			tmp.y += s;
		}
	}
}

GPUBigImage::GPUBigImage(SDL_Surface *surface) {
	if (!surface || surface->w == 0 || surface->h == 0)
		return;

	w        = surface->w;
	h        = surface->h;
	channels = surface->format->BytesPerPixel;

	create(surface);
}

GPUBigImage::GPUBigImage(uint16_t w_, uint16_t h_, int channels_)
    : w(w_), h(h_), channels(channels_) {
	if (w == 0 || h == 0)
		return;

	create();
}

std::vector<std::pair<GPU_Image *, GPU_Rect>> GPUBigImage::getImagesForArea(GPU_Rect &area) {
	if (area.w < 1 || area.h < 1)
		return {};

	std::vector<std::pair<GPU_Image *, GPU_Rect>> seq;

	float s = gpu.max_texture;

	int x_start = area.x / s;
	int y_start = area.y / s;

	// by chunk boundary
	int x_start_coord = area.x - static_cast<int>(area.x) % static_cast<int>(s);
	int y_start_coord = area.y - static_cast<int>(area.y) % static_cast<int>(s);

	int x_off = x_start;
	int x_end = std::ceil((area.x + area.w) / s - 1);
	int y_off = y_start;
	int y_end = std::ceil((area.y + area.h) / s - 1);
	int x_num = std::ceil(w / s - 1) + 1;

	std::pair<GPU_Image *, GPU_Rect> tmp;

	while (y_off <= y_end) {
		while (x_off <= x_end) {
			tmp.first    = images[y_off * x_num + x_off].img;
			tmp.second.x = x_start_coord + (x_off - x_start) * s;
			tmp.second.y = y_start_coord + (y_off - y_start) * s;
			tmp.second.w = tmp.first->w;
			tmp.second.h = tmp.first->h;
			seq.emplace_back(tmp);
			x_off++;
		}
		x_off = x_start;
		y_off++;
	}

	return seq;
}

bool GPUImageDiff::operator==(const GPUImageDiff &two) const {
	return w == two.w && h == two.h && format == two.format;
}

#ifdef DEBUG

void dbgSaveImg(void *ptr) {
	GPU_SaveImage(static_cast<GPU_Image *>(ptr), "/Users/user/Desktop/1.png", GPU_FILE_AUTO);
}

void dbgSaveTgt(void *ptr) {
	GPU_SaveImage(GPU_CopyImageFromTarget(static_cast<GPU_Target *>(ptr)), "/Users/user/Desktop/1.png", GPU_FILE_AUTO);
}

void dbgSaveImgR(GPU_Image *ptr) {
	GPU_SaveImage(ptr, "/Users/user/Desktop/1.png", GPU_FILE_AUTO);
}

void dbgSaveTgtR(GPU_Target *ptr) {
	GPU_SaveImage(GPU_CopyImageFromTarget(ptr), "/Users/user/Desktop/1.png", GPU_FILE_AUTO);
}

#endif
