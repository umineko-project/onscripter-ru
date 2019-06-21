/**
 *  DirPaths.cpp
 *  ONScripter-RU
 *
 *  Multiple directory paths access.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Support/DirPaths.hpp"
#include "Support/Unicode.hpp"

#include <unordered_set>

void DirPaths::add(const DirPaths &dp) {
	paths.insert(paths.end(), dp.paths.begin(), dp.paths.end());
	if (dp.maxPathLen > maxPathLen)
		maxPathLen = dp.maxPathLen;
	if (dp.maxWidePathLen > maxWidePathLen)
		maxWidePathLen = dp.maxWidePathLen;
}

void DirPaths::add(const char *new_paths) {
	const char *prev = new_paths, *next = nullptr;
	while ((next = std::strchr(new_paths, PATH_DELIMITER))) {
		addSingle(prev, next - prev);
		prev = next + 1;
	}
	addSingle(prev, std::strlen(prev));
	deduplicate();
}

void DirPaths::add(const wchar_t *new_paths) {
	const wchar_t *prev = new_paths, *next = nullptr;
	while ((next = wcschr(new_paths, PATH_DELIMITER))) {
		addSingle(prev, next - prev);
		prev = next + 1;
	}
	addSingle(prev, wcslen(prev));
	deduplicate();
}

void DirPaths::deduplicate() {
	auto readIt = paths.begin(), writeIt = paths.begin();
	std::unordered_set<std::string> tmpPaths;
	while (readIt != paths.end()) {
		if (tmpPaths.insert(readIt->path).second)
			*writeIt++ = *readIt;
		++readIt;
	}
	paths.erase(writeIt, paths.end());
}

const std::string DirPaths::getAllPaths() const {
	std::string all;
	auto it = paths.begin();
	if (it != paths.end()) {
		all += (it++)->path;
		while (it != paths.end())
			all += PATH_DELIMITER + (it++)->path;
	}
	return all;
}

DirPaths::Path::Path(const char *s, size_t len) {
	path    = std::string(s, len);
	unicode = hasUnicode(s, len);
	if (unicode)
		wpath = decodeUTF8StringWide(s, static_cast<int>(len + 1));
	else
		wpath = std::wstring(path.begin(), path.end());
}

DirPaths::Path::Path(const wchar_t *s, size_t len) {
	wpath   = std::wstring(s, len);
	unicode = hasUnicode(s, len);
	if (unicode)
		path = decodeUTF16String(wpath);
	else
		path = std::string(wpath.begin(), wpath.end());
}
