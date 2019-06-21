/**
 *  FileIO.hpp
 *  ONScripter-RU
 *
 *  Contains code to access all sorts of filesystems.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Support/FileDefs.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <climits>

namespace FileIO {
enum class LogMode {
	Unspecified,
	Console,
	File
};

void init(const char *provider, const char *appname);
bool initialised();
void prepareConsole(int cols, int lines, bool force = false);
void waitConsole();
LogMode getLogMode();
void setLogMode(LogMode mode);
void log(LogLevel level, const char *format, va_list args);

size_t getLastDelimiter(const char *path);
char *safePath(const char *path, bool isdir, bool forargs = false);
char *extractDirpath(const char *src);
void terminatePath(char *path, size_t bufsz = PATH_MAX);
bool validatePathCase(const std::string &path, FILE *fp, bool strict = false);

void setPathCaseValidation(bool on);
bool setArguments(int &argc, char **&argv, int sysargc = 0, char **sysargv = nullptr);
bool setStorageDir(bool force_userdir = false);
bool restartApp(const std::vector<char *> &args);
bool shellOpen(const std::string &path, FileType type = FileType::Any);

const char *getLaunchDir();
const char *getWorkingDir();
const char *getHomeDir();
const char *getPlatformSpecificDir(); // may return nullptr
const char *getStorageDir(bool cloud = false);

bool accessFile(const std::string &path, FileType type = FileType::Any, size_t *len = nullptr, bool unicode = true);
int seekFile(FILE *fp, size_t off, int m);
FILE *openFile(const std::string &path, const char *mode, bool unicode = true);
bool readFile(FILE *fp, size_t &len, uint8_t **buffer, bool autoclose = false);
bool readFile(FILE *fp, size_t &len, std::vector<uint8_t> &buffer, bool autoclose = false);
bool writeFile(FILE *fp, const uint8_t *buffer, size_t len, bool autoclose = false);
bool makeDir(const std::string &path, bool recursive = false);
std::vector<std::string> scanDir(const std::string &path, FileType type = FileType::Any);
bool removeDir(const std::string &path);
bool removeFile(const std::string &path);
bool renameFile(const std::string &src, const std::string &dst, bool overwrite = false);
bool fileHandleReopen(const std::string &dst, FILE *src, const char *mode = "w");

// Compatibility wrappers mainly for C strings and stacked calls

inline bool shellOpen(const char *path, const char *root = nullptr, FileType type = FileType::Any) {
	auto fpath = std::string(root ? root : "") + path;
	return shellOpen(fpath, type);
}
inline bool accessFile(const char *path, const char *root = nullptr, FileType type = FileType::Any, size_t *len = nullptr, bool unicode = true) {
	auto fpath = std::string(root ? root : "") + path;
	return accessFile(fpath, type, len, unicode);
}
inline FILE *openFile(const char *path, const char *mode, const char *root = nullptr, bool unicode = true) {
	auto fpath = std::string(root ? root : "") + path;
	return openFile(fpath, mode, unicode);
}
inline bool readFile(const char *path, const char *root, size_t &len, uint8_t **buffer) {
	auto fpath = std::string(root ? root : "") + path;
	return readFile(openFile(fpath, "rb"), len, buffer, true);
}
inline bool readFile(const char *path, const char *root, size_t &len, std::vector<uint8_t> &buffer) {
	auto fpath = std::string(root ? root : "") + path;
	return readFile(openFile(fpath, "rb"), len, buffer, true);
}
inline bool readFile(const std::string &path, size_t &len, uint8_t **buffer) {
	return readFile(openFile(path, "rb"), len, buffer, true);
}
inline bool readFile(const std::string &path, size_t &len, std::vector<uint8_t> &buffer) {
	return readFile(openFile(path, "rb"), len, buffer, true);
}
inline bool writeFile(const char *path, const char *root, const uint8_t *buffer, size_t len) {
	auto fpath = std::string(root ? root : "") + path;
	return writeFile(openFile(fpath, "wb"), buffer, len, true);
}
inline bool makeDir(const char *path, const char *root = nullptr, bool recursive = false) {
	auto fpath = std::string(root ? root : "") + path;
	return makeDir(fpath, recursive);
}
inline bool writeFile(const std::string &path, const uint8_t *buffer, size_t len) {
	return writeFile(openFile(path, "wb"), buffer, len, true);
}
inline bool removeDir(const char *path, const char *root = nullptr) {
	auto fpath = std::string(root ? root : "") + path;
	return removeDir(fpath);
}
inline bool removeFile(const char *path, const char *root = nullptr) {
	auto fpath = std::string(root ? root : "") + path;
	return removeFile(fpath);
}
inline bool renameFile(const char *src, const char *dst, const char *root = nullptr, bool overwrite = false) {
	auto fsrc = std::string(root ? root : "") + src;
	auto fdst = std::string(root ? root : "") + dst;
	return renameFile(fsrc, fdst, overwrite);
}
inline bool fileHandleReopen(const char *dst, FILE *src, const char *mode = "w", const char *root = nullptr) {
	auto fdst = std::string(root ? root : "") + dst;
	return fileHandleReopen(fdst, src, mode);
}
} // namespace FileIO
