/**
 *  Cache.hpp
 *  ONScripter-RU
 *
 *  Object caching interface with prebuilt implementations for some classes.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "External/LRUCache.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_gpu.h>

#include <unordered_map>
#include <string>
#include <memory>
#include <cassert>

struct Wrapped_SDL_Surface {
	SDL_Surface *surface = nullptr;
	bool has_alpha       = false;
	Wrapped_SDL_Surface(SDL_Surface *_surface, bool _has_alpha)
	    : surface(_surface), has_alpha(_has_alpha) {
		//if (surface)
		//	sendToLog(LogLevel::Info, "CREATE %p - %p with ref %d\n",this,surface,surface->refcount);
	}
	~Wrapped_SDL_Surface() {
		if (surface) {
			//sendToLog(LogLevel::Info, "DELETE %p - %p with ref %d\n",this,surface,surface->refcount);
			SDL_FreeSurface(surface);
		}
	}
	Wrapped_SDL_Surface(const Wrapped_SDL_Surface &other) {
		surface = other.surface;
		if (surface)
			surface->refcount++;
		has_alpha = other.has_alpha;
	}
	Wrapped_SDL_Surface &operator=(const Wrapped_SDL_Surface &other) {
		if (other.surface)
			other.surface->refcount++;
		if (surface)
			SDL_FreeSurface(surface);
		surface   = other.surface;
		has_alpha = other.has_alpha;
		return *this;
	}
};

struct Wrapped_GPU_Image {
	GPU_Image *img{nullptr};
	Wrapped_GPU_Image(GPU_Image *image) {
		//assert(image != nullptr && image->refcount == 1);
		img = image;
	}
	Wrapped_GPU_Image(Wrapped_GPU_Image &&image) noexcept {
		img       = image.img;
		image.img = nullptr;
	}
	Wrapped_GPU_Image(const Wrapped_GPU_Image &image) {
		if (image.img)
			img = GPU_CopyImage(image.img);
	}
	Wrapped_GPU_Image &operator=(const Wrapped_GPU_Image &) = delete;
	Wrapped_GPU_Image &operator=(Wrapped_GPU_Image &&) = delete;
	~Wrapped_GPU_Image() {
		if (img)
			GPU_FreeImage(img);
	}
};

class Wrapped_Mix_Chunk {
public:
	Mix_Chunk *chunk{nullptr};
	Wrapped_Mix_Chunk(Mix_Chunk *_chunk)
	    : chunk(_chunk) {}
	~Wrapped_Mix_Chunk() {
		if (chunk)
			Mix_FreeChunk(chunk);
	}
	Wrapped_Mix_Chunk(const Wrapped_Mix_Chunk &other) = delete;
	Wrapped_Mix_Chunk &operator=(const Wrapped_Mix_Chunk &other) = delete;
};

template <typename SETELEM, typename KEY = std::string>
class CachedSet {
public:
	virtual void add(KEY keyname, std::shared_ptr<SETELEM> elem) = 0;
	virtual void clear()                                         = 0;
	virtual void remove(const KEY &keyname)                      = 0;
	virtual std::shared_ptr<SETELEM> get(KEY keyname)            = 0;
	virtual ~CachedSet()                                         = default;
};

template <typename SETELEM, typename KEY = std::string>
class LRUCachedSet : public CachedSet<SETELEM, KEY> {
protected:
	int capacity = 0;
	LRUCache<KEY, std::shared_ptr<SETELEM>, std::unordered_map> elemCache;

public:
	void add(KEY keyname, std::shared_ptr<SETELEM> elem) {
		elemCache.set(keyname, elem);
	}
	std::shared_ptr<SETELEM> get(KEY keyname) {
		try {
			auto wrapped = elemCache.get(keyname);
			//sendToLog(LogLevel::Info, "(LRU) Found image cache entry %s\n", filename.c_str());
			return wrapped;
		} catch (int) {
			//sendToLog(LogLevel::Info, "(LRU) Failed to find cache entry %s\n", filename.c_str());
			return nullptr;
		}
	}
	void remove(const KEY &keyname) {
		elemCache.remove(keyname);
	}
	void clear() {
		elemCache.resize(0);        // evict all elements
		elemCache.resize(capacity); // make it capable of holding the original capacity again
		                            // (lru cache has no clear() method)
	}
	LRUCachedSet(int capacity)
	    : capacity(capacity), elemCache(capacity) {}
};

template <typename SETELEM, typename KEY = std::string>
class UnlimitedCachedSet : public CachedSet<SETELEM, KEY> {
protected:
	std::unordered_map<std::string, std::shared_ptr<SETELEM>> elemCache;

public:
	void add(KEY keyname, std::shared_ptr<SETELEM> elem) {
		elemCache.emplace(keyname, elem);
	}
	std::shared_ptr<SETELEM> get(KEY keyname) {
		auto iterator = elemCache.find(keyname);
		if (iterator != elemCache.end()) {
			//sendToLog(LogLevel::Info, (Unlimited) Found image cache entry %s\n", filename.c_str());
			return iterator->second;
		}
		//sendToLog(LogLevel::Info, (Unlimited) Failed to find cache entry %s\n", filename.c_str());
		return nullptr;
	}
	void remove(const KEY &keyname) {
		auto iterator = elemCache.find(keyname);
		if (iterator != elemCache.end()) {
			elemCache.erase(iterator);
		}
	}
	void clear() {
		elemCache.clear();
	}
	UnlimitedCachedSet() = default;
};

template <typename SETELEM>
class CacheController {
	friend class CachedImageSet;

protected:
	void deleteExistingSet(int cacheSetNumber) {
		CachedSet<SETELEM> *set = cacheSets.at(cacheSetNumber);
		set->clear();
		delete set;
		cacheSets.erase(cacheSetNumber);
	}
	std::unordered_map<int, CachedSet<SETELEM> *> cacheSets;

public:
	void clearAll() {
		for (auto &number_set_pair : cacheSets) number_set_pair.second->clear();
	}
	void clear(int cacheSetNumber) {
		try {
			CachedSet<SETELEM> *set = cacheSets.at(cacheSetNumber);
			set->clear();
		} catch (std::out_of_range &) {
			return;
		}
	}
	void makeLRU(int cacheSetNumber, int capacity) {
		if (cacheSets.count(cacheSetNumber) > 0)
			deleteExistingSet(cacheSetNumber);
		auto set = new LRUCachedSet<SETELEM>(capacity);
		cacheSets.emplace(cacheSetNumber, set);
	}
	void makeUnlimited(int cacheSetNumber) {
		if (cacheSets.count(cacheSetNumber) > 0)
			deleteExistingSet(cacheSetNumber);
		auto set = new UnlimitedCachedSet<SETELEM>();
		cacheSets.emplace(cacheSetNumber, set);
	}
	void add(int cacheSetNumber, const std::string &filename, std::shared_ptr<SETELEM> elem) {
		assert(elem);
		CachedSet<SETELEM> *set = nullptr;
		if (cacheSets.count(cacheSetNumber) == 0) {
			// that set didn't exist, add it as default (unlimited)
			set = new UnlimitedCachedSet<SETELEM>();
			cacheSets.emplace(cacheSetNumber, set);
		} else {
			set = cacheSets.at(cacheSetNumber);
		}

		set->add(filename, elem);
	}
	void remove(int cacheSetNumber, const std::string &filename) {
		if (cacheSets.count(cacheSetNumber) != 0) {
			// That set doesn't exist; cannot remove
			return;
		}
		cacheSets.at(cacheSetNumber)->remove(filename);
	}
	void removeAll(std::string filename) {
		for (auto &number_set_pair : cacheSets) {
			number_set_pair.second->remove(filename);
		}
	}
	virtual std::shared_ptr<SETELEM> get(const std::string &filename) {
		for (auto &number_set_pair : cacheSets) {
			std::shared_ptr<SETELEM> elem = number_set_pair.second->get(filename);
			if (elem) {
				return elem;
			}
		}
		return nullptr;
	}
};

class ImageCacheController : public CacheController<Wrapped_SDL_Surface> {
public:
	void add(int cacheSetNumber, const std::string &filename, const std::shared_ptr<Wrapped_SDL_Surface> &surface);
	std::shared_ptr<Wrapped_SDL_Surface> get(const std::string &filename) override;
};

class SoundCacheController : public CacheController<Wrapped_Mix_Chunk> {
public:
	void add(int cacheSetNumber, const std::string &filename, const std::shared_ptr<Wrapped_Mix_Chunk> &chunk);
	std::shared_ptr<Wrapped_Mix_Chunk> get(const std::string &filename) override;
};
