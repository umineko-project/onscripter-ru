/**
 *  Cache.cpp
 *  ONScripter-RU
 *
 *  Object caching interface with prebuilt implementations for some classes.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Support/Cache.hpp"

#include <cassert>

// --------------------- ImageCacheController methods ---------------------

void ImageCacheController::add(int cacheSetNumber, const std::string &filename, const std::shared_ptr<Wrapped_SDL_Surface> &surface) {
	assert(surface);

	if (!surface->surface)
		return; //Don't add nullptrs to cache

	CacheController<Wrapped_SDL_Surface>::add(cacheSetNumber, filename, surface);
}

std::shared_ptr<Wrapped_SDL_Surface> ImageCacheController::get(const std::string &filename) {
	std::shared_ptr<Wrapped_SDL_Surface> res = CacheController<Wrapped_SDL_Surface>::get(filename);
	if (!res)
		return nullptr;
	assert(res->surface);
	//get should not be responsible for refcounts!
	return res;
}

// --------------------- SoundCacheController methods ---------------------

void SoundCacheController::add(int cacheSetNumber, const std::string &filename, const std::shared_ptr<Wrapped_Mix_Chunk> &chunk) {
	assert(chunk);
	if (!chunk->chunk)
		return; //Don't add nullptrs to cache

	CacheController<Wrapped_Mix_Chunk>::add(cacheSetNumber, std::move(filename), chunk);
}

std::shared_ptr<Wrapped_Mix_Chunk> SoundCacheController::get(const std::string &filename) {
	std::shared_ptr<Wrapped_Mix_Chunk> res = CacheController<Wrapped_Mix_Chunk>::get(filename);
	if (!res)
		return nullptr;
	assert(res->chunk); //we didn't add nullptrs
	return res;
}
