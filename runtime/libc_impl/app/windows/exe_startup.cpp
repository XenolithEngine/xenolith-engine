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
	Executable entry point for images linking the shared runtime (sprt.dll).

	Its own translation unit, apart from app_startup.cpp, because the two entry points are
	alternatives and an archive member is pulled whole: a DLL that dragged this object in
	would inherit the reference to main() below and fail to link. Kept separate, the linker
	pulls whichever entry point the image kind actually needs, and both share the per-image
	half from app_startup.cpp.

	The runtime is already up when this runs - the executable imports sprt.dll, so the
	loader has executed its DLL_PROCESS_ATTACH first.
*/

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/__sprt_stdlib.h>
#include <sprt/wrappers/windows/app_startup.h>

__cdecl int main(int argc, const char *argv[]);

extern "C" {

/* Defined in app_startup.cpp, on this image's own sections. */
int __sprt_image_init_c(void);
int __sprt_image_init_cxx(void);
void __sprt_image_init_tls(void);
void __sprt_image_init_cookie(void);
void __sprt_image_run_terminators(void);

/*
	weak: an image that wants to own its entry point just defines mainCRTStartup, and the
	strong definition wins instead of colliding.

	safebuffers because this seeds the image's /GS cookie - it must not be instrumented
	against a value it is about to change.
*/
__attribute__((weak)) __declspec(safebuffers) int mainCRTStartup() {
	__sprt_image_init_cookie();

	// Registered before the initializers run, so LIFO ordering puts it behind every static
	// destructor they register. __sprt_atexit is the runtime's exported implementation -
	// the plain atexit name would resolve through the import thunk just as well, but this
	// stub is runtime code and can name the internal entry point directly.
	if (__sprt_atexit(&__sprt_image_run_terminators) != 0) {
		return 1;
	}

	// This image's C initializers, then its static C++ constructors, then its thread_local
	// constructors for the main thread. The runtime's own already ran inside sprt.dll, on
	// sprt.dll's sections.
	if (__sprt_image_init_c() != 0) {
		return 1;
	}

	if (__sprt_image_init_cxx() != 0) {
		return 1;
	}

	__sprt_image_init_tls();

	// Never returns - ends in exit(), which drains atexit handlers and static destructors
	// inside the runtime.
	return __sprt_app_startup(&main);
}

} // extern "C"
