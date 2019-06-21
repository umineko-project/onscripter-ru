/* -*- C++ -*-
 *
 *  Layer.cpp - Code for effect layers for ONScripter-EN
 *
 *  Copyright (c) 2009-2011 "Uncle" Mion Sonozaki
 *
 *  UncleMion@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>
 *  or write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  Emulation of Takashi Toyama's "snow.dll", and "hana.dll"
 *  NScripter plugin filters.
 */
#include "Engine/Layers/Layer.hpp"
#include "Engine/Graphics/GPU.hpp"
#include "Engine/Components/Async.hpp"
#include "Support/FileDefs.hpp"

#include <cstdlib>
#include <cmath>
#include <cstdio>

void Layer::drawLayerToGPUTarget(GPU_Target *target, AnimationInfo *anim, GPU_Rect &clip, float x, float y) {
	if (anim->gpu_image == nullptr) {
		sendToLog(LogLevel::Error, "Layer@gpu_image is null; something went wrong\n");
		return;
	}

	if (clip.w == 0 || clip.h == 0) {
		return;
	}

	gpu.copyGPUImage(anim->gpu_image, nullptr, &clip, target, x + anim->pos.x, y + anim->pos.y);
}

BlendModeId Layer::blendingModeSupported(int rm) {
	if (!sprite)
		return BlendModeId::NORMAL;
	AnimationInfo *ai = sprite->oldNew(rm);
	return ai->exists ? ai->blending_mode : BlendModeId::NORMAL;
}
