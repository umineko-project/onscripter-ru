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
	auto cdata = static_cast<GPU_CONTEXT_DATA *>(GPU_GetCurrentRenderer()->current_context_target->context->data);
	if (cdata->blit_buffer_num_vertices > 0 && cdata->last_target && cdata->last_image)
		sendToLog(LogLevel::Info, "Blit buffer size: %u\n", cdata->blit_buffer_num_vertices);
	else
		sendToLog(LogLevel::Info, "Blit buffer empty.\n");
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
