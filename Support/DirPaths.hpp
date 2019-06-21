/**
 *  DirPaths.hpp
 *  ONScripter-RU
 *
 *  Multiple directory paths access.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Support/FileDefs.hpp"

#include <memory>
#include <string>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

class DirPaths {
public:
	DirPaths() = default;
	DirPaths(const char *new_paths) {
		add(new_paths);
	}
	DirPaths(const wchar_t *new_wpaths) {
		add(new_wpaths);
	}

	// Specifics:
	// 1. Empty paths will not be added unless there are no paths.
	// 2. Added empty paths are assumed to be "." paths.
	// 3. Each path will be DELIMITER terminated.
	void add(const char *new_paths);
	void add(const wchar_t *new_wpaths);
	void add(const DirPaths &dp);

	// paths numbered from 0 to num_paths-1
	const char *getPath(size_t n) const {
		return paths[n].path.c_str();
	}
	const wchar_t *getWidePath(size_t n) const {
		return paths[n].wpath.c_str();
	}
	bool isUnicodePath(size_t n) const {
		return paths[n].unicode;
	}
	// Returns the number of paths contained
	size_t getPathNum() const {
		return paths.size();
	}
	// Returns the length of the longest path
	size_t getMaxPathLen() const {
		return maxPathLen;
	}
	size_t getMaxWidePathLen() const {
		return maxWidePathLen;
	}
	// Returns a delimited string containing all paths
	const std::string getAllPaths() const;

private:
	struct Path {
		Path(const char *s, size_t len);
		Path(const wchar_t *s, size_t len);
		std::string path;
		std::wstring wpath;
		bool unicode{false};
	};

	template <typename T>
	void addSingle(const T *path, size_t len) {
		if (len == 0 && paths.empty()) {
			paths.emplace_back(CURRENT_REL_PATH, CURRENT_REL_PATH_LEN);
		} else if (len > 0) {
			if (path[len - 1] == DELIMITER) {
				paths.emplace_back(path, len);
			} else {
				std::basic_string<T> tmp(path, len);
				tmp += DELIMITER;
				paths.emplace_back(tmp.c_str(), tmp.size());
			}
		} else {
			return;
		}
		updateMaxLengths(paths.back());
	}

	// Removes duplicating paths while maintaining path priority
	void deduplicate();

	void updateMaxLengths(const Path &p) {
		if (p.path.size() > maxPathLen)
			maxPathLen = p.path.size();
		if (p.wpath.size() > maxWidePathLen)
			maxWidePathLen = p.wpath.size();
	}

	std::vector<Path> paths;
	size_t maxPathLen{0};
	size_t maxWidePathLen{0};
};
