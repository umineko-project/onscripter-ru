/**
 *  Nsa.cpp
 *  ONScripter-RU
 *
 *  NSA archive game resources reader.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Readers/Nsa.hpp"
#include "Support/FileIO.hpp"

#include <cstdio>
#include <cstring>

NsaReader::NsaReader(DirPaths &path, size_t nsaoffset)
    : SarReader(path) {
	sar_flag            = true;
	nsa_offset          = nsaoffset;
	num_of_nsa_archives = num_of_ns2_archives = 0;
	nsa_archive_ext                           = "nsa";
	ns2_archive_ext                           = "ns2";
}

NsaReader::~NsaReader() = default;

int NsaReader::open(const char *nsa_path) {
	const DirPaths paths(nsa_path);
	return processArchives(paths);
}

int NsaReader::processArchives(const DirPaths &nsa_path) {

	char archive_name[PATH_MAX];
	sar_flag                = !SarReader::open("arc.sar");
	size_t num_of_nsa_paths = nsa_path.getPathNum();

	for (num_of_nsa_archives = 0; num_of_nsa_archives <= MAX_EXTRA_ARCHIVE; num_of_nsa_archives++) {
		FILE *fp = nullptr;
		for (size_t nd = 0; nd < num_of_nsa_paths && !fp; nd++) {
			if (num_of_nsa_archives == 0)
				std::snprintf(archive_name, PATH_MAX, "%s%s.%s", nsa_path.getPath(nd), NSA_ARCHIVE_NAME, nsa_archive_ext);
			else
				std::snprintf(archive_name, PATH_MAX, "%s%s%zu.%s", nsa_path.getPath(nd), NSA_ARCHIVE_NAME, num_of_nsa_archives, nsa_archive_ext);

			fp = FileIO::openFile(archive_name, "rb");

			if (fp) {
				//sendToLog(LogLevel::Info, "Found archive %s\n", archive_name2); fflush(stdout);
				if (num_of_nsa_archives == 0) {
					archive_info_nsa.file_handle = fp;
					archive_info_nsa.file_name   = copystr(archive_name);
					readArchive(&archive_info_nsa, ARCHIVE_TYPE_NSA, nsa_offset);
				} else {
					archive_info2[num_of_nsa_archives - 1].file_handle = fp;
					archive_info2[num_of_nsa_archives - 1].file_name   = copystr(archive_name);
					readArchive(&archive_info2[num_of_nsa_archives - 1], ARCHIVE_TYPE_NSA, nsa_offset);
				}
			}
		}
		if (!fp) {
			break;
		}
	}

	bool has_ns2 = false;

	for (size_t nd = 0; nd < num_of_nsa_paths; nd++) {
		std::snprintf(archive_name, PATH_MAX, "%s00.%s", nsa_path.getPath(nd), ns2_archive_ext);
		size_t length;
		if (DirectReader::getFile(archive_name, length, nullptr)) {
			has_ns2 = true;
			break;
		}
	}

	if (has_ns2) {
		for (size_t i = MAX_NS2_ARCHIVE; i > 0; i--) {
			FILE *fp = nullptr;
			for (size_t nd = 0; nd < num_of_nsa_paths; nd++) {
				std::snprintf(archive_name, PATH_MAX, "%s%02zu.%s", nsa_path.getPath(nd), i - 1, ns2_archive_ext);
				fp = FileIO::openFile(archive_name, "rb");
				if (fp) {
					archive_info_ns2[num_of_ns2_archives].file_handle = fp;
					archive_info_ns2[num_of_ns2_archives].file_name   = copystr(archive_name);
					readArchive(&archive_info_ns2[num_of_ns2_archives], ARCHIVE_TYPE_NS2);
					num_of_ns2_archives++;
					break;
				}
			}
		}
	}

	if (num_of_nsa_paths == 0 && num_of_ns2_archives == 0) {
		// didn't find any (main) archive files
		sendToLog(LogLevel::Error, "can't open nsa archive file %s.%s ns2_archive_ext\n", NSA_ARCHIVE_NAME, nsa_archive_ext, ns2_archive_ext);
		return -1;
	}

	return 0;
}

const char *NsaReader::getArchiveName() const {
	return "nsa";
}

size_t NsaReader::getNumFiles() {
	size_t total = archive_info.num_of_files; // start with sar files, if any

	total += archive_info_nsa.num_of_files; // add in the arc.nsa files

	for (size_t i = 0; i < num_of_nsa_archives - 1; i++)
		total += archive_info2[i].num_of_files; // add in the arc?.nsa files

	for (size_t i = 0; i < num_of_ns2_archives; i++)
		total += archive_info_ns2[i].num_of_files; // add in the ##.ns2 files

	return total;
}

size_t NsaReader::getFileLengthSub(ArchiveInfo *ai, const char *file_name) {
	size_t i = getIndexFromFile(ai, file_name);

	if (i == ai->num_of_files)
		return 0;

	return ai->fi_list[i].original_length;
}

bool NsaReader::getFile(const char *file_name, size_t &len, uint8_t **buffer) {
	// direct read
	if (DirectReader::getFile(file_name, len, buffer))
		return true;

	// ns2 read
	for (size_t i = 0; i < num_of_ns2_archives; i++) {
		if (getFileSub(&archive_info_ns2[i], file_name, len, buffer))
			return true;
	}

	// nsa read
	if (getFileSub(&archive_info_nsa, file_name, len, buffer))
		return true;

	// nsa? read
	if (num_of_nsa_archives > 0) {
		for (size_t i = 0; i < num_of_nsa_archives - 1; i++) {
			if (getFileSub(&archive_info2[i], file_name, len, buffer))
				return true;
		}
	}

	// sar read
	if (sar_flag)
		return SarReader::getFile(file_name, len, buffer);

	return false;
}

bool NsaReader::getFile(const char *file_name, size_t &len, std::vector<uint8_t> &buffer) {
	if (DirectReader::getFile(file_name, len, buffer))
		return true;

	uint8_t *tmp = nullptr;
	if (getFile(file_name, len, &tmp)) {
		return updateVector(buffer, tmp, len);
	}

	return false;
}
