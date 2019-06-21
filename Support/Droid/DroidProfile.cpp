/**
 *  DroidProfile.cpp
 *  ONScripter-RU
 *
 *  Implements basic droid profiler.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Support/Droid/DroidProfile.hpp"

#if defined(DEBUG) || defined(PROFILE)

#include "Support/FileIO.hpp"

#include <SDL2/SDL.h>
#include <signal.h>
#include <unistd.h>

// At this moment libunwind is not compatible with 32-bit arm (due to old API target used?)
#ifndef __has_include
#define __has_include(x) 0
#warning "Your compiler should support __has_include"
#endif
#if __has_include(<libunwind/libunwind.h>)
#include <libunwind/libunwind.h>
#define USE_LIBUNWIND 1
#else
using unw_word_t                      = uintptr_t;
using unw_context_t                   = void *;
#define unw_getcontext(x) \
	do {                  \
	} while (0)
#define unw_get_proc_name_by_ip(a, b, data, c, d, e) \
	do {                                             \
		(data)[0] = '\0';                            \
	} while (0)
#endif

#include <array>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <cerrno>
#include <cstdio>
#include <cstring>

/********** User configurable options START **********/

// Immediate name resolving is 200x times slower
#define DELAY_NAME_RESOLVE 1

// Amount of force checked stack entries
// Anything above 2 is unsafe. Useful for plain profiling.
#define DIRECT_CALLSTACK_UNWIND 1

// Droid TLS is emulated and relies on pthreads, there is no reason to use it.
//#define USE_THREAD_LOCAL_STORAGE 1

// libc++ specific, seems to be faster
#define USE_UNORDERED_MAP 1

// Print complete call stacks
//#define PRINT_EVERYTHING 1

// On Linux each signal is delivered to the current active thread, in our case it is undesired.
//#define MEASURE_CURRENT_THREAD_ONLY 1

static constexpr size_t MaxSymbolLength = 512;

static constexpr size_t IndentWidth = 2;
static constexpr size_t IndentNum   = 8;
static constexpr char IndentSym     = ' ';
static constexpr char FoldSym       = '*';
static constexpr size_t MinLevel    = 16;
static constexpr char OutputPath[]  = "/sdcard/profile.yml";

static constexpr size_t ThreadReserveNum    = 8;
static constexpr size_t ThreadReserveMain   = 128;
static constexpr size_t ThreadReserveWorker = 64;

static constexpr size_t ThreadMax = 256;

/********** User configurable options END **********/

#ifdef USE_UNORDERED_MAP
#include <unordered_map>
#define MAP_TYPE std::unordered_map
#else
#include <map>
#define MAP_TYPE std::map
#endif

#ifdef PRINT_EVERYTHING
static constexpr bool PrintEverything = true;
#else
static constexpr bool PrintEverything = false;
#endif

#if !defined(DIRECT_CALLSTACK_UNWIND) && !defined(USE_LIBUNWIND)
#define DIRECT_CALLSTACK_UNWIND 1
#endif

#if (defined(DIRECT_CALLSTACK_UNWIND) && DIRECT_CALLSTACK_UNWIND > 0) && !defined(DELAY_NAME_RESOLVE)
#define DELAY_NAME_RESOLVE 1
#endif

static std::atomic<bool> profileEnabled{false};
static uint32_t profileStartTime;
static size_t profileResolution;

std::string padStr(std::string &&str, size_t sz) {
	if (str.size() < sz)
		str.insert(str.begin(), sz - str.size(), IndentSym);
	return str;
}

template <typename T>
std::string padVal(T val, size_t sz) {
	return padStr(std::to_string(val), sz);
}

std::string indentStr(size_t level) {
	return std::string(level * IndentWidth + 1, IndentSym);
}

template <typename T>
struct CallStack {
	CallStack &operator[](const T &key) {
		return children[key];
	}

	size_t count() {
		size_t res = counter;
		for (auto &child : children)
			res += child.second.count();
		return res;
	}

	template <typename Y>
	static std::vector<std::pair<T, CallStack<T>>> getSorted(Y &ref) {
		std::vector<std::pair<T, CallStack<T>>> sortedStack(ref.begin(), ref.end());
		std::sort(sortedStack.begin(), sortedStack.end(), [](auto &a, auto &b) {
			return a.second.count() > b.second.count();
		});
		return sortedStack;
	}

	std::string dump(const T &name, size_t level = 0) {
		std::string out;
		if (PrintEverything || count() > 0) {
			out += indentStr(level) + FoldSym + indentStr(MinLevel > level ? MinLevel - level : 0) +
			       padVal(counter, IndentNum) + indentStr(0) + padVal(count(), IndentNum) + indentStr(level) + name + "\n";

			for (auto &child : getSorted(children)) {
				out += child.second.dump(child.first, level + 1);
			}
		}
		return out;
	}

	MAP_TYPE<T, CallStack> children;
	size_t counter;
};

#ifdef DELAY_NAME_RESOLVE
static std::array<MAP_TYPE<unw_word_t, CallStack<unw_word_t>>, ThreadMax> threadRawStacks;
using CallStackEntry = unw_word_t;
#else
struct CallStackEntry {
	char name[MaxSymbolLength];
	unw_word_t off;
	unw_word_t ip;
};
#endif

static std::atomic<size_t> threadNum{0};
static std::array<pid_t, ThreadMax> threadIndices;
#ifndef USE_THREAD_LOCAL_STORAGE
static std::array<bool, ThreadMax> threadStatuses;
#endif
static std::array<size_t, ThreadMax> threadUnknown;
static std::array<std::atomic<size_t *>, ThreadMax> threadCurrent;
static std::array<std::vector<CallStackEntry>, ThreadMax> threadStackEntries;
static std::array<MAP_TYPE<std::string, CallStack<std::string>>, ThreadMax> threadStacks;

static inline size_t getThreadIndex() {
	size_t currentNum = threadNum;
	auto thread       = gettid();

	for (size_t i = 0; i < currentNum; i++) {
		if (threadIndices[i] == thread)
			return i;
	}

	auto index = threadNum++;
	if (index < ThreadMax) {
		threadIndices[index] = thread;
	} else {
		sendToLog(LogLevel::Error, "Exceeded thread number");
		std::terminate();
	}

	return index;
}

#if defined(DIRECT_CALLSTACK_UNWIND) && DIRECT_CALLSTACK_UNWIND > 0

template <size_t I>
FORCE_INLINE uintptr_t returnAddress() {
	return reinterpret_cast<uintptr_t>(__builtin_return_address(I));
}

template <size_t I>
FORCE_INLINE CallStack<unw_word_t> *unwindStack(CallStack<unw_word_t> *entry) {
	auto nextEntry = &(*entry)[returnAddress<DIRECT_CALLSTACK_UNWIND - I>()];
	if (nextEntry)
		return unwindStack<I - 1>(nextEntry);
	else
		return entry;
}

template <>
FORCE_INLINE CallStack<unw_word_t> *unwindStack<0>(CallStack<unw_word_t> *entry) {
	return entry;
}

FORCE_INLINE CallStack<unw_word_t> *unwindStack(size_t threadid) {
	auto entry = &threadRawStacks[threadid][returnAddress<0>()];

	if (!entry)
		return nullptr;
	else
		return unwindStack<DIRECT_CALLSTACK_UNWIND - 1>(entry);
}

#endif

extern "C" void mcount() {
	// Prevent stack recording when profiling is off
	if (!profileEnabled.load(std::memory_order_acquire))
		return;

#ifdef USE_THREAD_LOCAL_STORAGE
	// Prevent self invocation
	thread_local static bool entered;
	if (entered)
		return;
	entered = true;
#endif

	// Obtain thread index
	auto threadid = getThreadIndex();

#ifndef USE_THREAD_LOCAL_STORAGE
	if (threadStatuses[threadid])
		return;
	threadStatuses[threadid] = true;
#endif

#if !defined(DIRECT_CALLSTACK_UNWIND) || DIRECT_CALLSTACK_UNWIND <= 0
	// Obtain call stack
	unw_context_t uc;
	unw_getcontext(&uc);

	unw_cursor_t cursor;
	unw_init_local(&cursor, &uc);

	auto &callStack = threadStackEntries[threadid];
	callStack.clear();

	//sendToLog(LogLevel::Info, "Dumping...\n");

	while (unw_step(&cursor) > 0) {
#ifndef DELAY_NAME_RESOLVE
		callStack.emplace_back();
		auto &stack = callStack.back();

		// FIXME: unw_get_proc_name seems to constantly return UNW_EUNSPEC instead of 0
		unw_get_proc_name(&cursor, stack.name, sizeof(stack.name), &stack.off);
		if (stack.name[0] == '\0')
			std::snprintf(stack.name, sizeof(stack.name), "unk:%x", stack.ip);

		// sendToLog(LogLevel::Info, "Sym %s\n", stack.name);

		unw_get_reg(&cursor, UNW_REG_IP, &stack.ip);
#else
		unw_word_t ip;
		unw_get_reg(&cursor, UNW_REG_IP, &ip);
		callStack.emplace_back(ip);
#endif
	}

	// Save callstack and current function pointers
	if (!callStack.empty()) {
#ifndef DELAY_NAME_RESOLVE
		//sendToLog(LogLevel::Info, "Added %s to %zu threadid\n", callStack.back().name.c_str(), threadid);
		auto entry = &threadStacks[threadid][callStack.back().name];
		for (auto it = callStack.rbegin() + 1; it != callStack.rend(); it++)
			entry = &(*entry)[it->name];
#else
		auto entry = &threadRawStacks[threadid][callStack.back()];
		for (auto it = callStack.rbegin() + 1; it != callStack.rend(); it++)
			entry = &(*entry)[*it];
#endif
		threadCurrent[threadid] = &entry->counter;
	} else {
		threadCurrent[threadid] = &threadUnknown[threadid];
	}
#else
	// Obtain return address
	auto entry = unwindStack(threadid);
	if (entry)
		threadCurrent[threadid] = &entry->counter;
	else
		threadCurrent[threadid] = &threadUnknown[threadid];
#endif

	// Exit the call
#ifdef USE_THREAD_LOCAL_STORAGE
	entered = false;
#else
	threadStatuses[threadid] = false;
#endif
}

static void profileTimer(int) {
	if (profileEnabled.load(std::memory_order_acquire)) {
#ifdef MEASURE_CURRENT_THREAD_ONLY
		auto threadid = getThreadIndex();
		threadCurrent[threadid][0] += 1;
#else
		size_t count = threadNum.load(std::memory_order_relaxed);
		for (size_t i = 0; i < count; i++)
			threadCurrent[i][0] += 1;
#endif
	}
}

static bool resetTimers() {
	auto threadid = getThreadIndex();

	struct sigaction sigact {};
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags   = SA_RESTART;
	sigact.sa_handler = profileTimer;
	int result        = sigaction(SIGPROF, &sigact, nullptr);
	if (result) {
		sendToLog(LogLevel::Info, "Unable to set profile timer, sigaction err: %d, errno %d\n", result, errno);
		return false;
	}

	struct itimerval timer {};
	timer.it_interval.tv_usec = profileResolution;
	timer.it_value            = timer.it_interval;
	result                    = setitimer(ITIMER_PROF, &timer, 0);
	if (result) {
		sendToLog(LogLevel::Info, "Unable to set profile timer, setitimer err: %d, errno %d\n", result, errno);
		return false;
	}

	return true;
}

#ifdef DELAY_NAME_RESOLVE

template <typename T>
void mergeUpdater(unw_context_t &uc, T &dst, CallStack<unw_word_t> &src, unw_word_t ip) {
	unw_word_t off;
	char symname[MaxSymbolLength];

	unw_get_proc_name_by_ip(unw_local_addr_space, ip, symname, sizeof(symname), &off, &uc);
	if (symname[0] == '\0')
		std::snprintf(symname, sizeof(symname), "unk:%x", ip);

	auto &dsttop = dst[symname];
	dsttop.counter += src.counter;

	for (auto &entry : src.children)
		mergeUpdater(uc, dsttop, entry.second, entry.first);
}

#endif

void profileStart(size_t freq) {
	if (profileEnabled.load(std::memory_order_acquire))
		throw std::runtime_error("Profiling is already running");

	// Reset state
	threadNum.store(0, std::memory_order_relaxed);
	for (size_t i = 0; i < ThreadMax; i++) {
		threadUnknown[i] = 0;
		threadCurrent[i] = &threadUnknown[i];
		threadIndices[i] = 0;
	}
	threadStackEntries[0].reserve(ThreadReserveMain);
	for (size_t i = 1; i < ThreadReserveNum; i++)
		threadStackEntries[0].reserve(ThreadReserveWorker);
	FileIO::removeFile(OutputPath);

	// Push main thread
	getThreadIndex();
	//sendToLog(LogLevel::Info, "Main thread id %d\n", threadIndices[0]);

	// Initialise the timers
	profileResolution = 1000000 / freq;
	if (!resetTimers())
		return;

	// Start profiling
	profileStartTime = SDL_GetTicks();
	profileEnabled.store(true, std::memory_order_release);
}

void profileStop() {
	if (!profileEnabled.load(std::memory_order_acquire))
		throw std::runtime_error("Profiling is not running");

	// Stop profiling
	profileEnabled.store(std::memory_order_acq_rel) = false;
	auto totalTime                                  = SDL_GetTicks() - profileStartTime;
	sendToLog(LogLevel::Info, "Profiling finished after %0.2f seconds (%d ms)", totalTime / 1000.0, totalTime);

	// Kill timers
	struct itimerval timer {};
	int result = setitimer(ITIMER_PROF, &timer, nullptr);
	if (result)
		sendToLog(LogLevel::Info, "Unable to unset profile timer, setitimer err: %d, errno %d\n", result, errno);

#ifdef DELAY_NAME_RESOLVE
	unw_context_t uc;
	unw_getcontext(&uc);
	for (size_t i = 0; i < threadNum.load(std::memory_order_relaxed); i++) {
		for (auto &top : threadRawStacks[i])
			mergeUpdater(uc, threadStacks[i], top.second, top.first);
	}
#endif

	// Save the results
	std::string out = "Time is given in timer ticks.\nTimer resolution " + std::to_string(profileResolution) + " microseconds.\n\n";

	for (size_t i = 0; i < threadNum.load(std::memory_order_relaxed); i++) {
		out += "Thread " + std::to_string(i) + " dump:\n";
		out += indentStr(0) + FoldSym + indentStr(MinLevel) + padStr("Self", IndentNum) + indentStr(0) + padStr("Total", IndentNum) + indentStr(0) + "Symbol name\n";

		for (auto &top : CallStack<std::string>::getSorted(threadStacks[i]))
			out += top.second.dump(top.first);
		out += indentStr(0) + FoldSym + indentStr(MinLevel) + indentStr(0) + padVal(threadUnknown[i], IndentNum * 2) + indentStr(0) + "Unknown position\n\n";
	}

	FILE *fp = std::fopen("/proc/self/maps", "r");
	if (fp) {
		sendToLog(LogLevel::Info, "Managed to open /proc/self/maps for reading\n");
		out += "Proc mapping is as follows:\n\n";
		char tmp[PATH_MAX];
		while (std::fgets(tmp, sizeof(tmp), fp))
			out += tmp;
		std::fclose(fp);
	} else {
		sendToLog(LogLevel::Info, "Failed to open /proc/self/maps for reading (errno: %d)\n", errno);
	}

	FileIO::writeFile(OutputPath, reinterpret_cast<const uint8_t *>(out.data()), out.size());
}

#else

// Provide mcount symbol on unsupported arch
#if defined(PROFILE) || defined(DEBUG)
extern "C" void mcount() {}
#endif

void profileStart(size_t) {}
void profileStop() {}

#endif
