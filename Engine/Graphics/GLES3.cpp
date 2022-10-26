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

#include <epoxy/gl.h>
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
	auto cdata = static_cast<GPU_CONTEXT_DATA *>(GPU_GetCurrentRenderer()->current_context_target->context->data);
	if (cdata->blit_buffer_num_vertices > 0 && cdata->last_target && cdata->last_image)
		sendToLog(LogLevel::Info, "Blit buffer size: %u\n", cdata->blit_buffer_num_vertices);
	else
		sendToLog(LogLevel::Info, "Blit buffer empty.\n");
}

void GPUController::syncRendererStateANGLE3() {
	epoxy_glFinish();
}

int GPUController::getMaxTextureSizeANGLE3() {
	int size;
	epoxy_glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
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
	auto cdata = static_cast<GPU_CONTEXT_DATA *>(GPU_GetCurrentRenderer()->current_context_target->context->data);
	if (cdata->blit_buffer_num_vertices > 0 && cdata->last_target && cdata->last_image)
		sendToLog(LogLevel::Info, "Blit buffer size: %u\n", cdata->blit_buffer_num_vertices);
	else
		sendToLog(LogLevel::Info, "Blit buffer empty.\n");
}

void GPUController::syncRendererStateGLES3() {
	glFinish();
}

int GPUController::getMaxTextureSizeGLES3() {
	int size;
	epoxy_glGetIntegerv(GL_MAX_TEXTURE_SIZE, &size);
	return size;
}

#endif
