/**
 *  compatibility.hpp
 *  ONScripter-RU
 *
 *  Compatibility header included by all files, primarily for Windows.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#if !defined(MACOSX) && !defined(IOS) && !defined(LINUX) && !defined(WIN32) && !defined(DROID)
	#error "Unknown operating system configuration!"
#endif

// WinNT version fix
#if defined(WIN32) && !defined(_WIN32_WINNT)
	#define _WIN32_WINNT 0x0501
#endif

// Hide specific constructor attribute
#ifndef CONSTRUCTOR
	#define CONSTRUCTOR __attribute__((constructor)) static void
#endif

// EXPORT macro will make symbol visible in the resulting binary
#ifndef EXPORT
	#define EXPORT __attribute__((visibility("default")))
#endif

// USEDSYM macro will make symbol present in the resulting binary
#ifndef USEDSYM
	#define USEDSYM __attribute__((used))
#endif

// Hide specific inline attributes
#ifndef FORCE_INLINE
	#define FORCE_INLINE __attribute__((always_inline)) inline
	#define FORCE_NOINLINE __attribute__((noinline))
#endif

// route prediction
#ifndef LIKELY
	#define LIKELY(x)       __builtin_expect(!!(x), 1)
	#define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#endif

// Mathematical constants
#ifndef HAS_MATH_CONSTANTS
	#ifndef _USE_MATH_DEFINES
		#define _USE_MATH_DEFINES
	#endif
	#include <cmath>
	#ifndef M_PI
		#define M_PI 3.14159265358979323846
	#endif
	#define HAS_MATH_CONSTANTS
#endif

// Vector types similar to what nvidia cuda offers
#ifndef HAS_VECTOR_TYPES
	#include <cstddef>
	template <typename T, size_t Len>
	struct vec { };

	template <typename T, size_t Len>
	FORCE_INLINE bool operator!=(const vec<T, Len> &lhs, const vec<T, Len> &rhs) { return !(lhs == rhs); }

	template <typename T>
	struct vec<T, 1> {
		T x;
		bool operator==(const vec &rhs) const { return x == rhs.x; }
	};

	template <typename T>
	struct vec<T, 2> {
		T x, y;
		bool operator==(const vec &rhs) const { return x == rhs.x && y == rhs.y ; }
	};

	template <typename T>
	struct vec<T, 3> {
		T x, y, z;
		bool operator==(const vec &rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
	};

	template <typename T>
	struct vec<T, 4> {
		T x, y, z, u;
		bool operator==(const vec &rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z && u == rhs.u; }
	};

	using uchar1 = vec<unsigned char, 1>;
	using uchar2 = vec<unsigned char, 2>;
	using uchar3 = vec<unsigned char, 3>;
	using uchar4 = vec<unsigned char, 4>;

	using char1 = vec<char, 1>;
	using char2 = vec<char, 2>;
	using char3 = vec<char, 3>;
	using char4 = vec<char, 4>;

	using float1 = vec<float, 1>;
	using float2 = vec<float, 2>;
	using float3 = vec<float, 3>;
	using float4 = vec<float, 4>;

	#define HAS_VECTOR_TYPES
#endif

// Until we have C++17
#ifndef HAS_HANDYCPP
	#include <algorithm>
	#include <atomic>
	#include <functional>
	#include <memory>
	#include <stdexcept>
	#include <type_traits>

	namespace cmp {
		template <typename T>
		using unique_ptr_del =
		std::unique_ptr<T, std::function<void (typename std::add_pointer<typename std::remove_extent<T>::type>::type)>>;
		
		// Helper class for storing partially specified objects
		template <typename T>
		class any {
			std::unique_ptr<T> item;
		public:
			any() : item(std::make_unique<T>()) {}
			operator T&() {
				return *item;
			}
		};
		
		// Helper class for optional objects
		template <typename T>
		class optional {
			 // allows set(nullptr) to be a valid value, different from unset() semantically
			std::atomic<bool> has_ {false};
			std::unique_ptr<T> ptr {nullptr};
		public:
			bool has() const {
				return has_.load(std::memory_order_relaxed);
			}
			T &get() const {
				if (UNLIKELY(!has()))
					throw std::runtime_error("Failed to get optional item");
				return *ptr;
			}
			T get(const T&& def) const {
				if (!has()) return def;
				return *ptr;
			}
			T get(const T &def) const {
				if (!has()) return def;
				return *ptr;
			}
			T &set() {
				ptr = std::make_unique<T>();
				has_.store(true, std::memory_order_release);
				return *ptr;
			}
			T &set(T &&t) {
				ptr = std::make_unique<T>(t); // pray that it has a copy constructor? ><
				has_.store(true, std::memory_order_release);
				return *ptr;
			}
			T &set(T &t) {
				ptr = std::make_unique<T>(t); // pray that it has a copy constructor? ><
				has_.store(true, std::memory_order_release);
				return *ptr;
			}
			void unset() {
				has_.store(false, std::memory_order_release);
				ptr = nullptr;
			}
			optional() = default;
			optional(const optional &o) {
				*this = o;
			}
			optional &operator = (const optional &o) {
				if (o.has()) set(o.get());
				else unset();
				return *this;
			}
			optional &operator |= (const optional &o) {
				if (o.has()) set(o.get());
				return *this;
			}
		};
		
		template <typename T>
		FORCE_INLINE constexpr const T clamp(const T &v, const T &lo, const T &hi) {
			return std::max(lo, std::min(v, hi));
		}
		
		// Fixed begin/end for multidimensial arrays
		// Taken from http://stackoverflow.com/a/26950176
		template <typename T>
		FORCE_INLINE typename std::remove_all_extents<T>::type *arrbegin(T& arr) {
			return reinterpret_cast<typename std::remove_all_extents<T>::type*>(&arr);
		}
		template <typename T>
		FORCE_INLINE typename std::remove_all_extents<T>::type *arrend(T& arr) {
			return reinterpret_cast<typename std::remove_all_extents<T>::type*>(&arr)+
			sizeof(T) / sizeof(typename std::remove_all_extents<T>::type);
		}

		// For wrapping capturing lambdas in functions accepting void *user
		// Taken from https://stackoverflow.com/a/33047781
		template<typename Tret, typename T>
		Tret lambda_ptr_exec(T *v) {
			return (Tret) (*v)();
		}

		template<typename Tret = void, typename Tfp = Tret(*)(void *), typename T>
		Tfp lambda_ptr(T &) {
			return (Tfp)lambda_ptr_exec<Tret, T>;
		}
	}
	#define HAS_HANDYCPP
#endif

// To wrap some old legacy code
#ifndef HAS_HANDYC
	#include <cstring>
	#include <algorithm>

	FORCE_INLINE bool equalstr(const char *str1, const char *str2) {
		return (!str1 && !str2) || (str1 && str2 && str1[0] == str2[0] && !std::strcmp(str1,str2));
	}

	FORCE_INLINE void copystr(char *dst, const char *src, size_t s) {
	#if defined(WIN32) || defined(LINUX)
		// Based on OpenBSD implementation of strlcpy
		size_t n = s;
		
		if (n != 0 && --n != 0) {
			do {
				if ((*dst++ = *src++) == 0)
					break;
			} while (--n != 0);
		}
		
		if (n == 0 && s != 0)
			*dst = '\0';
	#else
		strlcpy(dst, src, s);
	#endif
	}

	FORCE_INLINE void appendstr(char *dst, const char *src, size_t s) {
	#if defined(WIN32) || defined(LINUX)
		// Based on OpenBSD implementation of strlcat
		const char *odst = dst;
		size_t n = s;

		/* Find the end of dst and adjust bytes left but don't go past end. */
		while (n-- != 0 && *dst != '\0')
			dst++;
		size_t dlen = dst - odst;
		n = s - dlen;

		if (n-- == 0)
			return;
		while (*src != '\0') {
			if (n != 0) {
				*dst++ = *src;
				n--;
			}
			src++;
		}
		*dst = '\0';
	#else
		strlcat(dst, src, s);
	#endif
	}

	FORCE_INLINE char *copystr(const char *str) {
		if (!str) return nullptr;
		size_t len = std::strlen(str)+1;
		char *newstr = new char[len];
		copystr(newstr, str, len);
		return newstr;
	}

	// Dynamic
	template <typename T>
	FORCE_INLINE T *copyarr(T *src, size_t s) {
		if (!src) return nullptr;
		auto arr = new T[s];
		std::copy(src, src + s, arr);
		return arr;
	}

	// Static
	template <typename T>
	FORCE_INLINE void copyarr(T &dst_arr, const T &src_arr) {
		std::copy(cmp::arrbegin(src_arr), cmp::arrend(src_arr), cmp::arrbegin(dst_arr));
	}

	template <typename T>
	FORCE_INLINE void freearr(T **arr) {
		if (*arr) {
			delete[] *arr;
			*arr = nullptr;
		}
	}

	template <typename T>
	FORCE_INLINE void freevar(T **var) {
		if (*var) {
			delete *var;
			*var = nullptr;
		}
	}

	FORCE_INLINE uint16_t swap16(uint16_t v) {
		return __builtin_bswap16(v);
	}

	FORCE_INLINE uint32_t swap32(uint32_t v) {
		return __builtin_bswap32(v);
	}

	FORCE_INLINE uint64_t swap64(uint64_t v) {
		return __builtin_bswap64(v);
	}

	#define HAS_HANDYC
#endif
