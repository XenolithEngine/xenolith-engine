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
	Consumer-side CRT stub for images that link the shared runtime (sprt.dll): the
	per-image machinery every PE image carries for itself, plus the DLL entry point. The
	executable entry point lives in exe_startup.cpp, separately - see _DllMainCRTStartup
	below for why they must not share an object.

	See <sprt/wrappers/windows/app_startup.h> for why these pieces cannot be imported from
	the DLL at all.

	It lives outside libc_impl/src on purpose: that directory is swept into the runtime
	module itself, and this translation unit belongs to the *consumer* image.

	The runtime is already up by the time any of this runs - the consuming image imports
	sprt.dll, so the loader has executed its DLL_PROCESS_ATTACH (which constructs the heap,
	stdio, TLS and exception machinery) first.
*/

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/wrappers/windows/app_startup.h>
#include <sprt/wrappers/windows/dl_api.h>

// Private to the runtime tree, but this stub is the runtime's own executable-side half.
#include "../../src/windows/initterm.h"

// The .CRT initializer sections, the TLS directory and _fltused. Shared verbatim with
// the runtime's own entry point (libc_impl/src/windows/startup.cc), because these are
// per-image parts that each image has to carry for itself - see the subunit's header
// comment for why none of them can come from the DLL.
#include "../../src/windows/crt_image.cc"

extern "C" {

/*
	Terminators, mirroring the initializer markers in crt_image.cc.

	Not shared with the runtime even though both images have these sections, because the
	two drive them differently: the runtime walks its own directly from exit(), while an
	executable has to register a walker with atexit() since exit() lives in the DLL.

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
	Walks this image's terminator sections, in reverse within each as the CRT contract
	requires. External linkage: the executable entry point (exe_startup.cpp) registers it
	with atexit, while the DLL entry point below calls it from DLL_PROCESS_DETACH.

	For an executable it is registered before this image's initializers run, so the
	runtime's LIFO atexit list reproduces the order a static build gets from exit(): every
	static destructor first (each registered later, therefore drained earlier), then the
	pre-terminators, then the terminators - and only after all of that does the runtime
	reach its own sections and close the streams.
*/
void __sprt_image_run_terminators(void) {
	__initterm(__c_preterm_start, __c_preterm_end, true);
	__initterm(__c_term_start, __c_term_end, true);
}

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

/*
	Default DllMain.

	_DllMainCRTStartup calls DllMain unconditionally, the way MSVC's does, so a consumer
	that wants one just defines it and the strong definition wins over this weak stub.
	Neither libclang.dll nor LTO.dll defines one, which is the common case.
*/
__attribute__((weak)) BOOL WINAPI DllMain(void *, DWORD, void *) { return TRUE; }

/*
	Entry point for a consumer DLL.

	Deliberately NOT in the same object as mainCRTStartup: they are alternatives, and an
	archive member is pulled whole. With both in one object a DLL would drag in
	mainCRTStartup as well and fail on its unresolved reference to main - which is exactly
	what libclang.dll and LTO.dll did. Keeping the entry points apart lets the linker pull
	only the one that matches the image kind, while this file's per-image half (the .CRT
	sections, TLS directory and /GS cookie above) serves both.

	The runtime is up before any of this runs: sprt.dll is an import of the consuming DLL,
	so the loader initializes it first.

	safebuffers because this seeds the image's own /GS cookie - it must not be instrumented
	against a value it is about to change.
*/
__attribute__((weak)) __declspec(safebuffers) BOOL WINAPI _DllMainCRTStartup(void *instance,
		DWORD reason, void *reserved) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		__sprt_image_init_cookie();

		if (__sprt_image_init_c() != 0 || __sprt_image_init_cxx() != 0) {
			return FALSE;
		}

		// For the thread performing the load; later threads come through the TLS callback.
		__sprt_image_init_tls();
		break;

	case DLL_THREAD_ATTACH: __sprt_image_init_tls(); break;

	case DLL_PROCESS_DETACH:
		// Run this image's terminator sections here rather than from atexit: a DLL can be
		// unloaded long before exit(), and after FreeLibrary an atexit entry pointing into
		// it would be a dangling call.
		//
		// Static destructors are a different matter - the MSVC ABI registers them with
		// atexit, into the runtime's process-global list, so they run at exit() and not at
		// unload. That is correct for a DLL that stays loaded for the process lifetime
		// (libclang.dll, LTO.dll); a DLL that is genuinely unloaded early would need
		// per-module onexit tables, which sprt does not have.
		__sprt_image_run_terminators();
		break;

	case DLL_THREAD_DETACH:
	default: break;
	}

	return DllMain(instance, reason, reserved);
}

} // extern "C"
