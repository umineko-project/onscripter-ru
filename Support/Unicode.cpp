/**
 *  Unicode.cpp
 *  ONScripter-RU
 *
 *  Contains code to convert between UTF-8 and a variant of UTF-16.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Support/Unicode.hpp"

int decodeUTF8Symbol(const char *inBuf, uint32_t &codepoint) {
	int charpos    = 0;
	codepoint      = 0;
	uint32_t state = 0;
	while (decodeUTF8(&state, &codepoint, inBuf[charpos])) charpos++;
	return charpos + 1;
}

template <typename T>
inline void decodeUTF8StringInternal(const char *inBuf, T &outBuf, int endpos) {
	int charpos        = 0;
	uint32_t codepoint = 0;
	uint32_t state     = 0;
	while (true) {
		codepoint = 0;
		state     = 0;
		while (decodeUTF8(&state, &codepoint, inBuf[charpos])) charpos++;
		charpos++;

		if (codepoint == 0 || endpos == charpos)
			break;

		// We are actually returning characters as UTF-16 codes, shorten to two bytes
		outBuf += static_cast<typename T::value_type>(codepoint);
	}
}

template <>
void decodeUTF8String(const char *inBuf, std::u16string &outBuf, int endpos) {
	decodeUTF8StringInternal(inBuf, outBuf, endpos);
}

template <>
void decodeUTF8String(const char *inBuf, std::wstring &outBuf, int endpos) {
	decodeUTF8StringInternal(inBuf, outBuf, endpos);
}

template <typename T>
static inline std::string decodeUTF16StringGeneric(T &&inBuf) {
	std::string result;
	// Taken from UTF-8 CPP
	for (auto cp : inBuf) {
		if (cp < 0x80) { // one octet
			result += static_cast<char>(cp);
		} else if (cp < 0x800) { // two octets
			result += static_cast<char>((cp >> 6) | 0xc0);
			result += static_cast<char>((cp & 0x3f) | 0x80);
		} else if (cp < 0x10000) { // three octets
			result += static_cast<char>((cp >> 12) | 0xe0);
			result += static_cast<char>(((cp >> 6) & 0x3f) | 0x80);
			result += static_cast<char>((cp & 0x3f) | 0x80);
		} else { // four octets
			result += static_cast<char>((cp >> 18) | 0xf0);
			result += static_cast<char>(((cp >> 12) & 0x3f) | 0x80);
			result += static_cast<char>(((cp >> 6) & 0x3f) | 0x80);
			result += static_cast<char>((cp & 0x3f) | 0x80);
		}
	}
	return result;
}

template <>
std::string decodeUTF16String(std::u16string &&inBuf) {
	return decodeUTF16StringGeneric(inBuf);
}

template <>
std::string decodeUTF16String(std::wstring &&inBuf) {
	return decodeUTF16StringGeneric(inBuf);
}

template <>
std::string decodeUTF16String(std::u16string &inBuf) {
	return decodeUTF16StringGeneric(inBuf);
}

template <>
std::string decodeUTF16String(std::wstring &inBuf) {
	return decodeUTF16StringGeneric(inBuf);
}

template <>
std::string decodeUTF16String(char16_t *inBuf) {
	return decodeUTF16StringGeneric(std::u16string(inBuf));
}

template <>
std::string decodeUTF16String(wchar_t *inBuf) {
	return decodeUTF16StringGeneric(std::wstring(inBuf));
}

const uint8_t utf8d[]{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 00..1f
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 20..3f
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 40..5f
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 60..7f
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, // 80..9f
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, // a0..bf
	8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // c0..df
	0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 0x3,                 // e0..ef
	0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,                 // f0..ff
	0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4, 0x6, 0x1, 0x1, 0x1, 0x1,                 // s0..s0
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, // s1..s2
	1, 2, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, // s3..s4
	1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, // s5..s6
	1, 3, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // s7..s8
};
