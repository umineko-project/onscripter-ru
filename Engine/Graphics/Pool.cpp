/**
 *  Pool.cpp
 *  ONScripter-RU
 *
 *  Contains graphics pools for load and preserve.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Graphics/Pool.hpp"
#include "Engine/Graphics/PNG.hpp"
#include "Engine/Components/Async.hpp"
#include "Support/FileDefs.hpp"

SDL_Surface *TempImagePool::getImage() {
	Lock lock(this);
	// Look for unused temp image
	auto i = std::find_if(pool.begin(), pool.end(), [](const std::unordered_map<SDL_Surface *, bool>::value_type &e) { return !e.second; });
	// If we found one, return that, otherwise make a new one
	SDL_Surface *r = i != pool.end() ? i->first :
	                                   SDL_CreateRGBSurface(SDL_SWSURFACE, size.x, size.y, 24,
	                                                        0x000000ff, 0x0000ff00, 0x00ff0000, 0);
	pool[r] = true;
	return r;
}

void TempImagePool::giveImage(SDL_Surface *im) {
	Lock lock(this);
	pool[im] = false;
}

void TempImagePool::addImages(int n) {
	Lock lock(this);
	SDL_Surface *im;
	for (int i = 0; i < n; i++) {
		im       = SDL_CreateRGBSurface(SDL_SWSURFACE, size.x, size.y, 24,
                                  0x000000ff, 0x0000ff00, 0x00ff0000, 0);
		pool[im] = false;
	}
}

TempImagePool::~TempImagePool() {
	for (auto diver : pool) {
		if (!diver.second) {
			SDL_FreeSurface(diver.first);
		} else {
			sendToLog(LogLevel::Error, "~TempImagePool@Diver cannot be eaten\n");
		}
	}
}

PNGLoader *TempImageLoaderPool::getLoader() {
	Lock lock(this);
	// Look for unused temp loader
	auto i = std::find_if(pool.begin(), pool.end(), [](const std::unordered_map<PNGLoader *, bool>::value_type &e) { return !e.second; });
	// If we found one, return that, otherwise make a new one
	PNGLoader *r = i != pool.end() ? i->first : new PNGLoader();
	pool[r]      = true;
	return r;
}

void TempImageLoaderPool::giveLoader(PNGLoader *ldr) {
	Lock lock(this);
	pool[ldr] = false;
}

void TempImageLoaderPool::addLoaders(int n) {
	Lock lock(this);
	PNGLoader *ldr;
	for (int i = 0; i < n; i++) {
		ldr       = new PNGLoader();
		pool[ldr] = false;
	}
}

TempImageLoaderPool::~TempImageLoaderPool() {
	for (auto diver : pool) {
		if (!diver.second) {
			delete[] diver.first;
		} else {
			sendToLog(LogLevel::Error, "~TempImageLoaderPool@Diver cannot be eaten\n");
		}
	}
}

TempImageLoaderPool pngImageLoaderPool;
