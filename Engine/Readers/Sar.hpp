/**
 *  Sar.hpp
 *  ONScripter-RU
 *
 *  SAR archive game resources reader.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Readers/Direct.hpp"

class SarReader : public DirectReader {
public:
	SarReader(DirPaths &path);
	~SarReader() override;

	int open(const char *name) override;
	int close() override;
	const char *getArchiveName() const override;
	size_t getNumFiles() override;

	bool getFile(const char *file_name, size_t &len, uint8_t **buffer = nullptr) override;
	bool getFile(const char *file_name, size_t &len, std::vector<uint8_t> &buffer) override;

protected:
	ArchiveInfo archive_info;
	ArchiveInfo *root_archive_info, *last_archive_info;
	size_t num_of_sar_archives;

	int readArchive(ArchiveInfo *ai, int archive_type = ARCHIVE_TYPE_SAR, size_t offset = 0);
	size_t getIndexFromFile(ArchiveInfo *ai, const char *file_name);
	bool getFileSub(ArchiveInfo *ai, const char *file_name, size_t &len, uint8_t **buffer);

	bool updateVector(std::vector<uint8_t> &buffer, uint8_t *tmp, size_t len);
};
