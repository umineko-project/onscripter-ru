/**
 *  Direct.cpp
 *  ONScripter-RU
 *
 *  Direct filesystem game resources reader.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Readers/Direct.hpp"
#include "Engine/Components/Async.hpp"
#include "Support/FileIO.hpp"
#include "Support/Unicode.hpp"

#include <algorithm>
#include <string>

FILE *DirectReader::lookupFile(const char *path, const char *mode) {
	FILE *fp  = nullptr;
	size_t sz = archive_path.getPathNum();

#ifdef WIN32
	// Attempt to go fast path on Windows if possible
	bool unicode = hasUnicode(path, std::strlen(path));
	for (size_t n = 0; !fp && n < sz; n++)
		fp = FileIO::openFile(path, mode, archive_path.getPath(n), unicode || archive_path.isUnicodePath(n));
#else
	for (size_t n = 0; !fp && n < sz; n++)
		fp = FileIO::openFile(path, mode, archive_path.getPath(n));
#endif

	return fp;
}

bool DirectReader::getFile(const char *file_name, size_t &len, uint8_t **buffer) {
	return FileIO::readFile(lookupFile(file_name, "rb"), len, buffer, true);
}

bool DirectReader::getFile(const char *file_name, size_t &len, std::vector<uint8_t> &buffer) {
	return FileIO::readFile(lookupFile(file_name, "rb"), len, buffer, true);
}

char *DirectReader::completePath(const char *path, FileType type, size_t *len) {
	size_t sz = archive_path.getPathNum();
	std::string fpath(path);

#ifdef WIN32
	// Attempt to go fast path on Windows if possible
	bool unicode = hasUnicode(path, std::strlen(path));
	for (size_t n = 0; n < sz; n++) {
		std::string currpath = archive_path.getPath(n) + fpath;
		if (FileIO::accessFile(currpath, type, len, unicode || archive_path.isUnicodePath(n)))
			return copystr(currpath.c_str());
	}
#else
	for (size_t n = 0; n < sz; n++) {
		std::string currpath = archive_path.getPath(n) + fpath;
		if (FileIO::accessFile(currpath, type, len))
			return copystr(currpath.c_str());
	}
#endif

	return nullptr;
}

uint8_t DirectReader::read8(FILE *fp) {
	uint8_t ret = 0;
	if (std::fread(&ret, 1, 1, fp) == 1)
		return ret;

	return 0;
}

uint16_t DirectReader::read16(FILE *fp) {
	uint8_t buf[2];
	if (std::fread(&buf, 2, 1, fp) == 1)
		return buf[0] << 8 | buf[1];

	return 0;
}

uint32_t DirectReader::read32(FILE *fp) {
	uint32_t ret = 0;
	uint8_t buf[4];

	if (std::fread(&buf, 4, 1, fp) == 1) {
		ret = buf[0];
		ret = ret << 8 | buf[1];
		ret = ret << 8 | buf[2];
		ret = ret << 8 | buf[3];
	}

	return ret;
}

void DirectReader::write8(FILE *fp, uint8_t ch) {
	if (std::fwrite(&ch, 1, 1, fp) != 1)
		sendToLog(LogLevel::Warn, "Warning: writeChar failed\n");
}

void DirectReader::write16(FILE *fp, uint16_t ch) {
	uint8_t buf[2];

	buf[0] = (ch >> 8) & 0xff;
	buf[1] = ch & 0xff;
	if (std::fwrite(&buf, 2, 1, fp) != 1)
		sendToLog(LogLevel::Warn, "Warning: writeShort failed\n");
}

void DirectReader::write32(FILE *fp, uint32_t ch) {
	uint8_t buf[4];

	buf[0] = static_cast<uint8_t>((ch >> 24) & 0xff);
	buf[1] = static_cast<uint8_t>((ch >> 16) & 0xff);
	buf[2] = static_cast<uint8_t>((ch >> 8) & 0xff);
	buf[3] = static_cast<uint8_t>(ch & 0xff);
	if (std::fwrite(&buf, 4, 1, fp) != 1)
		sendToLog(LogLevel::Warn, "Warning: writeLong failed\n");
}

int DirectReader::open(const char * /*name*/) {
	return 0;
}

int DirectReader::close() {
	return 0;
}

const char *DirectReader::getArchiveName() const {
	return "direct";
}

size_t DirectReader::getNumFiles() {
	return 0;
}
