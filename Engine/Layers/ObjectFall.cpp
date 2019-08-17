/**
 *  ObjectFall.cpp
 *  ONScripter-RU
 *
 *  "snow.dll" analogue with improved dencity and performance.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Layers/ObjectFall.hpp"
#include "Engine/Core/ONScripter.hpp"
#include "Engine/Graphics/GPU.hpp"

#include <SDL2/SDL_gpu.h>

#include <cmath>
#include <random>

ObjectFallLayer::ObjectFallLayer(uint32_t w, uint32_t h)
    : Layer(w, h) {
	baseDrop = gpu.createImage(baseDropWidth, baseDropHeight, 4);
	GPU_GetTarget(baseDrop);
	gpu.clear(baseDrop->target, baseDropColour.r, baseDropColour.g, baseDropColour.b, baseDropColour.a);
	gpu.multiplyAlpha(baseDrop);
}

ObjectFallLayer::~ObjectFallLayer() {
	gpu.freeImage(baseDrop);
}

void ObjectFallLayer::setDims(uint32_t w, uint32_t h) {
	dropW = w;
	dropH = h;
}

void ObjectFallLayer::setSpeed(uint32_t speed) {
	if (!speed)
		dropSpeed = height * 0.35; // hardcoded atm
	else
		dropSpeed = speed;
}

void ObjectFallLayer::setCustomSpeed(uint32_t speed) {
	dropSpeed = speed / 4 * speedAmplifier;           // 1
	dropW     = (speed + 400) / 300 * widthAmplifier; // 1
	dropH     = speed / 3.2 * heightAmplifier;        // 0.875
}

void ObjectFallLayer::setAmplifiers(float s, float w, float h, float r, float m) {
	speedAmplifier  = s;
	widthAmplifier  = w;
	heightAmplifier = h;
	randomAmplifier = r;
	windAmplifier   = m;

	assert(heightAmplifier > 0);
}

void ObjectFallLayer::setAmount(uint32_t dropNum) {
	// Applies to each size
	if (randomAmplifier != 0)
		dropNum *= 3;

	dropAmount = dropNum;
	dropSpawnOrder.clear();

	// Create the drop spawn order list.
	// This specifies the order of the positions along the sky axis to make the drops fall from.
	// By having a shuffled list rather than just using a rand function to determine the position, we aim for greater "evenness" and avoid empty spots.
	for (uint32_t i = 0; i < dropNum; i++) dropSpawnOrder.emplace_back(i);
	std::random_device rng;
	std::default_random_engine urng(rng());
	std::shuffle(dropSpawnOrder.begin(), dropSpawnOrder.end(), urng);
}

void ObjectFallLayer::setWind(int32_t factor) {
	// We will create a rotated coordinate system so that the drops always fall downwards and (0,0) is at the topleft of the bounding box containing the screen
	// e.g.
	//        ^^ windFactor θ = 135°         (0,0)___._  .=top     here the diamond is the original screen and the rectangle is the new bounding box.
	//    ___//__                                 | /\φ|           the drops fall from top to bottom ("fall axis")
	//   |  //   |               -->         left ./||\. right     exactly parallel to left and right
	//   .__/θ)__|                                |\vv/|           and between the bounds left and right ("sky axis")
	//top^  |/                                    |_\/_|
	//   =(0,1080)                                  bottom         φ = 135° % 90 = 45°

	MathVector<float> corners[]{MathVector<float>(0, 0),
	                            MathVector<float>(0, height),
	                            MathVector<float>(width, height),
	                            MathVector<float>(width, 0)};

	for (int i = 0; i < 3; i++) {
		auto &transform = transforms[i];
		auto realFactor = factor - randomAmplifier * (i - 1) * factor * windAmplifier;

		float radians    = realFactor * transFactor * M_PI / 180.0;
		transform.sin    = std::sin(radians);
		transform.cos    = std::cos(radians);
		transform.factor = realFactor;

		// Find which corners will be the top, left, bottom, and right corners after the rotation into the ij coordinate system
		// This depends on which quadrant (0~90, 90~180, etc) the angle is in.

		int a        = std::remainder(realFactor * transFactor, 360) + 360;
		int quadrant = a / 90 % 4;

		transform.top         = corners[quadrant];
		transform.left        = corners[(quadrant + 1) % 4];
		transform.bottom      = corners[(quadrant + 2) % 4];
		transform.right       = corners[(quadrant + 3) % 4];
		transform.originalTop = transform.top; // save top's original XY position, we will need it later for the reverse transformation

		// Translate these points into the ij coordinate system
		for (auto point : {&transform.top, &transform.left, &transform.bottom, &transform.right}) {
			MathVector<float> &ref = *point;
			ref                    = (ref - transform.originalTop).rotate(transform.sin, transform.cos);
		}
		for (auto point : {&transform.top, &transform.bottom, &transform.right, &transform.left}) { // do left last as it's used in the calculation
			MathVector<float> &ref = *point;
			ref                    = ref.translate(-transform.left.x, 0);
		}
	}
	// All drops from this point on will now be created with this wind.
	// But changing the wind later won't affect drops that were already created.
}

void ObjectFallLayer::setBaseDrop(GPU_Image *newBaseDrop) {
	drops.clear();
	gpu.freeImage(baseDrop);
	baseDrop = newBaseDrop;
	dropW    = newBaseDrop->w;
	dropH    = newBaseDrop->h;
}

void ObjectFallLayer::setBaseDrop(SDL_Color &colour, uint32_t w, uint32_t h) {
	drops.clear();
	//TODO: add some gradients?
	if (baseDrop->w != w || baseDrop->h != h) {
		gpu.freeImage(baseDrop);
		baseDrop = gpu.createImage(baseDropWidth, baseDropHeight, 4);
		GPU_GetTarget(baseDrop);
	}

	gpu.clear(baseDrop->target, colour.r, colour.g, colour.b, colour.a);
	dropW = baseDrop->w;
	dropH = baseDrop->h;
}

void ObjectFallLayer::setPause(bool state) {
	if (paused[CurrentScene] == state)
		return;
	paused[FormerScene]  = paused[CurrentScene];
	paused[CurrentScene] = state;
	old_drops.set(drops);
	if (sprite && sprite->exists)
		ons.backupState(sprite);
}

void ObjectFallLayer::setBlend(BlendModeId mode) {
	blendMode = mode;
}

void ObjectFallLayer::coverScreen() {
	uint32_t num = height / dropH * 3;
	for (uint32_t i = 0; i < num; i++)
		update(true);
}

bool ObjectFallLayer::update(bool old) {
	if ((paused[FormerScene] && old) || (paused[CurrentScene] && !old))
		return true;

	auto &rdrops = (old && old_drops.has()) ? old_drops.get() : drops;

	// Firstly, remove the drops that have dropped offscreen (past their jMax)
	for (auto it = rdrops.begin(); it != rdrops.end();) {
		float topJ = it->j - it->h / 2.0;
		if (topJ >= it->jMax) {
			std::swap(*it, rdrops.back());
			rdrops.pop_back();
		} else {
			++it;
		}
	}

	// Secondly, move the drops
	for (auto &drop : rdrops) {
		drop.j += dropSpeed + dropSpeed * drop.r;
	}

	// Thirdly, add the necessary drops
	while (rdrops.size() < dropAmount) {
		Drop d;
		int r           = std::rand() % 100;
		auto &transform = transforms[r % 3];
		d.r             = ((r % 3) - 1) * randomAmplifier;
		d.w             = dropW + d.r * dropW;
		d.h             = dropH + d.r * dropH;

		// Get a spawn position and cycle the list
		if (dropSpawnOrder.size() != dropAmount) {
			setAmount(dropAmount);
		} // might happen on usage of default value
		double mySpawnOrder = static_cast<double>(dropSpawnOrder.front());
		dropSpawnOrder.pop_front();
		if (overlapForcePercentage && mySpawnOrder && static_cast<uint32_t>(r) < overlapForcePercentage) {
			// Make another one here soon! (Creates consecutive drops next to each other for "longer rain streaks")
			dropSpawnOrder.insert(dropSpawnOrder.begin() + (std::rand() % overlapForceProximity), mySpawnOrder);
		} else {
			// To the back (now zero is guaranteed to come before this order comes again, meaning jiggle will be changed and we will not occupy the same i-pos again)
			dropSpawnOrder.push_back(mySpawnOrder);
		}

		// Once per cycle of the order queue, change the random jiggle value
		if (mySpawnOrder == 0) {
			currentJiggle = (std::rand() % 10000) / 10000.0;
		}

		// Jiggle makes rain not fall in predictable vertical slots all the time, by slightly adjusting the i-position for each slot
		auto thisJiggle = (mySpawnOrder + 1) * currentJiggle;
		thisJiggle      = thisJiggle - static_cast<long>(thisJiggle); // fractional part only
		mySpawnOrder += thisJiggle;

		// Position the drop in its ij coordinate system
		// CHECKME: should i be added d.w/2.0?
		float renderPosI = ((mySpawnOrder / dropAmount) * transform.right.x) - (d.w / 2.0);
		float renderPosJ = -(d.h / 2.0);

		// To prevent all the drops appearing at the same time at the start, the more there are left to add, the higher they should be added
		// (plus a random factor to help remove any random clustering that might happen)
		auto remaining = (dropAmount - rdrops.size()) - 1;
		renderPosJ -= (((std::rand() % 5) + 1) * remaining * transform.bottom.y) / dropAmount;

		d.i           = renderPosI;
		d.j           = renderPosJ;
		d.top         = transform.top;
		d.originalTop = transform.originalTop;
		d.jMax        = transform.bottom.y;
		d.angle       = transform.factor * -transFactor;
		d.sin         = transform.sin;
		d.cos         = transform.cos;
		rdrops.push_back(d);
	}

	return true;
}

void ObjectFallLayer::refresh(GPU_Target *target, GPU_Rect &clip, float x, float y, bool /*centre_coordinates*/, int rm, float /*scalex*/, float /*scaley*/) {
	bool scene   = (rm & REFRESH_BEFORESCENE_MODE && old_drops.has());
	auto &rdrops = scene ? old_drops.get() : drops;

	if (clip.w == 0 || clip.h == 0 || rdrops.empty())
		return;

	for (auto &drop : rdrops) {
		// Transform the drop's coordinate system back into xy coordinates
		auto v = (drop.pos() - drop.top).rotate(-drop.sin, drop.cos) + drop.originalTop;
		gpu.copyGPUImage(baseDrop, nullptr, &clip, target, v.x + x, v.y + y,
		                 drop.w / static_cast<float>(baseDrop->w), drop.h / static_cast<float>(baseDrop->h), drop.angle, true);
	}
}

void ObjectFallLayer::commit() {
	//sendToLog(LogLevel::Info, "Time to commit ObjectFallLayer\n");
	if (paused[CurrentScene] != paused[FormerScene]) {
		old_drops.unset();
		paused[FormerScene] = paused[CurrentScene];
	}
}

std::unordered_map<std::string, DynamicPropertyInterface> ObjectFallLayer::properties() {
	return {
	    {"fallamount",
	     {[](void *layer) -> double {
		      auto fall = static_cast<ObjectFallLayer *>(layer);
		      return fall->dropAmount;
	      },
	      [](void *layer, double value) -> void {
		      auto fall = static_cast<ObjectFallLayer *>(layer);
		      fall->setAmount(value);
	      }}}};
}
