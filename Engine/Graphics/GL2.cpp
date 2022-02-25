/**
 *  GL2.cpp
 *  ONScripter-RU
 *
 *  Contains driver-specific SDL_gpu/GL instructions.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#if defined(MACOSX) || defined(LINUX) || defined(WIN32)

#if !defined(SDL_GPU_USE_EPOXY)
#define SDL_GPU_USE_EPOXY 1
#endif

#include "Engine/Graphics/GPU.hpp"
#include "Support/FileDefs.hpp"

#include <SDL2/SDL_gpu_OpenGL_2.h>

GPU_RendererID GPUController::makeRendererIdGL2() {
	return GPU_MakeRendererID("OpenGL 2", GPU_RENDERER_OPENGL_2, 2, 1);
	;
}

void GPUController::initRendererFlagsGL2() {
}

int GPUController::getImageFormatGL2(GPU_Image *image) {
	return static_cast<GPU_IMAGE_DATA *>(image->data)->format;
}

void GPUController::printBlitBufferStateGL2() {
	
	sendToLog(LogLevel::Warn, "This is deprecated!");
}

void GPUController::syncRendererStateGL2() {
	glFinish();
}

int GPUController::getMaxTextureSizeGL2() {
	int size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
	return size;
}

#endif
