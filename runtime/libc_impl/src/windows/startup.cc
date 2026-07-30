/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#pragma clang diagnostic ignored "-Wmicrosoft-anon-tag"

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/sys/__sprt_sprt.h>
#include <sprt/cxx/detail/ctypes.h>
#include <sprt/cxx/mutex>
#include <sprt/cxx/condition_variable>

#include <sprt/wrappers/windows/dl_api.h>
#include <sprt/wrappers/windows/context_api.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/wrappers/windows/winsock.h>
#include <sprt/wrappers/windows/app_startup.h>

#include "stdlib.h"
#include "stdio.h"
#include "../../include/__impl_libc.h"

#include "initterm.h"
#include "dll/dllloader.h"

#if !defined(SPRT_BUILD_SHARED_RUNTIME)
__cdecl int main(int argc, const char *argv[]);
#endif

struct NonTrivialType {
	NonTrivialType() { printf("%s\n", "constructed"); }
	~NonTrivialType() { printf("%s\n", "destroyed"); }
};

// The .CRT initializer sections, the TLS directory and _fltused - everything a PE image
// carries for itself rather than sharing with the rest of the process. The executable
// half of the shared-runtime build includes the same subunit; see its header comment.
#include "crt_image.cc"

/*
	/Zc:threadSafeInit support
*/

__SPRT_C_FUNC sprt::atomic<int> _Init_global_epoch = sprt::Min<int>;

// With some compiler support, it's implementable with a pure futex, but not today...
static sprt::mutex s_threadGuardMutex;
static sprt::condition_variable s_threadGuardWaitCond;

__SPRT_C_FUNC void __cdecl _Init_thread_header(int *const pOnce) noexcept {
	sprt_plock_lock(pOnce, 0, nullptr);
	if (*pOnce == 0) {
		*pOnce = -1;
	} else {
		// successfully set before us, exit
		sprt_plock_unlock(pOnce, 0, nullptr);
	}
}

// Exception during init, we should drop LOCK_BIT and signal to wakeup
// Calling thread should own the lock
__SPRT_C_FUNC void __cdecl _Init_thread_abort(__sprt_uint32_t *const pOnce) noexcept {
	if (*pOnce == -1) {
		*pOnce = 0;
		sprt_plock_unlock(pOnce, 0, nullptr);
	}
}

__SPRT_C_FUNC void __cdecl _Init_thread_footer(__sprt_uint32_t *const pOnce) noexcept {
	if (*pOnce == -1) {
		// we already hold the lock

		++_Init_global_epoch;
		_Init_thread_epoch = _Init_global_epoch;
		*pOnce = _Init_thread_epoch;

		sprt_plock_unlock(pOnce, 0, nullptr);
	}
}

/*
	GS support (based on https://github.com/sysfce2/nocrt/blob/main/nocrt_exe.c)
*/

// The cookie variables and __security_check_cookie live in crt_image.cc: they are
// per-image, and the executable half of a shared-runtime build needs its own set. Only
// the entropy source is here, because reaching it needs the DLL loader.
//
// Exported so that half can seed its own cookie from the same source; declared in
// <sprt/wrappers/windows/app_startup.h>.
__SPRT_C_FUNC SPRT_API __declspec(safebuffers) UINT_PTR __sprt_gencookie() {
	UINT_PTR cookie = 0;

	auto loader = sprt::DllLoader::get();
	if (loader) {
		HMODULE BCryptPrimitives = loader->__LoadLibraryW(L"BCryptPrimitives.dll");
		if (BCryptPrimitives) {
			BOOL (*ProcessPrng)(PBYTE, SIZE_T) = nullptr;
			ProcessPrng = reinterpret_cast<decltype(ProcessPrng)>(
					loader->__GetProcAddress(BCryptPrimitives, "ProcessPrng"));
			if (ProcessPrng && !ProcessPrng((PBYTE)&cookie, sizeof(cookie))) {
				cookie = 0;
			}
			loader->__FreeLibrary(BCryptPrimitives);
		}
	}

	if (cookie == 0) {
		// Failed to use ProcessPrng, fallback to processor counter,
		// No other available entropy source at this moment;
#if defined(_M_ARM64) || defined(__aarch64__)
		unsigned long long counter;
		__asm__ volatile("mrs %0, cntvct_el0" : "=r"(counter));
#else
		unsigned long long counter = __rdtsc();
#endif
		cookie = (counter ^ 0x7A2D'9F1B'4E63'C082ll) & 0x0000'ffff'ffff'ffffll;
	}

	// The default doubles as an "uninitialized" marker for callers, so never hand it back.
	if (cookie == __SPRT_DEFAULT_SECURITY_COOKIE) {
		cookie = __SPRT_DEFAULT_SECURITY_COOKIE + 1;
	}

	return cookie;
}

// The libc struct lives in this uninitialized static memory block,
static unsigned char s_libcBuffer[sizeof(sprt::__libc)];

sprt::__libc *sprt::__libc::get() { return reinterpret_cast<__libc *>(s_libcBuffer); }

__SPRT_C_FUNC __sprt_uint64_t __libc_main_thread = 0;

// -----------------------------------------------------------------------------
// Clean-crash policy for headless / non-interactive runs.
//
// By default an unhandled hardware fault (an access violation, or the
// __builtin_trap()/ud2 that backs a default SIGABRT and every failed
// assertion) propagates to the OS top-level handler, which under Windows pops a
// modal "program error" dialog (winedbg under wine) and BLOCKS until it is
// dismissed. A headless harness (CI, the conformance runner, wine without a
// desktop) then looks hung until an external timeout kills it, and the real
// exit status is lost. Suppress the fault UI (SetErrorMode) and install a
// top-level filter that terminates the process immediately, carrying the
// exception code as a non-zero exit status, so a crash is a clean, promptly
// observable failure instead of a hang.
extern "C" __SPRT_WIN_IMPORT WINAPI UINT SetErrorMode(UINT uMode);

static LONG WINAPI __sprt_clean_crash_filter(EXCEPTION_POINTERS *info) {
	UINT code = (info && info->ExceptionRecord) ? (UINT)info->ExceptionRecord->ExceptionCode : 3u;
	if (code == 0) {
		code = 3u; // 3 == C runtime abort() exit convention
	}
	TerminateProcess(GetCurrentProcess(), code);
	return __SPRT_EXCEPTION_EXECUTE_HANDLER; // unreachable: process is already gone
}

static void __sprt_install_clean_crash() {
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	SetUnhandledExceptionFilter(&__sprt_clean_crash_filter);
}

// Bring the runtime up inside the image that owns it. Shared by the freestanding
// executable entry (mainCRTStartup) and the shared-runtime library entry
// (_DllMainCRTStartup) - the sequence is identical, only what follows it differs:
// the executable goes on to call main(), the DLL returns to the loader, which then
// initializes the rest of the process.
//
// The __sprt_image_init_* helpers come from crt_image.cc and act on the image this
// translation unit was linked into, so the DLL runs the DLL's initializers here and the
// application runs its own from its startup stub.
// safebuffers: this function seeds the image's /GS cookie partway through, so it must
// not be instrumented itself - a prologue that captured the pre-seed value would check
// against the seeded one on return and trap.
__declspec(safebuffers) static int __sprt_runtime_attach() {
	// Load all required DLLs for SPRT.
	// If some DLLs are missed, or some required functions are missed - abort immediately
	auto loader = sprt::DllLoader::construct();
	auto ret = loader->load();
	if (ret != 0) {
		return ret;
	}

	// Turn any later unhandled fault into a clean, immediate, non-zero exit rather
	// than a modal crash dialog that blocks headless runs (see the filter above).
	// Installed before static initializers and main() so a crash anywhere is covered.
	__sprt_install_clean_crash();

	// Seed this image's /GS cookie. Needs the loader, so it goes after load() above.
	__sprt_image_init_cookie();

	// Create __libc struct in static memory block
	// this will initialize fds locales, exceptions and all other required libc features
	auto libc = new (s_libcBuffer, sprt::nothrow) sprt::__libc;

	__libc_main_thread = libc->mainThread;

	// At this moment, there are no other code running in application, except for the
	// entry point. No other code -> no other threads -> no race conditions possible ->
	// no need for locking. This will change after static initializers calling.

	// Call c initializers
	if (__sprt_image_init_c() != 0) {
		return LOADER_ERROR_STATIC_C_INIT_FAILED; // Error in c initialization
	}

	// Call static c++ constructors
	if (__sprt_image_init_cxx() != 0) {
		return LOADER_ERROR_STATIC_CXX_INIT_FAILED; // Error in c++ initialization
	}

	// Call thread_local constructors for the main thread
	__sprt_image_init_tls();

	// This will attach and initialize main thread as pthread, if it was not initializd before
	__sprt_pthread_self();

	return 0;
}

#if defined(SPRT_BUILD_SHARED_RUNTIME)

// Shared runtime: this image owns the process-wide C/C++ runtime state, and the loader
// initializes it before the executable's entry point runs - so by the time application
// static initializers execute, the heap, stdio, TLS and exception machinery are live.
//
// lld-link uses _DllMainCRTStartup as the default entry point for /DLL, so no explicit
// -Wl,-entry: is needed.
__SPRT_C_FUNC __declspec(safebuffers) BOOL WINAPI _DllMainCRTStartup(void *, DWORD reason, void *) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		if (__sprt_runtime_attach() != 0) {
			return FALSE;
		}

		// load WSA unconditionally so c socket API should work natively
		{
			WSADATA wsaData;
			if (WSAStartup(0x0202, &wsaData) != 0) { // winsock 2.2
				return FALSE;
			}
			if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
				WSACleanup();
				return FALSE;
			}
		}
		break;
	case DLL_THREAD_ATTACH:
		// Run thread_local constructors for threads created by code that does not go
		// through sprt::thread (the loader calls us for every thread in the process).
		__dyn_tls_init(nullptr, DLL_THREAD_ATTACH, nullptr);
		break;
	case DLL_PROCESS_DETACH: WSACleanup(); break;
	case DLL_THREAD_DETACH:
	default: break;
	}
	return TRUE;
}

#endif // SPRT_BUILD_SHARED_RUNTIME

// Convert the (attacker-controlled) wide command line into argv and hand control to
// main(). Never returns: it ends in exit(), which drains atexit handlers and static
// destructors.
//
// manageWsa is false when the caller already brought Winsock up for the whole process
// (the shared runtime does so in DLL_PROCESS_ATTACH and tears it down in
// DLL_PROCESS_DETACH); the freestanding executable owns that lifetime itself.

__SPRT_C_FUNC SPRT_API int __argc = 0;
__SPRT_C_FUNC SPRT_API char **__argv = nullptr;

static int __sprt_invoke_main(__sprt_main_fn mainFn, bool manageWsa) {
	int ret = 0;

	// __try/__finally wrapper is required for windows CRT/Loader interoperability logic
	__try {
		auto wCommandLine = GetCommandLineW();
		int argc = 0;
		wchar_t **wargv = wCommandLine ? CommandLineToArgvW(wCommandLine, &argc) : nullptr;

		char *buf = nullptr;
		char **argvTarget = nullptr;
		int outArgc = 0;

		// The command line is attacker-controlled. wcstombs returns (size_t)-1
		// for any argument containing a character not representable in the
		// active multibyte locale; using that as a length under-allocates and
		// then performs a wild out-of-bounds store. Validate every conversion
		// and every Win32 return; on any failure fall back to an empty argv
		// rather than corrupting memory before main() runs.
		if (wargv && argc > 0) {
			size_t blockSize = (size_t)argc * sizeof(char *);
			bool ok = true;
			for (int i = 0; i < argc; ++i) {
				size_t len = wcstombs(nullptr, wargv[i], 0);
				if (len == (size_t)-1) {
					ok = false;
					break;
				}
				blockSize += len + 2;
			}

			if (ok) {
				buf = (char *)malloc(blockSize);
			}

			if (buf) {
				argvTarget = (char **)buf;
				char *stringsTarget = buf + (size_t)argc * sizeof(char *);
				size_t bufferSize = blockSize - (size_t)argc * sizeof(char *);

				for (int i = 0; i < argc; ++i) {
					size_t len = wcstombs(stringsTarget, wargv[i], bufferSize);
					if (len == (size_t)-1 || len >= bufferSize) {
						ok = false;
						break;
					}
					stringsTarget[len] = 0;

					argvTarget[i] = stringsTarget;
					stringsTarget += len + 1;
					bufferSize -= len + 1;
				}

				if (ok) {
					outArgc = argc;
				} else {
					// Partial/failed conversion: do not hand main() a
					// half-populated argv.
					argvTarget = nullptr;
				}
			}
		}

		if (wargv) {
			LocalFree(wargv);
		}

		// load WSA unconditionally so c socket API should work natively
		int wsaStartupResult = -1;
		if (manageWsa) {
			WSADATA wsaData;
			wsaStartupResult = WSAStartup(0x0202, &wsaData); // winsock 2.2

			if (wsaStartupResult != 0) {
				printf("WSAStartup failed: %d\n", wsaStartupResult);
			} else if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
				printf("Could not find a usable version of Winsock.dll\n");
				WSACleanup();
			}
		}

		__argc = outArgc;
		__argv = argvTarget;

		ret = mainFn(outArgc, (const char **)argvTarget);

		if (manageWsa && wsaStartupResult == 0) {
			WSACleanup();
		}

		free(buf); // free(nullptr) is a no-op
	}
	__finally {
	}

	// Exit normally
	exit(ret);

	// Are you dead yet?
	__builtin_unreachable();

	return ret;
}

#if defined(SPRT_BUILD_SHARED_RUNTIME)

// Entry point body for applications that link the shared runtime. The application
// image keeps only a tiny stub (see include/sprt/wrappers/windows/app_startup.h): the
// stub runs its own .CRT initializers - which are per-image and therefore invisible
// from here - and then hands its main() to this exported function. Everything the stub
// would otherwise have to duplicate (command-line conversion, exit sequencing) stays in
// the one image that owns the runtime.
__SPRT_C_FUNC SPRT_API int __sprt_app_startup(__sprt_main_fn mainFn) {
	return __sprt_invoke_main(mainFn, false);
}

#else // SPRT_BUILD_SHARED_RUNTIME

// weak for the same reason as the shared-runtime stub's entry point: an application that
// links the static runtime and wants to own its entry point can define mainCRTStartup
// itself, and the strong definition wins rather than colliding with this one.
__SPRT_C_FUNC __attribute__((weak)) __declspec(safebuffers) int mainCRTStartup() {
	auto ret = __sprt_runtime_attach();
	if (ret != 0) {
		return ret;
	}

	return __sprt_invoke_main(&main, true);
}

#endif // SPRT_BUILD_SHARED_RUNTIME
