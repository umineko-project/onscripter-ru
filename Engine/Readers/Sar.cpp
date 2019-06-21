/**
 *  Sar.cpp
 *  ONScripter-RU
 *
 *  SAR archive game resources reader.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Readers/Sar.hpp"
#include "Support/FileIO.hpp"

SarReader::SarReader(DirPaths &path)
    : DirectReader(path) {
	root_archive_info = last_archive_info = &archive_info;
	num_of_sar_archives                   = 0;
}

SarReader::~SarReader() {
	close();
}

int SarReader::open(const char *name) {
	ArchiveInfo *info = new ArchiveInfo();

	if ((info->file_handle = lookupFile(name, "rb")) == nullptr) {
		delete info;
		return -1;
	}

	info->file_name = new char[std::strlen(name) + 1];
	std::memcpy(info->file_name, name, strlen(name) + 1);

	readArchive(info);

	last_archive_info->next = info;
	last_archive_info       = last_archive_info->next;
	num_of_sar_archives++;

	return 0;
}

int SarReader::readArchive(ArchiveInfo *ai, int archive_type, size_t offset) {
	unsigned int i = 0;

	/* Read header */
	for (size_t j = 0; j < offset; j++)
		read8(ai->file_handle);
	if (archive_type == ARCHIVE_TYPE_NS2) {
		// new archive type since NScr2.91
		// - header starts with base_offset (byte-swapped), followed by
		//   filename data - doesn't tell how many files!
		// - filenames are surrounded by ""s
		// - new NS2 filename def: "filename", length (4bytes, swapped)
		// - no compression type? really, no compression.
		// - not sure if NS2 uses key_table or not, using default funcs for now
		ai->base_offset = swap32(read32(ai->file_handle));
		ai->base_offset += offset;

		// need to parse the whole header to see how many files there are
		ai->num_of_files  = 0;
		size_t cur_offset = offset + 5;
		// there's an extra byte at the end of the header, not sure what for
		while (cur_offset < ai->base_offset) {
			//skip the beginning double-quote
			cur_offset++;
			do
				cur_offset++;
			while (std::fgetc(ai->file_handle) != '"');
			read32(ai->file_handle);
			cur_offset += 4;
			ai->num_of_files++;
		}
		ai->fi_list = new FileInfo[ai->num_of_files];

		// now go back to the beginning and read the file info
		cur_offset = ai->base_offset;
		FileIO::seekFile(ai->file_handle, 4 + offset, SEEK_SET);
		for (i = 0; i < ai->num_of_files; i++) {
			unsigned int count = 0;
			//skip the beginning double-quote
			uint8_t ch = std::fgetc(ai->file_handle);
			//error if _not_ a double-quote
			if (ch != '"') {
				sendToLog(LogLevel::Error, "file does not seem to be a valid NS2 archive\n");
				return -1;
			}
			while ((ch = std::fgetc(ai->file_handle)) != '"') {
				if ('a' <= ch && ch <= 'z')
					ch += 'A' - 'a';
				ai->fi_list[i].name[count++] = ch;
			}
			ai->fi_list[i].name[count]     = '\0';
			ai->fi_list[i].offset          = cur_offset;
			ai->fi_list[i].length          = swap32(read32(ai->file_handle));
			ai->fi_list[i].original_length = ai->fi_list[i].length;
			cur_offset += ai->fi_list[i].length;
		}
		//
		// old NSA filename def: filename, ending '\0' byte , compr-type byte,
		// start (4byte), length (4byte))
	} else {
		ai->num_of_files = read16(ai->file_handle);
		ai->fi_list      = new struct FileInfo[ai->num_of_files];

		ai->base_offset = read32(ai->file_handle);
		ai->base_offset += offset;

		for (i = 0; i < ai->num_of_files; i++) {
			uint8_t ch;
			int count = 0;

			while ((ch = std::fgetc(ai->file_handle))) {
				if ('a' <= ch && ch <= 'z')
					ch += 'A' - 'a';
				ai->fi_list[i].name[count++] = ch;
			}
			ai->fi_list[i].name[count] = ch;

			if (archive_type == ARCHIVE_TYPE_NSA && read8(ai->file_handle) != 0) {
				sendToLog(LogLevel::Error, "Reading of %s might fail due to compression.\n"
				                           "Refrain from using any compression on media files!\n",
				          ai->fi_list[i].name);
			}

			ai->fi_list[i].offset = read32(ai->file_handle) + ai->base_offset;
			ai->fi_list[i].length = read32(ai->file_handle);

			if (archive_type == ARCHIVE_TYPE_NSA) {
				ai->fi_list[i].original_length = read32(ai->file_handle);
			} else {
				ai->fi_list[i].original_length = ai->fi_list[i].length;
			}
		}
	}

	return 0;
}

int SarReader::close() {
	ArchiveInfo *info = archive_info.next;

	for (size_t i = 0; i < num_of_sar_archives; i++) {
		last_archive_info = info;
		info              = info->next;
		delete last_archive_info;
	}
	num_of_sar_archives = 0;

	return 0;
}

const char *SarReader::getArchiveName() const {
	return "sar";
}

size_t SarReader::getNumFiles() {
	ArchiveInfo *info = archive_info.next;
	size_t num        = 0;

	for (size_t i = 0; i < num_of_sar_archives; i++) {
		num += info->num_of_files;
		info = info->next;
	}

	return num;
}

size_t SarReader::getIndexFromFile(ArchiveInfo *ai, const char *file_name) {

	char *name_copy = copystr(file_name);

	for (size_t i = 0; name_copy[i]; i++) {
		if ('a' <= name_copy[i] && name_copy[i] <= 'z')
			name_copy[i] += 'A' - 'a';
		else if (name_copy[i] == '/')
			name_copy[i] = '\\';
	}

	size_t i;
	for (i = 0; i < ai->num_of_files; i++) {
		if (equalstr(name_copy, ai->fi_list[i].name))
			break;
	}

	freearr(&name_copy);

	return i;
}

bool SarReader::getFileSub(ArchiveInfo *ai, const char *file_name, size_t &len, uint8_t **buffer) {
	size_t i = getIndexFromFile(ai, file_name);
	if (i == ai->num_of_files)
		return false;

	len = ai->fi_list[i].length;

	if (len > 0 && buffer) {
		*buffer = new uint8_t[len + 1];

		FileIO::seekFile(ai->file_handle, ai->fi_list[i].offset, SEEK_SET);
		if (std::fread(*buffer, len, 1, ai->file_handle) != 1)
			throw std::runtime_error("Error reading file");
	}

	return true;
}

bool SarReader::getFile(const char *file_name, size_t &len, uint8_t **buffer) {
	if (DirectReader::getFile(file_name, len, buffer))
		return true;

	ArchiveInfo *info = archive_info.next;

	for (size_t i = 0; i < num_of_sar_archives; i++) {
		if (getFileSub(info, file_name, len, buffer))
			return true;
		info = info->next;
	}

	return false;
}

bool SarReader::getFile(const char *file_name, size_t &len, std::vector<uint8_t> &buffer) {
	if (DirectReader::getFile(file_name, len, buffer))
		return true;

	uint8_t *tmp = nullptr;
	if (getFile(file_name, len, &tmp)) {
		return updateVector(buffer, tmp, len);
	}

	return false;
}

bool SarReader::updateVector(std::vector<uint8_t> &buffer, uint8_t *tmp, size_t len) {
	// We should not really need this, so let's just have a low-speed version for completeness.
	if (tmp) {
		if (buffer.size() < len + 1)
			buffer.resize(len + 1);
		std::memcpy(buffer.data(), tmp, len);
		freearr(&tmp);
	}
	return true;
}
