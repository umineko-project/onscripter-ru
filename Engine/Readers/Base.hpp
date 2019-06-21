/**
 *  Base.hpp
 *  ONScripter-RU
 *
 *  Base class of game resources reader.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Support/FileDefs.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <cstdio>

class BaseReader {
public:
	enum {
		ARCHIVE_TYPE_NONE = 0,
		ARCHIVE_TYPE_SAR  = 1,
		ARCHIVE_TYPE_NSA  = 2,
		ARCHIVE_TYPE_NS2  = 3 //new format since NScr2.91, uses ext ".ns2"
	};

	struct FileInfo {
		char name[PATH_MAX]{};
		size_t offset{0};
		size_t length{0};
		size_t original_length{0};
	};

	struct ArchiveInfo {
		ArchiveInfo *next{nullptr};
		FILE *file_handle{nullptr};
		char *file_name{nullptr};
		FileInfo *fi_list{nullptr};
		size_t num_of_files{0};
		size_t base_offset{0};
		ArchiveInfo()                    = default;
		ArchiveInfo(const ArchiveInfo &) = delete;
		ArchiveInfo operator=(const ArchiveInfo &) = delete;
		~ArchiveInfo() {
			if (file_handle)
				std::fclose(file_handle);
			delete[] file_name;
			delete[] fi_list;
		}
	};

	BaseReader()                   = default;
	BaseReader(const BaseReader &) = delete;
	BaseReader &operator=(const BaseReader &) = delete;
	virtual ~BaseReader()                     = default;

	virtual int open(const char *name = nullptr) = 0;
	virtual int close()                          = 0;

	virtual const char *getArchiveName() const = 0;
	virtual size_t getNumFiles()               = 0;

	// Generic interfaces
	virtual bool getFile(const char *file_name, size_t &len, uint8_t **buffer = nullptr)   = 0;
	virtual bool getFile(const char *file_name, size_t &len, std::vector<uint8_t> &buffer) = 0;

	virtual char *completePath(const char *path, FileType type = FileType::Any, size_t *len = nullptr) = 0;
};
