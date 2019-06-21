/**
 *  Unicode.hpp
 *  ONScripter-RU
 *
 *  Contains code to convert between UTF-8 and a variant of UTF-16.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <string>
#include <type_traits>

#include <cstdint>

extern const uint8_t utf8d[];

inline uint32_t decodeUTF8(uint32_t *state, uint32_t *codep, uint8_t byte) {
	uint32_t type = utf8d[byte];
	*codep        = (*state != 0 /*UTF8_ACCEPT*/) ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & (byte);
	*state        = utf8d[256 + *state * 16 + type];
	return *state;
}

int decodeUTF8Symbol(const char *inBuf, uint32_t &codepoint);

// UTF-8 decoding API

template <typename T>
inline bool hasUnicode(const T *inBuf, size_t len) {
	for (size_t i = 0; i < len; i++) {
		if (static_cast<typename std::make_unsigned<T>::type>(inBuf[i]) > 0x7F)
			return true;
	}
	return false;
}

template <typename T>
void decodeUTF8String(const char *inBuf, T &outBuf, int endpos = -1);

template <>
void decodeUTF8String(const char *inBuf, std::u16string &outBuf, int endpos);

template <>
void decodeUTF8String(const char *inBuf, std::wstring &outBuf, int endpos);

inline std::u16string decodeUTF8StringShort(const char *inBuf, int endpos = -1) {
	std::u16string str;
	decodeUTF8String(inBuf, str, endpos);
	return str;
}

inline std::wstring decodeUTF8StringWide(const char *inBuf, int endpos = -1) {
	std::wstring str;
	decodeUTF8String(inBuf, str, endpos);
	return str;
}

// UTF-8 encoding API

template <typename T>
std::string decodeUTF16String(T &&input);

template <typename T>
std::string decodeUTF16String(T *input);

template <>
std::string decodeUTF16String(std::u16string &&input);

template <>
std::string decodeUTF16String(std::wstring &&input);

template <>
std::string decodeUTF16String(std::u16string &input);

template <>
std::string decodeUTF16String(std::wstring &input);

template <>
std::string decodeUTF16String(wchar_t *input);

template <>
std::string decodeUTF16String(char16_t *input);
