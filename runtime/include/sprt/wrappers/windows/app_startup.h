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

#ifndef RUNTIME_INCLUDE_SPRT_WRAPPERS_WINDOWS_APP_STARTUP_H_
#define RUNTIME_INCLUDE_SPRT_WRAPPERS_WINDOWS_APP_STARTUP_H_

/*
	Executable-side entry point contract for the shared runtime (sprt.dll).

	With the static runtime the executable *is* the runtime: a single mainCRTStartup in
	sprt.lib brings up the heap, stdio, TLS and exception machinery, runs the image's
	static initializers and calls main().

	With the shared runtime that job is split, because three things are per-image on PE
	and cannot be imported from a DLL no matter how the export table is written:

	  - the .CRT$X* initializer sections (each image has its own set of static
	    initializers, delimited by markers the linker resolves within that image);
	  - the /GS security cookie, which the image's own prologues reference directly;
	  - the TLS directory, if the image declares thread_local storage.

	So sprt.dll brings the runtime up in DLL_PROCESS_ATTACH - which the loader runs
	*before* the executable's entry point, so the heap and stdio are already live when
	application static initializers execute - and the executable keeps a small stub
	(libc_impl/src/windows/app_startup.cc) that owns only the per-image parts and then
	calls __sprt_app_startup() for everything else.
*/

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_uintptr_t.h>

__SPRT_BEGIN_DECL

typedef int(__cdecl *__sprt_main_fn)(int, const char **);

/*
	Produce a fresh /GS security cookie.

	The cookie variable itself is per-image (below) - the image's own function prologues
	reference it directly, so it cannot be shared through an import. Only the entropy
	source is shared, and it has to be: reaching it needs the DLL loader, which is
	runtime-internal.

	Never returns __SPRT_DEFAULT_SECURITY_COOKIE, so a caller can use that value as an
	"uninitialized" marker.
*/
SPRT_API __SPRT_ID(uintptr_t) __sprt_gencookie(void);

/*
	The value the cookie variables hold before the entry point seeds them.
*/
#define __SPRT_DEFAULT_SECURITY_COOKIE 0x00002B992DDFA232ll

/*
	This image's /GS cookie, defined by the startup stub (through crt_image.cc) and
	seeded from __sprt_gencookie before any instrumented code in the image runs.

	Deliberately NOT declared SPRT_API: these are per-image definitions, and marking them
	dllimport would point the image's own prologues at sprt.dll's copy instead - a cookie
	only ever has to agree with itself, so sharing one across images is at best pointless
	and at worst a mismatch waiting to trap.
*/
extern __SPRT_ID(uintptr_t) __security_cookie;
extern __SPRT_ID(uintptr_t) __security_cookie_complement;

/*
	Convert the command line to argv, call mainFn and exit. Never returns.

	Implemented in the image that owns the runtime, so the executable stub does not
	duplicate command-line conversion or the exit sequence.
*/
SPRT_API int __sprt_app_startup(__sprt_main_fn mainFn);

__SPRT_END_DECL

#endif // RUNTIME_INCLUDE_SPRT_WRAPPERS_WINDOWS_APP_STARTUP_H_
