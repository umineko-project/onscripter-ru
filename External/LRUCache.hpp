//
// Copyright Erik Dubbelboer and other contributors. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// Based loosely on http://timday.bitbucket.org/lru.html by Tim Day.
//
// Possible improvements:
//  - return an interator instead of a value.
//

#ifndef __LRUCACHE_HPP__
#define __LRUCACHE_HPP__

#include <list>
#include <unordered_map>
#include <cassert>


/**
 * Simple Least Recently Used (LRU) cache.
 *
 * This cache will discards the least recently used item when space is needed.
 *
 * Both KeyType and ItemType can be any type.
 * Keep in mind that internally an std::unordered_map is used to when using strings as keys use std::string.
 * When ItemType is a pointer the destructor will be called on eviction.
 */
template <class KeyType, class ItemType, template<typename...> class MapType = std::unordered_map, class Hash = std::hash<KeyType>, class Pred = std::equal_to<KeyType> >
class LRUCache {
private:
	using KeyTypeList = std::list<KeyType>;
	using KeyItemMap = MapType<KeyType, std::pair<ItemType, typename KeyTypeList::iterator>, Hash, Pred>;
	
	KeyTypeList lru;    // List of recently accessed items. The last item is the most recent.
	KeyItemMap  cache;  // Map of cached items.
	size_t      capacity;
	bool destruct_pointers;
	
	
	/**
	 * These two classes are helper classes. They help make sure destructors are
	 * called when ItemType is a pointer type.
	 * The compiler will select the correct class to use based on ItemType.
	 */
	template<typename T>
	class Pointer {
	public:
		static void Delete(const T  /*v*/) {
			// Not a pointer so do nothing.
		}
	};
	
	template<typename T>
	class Pointer<T*> {
	public:
		static void Delete(T *v) {
			delete v;
		}
	};
	
	
	/**
	 * Remove the least recently accessed element.
	 */
	void evict() {
		const auto i = cache.find(lru.front());
		
		assert(i != cache.end());
		
		// Make sure the destructor is called.
		if (destruct_pointers)
			Pointer<ItemType>::Delete(i->second.first);

		cache.erase(i);
		lru.pop_front();
	}
	
	
public:
	explicit LRUCache(size_t capacity, bool destruct_pointers=true) : capacity(capacity), destruct_pointers(destruct_pointers) {
	}
	
	
	size_t size() {
		return capacity;
	}
	
	
	/**
	 * resize the cache.
	 */
	void resize(size_t cap) {
		while (cache.size() > cap) {
			evict();
		}
		
		capacity = cap;
	}
	
	
	/**
	 * Get the entry at with the specified key.
	 *
	 * If the key was not set the default value for ItemType is returned.
	 * For example for pointers this will be a NULL pointer. For integers
	 * it will be 0.
	 */
	ItemType get(const KeyType& key) {
		const auto i = cache.find(key);
		
		if (i == cache.end()) {
			throw 0;  // Return the default value for ItemType.
		} else {
			// Move the key back to the end of the lru (making it the most recently visited).
			lru.splice(lru.end(), lru, i->second.second);
			
			return i->second.first;
		}
	}
	
	
	/**
	 * Set the entry at key to value.
	 * Removing the least recently accessed element if needed.
	 */
	void set(const KeyType& key, const ItemType &value) {
		// Setting the capacity to 0 will disable the cache.
		if (capacity == 0) {
			return;
		}
		
		const auto i = cache.find(key);
		
		if (i == cache.end()) {
			// Make sure we have room.
			if (cache.size() == capacity) {
				evict();
			}
			
			// Insert the key into the back of the lru list (making it the most recently visited).
			// We need a pointer to the entry for the map.
			const auto j = lru.insert(lru.end(), key);
			
			// Insert the value into the map.
			cache.insert(std::make_pair(key, std::make_pair(value, j)));
		} else {
			// Make sure the destructor is called.
			if (destruct_pointers)
				Pointer<ItemType>::Delete(i->second.first);
			
			i->second.first = value;
			
			// Move the key back to the end of the lru (making it the most recently visited).
			lru.splice(lru.end(), lru, i->second.second);
		}
	}
	
	
	/**
	 * Remove the element with the specified key.
	 */
	void remove(const KeyType& key) {
		const auto i = cache.find(key);
		
		if (i == cache.end()) {
			return;
		}
	
		// Make sure the destructor is called.
		if (destruct_pointers)
			Pointer<ItemType>::Delete(i->second.first);
	
		cache.erase(i);
		lru.remove(key);
	}
	
	
	/**
	 * Return a list of keys in the order of most to least recently used.
	 */
	KeyTypeList list() {
		return KeyTypeList(lru.rbegin(), lru.rend());
	}
};

#endif  // __LRUCACHE_H__
