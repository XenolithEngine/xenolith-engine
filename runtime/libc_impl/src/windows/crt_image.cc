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

/*
	Per-image CRT machinery: the parts of the C runtime that every PE image carries for
	itself, no matter who owns the process-wide runtime state.

	Included by both entry points, exactly once per image:
	  - windows/startup.cc          - the runtime itself (sprt.lib, or sprt.dll)
	  - app/windows/app_startup.cpp - an executable linking the runtime as a DLL

	Nothing here can be shared through a DLL export, which is why it is a .cc subunit
	rather than a library:

	  - the .CRT$X* markers are resolved by the linker within one image, so each image's
	    initializer sections are invisible to every other image;
	  - the TLS directory is per-image by construction - the loader hands out one slot
	    index per image that declares thread_local data, copies that image's .tls
	    template into every thread, and stores the index where that image's own code
	    reads it. Importing the runtime's __dyn_tls_init or __tls_guard would walk
	    sprt.dll's section and flip sprt.dll's guard, leaving the including image's
	    thread_local variables unconstructed while looking initialized;
	  - _fltused only has to exist in whichever image touches floating point.

	The symbol names are fixed by the compiler and the linker, not chosen here. None of
	them are exported from sprt.dll, precisely so each image can define its own.

	NOT here, and deliberately so: the .CRT$XP and .CRT$XT terminator sections. Both
	images have them, but they are driven differently - the runtime walks its own from
	exit(), while an executable has to register a walker with atexit() because exit()
	lives elsewhere. See terminate.cc and app_startup.cpp respectively.

	Expects <sprt/wrappers/windows/dl_api.h>, <sprt/wrappers/windows/app_startup.h> and
	"initterm.h" to have been included.
*/


extern "C" {

/*
	Static initializers.

	These markers bracket the .CRT sections of the image this subunit was compiled into.
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

// Consumed by the linker through their section placement rather than by any code here,
// so mark them used: it both documents the fact and keeps optimization from dropping
// them.
static __declspec(allocate(".CRT$XLA")) PIMAGE_TLS_CALLBACK __tls_storage_start = 0;
[[gnu::used]]
static __declspec(allocate(".CRT$XLZ")) PIMAGE_TLS_CALLBACK __tls_storage_end = 0;

// Presence of _tls_used is what makes the linker emit a TLS directory into the PE header.
__declspec(allocate(".rdata$T")) extern const IMAGE_TLS_DIRECTORY64 _tls_used = {
	(ULONGLONG)&_tls_index_start,
	(ULONGLONG)&_tls_index_end,
	(ULONGLONG)&_tls_index,
	(ULONGLONG)(&__tls_storage_start + 1),
	(ULONG)0,
	{(ULONG)0},
};

extern const PIMAGE_TLS_CALLBACK __dyn_tls_init_callback = __dyn_tls_init;

[[gnu::used]]
static __declspec(allocate(".CRT$XLC")) PIMAGE_TLS_CALLBACK __tls_delegate = __dyn_tls_init;
static __declspec(allocate(".CRT$XDA")) __funcptr __tls_init_start_fn = nullptr;
static __declspec(allocate(".CRT$XDZ")) __funcptr __tls_init_end_fn = nullptr;

// Read and written through this image's own TLS slot, so it tracks initialization per
// thread. Also what clang consults to guard a thread_local's dynamic initializer.
thread_local bool __tls_guard = false;

void __dyn_tls_init(PVOID, DWORD dwReason, LPVOID) noexcept {
	if (dwReason != DLL_THREAD_ATTACH || __tls_guard == true) {
		return;
	}

	__tls_guard = true;

	__initterm(&__tls_init_start_fn, &__tls_init_end_fn);
}

// clang emits a call to this at every access to a thread_local with a dynamic
// initializer, for the case where the accessing thread has not been through a TLS
// callback yet.
void __dyn_tls_on_demand_init() noexcept {
	__dyn_tls_init(nullptr, DLL_THREAD_ATTACH, nullptr); //
}

// Legacy floating point library support flag. Referenced by the image's own code
// whenever it touches floating point; only its existence matters, never its value.
int _fltused = 1;

/*
	/Zc:threadSafeInit epoch for this image.

	thread_local, so it belongs to the image's own TLS directory, and the compiler reads
	it directly at every function-local static with no declaration in sight - the same
	reason __tls_guard cannot be imported either. The process-wide counter it is compared
	against, and the _Init_thread_* helpers that advance it, stay in the runtime.
*/
__declspec(thread) int _Init_thread_epoch = (int)0x8000'0000;

/*
	/GS security cookie.

	Per-image like everything else here, and for a stronger reason than convention: the
	compiler bakes a direct reference to __security_cookie into every instrumented
	function's prologue and epilogue, so it is not something an import thunk could stand
	in for. MSVC arranges it the same way - a copy per image, out of the static
	libvcruntime stub, even when the CRT is a DLL.

	Two images therefore hold two independent cookies, which is exactly right: a cookie
	only ever has to agree with itself, between one function's prologue and its own
	epilogue, and never travels across an image boundary.

	Only the entropy source is shared, through __sprt_gencookie in the runtime.
*/

__declspec(selectany) UINT_PTR __security_cookie = __SPRT_DEFAULT_SECURITY_COOKIE;
__declspec(selectany) UINT_PTR __security_cookie_complement = ~(__SPRT_DEFAULT_SECURITY_COOKIE);

__declspec(safebuffers) void __fastcall __security_check_cookie(UINT_PTR cookie) __SPRT_NOEXCEPT {
	if (cookie != __security_cookie) {
		__debugbreak();
	}
}

} // extern "C"

/*
	Run this image's C initializers. Returns non-zero if one of them failed.
*/
__SPRT_C_FUNC int __sprt_image_init_c() { return __initterm(__c_init_start, __c_init_end); }

/*
	Run this image's static C++ constructors. Returns non-zero if one of them failed.
*/
__SPRT_C_FUNC int __sprt_image_init_cxx() { return __initterm(__cxx_init_start, __cxx_init_end); }

/*
	Run this image's thread_local constructors for the calling thread.

	Needed explicitly for the thread that reaches the entry point: the loader invokes TLS
	callbacks with DLL_THREAD_ATTACH only for threads created later. __tls_guard makes it
	idempotent, so it is also safe after a static initializer has already touched a
	thread_local through __dyn_tls_on_demand_init.
*/
__SPRT_C_FUNC void __sprt_image_init_tls() { __dyn_tls_init(nullptr, DLL_THREAD_ATTACH, nullptr); }

/*
	Seed this image's /GS cookie.

	Must run before any instrumented function in this image returns, because a prologue
	that captured the old value would then check against the new one and trap. That is
	why the entry points that call this are __declspec(safebuffers) - they must not be
	instrumented themselves.

	Idempotent: the compile-time default doubles as an "uninitialized" marker, and
	__sprt_gencookie never returns it.
*/
__SPRT_C_FUNC __declspec(safebuffers) void __sprt_image_init_cookie() {
	if (__security_cookie != __SPRT_DEFAULT_SECURITY_COOKIE) {
		return;
	}

	auto cookie = __sprt_gencookie();

	__security_cookie = cookie;
	__security_cookie_complement = ~cookie;
}
