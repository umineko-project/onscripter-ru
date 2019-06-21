/**
 *  Direct.hpp
 *  ONScripter-RU
 *
 *  Direct filesystem game resources reader.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Readers/Base.hpp"
#include "Support/DirPaths.hpp"

#include <string>
#include <vector>
#include <cstring>

class DirectReader : public BaseReader {
public:
	DirectReader(DirPaths &path)
	    : archive_path(path) {}

	int open(const char *name) override;
	int close() override;

	const char *getArchiveName() const override;
	size_t getNumFiles() override;

	bool getFile(const char *file_name, size_t &len, uint8_t **buffer) override;
	bool getFile(const char *file_name, size_t &len, std::vector<uint8_t> &buffer) override;
	char *completePath(const char *path, FileType type, size_t *len) override;

protected:
	DirPaths &archive_path;

	uint8_t read8(FILE *fp);
	uint16_t read16(FILE *fp);
	uint32_t read32(FILE *fp);
	void write8(FILE *fp, uint8_t ch);
	void write16(FILE *fp, uint16_t ch);
	void write32(FILE *fp, uint32_t ch);

	FILE *lookupFile(const char *path, const char *mode);
};
