/**
 *  limited_queue.hpp
 *  ONScripter-RU
 *
 *  Fast queue header needed for very tight sections.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <cstdlib>
#include <mm_malloc.h>

template <typename T, size_t MIN_AMOUNT, size_t CACHE_LINE_SIZE>
class base_limited_queue {
protected:
	static constexpr size_t buffer_line = sizeof(T) <= CACHE_LINE_SIZE ? CACHE_LINE_SIZE :
											(sizeof(T) / CACHE_LINE_SIZE + (sizeof(T) % CACHE_LINE_SIZE == 0 ? 0 : 1)) * CACHE_LINE_SIZE;
	static constexpr size_t elements_per_line = buffer_line / sizeof(T);
	static constexpr size_t lines_per_block = MIN_AMOUNT / elements_per_line + (MIN_AMOUNT % elements_per_line == 0 ? 0 : 1);
	static constexpr size_t elements_per_block = (MIN_AMOUNT / elements_per_line + (MIN_AMOUNT % elements_per_line == 0 ? 0 : 1)) * elements_per_line;
	
	struct Block {
		union Line {
			char full_size[buffer_line];
			T elements[elements_per_line];
		} table[lines_per_block];
	} *queue;
	
	size_t head {0};
	size_t tail {0};
	size_t count {0};
	size_t blocks {1};
	size_t curr_size {elements_per_block};
	
	FORCE_INLINE T *getRaw(size_t i) {
		size_t b = i / elements_per_block;
		i -= (elements_per_block * b);
		return &queue[b].table[i / elements_per_line].elements[i % elements_per_line];
	}
	
	FORCE_INLINE void add() {
		if (UNLIKELY(count == curr_size)) {
			queue = static_cast<Block *>(realloc(queue, (blocks + 1)*sizeof(Block))); //-V701
			if (!queue)
				throw std::runtime_error("Cannot reallocate limited_queue");
			size_t size = curr_size - head - 1; // head—old_end
			curr_size = ++blocks * elements_per_block;
			size_t dst_head = curr_size - size - 1; //new_head
			std::memcpy(getRaw(dst_head), getRaw(head), size); // copy head to the end
			head = dst_head;
		}
		tail++;
		count++;
		if (tail == curr_size)
			tail = 0;
	}
public:
	base_limited_queue() {
		queue = static_cast<Block *>(_mm_malloc(sizeof(Block), CACHE_LINE_SIZE));
		if (!queue)
			throw std::runtime_error("Cannot allocate limited_queue");
		//std::cout << buffer_line << ' ' << elements_per_block << ' ' << lines_per_block << ' ' << elements_per_line << std::endl;
	}
	
	base_limited_queue(const base_limited_queue &) = delete;
	base_limited_queue operator &= (const base_limited_queue &) = delete;
	
	~base_limited_queue() {
		_mm_free(queue);
		queue = nullptr;
	}
	
	void pop() {
		head++;
		count--;
		if (head == curr_size)
			head = 0;
	}
	
	FORCE_INLINE T &front() {
		return *getRaw(head);
	}
	
	FORCE_INLINE T &back() {
		return *getRaw(tail ? tail - 1 : curr_size - 1);
	}
	
	FORCE_INLINE bool empty() {
		return count == 0;
	}
};

template <typename T, size_t MIN_AMOUNT=10, size_t CACHE_LINE_SIZE=64>
class limited_queue_z : public base_limited_queue<T, MIN_AMOUNT, CACHE_LINE_SIZE> {
public:
	void emplace() {
		std::memset(this->getRaw(this->tail), 0, sizeof(T));
		this->add();
	}
	
	T &emplace_get() {
		// This is less optimal, but returning p as below is dangerous in case of
		// memory reallocation
		
		emplace();
		return this->back();
	}
};

template <typename T, size_t MIN_AMOUNT=10, size_t CACHE_LINE_SIZE=64>
class limited_queue : public base_limited_queue<T, MIN_AMOUNT, CACHE_LINE_SIZE> {
public:
	void emplace() {
		this->getRaw(this->tail) = T();
		this->add();
	}
	
	T &emplace_get() {
		auto p = this->getRaw(this->tail);
		*p = T();
		this->add();
		return this->back();
	}
};
