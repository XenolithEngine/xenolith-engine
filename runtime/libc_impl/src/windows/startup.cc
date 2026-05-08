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

#include "stdlib.h"
#include "../../include/__impl_libc.h"

#include "initterm.h"
#include "dll/dllloader.h"

#define DEFAULT_SECURITY_COOKIE 0x0000'2B99'2DDF'A232ll

__cdecl int main(int argc, const char *argv[]);

extern "C" {

/*
	Static initializers
*/

static __declspec(allocate(".CRT$XIA")) __ifuncptr __c_init_start[] = {nullptr};
static __declspec(allocate(".CRT$XIZ")) __ifuncptr __c_init_end[] = {nullptr};
static __declspec(allocate(".CRT$XCA")) __funcptr __cxx_init_start[] = {nullptr};
static __declspec(allocate(".CRT$XCZ")) __funcptr __cxx_init_end[] = {nullptr};

/*
	TLS routines
*/

void WINAPI __dyn_tls_init(PVOID, DWORD dwReason, LPVOID) noexcept;

ULONG _tls_index = 0;

#pragma data_seg(".tls")

static __declspec(allocate(".tls")) char _tls_index_start = 0;

#pragma data_seg(".tls$ZZZ")

static __declspec(allocate(".tls$ZZZ")) char _tls_index_end = 0;

#pragma data_seg()

static __declspec(allocate(".CRT$XLA")) PIMAGE_TLS_CALLBACK __tls_storage_start = 0;
static __declspec(allocate(".CRT$XLZ")) PIMAGE_TLS_CALLBACK __tls_storage_end = 0;

__declspec(allocate(".rdata$T")) extern const IMAGE_TLS_DIRECTORY64 _tls_used = {
	(ULONGLONG)&_tls_index_start,
	(ULONGLONG)&_tls_index_end,
	(ULONGLONG)&_tls_index,
	(ULONGLONG)(&__tls_storage_start + 1),
	(ULONG)0,
	{(ULONG)0},
};

extern const PIMAGE_TLS_CALLBACK __dyn_tls_init_callback = __dyn_tls_init;

static __declspec(allocate(".CRT$XLC")) PIMAGE_TLS_CALLBACK __tls_delegate = __dyn_tls_init;
static __declspec(allocate(".CRT$XDA")) __funcptr __tls_init_start_fn = nullptr;
static __declspec(allocate(".CRT$XDZ")) __funcptr __tls_init_end_fn = nullptr;

thread_local bool __tls_guard = false;

void __dyn_tls_init(PVOID, DWORD dwReason, LPVOID) noexcept {
	if (dwReason != DLL_THREAD_ATTACH || __tls_guard == true) {
		return;
	}

	__tls_guard = true;

	__initterm(&__tls_init_start_fn, &__tls_init_end_fn);
}

void __dyn_tls_on_demand_init() noexcept { __dyn_tls_init(nullptr, DLL_THREAD_ATTACH, nullptr); }

/*
	/Zc:threadSafeInit support
*/

int _Init_global_epoch = sprt::Min<int>;
__declspec(thread) int _Init_thread_epoch = sprt::Min<int>;

// With some compiler support, it's implementable with a pure futex, but not today...
static sprt::mutex s_threadGuardMutex;
static sprt::condition_variable s_threadGuardWaitCond;

void __cdecl _Init_thread_header(int *const pOnce) noexcept {
	sprt::unique_lock lock(s_threadGuardMutex);

	if (*pOnce == 0) {
		*pOnce = -1;
		return;
	} else {
		while (*pOnce == -1) {
			s_threadGuardWaitCond.wait(lock);
			if (*pOnce == 0) {
				*pOnce = -1;
				return;
			}
		}
	}
}

// Exception during init, we should drop LOCK_BIT and signal to wakeup
// Calling thread should own the lock
extern "C" void __cdecl _Init_thread_abort(__sprt_uint32_t *const pOnce) noexcept {
	// set complete flag and check for a waiters
	s_threadGuardMutex.lock();
	*pOnce = 0;
	s_threadGuardMutex.unlock();

	// Wake only one thread to try initialization again
	s_threadGuardWaitCond.notify_one();
}

extern "C" void __cdecl _Init_thread_footer(__sprt_uint32_t *const pOnce) noexcept {
	// set complete flag and check for a waiters
	s_threadGuardMutex.lock();
	++_Init_global_epoch;
	_Init_thread_epoch = _Init_global_epoch;
	*pOnce = _Init_thread_epoch;
	s_threadGuardMutex.unlock();

	// Wake all of the threads to exit from _Init_thread_header
	s_threadGuardWaitCond.notify_all();
}

/*
	GS support (based on https://github.com/sysfce2/nocrt/blob/main/nocrt_exe.c)
*/

__declspec(selectany) UINT_PTR __security_cookie = DEFAULT_SECURITY_COOKIE;
__declspec(selectany) UINT_PTR __security_cookie_complement = ~(DEFAULT_SECURITY_COOKIE);

SAFELOADER UINT_PTR __gencookie() {
	auto loader = sprt::DllLoader::get();
	UINT_PTR cookie = 0;
	if (__security_cookie != DEFAULT_SECURITY_COOKIE) {
		HMODULE BCryptPrimitives = loader->__LoadLibraryW(L"BCryptPrimitives.dll");
		if (BCryptPrimitives) {
			BOOL (*ProcessPrng)(PBYTE, SIZE_T) = nullptr;
			ProcessPrng = reinterpret_cast<decltype(ProcessPrng)>(
					loader->__GetProcAddress(BCryptPrimitives, "ProcessPrng"));
			if (ProcessPrng) {
				ProcessPrng((PBYTE)&__security_cookie, sizeof(__security_cookie));
			}
			loader->__FreeLibrary(BCryptPrimitives);
		}
	}

	if (cookie == 0) {
		// Failed to use ProcessPrng, fallback to processor counter,
		// No other available entropy source at this moment;
		cookie = (__rdtsc() ^ 0x7A2D'9F1B'4E63'C082ll) & 0x0000'ffff'ffff'ffffll;
	}

	if (cookie == DEFAULT_SECURITY_COOKIE) {
		cookie = DEFAULT_SECURITY_COOKIE + 1;
	}

	return cookie;
}

SAFELOADER void __security_init_cookie() {
	if (__security_cookie != DEFAULT_SECURITY_COOKIE) {
		return;
	}

	auto cookie = __gencookie();

	__security_cookie = cookie;
	__security_cookie_complement = ~cookie;
}

static unsigned char s_libcBuffer[sizeof(sprt::__libc)];

int _fltused = -1;

sprt::__libc *sprt::__libc::get() { return reinterpret_cast<__libc *>(s_libcBuffer); }

__cdecl int mainCRTStartup() {
	auto ret = sprt::DllLoader::construct()->load();
	if (ret != 0) {
		return ret;
	}

	new (s_libcBuffer, sprt::nothrow) sprt::__libc;

	_fltused = 1;

	{ __security_init_cookie(); }

	__dyn_tls_init(nullptr, DLL_THREAD_ATTACH, nullptr);

	if (__initterm(__c_init_start, __c_init_end) != 0) {
		return LOADER_ERROR_STATIC_C_INIT_FAILED; // Error in c initialization
	}
	if (__initterm(__cxx_init_start, __cxx_init_end) != 0) {
		return LOADER_ERROR_STATIC_CXX_INIT_FAILED; // Error in c++ initialization
	}

	ret = main(0, nullptr);

	exit(ret);

	__builtin_unreachable();

	return ret;
}
}
