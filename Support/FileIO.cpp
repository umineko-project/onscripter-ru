/**
 *  FileIO.cpp
 *  ONScripter-RU
 *
 *  Contains code to access all sorts of filesystems.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Support/FileIO.hpp"
#include "Support/Unicode.hpp"

#if defined(IOS) && defined(USE_OBJC)
#include "Support/Apple/UIKitWrapper.hpp"
#endif

#include <SDL2/SDL.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#if defined(MACOSX) || defined(IOS)
#include <mach-o/dyld.h>
#include <fcntl.h>
#elif defined(WIN32)
#include <windows.h>
#include <shlobj.h>
#elif defined(LINUX)
#include <sys/wait.h>
#elif defined(DROID)
#include <sys/wait.h>
#include <android/log.h>
#endif

#include <cerrno>
#include <cstdlib>

static const char *providerName;
static const char *applicationName;

void sendToLog(LogLevel level, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	FileIO::log(level, fmt, args);
	va_end(args);
}

bool FileIO::initialised() {
	return providerName && applicationName;
}

void FileIO::init(const char *provider, const char *application) {
	providerName    = provider;
	applicationName = application;
}

size_t FileIO::getLastDelimiter(const char *path) {
	size_t last_delim = 0;
	size_t len        = std::strlen(path);
	for (size_t i = 0; i < len; i++) {
		if (path[i] == '/' || path[i] == '\\') {
			last_delim = i;
		}
	}
	return last_delim;
}

char *FileIO::safePath(const char *path, bool isdir, bool forargs) {
	size_t len   = std::strlen(path);
	size_t bufsz = len + 3;
	char *buf    = new char[bufsz];
	buf[0]       = '\0';
#ifdef WIN32
	// On Windows there are various issues with trailing slashes and unquoted paths
	// Attempt to workaround them. C:\Folder\ could become C:\Folder\", and if we pass
	// an unquoted string to the arguments it is likely to fail.
	// The idea for dirs (normal and for args):
	//                -> .\        -> ""
	// \              -> \         -> ""
	// C:             -> C:\       -> "C:"
	// "\"            -> \         -> ""
	// The idea for files:
	//                ->           -> ""
	// ""\"""         ->           -> ""
	char *dst      = buf;
	bool had_slash = false;

	if (forargs) {
		dst[0] = '"';
		dst++;
	}

	while (*path == '"' || *path == DELIMITER) {
		if (*path == DELIMITER)
			had_slash = true;
		path++;
		len--;
	}

	copystr(dst, path, len + 1);

	if (len > 0) {
		size_t i = len - 1;
		while (dst[i] == DELIMITER || dst[i] == '"') {
			if (dst[i] == DELIMITER)
				had_slash = true;
			dst[i] = '\0';
			len--;
			i--;
			if (i == 0)
				break;
		}
	}

	if (forargs) {
		dst[len]     = '"';
		dst[len + 1] = '\0';
	} else if (isdir) {
		if (len == 0 && !had_slash) {
			dst[len] = '.';
			len++;
		}
		dst[len]     = DELIMITER;
		dst[len + 1] = '\0';
	}
#else
	// On Unix this should just be a call to terminatePath in case of isdir.
	(void)forargs;
	copystr(buf, path, bufsz);
	if (isdir)
		terminatePath(buf, bufsz);
#endif
	return buf;
}

char *FileIO::extractDirpath(const char *path) {
	size_t len = std::strlen(path);
	if (len == 0)
		throw std::runtime_error("Invalid file path");

	do {
		len--;
		if (path[len] == '\\' || path[len] == '/') {
			char *dir    = copystr(path);
			dir[len + 1] = '\0';
			return dir;
		}
	} while (len > 0);

	return copystr(CURRENT_REL_PATH);
}

void FileIO::terminatePath(char *path, size_t bufsz) {
	size_t len = std::strlen(path);
	if (len > 0) {
		if (path[len - 1] != '/' && path[len - 1] != '\\') {
			if (len >= bufsz - 1) {
				throw std::runtime_error("Received too long path!");
			}
			path[len]     = DELIMITER;
			path[len + 1] = '\0';
		}
	} else {
		copystr(path, CURRENT_REL_PATH, bufsz);
	}
}

static bool pathCaseValidation = false;

// WARNING: this code is only meant to be used for DEBUG & DEVELOPMENT!
// This means it comes without any warranty and could generate false alarms!
bool FileIO::validatePathCase(const std::string &path, FILE *fp, bool strict) {
	// Assume open failure as valid case
	bool ok = true;
	if (fp) {
		// All the systems but macOS and Windows probably use case-sensitive FS.
		// Still attempt to perform the necessary checks for a reason.
		// They could mount an NTFS partition after all.

		auto samePaths = [](std::string opened, std::string obtained) {
#ifdef WIN32
			// FindFirstFileNameW does not return hdd name
			size_t disk = opened.find_first_of(':');
			if (disk != std::string::npos)
				opened.replace(opened.begin(), opened.begin() + disk + 1, "");
#endif

			for (auto str : {&opened, &obtained}) {
				auto &p = *str;
				for (auto &c : p)
					if (c == '\\' || c == '/')
						c = DELIMITER;

				// Remove multiple relative sequences: /usr/./././lib -> /usr/lib
				do {
					size_t pos = p.find(CURRENT_REL_PATH);
					if (pos != std::string::npos)
						p.replace(pos, CURRENT_REL_PATH_LEN, "");
					else
						break;
				} while (true);

				// Removing trailing slashes: /usr/ -> usr
				if (p.size() > 0 && *p.begin() == DELIMITER)
					p.replace(p.begin(), p.begin() + 1, "");
				if (p.size() > 0 && *(p.end() - 1) == DELIMITER)
					p.replace(p.end() - 1, p.end(), "");
			}
			return obtained.find(opened) != std::string::npos;
		};

#if defined(WIN32)
		// Try the first hard link which is normally the file itself
		// It may fail with net shares, but I have no desire to rewrite it to use full path iteration

		using t_FindFirstFileNameW = HANDLE(WINAPI *)(LPCWSTR lpFileName, DWORD dwFlags, LPDWORD StringLength, PWSTR LinkName);
		using t_FindClose          = BOOL(WINAPI *)(HANDLE hFindFile);

		static t_FindFirstFileNameW findName;
		static t_FindClose findClose;

		if (!findName || !findClose) {
			auto kernel = SDL_LoadObject("kernel32");
			if (kernel) {
				findName  = reinterpret_cast<t_FindFirstFileNameW>(SDL_LoadFunction(kernel, "FindFirstFileNameW"));
				findClose = reinterpret_cast<t_FindClose>(SDL_LoadFunction(kernel, "FindClose"));
				if (!findName || !findClose) {
					SDL_UnloadObject(kernel);
					throw std::runtime_error("To use path case validation be on Windows Vista or newer!");
				}
			} else {
				throw std::runtime_error("Failed to obtain kernel32!");
			}
		}

		auto wpath = decodeUTF8StringWide(path.c_str());
		wchar_t tmp_path[PATH_MAX];
		DWORD tmp_size = PATH_MAX;
		HANDLE handle  = findName(wpath.c_str(), 0, &tmp_size, tmp_path);
		if (handle != INVALID_HANDLE_VALUE && findClose(handle))
			ok = samePaths(path, decodeUTF16String(tmp_path));
#elif defined(LINUX) || defined(DROID)
		// readlink does not seem to help, so we only use it to obtain the path to iterate through
		int fd = fileno(fp);
		char tmp_path[PATH_MAX]{}; // readlink does not null-terminate
		if (fd >= 0 && readlink(("/proc/self/fd/" + std::to_string(fd)).c_str(), tmp_path, PATH_MAX - 1) != -1) {
			ok = samePaths(path, tmp_path);
			if (ok) {
				char *prev = nullptr, *next = tmp_path;
				do {
					prev = next ? std::strchr(next, DELIMITER) : NULL;
					next = prev ? std::strchr(prev + 1, DELIMITER) : NULL;

					if (prev) {
						prev[0]  = '\0';
						DIR *dir = opendir(tmp_path);
						prev[0]  = DELIMITER;
						if (dir) {
							dirent *entry = nullptr;
							ok            = false;
							if (next)
								next[0] = '\0';
							while ((entry = readdir(dir))) {
								if (!std::strcmp(entry->d_name, prev + 1)) {
									ok = true;
									break;
								}
							}
							if (next)
								next[0] = DELIMITER;
							closedir(dir);
						}
					}
				} while (ok && prev && next);
			}
		}
#else
		// This actually works well but is supported mainly on BSD systems
		int fd = fileno(fp);
		char tmp_path[PATH_MAX]{};
		if (fd >= 0 && fcntl(fd, F_GETPATH, tmp_path) != -1)
			ok = samePaths(path, tmp_path);
#endif

		if (!ok && strict)
			throw std::runtime_error("Received path written in a wrong case: " + path + ". This is fatal for portability!");
	}

	return ok;
}

void FileIO::setPathCaseValidation(bool on) {
	pathCaseValidation = on;
}

bool FileIO::setArguments(int &argc, char **&argv, int sysargc, char **sysargv) {
	static bool obtained = false;
	static int ownArgc;
	static char **ownArgv;
#ifdef WIN32
	static std::vector<std::string> unicodeArgs;
	static std::vector<const char *> unicodeArgsPtrs;
#endif
	if (!obtained) {
		if (sysargv == nullptr && sysargc == 0)
			return false;
#ifdef WIN32
		// On Windows the incoming arguments will be in OEM encoding but we want unicode
		wchar_t **wideArgv = CommandLineToArgvW(GetCommandLineW(), &ownArgc);
		if (!wideArgv)
			return false;

		if (ownArgc != sysargc)
			sendToLog(LogLevel::Warn, "Warning: discovered %d arguments compared to %d received\n", ownArgc, sysargc);

		for (int i = 0; i < ownArgc; i++)
			unicodeArgs.emplace_back(decodeUTF16String(wideArgv[i]));

		LocalFree(wideArgv);

		for (auto &item : unicodeArgs)
			unicodeArgsPtrs.emplace_back(item.c_str());

		ownArgv = const_cast<char **>(unicodeArgsPtrs.data());
#else
		// On all other systems it seems to be dead simple
		ownArgc = sysargc;
		ownArgv = sysargv;
#endif
		obtained = true;
	}
	argc = ownArgc;
	argv = ownArgv;
	return true;
}

bool FileIO::restartApp(const std::vector<char *> &args) {
	(void)args;
#if defined(WIN32)
	// Create a new process and do not wait for it on Windows
	PROCESS_INFORMATION processInfo{};
	STARTUPINFOW startupInfo{};
	startupInfo.cb = sizeof(STARTUPINFOW);

	std::wstring cmdArgs;
	for (auto &arg : args) {
		if (arg) {
			cmdArgs += decodeUTF8StringWide(arg);
			cmdArgs += L' ';
		}
	}

	if (CreateProcessW(nullptr, const_cast<wchar_t *>(cmdArgs.c_str()), nullptr, nullptr, false, 0, nullptr, nullptr, &startupInfo, &processInfo))
		return true;
#elif defined(MACOSX) || defined(LINUX)
	// Use execv on macOS and Linux
	execv(args[0], args.data());
#endif

	// Simply return false on all the other systems, like iOS
	return false;
}

bool FileIO::shellOpen(const std::string &path, FileType type) {
	(void)path;
	(void)type;
#if defined(WIN32)
	// If the function succeeds, it returns a value greater than 32.
	auto wpath = decodeUTF8StringWide(path.c_str());
	return ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > reinterpret_cast<HINSTANCE>(32);
#elif defined(MACOSX)
	auto cmd = "open \"" + path + '"';
	return !system(cmd.c_str());
#elif defined(LINUX)
	auto tryXdgOpen = [](const std::string &target) {
		pid_t child = vfork();
		if (child == -1) {
			// Parent, failed
			sendToLog(LogLevel::Error, "Could not open `%s': fork error: %s\n", target.c_str(), strerror(errno));
		} else if (child) {
			// Parent, success
			int status;
			waitpid(child, &status, 0);
			if (WIFEXITED(status))
				return WEXITSTATUS(status) == 0;
		} else {
			// Child
			execlp("xdg-open", "xdg-open", target.c_str(), nullptr);
			std::exit(EXIT_FAILURE);
		}
		return false;
	};

	auto trySystem = [](const std::string &target) {
		auto browser = std::getenv("BROWSER");
		if (browser) {
			auto cmd = std::string(browser) + " \"" + target + '"';
			return system(cmd.c_str()) == 0;
		}
		return false;
	};

	return tryXdgOpen(path) || (type == FileType::URL && trySystem(path));
#elif defined(IOS) && defined(USE_OBJC)
	return (type == FileType::URL && openURL(path.c_str()));
#elif defined(DROID)
	if (type == FileType::URL) {
		pid_t child = vfork();
		if (child == -1) {
			// Parent, failed
			sendToLog(LogLevel::Error, "Could not open `%s': fork error: %s\n", path.c_str(), strerror(errno));
		} else if (child) {
			// Parent, success
			int status;
			waitpid(child, &status, 0);
			if (WIFEXITED(status))
				return WEXITSTATUS(status) == 0;
		} else {
			// Child, --user 0 may not be necessary prior to 4.2 but it does not do any harm
			execlp("/system/bin/am", "/system/bin/am", "start", "--user", "0", "-a", "android.intent.action.VIEW", "-d", path.c_str(), nullptr);
			std::exit(EXIT_FAILURE);
		}
	}
	return false;
	//std::string cmd =
	//char * args = {"/system/bin/am", "start", "-a", "android.intent.action.VIEW", "http://www.google.com/"};
	//if(fork() == 0) {
	//    execvp("/system/bin/am", args);
	//}

#else
	return false;
#endif
}

const char *FileIO::getLaunchDir() {
	static char launchDir[PATH_MAX];
	static bool obtained = false;
	if (!obtained) {
#if defined(WIN32)
		wchar_t wpath[PATH_MAX];
		if (GetModuleFileNameW(nullptr, wpath, PATH_MAX))
			copystr(launchDir, decodeUTF16String(wpath).c_str(), PATH_MAX);
#elif defined(MACOSX)
		uint32_t bufsize = PATH_MAX;
		_NSGetExecutablePath(launchDir, &bufsize);
#elif defined(LINUX)
		if (readlink("/proc/self/exe", launchDir, PATH_MAX - 1) == -1) {
		}
#elif defined(IOS)
		const char *home = getHomeDir();
		std::snprintf(launchDir, PATH_MAX, "%sDocuments%c", home, DELIMITER);
#elif defined(DROID)
		const char *home = std::getenv("EXTERNAL_STORAGE");
		if (home)
			std::snprintf(launchDir, PATH_MAX, "%s%c%s%c", home, DELIMITER, providerName, DELIMITER);
		else
			sendToLog(LogLevel::Error, "No EXTERNAL_STORAGE available on the device!\n");
#endif
		char *ptr = std::strrchr(launchDir, DELIMITER);
		if (ptr) {
			ptr[1] = '\0';
		} else {
			sendToLog(LogLevel::Error, "LaunchDir: Falling back to current dir!\n");
			copystr(launchDir, CURRENT_REL_PATH, PATH_MAX);
		}
		obtained = true;
	}
	return launchDir;
}

const char *FileIO::getWorkingDir() {
	static char workingDir[PATH_MAX];
	static bool obtained = false;
	if (!obtained) {
#ifdef WIN32
		// Convert UTF-16 working dir to UTF-8 on Windows
		wchar_t wideWorkingDir[PATH_MAX];
		if (_wgetcwd(wideWorkingDir, PATH_MAX)) {
			copystr(workingDir, decodeUTF16String(wideWorkingDir).c_str(), PATH_MAX);
		}
#else
		// Assume UTF-8 support on all other systems
		if (getcwd(workingDir, PATH_MAX)) {
		}
#endif
		terminatePath(workingDir);
		obtained = true;
	}
	return workingDir;
}

const char *FileIO::getHomeDir() {
	static char homeDir[PATH_MAX];
	static bool obtained = false;
	if (!obtained) {

#if defined(WIN32)
		wchar_t wpath[PATH_MAX];
		HRESULT res = SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, wpath);
		if (res == S_OK)
			copystr(homeDir, decodeUTF16String(wpath).c_str(), PATH_MAX);
#else
		// An application that wants to determine its user's home directory should inspect the value
		// of HOME (rather than the value getpwuid(getuid())->pw_dir) since this allows the user to
		// modify their notion of "the home directory" during a login session.
		const char *home = std::getenv("HOME");
		if (home) {
#ifdef IOS
			// We cannot access /private/var from sandbox directly but /var symlink is fine.
			if (!std::strncmp("/private", home, std::strlen("/private")))
				home += std::strlen("/private");
#endif
			copystr(homeDir, home, PATH_MAX);
		}
#endif
		terminatePath(homeDir);
		obtained = true;
	}
	return homeDir;
}

const char *FileIO::getPlatformSpecificDir() {
	static const char *platformSpecificDir = nullptr;
#if defined(MACOSX)
	static char bundleParentDir[PATH_MAX];
	static bool obtained = false;
	if (!obtained) {
		// bundleParentDir/Application.app/Contents/MacOS/Application
		const char *launch = getLaunchDir();
		copystr(bundleParentDir, launch, PATH_MAX);
		bool valid = false;
		for (size_t i = 0; i < 4; i++) {
			char *del = std::strrchr(bundleParentDir, DELIMITER);
			if (del) {
				if (i == 3) {
					del[1] = '\0';
					valid  = true;
				} else {
					del[0] = '\0';
				}
			} else {
				break;
			}
		}

		if (valid)
			platformSpecificDir = bundleParentDir;
		else
			sendToLog(LogLevel::Error, "Failed to obtain bundle parent dir!\n");

		obtained = true;
	}
#elif defined(DROID)
	static char extSDcardDir[PATH_MAX];
	static bool obtained = false;
	if (!obtained) {
		auto tryEnv = [](const char *name) {
			const char *home = std::getenv(name);
			if (home) {
				std::snprintf(extSDcardDir, PATH_MAX, "%s%c%s%c", home, DELIMITER, providerName, DELIMITER);
				if (!accessFile(extSDcardDir, FileType::Directory)) {
					sendToLog(LogLevel::Error, "ExtSDcardDir: %s -> %s failed!\n", name, extSDcardDir);
					extSDcardDir[0] = '\0';
				}
			} else {
				sendToLog(LogLevel::Error, "ExtSDcardDir: %s returned null!\n", name);
			}
		};

		// Try SECONDARY_STORAGE
		tryEnv("SECONDARY_STORAGE");

		// If failed try EXTERNAL_SDCARD_STORAGE
		if (extSDcardDir[0] == '\0')
			tryEnv("EXTERNAL_SDCARD_STORAGE");

		// Return if we have something
		if (extSDcardDir[0] != '\0')
			platformSpecificDir = extSDcardDir;
	}
#endif
	return platformSpecificDir;
}

static char storageDir[PATH_MAX];
static char storageCloudDir[PATH_MAX];

bool FileIO::setStorageDir(bool force_userdir) {
	(void)force_userdir;
#if defined(MACOSX)
	// On macOS use ~/Library/Application Support/appname
	const char *home = getHomeDir();
	std::snprintf(storageDir, PATH_MAX, "%sLibrary%cApplication Support%c%s%c",
	              home, DELIMITER, DELIMITER, applicationName, DELIMITER);

	// With iCloud we use ~/Library/Mobile Documents/com~apple~CloudDocs/appname
	std::snprintf(storageCloudDir, PATH_MAX, "%sLibrary%cMobile Documents%c", home, DELIMITER, DELIMITER);
	if (accessFile(storageCloudDir, FileType::Directory))
		std::snprintf(storageCloudDir, PATH_MAX, "%sLibrary%cMobile Documents%ccom~apple~CloudDocs%c%s%c",
		              home, DELIMITER, DELIMITER, DELIMITER, applicationName, DELIMITER);
	else
		storageCloudDir[0] = '\0';
#elif defined(LINUX)
	// On Linux (and similar *nixen) save to ~/.appname/
	const char *home = getHomeDir();
	std::snprintf(storageDir, PATH_MAX, "%s.%s%c", home, applicationName, DELIMITER);
#elif defined(IOS) || defined(DROID)
	// On iOS and drod store in SaveData folder inside the data directory
	const char *launch = getLaunchDir();
	std::snprintf(storageDir, PATH_MAX, "%sSaveData%c", launch, DELIMITER);
#elif defined(WIN32)
	// On Windows store in [APPDATA]/appname
	// CSIDL_COMMON_APPDATA: [Profiles]/All Users/Application Data
	// CSIDL_APPDATA:        [Profiles]/[User]/Application Data

	wchar_t wpath[PATH_MAX];
	HRESULT res = SHGetFolderPathW(nullptr, force_userdir ? CSIDL_APPDATA : CSIDL_COMMON_APPDATA, nullptr, 0, wpath);
	if (res == S_OK)
		std::snprintf(storageDir, PATH_MAX, "%s%c%s%c", decodeUTF16String(wpath).c_str(), DELIMITER, applicationName, DELIMITER);

	// With iCloud we use ~/iCloudDrive/appname
	const char *home = getHomeDir();
	std::snprintf(storageCloudDir, PATH_MAX, "%siCloudDrive%c", home, DELIMITER);
	if (accessFile(storageCloudDir, FileType::Directory))
		std::snprintf(storageCloudDir, PATH_MAX, "%siCloudDrive%c%s%c", home, DELIMITER, applicationName, DELIMITER);
	else
		storageCloudDir[0] = '\0';
#endif
	// Should this return false?
	if (storageDir[0] == '\0') {
		sendToLog(LogLevel::Error, "StorageDir: Falling back to current dir!\n");
		copystr(storageDir, CURRENT_REL_PATH, PATH_MAX);
	}
	return true;
}

const char *FileIO::getStorageDir(bool cloud) {
	if (storageDir[0] == '\0')
		throw std::runtime_error("Undefined storage directory");
	if (cloud && storageCloudDir[0] != '\0')
		return storageCloudDir;
	return storageDir;
}

int FileIO::seekFile(FILE *fp, size_t off, int m) {
	return fseeko(fp, off, m);
}

bool FileIO::accessFile(const std::string &path, FileType type, size_t *len, bool unicode) {
	(void)unicode;
	auto cpath = path.c_str();

	// Assume that we have internet access
	if (type == FileType::URL)
		return true;

		// File type determination is not accurate because it assumes the existing file is either
		// a file or a directory (ignoring symlinks or dev devices). But it is ok for us.
#ifdef WIN32
	// Ignore disk drives
	if (cpath[0] >= 'A' && cpath[0] <= 'Z' && cpath[1] == ':' && (cpath[2] == '\0' || (cpath[3] == '\0' && (cpath[2] == '/' || cpath[2] == '\\'))))
		return true;

	WIN32_FILE_ATTRIBUTE_DATA attr;
	if (unicode && hasUnicode(cpath, path.size())) {
		auto wpath = decodeUTF8StringWide(cpath);
		if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &attr))
			return false;
	} else {
		if (!GetFileAttributesExA(cpath, GetFileExInfoStandard, &attr))
			return false;
	}

	if (len) {
		LARGE_INTEGER fsize{};
		fsize.LowPart  = attr.nFileSizeLow;
		fsize.HighPart = attr.nFileSizeHigh;
		*len           = static_cast<size_t>(fsize.QuadPart);
	}

	if (type == FileType::Any)
		return true;
	else if (type == FileType::File && (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		return true;
	else if (type == FileType::Directory && (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		return true;
#else
	struct stat buf;
	if (stat(cpath, &buf))
		return false;

	if (len)
		*len = static_cast<size_t>(buf.st_size);

	if (type == FileType::Any)
		return true;
	else if (type == FileType::File && (buf.st_mode & S_IFDIR) == 0)
		return true;
	else if (type == FileType::Directory && (buf.st_mode & S_IFDIR) != 0)
		return true;
#endif
	return false;
}

FILE *FileIO::openFile(const std::string &path, const char *mode, bool unicode) {
	(void)unicode;
	auto cpath = path.c_str();
	FILE *fp   = nullptr;
#ifdef WIN32
	if (unicode && hasUnicode(cpath, path.size())) {
		auto wpath = decodeUTF8StringWide(cpath);
		auto wmode = decodeUTF8StringWide(mode);
		fp         = _wfopen(wpath.c_str(), wmode.c_str());
	} else {
		fp = std::fopen(cpath, mode);
	}
#else
	fp = std::fopen(cpath, mode);
#endif


	// This check is here mainly for debugging, on Linux there may be other error codes
	// I think EPERM at the very least, and we should not crash on them.
#if 0
	// The check is written according to BSD docs.
	if (!fp && errno != EACCES && errno != ENOENT)
		throw std::runtime_error("Failed to open file " + path + ", errno: " + std::to_string(errno));
#endif

	// Using strict mode because wrong case is considered to be equal to a fatal I/O error.
	if (fp && pathCaseValidation)
		validatePathCase(path, fp, true);

	return fp;
}

bool FileIO::readFile(FILE *fp, size_t &len, uint8_t **buffer, bool autoclose) {
	if (!fp)
		return false;

	struct stat st;
	if (fstat(fileno(fp), &st))
		throw std::runtime_error("Error obtaining file size");
	len = static_cast<size_t>(st.st_size);

	if (buffer && len > 0) {
		*buffer = new uint8_t[len + 1];
		seekFile(fp, 0, SEEK_SET);
		if (std::fread(*buffer, len, 1, fp) != 1)
			throw std::runtime_error("Error reading file");
		(*buffer)[len] = 0x00;
	}

	if (autoclose)
		std::fclose(fp);

	return true;
}

bool FileIO::readFile(FILE *fp, size_t &len, std::vector<uint8_t> &buffer, bool autoclose) {
	if (!fp)
		return false;

	struct stat st;
	if (fstat(fileno(fp), &st))
		throw std::runtime_error("Error obtaining file size");
	len = static_cast<size_t>(st.st_size);

	if (len > 0) {
		if (buffer.size() < len + 1)
			buffer.resize(len + 1);
		seekFile(fp, 0, SEEK_SET);
		if (std::fread(buffer.data(), len, 1, fp) != 1)
			throw std::runtime_error("Error reading file");
		buffer[len] = 0x00;
	}

	if (autoclose)
		std::fclose(fp);

	return true;
}

bool FileIO::writeFile(FILE *fp, const uint8_t *buffer, size_t len, bool autoclose) {
	if (!fp)
		return false;

	if (buffer && len > 0) {
		seekFile(fp, 0, SEEK_SET);
		if (std::fwrite(buffer, len, 1, fp) != 1)
			throw std::runtime_error("Error wrting to file");
	}

	if (autoclose)
		std::fclose(fp);

	return true;
}

bool FileIO::makeDir(const std::string &path, bool recursive) {
	auto make = [](const std::string &path) {
		auto cpath = path.c_str();
#ifdef WIN32
		// On Windows we need to convert non-ascii paths into unicode
		// Directory creation is a not a common task, will not attempt to try the ascii version
		auto wpath = decodeUTF8StringWide(cpath);
		if (_wmkdir(wpath.c_str()) == 0 || errno == EEXIST)
			return true;
#else
		// Assume UTF-8 support on all other systems
		if (mkdir(cpath, 0755) == 0 || errno == EEXIST)
			return true;
#endif
		return false;
	};

	if (recursive) {
		size_t current_pos = 0;

		while ((current_pos = path.find_first_of("\\/", current_pos)) != std::string::npos) {
			if (current_pos > 0) {
				std::string subdir(path.begin(), path.begin() + current_pos);
				// Firstly check whether it is a directory
				if (!accessFile(subdir, FileType::Directory) && !make(subdir))
					return false;
			}
			current_pos++;
		}
	}

	return make(path);
}

std::vector<std::string> FileIO::scanDir(const std::string &path, FileType type) {
	std::vector<std::string> files;

#ifdef WIN32
	auto wpath = decodeUTF8StringWide(path.c_str());
	auto len   = wpath.length();
	if (len > 0 && wpath[len - 1] != DELIMITER)
		wpath.push_back(DELIMITER);
	wpath.push_back('*');

	WIN32_FIND_DATAW ffd;
	HANDLE hFind = FindFirstFileW(wpath.c_str(), &ffd);

	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			if (type == FileType::Any || (((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && type == FileType::Directory) ||
			                              (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && type == FileType::File))) {
				auto str = decodeUTF16String(ffd.cFileName);
				if (str != "." && str != "..")
					files.emplace_back(str);
			}
		} while (FindNextFileW(hFind, &ffd) != 0);
	}

	FindClose(hFind);
#else
	DIR *dir = opendir(path.c_str());
	if (dir) {
		dirent *entry = nullptr;
		while ((entry = readdir(dir))) {
			if (std::strcmp(entry->d_name, ".") && std::strcmp(entry->d_name, "..") &&
			    (type == FileType::Any || accessFile(path + DELIMITER + entry->d_name, type)))
				files.emplace_back(entry->d_name);
		}
		closedir(dir);
	}
#endif

	return files;
}

bool FileIO::removeDir(const std::string &path) {
#ifdef WIN32
	// SHFileOperationW requires a double-null terminated string.
	auto wpath = decodeUTF8StringWide(path.c_str()) + L'\0';
	SHFILEOPSTRUCTW op{};
	op.wFunc  = FO_DELETE;
	op.pFrom  = wpath.c_str();
	op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
	return !SHFileOperationW(&op);
#else
	DIR *dir = opendir(path.c_str());
	if (dir) {
		dirent *entry = nullptr;
		while ((entry = readdir(dir))) {
			if (std::strcmp(entry->d_name, ".") && std::strcmp(entry->d_name, "..")) {
				std::string tmp_path = path + DELIMITER + entry->d_name;
				DIR *subdir          = opendir(tmp_path.c_str());
				if (subdir) {
					closedir(subdir);
					removeDir(tmp_path);
				} else {
					removeFile(tmp_path);
				}
			}
		}
		closedir(dir);
	}
	return removeFile(path);
#endif
}

bool FileIO::removeFile(const std::string &path) {
	int code   = 0;
	auto cpath = path.c_str();
#ifdef WIN32
	if (hasUnicode(cpath, path.size())) {
		auto wpath = decodeUTF8StringWide(cpath);
		code       = _wremove(wpath.c_str());
	} else {
		code = std::remove(cpath);
	}
#else
	// Assume UTF-8 support on all other systems
	code = std::remove(cpath);
#endif

	if (code != 0) {
		//sendToLog(LogLevel::Error, "remove %s failed with %d\n", cpath, errno);
	}

	// There is no standard way to determine a successful removal without extra calls.
	// Assume it worked for now.
	return true;
}

bool FileIO::renameFile(const std::string &src, const std::string &dst, bool overwrite) {
	if (overwrite && !removeFile(dst))
		return false;
	int code  = 0;
	auto csrc = src.c_str();
	auto cdst = dst.c_str();
#ifdef WIN32
	if (hasUnicode(csrc, src.size()) || hasUnicode(cdst, dst.size())) {
		auto wsrc = decodeUTF8StringWide(csrc);
		auto wdst = decodeUTF8StringWide(cdst);
		code      = _wrename(wsrc.c_str(), wdst.c_str());
	} else {
		code = std::rename(csrc, cdst);
	}
#else
	// Assume UTF-8 support on all other systems
	code = std::rename(csrc, cdst);
#endif

	if (code != 0) {
		//sendToLog(LogLevel::Error, "rename %s to %s failed with %d\n", csrc, cdst, errno);
	}

	return code == 0;
}

bool FileIO::fileHandleReopen(const std::string &dst, FILE *src, const char *mode) {
	auto cdst = dst.c_str();
	FILE *fp  = nullptr;
#ifdef WIN32
	if (hasUnicode(cdst, dst.size())) {
		auto wdst  = decodeUTF8StringWide(cdst);
		auto wmode = decodeUTF8StringWide(mode);
		fp         = _wfreopen(wdst.c_str(), wmode.c_str(), src);
	} else {
		fp = std::freopen(cdst, mode, src);
	}
#else
	// Assume UTF-8 support on all other systems
	fp = std::freopen(cdst, mode, src);
#endif

	if (!fp) {
		const char *name = "file";
		if (src == stdout)
			name = "stdout";
		else if (src == stderr)
			name = "stderr";
		sendToLog(LogLevel::Error, "Warning: cannout reopen %s\n", name);
	}
	return fp;
}

static FileIO::LogMode logMode = FileIO::LogMode::Unspecified;

FileIO::LogMode FileIO::getLogMode() {
	return logMode;
}

void FileIO::setLogMode(FileIO::LogMode mode) {
	logMode = mode;
}

void FileIO::log(LogLevel level, const char *fmt, va_list args) {
	// Universal logging interface. Behaviour is platform-dependent.
#if defined(DROID)
	if (logMode != LogMode::File) {
		switch (level) {
			case LogLevel::Info:
				__android_log_vprint(ANDROID_LOG_INFO, providerName, fmt, args);
				break;
			case LogLevel::Warn:
				__android_log_vprint(ANDROID_LOG_WARN, providerName, fmt, args);
				break;
			case LogLevel::Error:
				__android_log_vprint(ANDROID_LOG_ERROR, providerName, fmt, args);
				break;
		}
		return;
	}
#endif
	switch (level) {
		case LogLevel::Info:
			std::vfprintf(stdout, fmt, args);
			break;
		case LogLevel::Warn:
		case LogLevel::Error:
			std::vfprintf(stderr, fmt, args);
			break;
	}

	if (logMode == LogMode::File) {
		std::fflush(stdout);
		std::fflush(stderr);
	}
}

void FileIO::prepareConsole(int cols, int lines, bool force) {
	// Might we want to utilises ncurses for unix?
	(void)cols;
	(void)lines;
	(void)force;
#ifdef WIN32
	char *shell = std::getenv("SHELL");
	if (shell) {
		sendToLog(LogLevel::Info, "Detected shell usage: %s\n", shell);
		setvbuf(stdout, nullptr, _IONBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);
		return;
	}

	static bool console_ready;
	if (!console_ready) {
		HWND console = GetConsoleWindow();
		if (!console) {
			AllocConsole();
			AttachConsole(GetCurrentProcessId());
			console = GetConsoleWindow();
		}
		if (console) {
			fileHandleReopen("CON", stdout, "w");
			fileHandleReopen("CON", stderr, "w");

			HANDLE hOutput = CreateFile("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, 0);
			if (hOutput != INVALID_HANDLE_VALUE) {
				CONSOLE_FONT_INFO fi;
				if (GetCurrentConsoleFont(hOutput, false, &fi)) {
					COORD fontSize = GetConsoleFontSize(hOutput, fi.nFont);
					if (fontSize.X > 0 && fontSize.Y > 0) {
						int reqWidth  = fontSize.X * cols;
						int reqHeight = fontSize.Y * lines;
						RECT r;
						GetWindowRect(console, &r);
						int width  = r.right - r.left;
						int height = r.bottom - r.top;

						if (force || width < reqWidth)
							width = reqWidth;
						if (force || height < reqHeight)
							height = reqHeight;
						MoveWindow(console, r.left, r.top, width, height, true);
					}
				}
				CloseHandle(hOutput);
			}
		}

		console_ready = true;
	}
#endif
}

void FileIO::waitConsole() {
#ifdef WIN32
	if (!std::getenv("SHELL")) {
		system("pause");
	}
#endif
}
