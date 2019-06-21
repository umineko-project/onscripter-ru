/**
 *  GLES2.cpp
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

#include "Engine/Graphics/GPU.hpp"
#include "Support/FileDefs.hpp"

#include <SDL2/SDL_gpu_GLES_2.h>

GPU_RendererID GPUController::makeRendererIdANGLE2() {
	SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
	return GPU_MakeRendererID("ANGLE 2", GPU_RENDERER_GLES_2, 2, 0);
}

void GPUController::initRendererFlagsANGLE2() {
	// Default to no rendering to self on ANGLE
	if (render_to_self < 0)
		render_to_self = 0;
}

int GPUController::getImageFormatANGLE2(GPU_Image *image) {
	return static_cast<GPU_IMAGE_DATA *>(image->data)->format;
}

void GPUController::printBlitBufferStateANGLE2() {
	auto cdata = static_cast<GPU_CONTEXT_DATA *>(GPU_GetCurrentRenderer()->current_context_target->context->data);
	if (cdata->blit_buffer_num_vertices > 0 && cdata->last_target && cdata->last_image)
		sendToLog(LogLevel::Info, "Blit buffer size: %u\n", cdata->blit_buffer_num_vertices);
	else
		sendToLog(LogLevel::Info, "Blit buffer empty.\n");
}

void GPUController::syncRendererStateANGLE2() {
	glFinish();
}

int GPUController::getMaxTextureSizeANGLE2() {
	int size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
	return size;
}

GPU_RendererID GPUController::makeRendererIdGLES2() {
	return GPU_MakeRendererID("OpenGL ES 2", GPU_RENDERER_GLES_2, 2, 0);
}

void GPUController::initRendererFlagsGLES2() {
}

int GPUController::getImageFormatGLES2(GPU_Image *image) {
	return static_cast<GPU_IMAGE_DATA *>(image->data)->format;
}

void GPUController::printBlitBufferStateGLES2() {
	auto cdata = static_cast<GPU_CONTEXT_DATA *>(GPU_GetCurrentRenderer()->current_context_target->context->data);
	if (cdata->blit_buffer_num_vertices > 0 && cdata->last_target && cdata->last_image)
		sendToLog(LogLevel::Info, "Blit buffer size: %u\n", cdata->blit_buffer_num_vertices);
	else
		sendToLog(LogLevel::Info, "Blit buffer empty.\n");
}

void GPUController::syncRendererStateGLES2() {
	glFinish();
}

int GPUController::getMaxTextureSizeGLES2() {
	int size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
	return size;
}

#endif
