/**
 *  Nsa.hpp
 *  ONScripter-RU
 *
 *  NSA archive game resources reader.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"
#include "Engine/Readers/Sar.hpp"

#define MAX_EXTRA_ARCHIVE 9
#define MAX_NS2_ARCHIVE 100
#define NSA_ARCHIVE_NAME "arc"

class NsaReader : public SarReader {
public:
	NsaReader(DirPaths &path, size_t nsaoffset = 0);
	~NsaReader() override;

	int open(const char *nsa_path = nullptr) override;
	int processArchives(const DirPaths &path);

	const char *getArchiveName() const override;
	size_t getNumFiles() override;

	bool getFile(const char *file_name, size_t &len, uint8_t **buffer = nullptr) override;
	bool getFile(const char *file_name, size_t &len, std::vector<uint8_t> &buffer) override;

private:
	bool sar_flag;
	size_t nsa_offset;
	size_t num_of_nsa_archives;
	size_t num_of_ns2_archives;
	const char *nsa_archive_ext;
	const char *ns2_archive_ext;
	struct ArchiveInfo archive_info_nsa;                  // for the arc.nsa file
	struct ArchiveInfo archive_info2[MAX_EXTRA_ARCHIVE];  // for the arc1.nsa, arc2.nsa files
	struct ArchiveInfo archive_info_ns2[MAX_NS2_ARCHIVE]; // for the ##.ns2 files

	size_t getFileLengthSub(ArchiveInfo *ai, const char *file_name);
};
