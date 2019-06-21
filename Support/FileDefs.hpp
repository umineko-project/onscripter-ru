/**
 *  FileDefs.hpp
 *  ONScripter-RU
 *
 *  Contains code for basic filesystem definitions.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#ifdef LINUX
#include <linux/limits.h>
#endif

#include <string>
#include <cstddef>

enum class FileType {
	Any,
	File,
	Directory,
	URL
};

// Logging API
enum class LogLevel {
	Info,
	Warn,
	Error
};

void sendToLog(LogLevel level, const char *fmt, ...);

#ifdef WIN32
const char DELIMITER              = '\\';
const char CURRENT_REL_PATH[]     = ".\\";
const size_t CURRENT_REL_PATH_LEN = 2;
const char PATH_DELIMITER         = ';';
#else
const char DELIMITER              = '/';
const char CURRENT_REL_PATH[]     = "./";
const size_t CURRENT_REL_PATH_LEN = 2;
const char PATH_DELIMITER         = ':';
#endif

#ifdef WIN32
// Windows supports either delimiter including mixed ones
inline void translatePathSlashes(std::string &) {}
inline void translatePathSlashes(char *) {}
#else
// On other systems we will perform the conversion to a native delimiter
inline void translatePathSlashes(std::string &path) {
	std::replace(path.begin(), path.end(), '\\', '/');
}
inline void translatePathSlashes(char *path) {
	for (size_t i = 0; path[i]; i++) {
		if (path[i] == '\\')
			path[i] = DELIMITER;
	}
}
#endif
