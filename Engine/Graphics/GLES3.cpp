/**
 *  GLES3.cpp
 *  ONScripter-RU
 *
 *  Contains driver-specific SDL_gpu/GL instructions.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#if defined(IOS) || defined(DROID) || defined(WIN32)

#if defined(WIN32) && !defined(SDL_GPU_DYNAMIC_GLES_2)
#define SDL_GPU_DYNAMIC_GLES_2 1
#endif

#if (defined(DROID) || defined(WIN32)) && !defined(SDL_GPU_DYNAMIC_GLES_3)
#define SDL_GPU_DYNAMIC_GLES_3 1
#endif

#include "Engine/Graphics/GPU.hpp"
#include "Support/FileDefs.hpp"

#include <SDL2/SDL_gpu_GLES_3.h>

GPU_RendererID GPUController::makeRendererIdANGLE3() {
	SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
	return GPU_MakeRendererID("ANGLE 3", GPU_RENDERER_GLES_3, 3, 0);
}

void GPUController::initRendererFlagsANGLE3() {
	// Default to no rendering to self on ANGLE
	if (render_to_self < 0)
		render_to_self = 0;
}

int GPUController::getImageFormatANGLE3(GPU_Image *image) {
	return static_cast<GPU_IMAGE_DATA *>(image->data)->format;
}

void GPUController::printBlitBufferStateANGLE3() {
	sendToLog(LogLevel::Warn, "This is deprecated!");
}

void GPUController::syncRendererStateANGLE3() {
	//glFinish();
}

int GPUController::getMaxTextureSizeANGLE3() {
	int size;
#if defined(WIN32)
	// NEEDS TO BE FIXED
	size=0x0D33;
#else
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
#endif
	return size;
}

GPU_RendererID GPUController::makeRendererIdGLES3() {
	return GPU_MakeRendererID("OpenGL ES 3", GPU_RENDERER_GLES_3, 3, 0);
}

void GPUController::initRendererFlagsGLES3() {
}

int GPUController::getImageFormatGLES3(GPU_Image *image) {
	return static_cast<GPU_IMAGE_DATA *>(image->data)->format;
}

void GPUController::printBlitBufferStateGLES3() {
	sendToLog(LogLevel::Warn, "This is deprecated!");
}

void GPUController::syncRendererStateGLES3() {
	//glFinish();
}

int GPUController::getMaxTextureSizeGLES3() {
	int size;
#if defined(WIN32)
	// NEEDS TO BE FIXED
	size=0x0D33;
#else
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
#endif
	return size;
}

#endif
