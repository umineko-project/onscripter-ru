/**
 *  Pool.hpp
 *  ONScripter-RU
 *
 *  Contains graphics pools for load and preserve.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <SDL2/SDL.h>

#include <unordered_map>

class PNGLoader;

class TempImagePool {
private:
	std::unordered_map<SDL_Surface *, bool> pool; // boolean = is this SDL_Surface* "checked-out"?
public:
	SDL_Point size;
	SDL_Surface *getImage();         // get a fresh temporary image
	void giveImage(SDL_Surface *im); // return a temporary image to the pool for reuse
	void addImages(int n);           // pre-create some blank temporary images to avoid delays later
	~TempImagePool();
};

class TempImageLoaderPool {
private:
	std::unordered_map<PNGLoader *, bool> pool;

public:
	PNGLoader *getLoader();
	void giveLoader(PNGLoader *ldr);
	void addLoaders(int n);
	~TempImageLoaderPool();
};

// This is out of a general model, because it does not have a state to protect.
// It might be an ONScripter object some day.
extern TempImageLoaderPool pngImageLoaderPool;
