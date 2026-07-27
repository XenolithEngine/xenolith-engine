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
	Executable-side CRT startup stub for applications that link the shared runtime
	(sprt.dll). Add this file to the application's sources; see
	<sprt/wrappers/windows/app_startup.h> for why these particular pieces cannot be
	imported from the DLL.

	It lives outside libc_impl/src on purpose: that directory is swept into the runtime
	module itself, and this translation unit belongs to the *consumer* image.

	The runtime is already up by the time mainCRTStartup runs - the executable imports
	sprt.dll, so the loader has executed its DLL_PROCESS_ATTACH (which constructs the
	heap, stdio, TLS and exception machinery) before transferring control here.
*/

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/wrappers/windows/app_startup.h>
#include <sprt/wrappers/windows/dl_api.h>

// Private to the runtime tree, but this stub is the runtime's own executable-side half.
#include "../../src/windows/initterm.h"

__cdecl int main(int argc, const char *argv[]);

extern "C" {

/*
	Static initializers.

	These markers bracket *this image's* .CRT sections. sprt.dll has its own set and runs
	them from its own entry point; neither image can see the other's.
*/

static __declspec(allocate(".CRT$XIA")) __ifuncptr __c_init_start[] = {nullptr};
static __declspec(allocate(".CRT$XIZ")) __ifuncptr __c_init_end[] = {nullptr};
static __declspec(allocate(".CRT$XCA")) __funcptr __cxx_init_start[] = {nullptr};
static __declspec(allocate(".CRT$XCZ")) __funcptr __cxx_init_end[] = {nullptr};

/*
	Terminators, mirroring the initializer markers above.

	Static destructors themselves need nothing here: the MSVC ABI registers each one with
	atexit() as its object is constructed, and exit() inside the DLL drains that list -
	which is process-global, so the executable's destructors are already covered.

	The .CRT terminator sections are the part that is not: exit() walks the ones in
	sprt.dll (see the tail of libc_impl/src/windows/terminate.cc), and those markers
	delimit the DLL image only. Anything the executable emits into its own .CRT$XP and
	.CRT$XT sections would otherwise be silently dropped.
*/

static __declspec(allocate(".CRT$XPA")) __funcptr __c_preterm_start[] = {nullptr};
static __declspec(allocate(".CRT$XPZ")) __funcptr __c_preterm_end[] = {nullptr};
static __declspec(allocate(".CRT$XTA")) __funcptr __c_term_start[] = {nullptr};
static __declspec(allocate(".CRT$XTZ")) __funcptr __c_term_end[] = {nullptr};

/*
	Registered with atexit() before this image's initializers run, so the runtime's LIFO
	atexit list reproduces the order a static build gets from exit(): every static
	destructor first (each registered later, therefore drained earlier), then this
	image's pre-terminators, then its terminators - and only after all of that does the
	runtime reach its own preterm/term sections and close the streams.

	Reverse order within each section, as the CRT contract requires.
*/
static void __sprt_app_run_terminators(void) {
	__initterm(__c_preterm_start, __c_preterm_end, true);
	__initterm(__c_term_start, __c_term_end, true);
}

/*
	Thread-local storage.

	The TLS directory is per-image by construction: the loader allocates one slot index
	per image that declares thread_local data, copies that image's .tls template into
	every thread, and stores the index where the image's own code reads it. sprt.dll's
	directory therefore covers the DLL's thread_local variables and nothing else - an
	executable that declares any of its own needs the whole apparatus again.

	Mirrors the block in libc_impl/src/windows/startup.cc. The symbol names are fixed by
	the compiler and linker, not chosen here; none of them are exported from sprt.dll,
	precisely so each image can define its own.

	Destructors are the exception and do not need a per-image copy: clang registers each
	thread_local destructor by calling __tlregdtor, which the runtime exports, and the
	list it feeds is thread-local state inside sprt.dll. The DLL's own TLS callback
	drains it on DLL_THREAD_DETACH, for threads and images alike.
*/

void WINAPI __dyn_tls_init(PVOID, DWORD dwReason, LPVOID) noexcept;

ULONG _tls_index = 0;

#pragma data_seg(".tls")

static __declspec(allocate(".tls")) char _tls_index_start = 0;

#pragma data_seg(".tls$ZZZ")

static __declspec(allocate(".tls$ZZZ")) char _tls_index_end = 0;

#pragma data_seg()

// Consumed by the linker through their section placement rather than by any code here,
// so mark them used: it both documents the fact and keeps -O2 from dropping them.
static __declspec(allocate(".CRT$XLA")) PIMAGE_TLS_CALLBACK __tls_storage_start = 0;
[[gnu::used]] static __declspec(allocate(".CRT$XLZ")) PIMAGE_TLS_CALLBACK __tls_storage_end = 0;

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

[[gnu::used]] static __declspec(allocate(".CRT$XLC")) PIMAGE_TLS_CALLBACK __tls_delegate =
		__dyn_tls_init;
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
// callback yet. It must NOT be imported from the runtime: the runtime's copy walks
// sprt.dll's .CRT$XD section and flips sprt.dll's __tls_guard, which would leave this
// image's thread_local variables unconstructed while looking initialized.
void __dyn_tls_on_demand_init() noexcept {
	__dyn_tls_init(nullptr, DLL_THREAD_ATTACH, nullptr); //
}

/*
	Legacy floating point library support flag. The image's own code references it
	whenever it touches floating point, so it has to be defined per image.
*/
int _fltused = 1;

/*
	type_info's vtable pointer.

	Every RTTI Type Descriptor clang emits for a type used with typeid, dynamic_cast or
	throw starts with a pointer to this symbol, and the reference is a *data* relocation
	the compiler emits with no declaration in scope - so unlike the ABI functions it
	cannot be resolved through an import thunk. MSVC has the same constraint and solves
	it the same way: a per-image copy out of the static libvcruntime stub, even when the
	CRT itself is a DLL.

	A second copy is harmless because type identity in the MSVC ABI is established by the
	descriptor's decorated name, not by the address of this vtable.

	The layout mirrors TypeDescriptor in libc_impl/src/windows/libcxx.cc; only the first
	field is ever read through this symbol.
*/
struct __sprt_type_descriptor {
	const void *type_info_vtable;
	void *spare;
	char __decorated_name[16];
};

extern const __sprt_type_descriptor __sprt_app_type_info_vftable __asm__("??_7type_info@@6B@");

const __sprt_type_descriptor __sprt_app_type_info_vftable = {
	&__sprt_app_type_info_vftable,
	nullptr,
	".?AVtype_info@@",
};

int mainCRTStartup() {
	// Registered before the initializers run, so LIFO ordering puts it behind every
	// static destructor they register. __sprt_atexit is the runtime's exported
	// implementation - the plain atexit name resolves through the import thunk, but this
	// stub is runtime code and can name the internal entry point directly.
	if (__sprt_atexit(&__sprt_app_run_terminators) != 0) {
		return 1;
	}

	// Run this image's C initializers, then its static C++ constructors. The runtime's
	// own initializers already ran inside sprt.dll.
	if (__initterm(__c_init_start, __c_init_end) != 0) {
		return 1;
	}

	if (__initterm(__cxx_init_start, __cxx_init_end) != 0) {
		return 1;
	}

	// Run this image's thread_local constructors for the main thread. The loader invokes
	// TLS callbacks with DLL_THREAD_ATTACH only for threads created later, so the thread
	// that reaches the entry point has to be initialized by hand. __tls_guard makes this
	// idempotent if a static initializer already touched a thread_local.
	__dyn_tls_init(nullptr, DLL_THREAD_ATTACH, nullptr);

	// Never returns - ends in exit(), which drains atexit handlers and static
	// destructors inside the runtime.
	return __sprt_app_startup(&main);
}

} // extern "C"
