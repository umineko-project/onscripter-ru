/**
 *  EffectTrig.cpp
 *  ONScripter-RU
 *
 *  Emulation of Takashi Toyama's "whirl.dll" and "trvswave.dll" NScripter plugin effects.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Core/ONScripter.hpp"
#include "Engine/Components/Window.hpp"

//
// Emulation of Takashi Toyama's "trvswave.dll" NScripter plugin effect
//
void ONScripter::effectTrvswave(const char * /*params*/, int duration) {

	auto temp = gpu.getCanvasImage();

	effectBlendToCombinedImage(nullptr, ALPHA_BLEND_CONST, 128 * effect_counter / duration, temp);

	pre_screen_render = true;
	if (pre_screen_gpu == nullptr)
		pre_screen_gpu = gpu.getScriptImage();

	gpu.setShaderProgram("effectTrvswave.frag");
	gpu.setShaderVar("script_width", temp->texture_w);
	gpu.setShaderVar("script_height", temp->texture_h);
	gpu.setShaderVar("effect_counter", effect_counter);
	gpu.setShaderVar("duration", duration);

	gpu.bindImageToSlot(temp, 0);
	gpu.copyGPUImage(temp, nullptr, nullptr, pre_screen_gpu->target);
	gpu.unsetShaderProgram();

	gpu.giveCanvasImage(temp);
}

//
// Emulation of Takashi Toyama's "whirl.dll" NScripter plugin effect
//

void ONScripter::effectWhirl(const char *params, int duration) {
	int direction;
	switch (params[0]) {
		case 'r': //estimated right
			direction = -1;
			break;
		case 'l': //estimated left
			direction = 1;
			break;
		case 'R': //original right
			direction = -2;
			break;
		case 'L': //original left
			direction = 2;
			break;
		case 'p': //ps3
		default:
			direction = 0;
			break;
	}

	auto temp = gpu.getCanvasImage();

	effectBlendToCombinedImage(nullptr, ALPHA_BLEND_CONST, 128 * effect_counter / duration, temp);

	pre_screen_render = true;
	if (pre_screen_gpu == nullptr)
		pre_screen_gpu = gpu.getScriptImage();

	gpu.setShaderProgram("effectWhirl.frag");
	gpu.setShaderVar("direction", direction);
	gpu.setShaderVar("effect_counter", effect_counter);
	gpu.setShaderVar("duration", duration);
	gpu.setShaderVar("render_width", static_cast<float>(window.script_width));
	gpu.setShaderVar("render_height", static_cast<float>(window.script_height));
	gpu.setShaderVar("texture_width", static_cast<float>(temp->texture_w));
	gpu.setShaderVar("texture_height", static_cast<float>(temp->texture_h));

	gpu.bindImageToSlot(temp, 0);
	gpu.copyGPUImage(temp, nullptr, nullptr, pre_screen_gpu->target);
	gpu.unsetShaderProgram();

	gpu.giveCanvasImage(temp);
}
