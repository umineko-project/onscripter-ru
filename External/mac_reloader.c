/**
 *  mac_reloader.c
 *  ONScripter-RU
 *
 *  Apple-specific multiarchitecture reloading code.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <string.h>

#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <spawn.h>
#include <cpuid.h>

#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>

int NSRunAlertPanel(CFStringRef strTitle, CFStringRef strMsg, CFStringRef strButton1, CFStringRef strButton2, CFStringRef strButton3, ...);

enum t_cpu_architecture {
	ARCH_UNKNOWN,
	ARCH_HASWELL,
	ARCH_X86_64,
	ARCH_I386,
	ARCH_MAX
};

static const char *cpu_architecture_desc[ARCH_MAX] = {
	"Unknown",
	"Haswell",
	"x86_64",
	"i386"
};

static enum t_cpu_architecture current_architecture() {
#if defined(__x86_64h__)
	return ARCH_HASWELL;
#elif defined(__x86_64__)
	return ARCH_X86_64;
#elif defined(__i386__)
	return ARCH_I386;
#else
	return ARCH_UNKNOWN;
#endif
}

#ifndef bit_AVX2
#define bit_AVX2 (1 << 5)
#endif

static bool has_avx2() {
	unsigned int eax, ebx, ecx, edx;
	__cpuid_count(7, 0, eax, ebx, ecx, edx);
	return ebx & bit_AVX2;
}

enum t_decide_64bit {
	DECIDE_64BIT_DEFAULT,
	DECIDE_64BIT_OFF,
	DECIDE_64BIT_ON
};

static int get_64bit_env() {
	char *request = getenv("USE_64BIT_MODE");
	if (!request)
		return DECIDE_64BIT_DEFAULT;
	return atoi(request) == 1 ? DECIDE_64BIT_ON : DECIDE_64BIT_OFF;
}

static void *zero_alloc(size_t num, size_t size) {
	void *buf = calloc(num, size);
	if (!buf) {
		fprintf(stderr, "Fatal error: Memory allocation of %zu*%zu failure!\n", num, size);
		abort();
	}
	return buf;
}

struct fat_arch_info {
	cpu_type_t cputype;
	cpu_subtype_t cpusubtype;
	off_t offset;
	size_t size;
};

union fat_arch_combined {
	struct fat_arch_64 a64;
	struct fat_arch a32;
	struct {
		cpu_type_t cputype;
		cpu_subtype_t cpusubtype;
	} common;
};

static int fat_arch(FILE *fat, uint32_t index, bool is_big_endian, bool is_64bit, struct fat_arch_info *info) {
	size_t arch_size = is_64bit ? sizeof(struct fat_arch_64) : sizeof(struct fat_arch);
	off_t offset = (off_t)(sizeof(struct fat_header) + arch_size * index);
	if (fseeko(fat, offset, SEEK_SET))
		return -1;
	
	union fat_arch_combined arch;
	
	if (fread(&arch, arch_size, 1, fat) != 1)
		return -1;

	info->cputype = is_big_endian ? __builtin_bswap32(arch.common.cputype) : arch.common.cputype;
	info->cpusubtype = is_big_endian ? __builtin_bswap32(arch.common.cpusubtype) : arch.common.cpusubtype;

	if (is_big_endian) {
		info->offset = is_64bit ? __builtin_bswap64(arch.a64.offset) : __builtin_bswap32(arch.a32.offset);
		info->size = is_64bit ? __builtin_bswap64(arch.a64.size) : __builtin_bswap32(arch.a32.size);
	} else {
		info->offset = is_64bit ? arch.a64.offset : arch.a32.offset;
		info->size = is_64bit ? arch.a64.size : arch.a32.size;
	}
	
	if (info->size < sizeof(struct mach_header))
		return -1;
	
	return 0;
}

static int fat_extract(const char *src, const char *dst, cpu_type_t cputype, cpu_subtype_t cpusubtype, cpu_type_t patch_cputype, cpu_subtype_t patch_cpusubtype) {
	FILE *fat = fopen(src, "r");
	FILE *slim = fopen(dst, "w");
	int error = -1;
	
	if (fat && slim) {
		struct fat_header header;
		if (!fseeko(fat, 0, SEEK_SET) &&
			fread(&header, sizeof(struct fat_header), 1, fat) == 1) {
			bool is_big_endian = header.magic == FAT_CIGAM || header.magic == FAT_CIGAM_64;
			bool is_64bit = header.magic == FAT_MAGIC_64 || header.magic == FAT_CIGAM_64;
			uint32_t archs = is_big_endian ? __builtin_bswap32(header.nfat_arch) : header.nfat_arch;
			
			for (uint32_t i = 0; i < archs; i++) {
				struct fat_arch_info info;
				if (fat_arch(fat, i, is_big_endian, is_64bit, &info))
					continue;
				
				if (info.cputype == cputype && info.cpusubtype == cpusubtype) {
					uint8_t *data = zero_alloc(info.size, sizeof(uint8_t));
					if (!fseeko(fat, info.offset, SEEK_SET) &&
						fread(data, info.size, 1, fat) == 1) {
						struct mach_header *header = (struct mach_header *)data;
						if (header->magic == MH_MAGIC || header->magic == MH_MAGIC_64) {
							header->cputype = patch_cputype;
							header->cpusubtype = patch_cpusubtype;
							
							if (!fseeko(slim, 0, SEEK_SET) &&
								fwrite(data, info.size, 1, slim) == 1)
								error = 0;
						}
						
					}
					
					free(data);
					break;
				}
			}
		}
	}
	
	if (fat) fclose(fat);
	if (slim) fclose(slim);
	return error;
}

static int launch_process(char *const *argv, cpu_type_t cpu_type) {
	posix_spawnattr_t attr;
	
	int error = posix_spawnattr_init(&attr);
	if (error)
		return error;
	
	if (cpu_type) {
		size_t ocount = 0;
		error = posix_spawnattr_setbinpref_np(&attr, 1, &cpu_type, &ocount);
		if (error)
			return error;
		if (ocount != 1)
			return -1;
	}
	
	pid_t pid = 0;
    error = posix_spawn(&pid, argv[0], NULL, &attr, argv, NULL);
    if (error)
    	return error;

    return 0;
}

int main(int argc, char *argv[]) {
	if (argc == 0) {
		fprintf(stderr, "Fatal error: Unknown launch arguments!\n");
		NSRunAlertPanel(CFSTR("Fatal error"), CFSTR("Unknown launch arguments!\n"), CFSTR("OK"), NULL, NULL);
		return EXIT_FAILURE;
	}
	
	fprintf(stderr, "Start: %s (%d)\n", argv[0], argc);
	
	enum t_cpu_architecture arch = current_architecture();
	fprintf(stderr, "Binary architecture: %s\n", cpu_architecture_desc[current_architecture()]);
	
	// libc++ is not available on 10.6 so we use 32-bit binary with static libc++
	bool legacy_os = floor(kCFCoreFoundationVersionNumber) < kCFCoreFoundationVersionNumber10_7;
	fprintf(stderr, "Legacy OS: %s (%f)\n", legacy_os ? "Yes" : "No", kCFCoreFoundationVersionNumber);
	
	// We optimise our 64-bit binarie with avx 2.0
	bool avx2_support = has_avx2();
	fprintf(stderr, "AVX2 support: %s\n", avx2_support ? "Yes" : "No");

	enum t_decide_64bit decide_64bit = get_64bit_env();
	fprintf(stderr, "Requests 64-bit: %s\n", decide_64bit == DECIDE_64BIT_ON ? "Yes" : "No");

#if defined(__x86_64__)
	if (arch != ARCH_X86_64) {
		fprintf(stderr, "Fatal error: Wrong architecture!\n");
		NSRunAlertPanel(CFSTR("Fatal error"), CFSTR("Wrong architecture!"), CFSTR("OK"), NULL, NULL);
		return EXIT_FAILURE;
	}
	
	size_t len = strlen(argv[0]) + strlen(".64") + 1;
	char *path = zero_alloc(len, sizeof(char));
	snprintf(path, len, "%s.64", argv[0]);
	
	// Try 64-bit mode on permission if we used it previously or explicitly request it
	bool exists_64bit = !access(path, F_OK|X_OK);
	bool goes_64bit = decide_64bit == DECIDE_64BIT_ON;
	if (decide_64bit != DECIDE_64BIT_OFF && exists_64bit) {
		fprintf(stderr, "Info: Trying 64-bit mode due to previous launch!\n");
		goes_64bit = true;
	}
	
	// Suggest 64-bit mode
	if (decide_64bit != DECIDE_64BIT_OFF && !legacy_os && avx2_support && !goes_64bit) {
		fprintf(stderr, "Warning: You might want to run in 64-bit mode!\n");
		fprintf(stderr, "Please set `USE_64BIT_MODE=1` environment var to do so.\n");
	}
	
	// Warn if unsupported
	if (goes_64bit && (legacy_os || !avx2_support)) {
		fprintf(stderr, "Warning: You are not allowed to use 64-bit mode on unsupported platform!\n");
		goes_64bit = false;
	}
	
	// If we asked 32-bit or 64-bit is no longer unsupported
	if (exists_64bit && !goes_64bit) {
		remove(path);
	}
	
	// Attempt to restart in 64-bit mode
	if (goes_64bit) {
		if (!exists_64bit && (fat_extract(argv[0], path, CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_H, CPU_TYPE_X86_64, CPU_SUBTYPE_LIB64|CPU_SUBTYPE_X86_64_ALL) ||
			chmod(path, 0755) || access(path, F_OK|X_OK))) {
			fprintf(stderr, "Warning: Failed to extract the 64-bit architecture!\n");
			remove(path);
			goes_64bit = false;
		}
		
		if (goes_64bit) {
			char **arguments = zero_alloc(argc+1, sizeof(char *));
			arguments[0] = path;
			for (int i = 1; i <= argc; i++) {
				arguments[i] = argv[i];
			}
			
			if (launch_process(arguments, CPU_TYPE_X86_64)) {
				fprintf(stderr, "Warning: Failed to launch the application in 64-bit mode!\n");
				goes_64bit = false;
			}
			
			free(arguments);
		}
		
	}
	
	free(path);
	
	// We either failed or simply want a normal launch
	if (!goes_64bit) {
		int error = launch_process(argv, CPU_TYPE_I386);
		if (error) {
			fprintf(stderr, "Fatal error: Failed to relaunch the application in 32-bit mode!\n");
			fprintf(stderr, "Please run it via `arch -i386 %s`\n", argv[0]);
			NSRunAlertPanel(CFSTR("Fatal error"), CFSTR("Failed to relaunch the application in 32-bit mode!\n\n"
							"Please set \"Open in 32-bit mode\" option in applications properties."), CFSTR("OK"), NULL, NULL);
			return EXIT_FAILURE;
		}
	}
#else
	fprintf(stderr, "Ok\n");
#endif
}
