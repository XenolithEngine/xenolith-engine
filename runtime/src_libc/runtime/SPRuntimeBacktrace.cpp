/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#define __SPRT_BUILD

#include <sprt/runtime/backtrace.h>
#include <sprt/runtime/log.h>

#include <unwind.h>

#if SPRT_WINDOWS
#include <sprt/runtime/mutex.h>

#include "private/SPRTDso.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dbghelp.h>
#endif

namespace sprt::backtrace::detail {

static StringView filepath_lastComponent(StringView path) {
#if SPRT_WINDOWS
	size_t pos = path.rfind('\\');
#else
	size_t pos = path.rfind('/');
#endif
	if (pos != size_t(0) - 1) {
		return path.sub(pos + 1);
	} else {
		return path;
	}
}

static size_t print(char *buf, size_t bufLen, uintptr_t pc, StringView filename, int lineno,
		StringView function) {
	char *target = buf;
	auto w = __sprt_snprintf(target, bufLen, "[%p]", (void *)pc);
	bufLen -= w;
	target += w;

	if (!filename.empty()) {
		auto name = filepath_lastComponent(filename);
		if (lineno >= 0) {
			w = __sprt_snprintf(target, bufLen, " %.*s:%d", int(name.size()), name.data(), lineno);
		} else {
			w = __sprt_snprintf(target, bufLen, " %.*s", int(name.size()), name.data());
		}
		bufLen -= w;
		target += w;
	}

	if (!function.empty()) {
#if SPRT_WINDOWS
		w = __sprt_snprintf(target, bufLen, " - %.*s", int(function.size()), function.data());
		bufLen -= w;
		target += w;
#else
		int status = 0;
		auto ptr = abi::__cxa_demangle(function.data(), nullptr, nullptr, &status);
		if (ptr) {
			w = __sprt_snprintf(target, bufLen, " - %s", ptr);
			bufLen -= w;
			target += w;
			__sprt_free(ptr);
		} else {
			w = __sprt_snprintf(target, bufLen, " - %.*s", int(function.size()), function.data());
			bufLen -= w;
			target += w;
		}
#endif
	}
	return target - buf;
}

} // namespace sprt::backtrace::detail

#if SPRT_WINDOWS

namespace sprt::backtrace::detail {

struct State {
	Dso handle;
	HANDLE hProcess = nullptr;

	decltype(&::SymSetOptions) SymSetOptions = nullptr;
	decltype(&::SymInitialize) SymInitialize = nullptr;
	decltype(&::SymCleanup) SymCleanup = nullptr;
	decltype(&::StackWalk64) StackWalk64 = nullptr;
	decltype(&::SymGetSymFromAddr64) SymGetSymFromAddr64 = nullptr;
	decltype(&::SymGetLineFromAddr64) SymGetLineFromAddr64 = nullptr;

	qmutex mutex;

	operator bool() const { return hProcess != nullptr; }
};

struct StackFrameSym {
	IMAGEHLP_LINE64 line;
	IMAGEHLP_SYMBOL64 sym;
	char symNameBuffer[1_KiB];
	char targetNameBuffer[1_KiB];

	StackFrameSym() {
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		sym.SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64) + 1_KiB;
		sym.MaxNameLength = 1_KiB;
	}
};

static void initState(State &state) {
	HANDLE hCurrentProcess;
	HANDLE hProcess;

	auto handle = Dso("Dbghelp.dll");
	if (!handle) {
		return;
	}

	state.handle = sprt::move(handle);
	state.SymSetOptions = state.handle.sym<decltype(&::SymSetOptions)>("SymSetOptions");
	state.SymInitialize = state.handle.sym<decltype(&::SymInitialize)>("SymInitialize");
	state.SymCleanup = state.handle.sym<decltype(&::SymCleanup)>("SymCleanup");
	state.StackWalk64 = state.handle.sym<decltype(&::StackWalk64)>("StackWalk64");
	state.SymGetSymFromAddr64 =
			state.handle.sym<decltype(&::SymGetSymFromAddr64)>("SymGetSymFromAddr64");
	state.SymGetLineFromAddr64 =
			state.handle.sym<decltype(&::SymGetLineFromAddr64)>("SymGetLineFromAddr64");

	if (!state.SymSetOptions || !state.SymInitialize || !state.SymCleanup || !state.StackWalk64
			|| !state.SymGetSymFromAddr64 || !state.SymGetLineFromAddr64) {
		state.handle.close();
		return;
	}

	state.SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);

	hCurrentProcess = GetCurrentProcess();

	if (!DuplicateHandle(hCurrentProcess, hCurrentProcess, hCurrentProcess, &hProcess, 0, FALSE,
				DUPLICATE_SAME_ACCESS)) {
		log::vperror(__SPRT_LOCATION, "Ref", "Fail to duplicate process handle");
		return;
	}

	if (!state.SymInitialize(hProcess, NULL, TRUE)) {
		log::vperror(__SPRT_LOCATION, "Ref", "Fail to load symbol info");
		return;
	}

	state.hProcess = hProcess;
}

static void termState(State &state) {
	if (state.hProcess) {
		state.SymCleanup(state.hProcess);
		CloseHandle(state.hProcess);
		state.hProcess = nullptr;
	}
	state.handle.close();
}

static void performBacktrace(State &state, size_t offset,
		const callback<void(uintptr_t, StringView)> &cb) {
	auto hThread = GetCurrentThread();

	DWORD machine = 0;
	CONTEXT context;
	STACKFRAME64 frame;
	RtlCaptureContext(&context);
	/*Prepare stackframe for the first StackWalk64 call*/
	frame.AddrFrame.Mode = frame.AddrPC.Mode = frame.AddrStack.Mode = AddrModeFlat;
#if (defined _M_IX86)
	machine = IMAGE_FILE_MACHINE_I386;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrPC.Offset = context.Eip;
	frame.AddrStack.Offset = context.Esp;
#elif (defined _M_X64)
	machine = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrPC.Offset = context.Rip;
	frame.AddrStack.Offset = context.Rsp;
#else
#pragma error("unsupported architecture")
#endif

	unique_lock lock(state.mutex);

	DWORD dwDisplacement;
	StackFrameSym stackSym;

	while (state.StackWalk64(machine, state.hProcess, hThread, &frame, &context, 0, 0, 0, 0)) {
		if (offset > 0) {
			--offset;
			continue;
		}

		BOOL hasSym = FALSE;
		BOOL hasLine = FALSE;

		hasSym = state.SymGetSymFromAddr64(state.hProcess, frame.AddrPC.Offset, nullptr,
				&stackSym.sym);
		hasLine = state.SymGetLineFromAddr64(state.hProcess, frame.AddrPC.Offset, &dwDisplacement,
				&stackSym.line);

		auto size = backtrace::detail::print(stackSym.targetNameBuffer, 1_KiB, frame.AddrPC.Offset,
				hasLine ? stackSym.line.FileName : nullptr, hasLine ? stackSym.line.LineNumber : 0,
				hasSym ? stackSym.sym.Name : nullptr);
		cb(frame.AddrPC.Offset, StringView(stackSym.targetNameBuffer, size));
	}
}

} // namespace sprt::backtrace::detail

#else

#if __has_include(<backtrace.h>)
#include <backtrace.h>
#else
#warning "No <backtrace.h> available, replacing with forward declaration"

struct backtrace_state;

typedef void (*backtrace_error_callback)(void *data, const char *msg, int errnum);

extern "C" struct backtrace_state *backtrace_create_state(const char *filename, int threaded,
		backtrace_error_callback error_callback, void *data);

typedef int (*backtrace_full_callback)(void *data, __SPRT_ID(uintptr_t) pc, const char *filename,
		int lineno, const char *function);

extern "C" int backtrace_full(struct backtrace_state *state, int skip,
		backtrace_full_callback callback, backtrace_error_callback error_callback, void *data);


#endif


// libbacktrace info
// see https://github.com/ianlancetaylor/libbacktrace
namespace sprt::backtrace::detail {

struct State {
	backtrace_state *state = nullptr;

	operator bool() const { return state != nullptr; }
};

static void debug_backtrace_error(void *data, const char *msg, int errnum) {
	::__sprt_perror("libbacktrace");
	::__sprt_perror(msg);
}

static int debug_backtrace_full_callback(void *data, uintptr_t pc, const char *filename, int lineno,
		const char *function) {
	if (pc != uintptr_t(0xffff'ffff'ffff'ffffLLU)) {
		auto ret = (const callback<void(uintptr_t, StringView)> *)data;
		char buf[1'024] = {0};
		auto size = backtrace::detail::print(buf, 1'024, pc, filename, lineno, function);
		(*ret)(pc, StringView(buf, size));
	}
	return 0;
}

static void initState(State &state) {
	state.state = ::backtrace_create_state(nullptr, 1, debug_backtrace_error, nullptr);
}

static void termState(State &state) {
	if (state.state) {
		//::backtrace_free_state(state.state, debug_backtrace_error, nullptr);
		state.state = nullptr;
	}
}

static void performBacktrace(State &state, size_t offset,
		const callback<void(uintptr_t, StringView)> &cb) {
	backtrace_full(state.state, int(offset), debug_backtrace_full_callback, debug_backtrace_error,
			(void *)&cb);
}

} // namespace sprt::backtrace::detail

#endif

#include "private/SPRTThread.h"

namespace sprt::backtrace {

struct BacktraceState {
	void init() { backtrace::detail::initState(state); }

	void term() { termState(state); }

	void getBacktrace(size_t offset, const callback<void(uintptr_t, StringView)> &cb) {
		if (state) {
			performBacktrace(state, offset + 2, cb);
		}
	}

	backtrace::detail::State state;
};

static BacktraceState s_backtraceState;

void initialize() { s_backtraceState.init(); }

void terminate() { s_backtraceState.term(); }

void getBacktrace(size_t offset, const callback<void(uintptr_t, StringView)> &cb) {
	s_backtraceState.getBacktrace(offset + 1, cb);
}

} // namespace sprt::backtrace
