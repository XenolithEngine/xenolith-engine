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

__SPRT_BEGIN_DECL

typedef int(__cdecl *__sprt_main_fn)(int, const char **);

/*
	Convert the command line to argv, call mainFn and exit. Never returns.

	Implemented in the image that owns the runtime, so the executable stub does not
	duplicate command-line conversion or the exit sequence.
*/
SPRT_API int __sprt_app_startup(__sprt_main_fn mainFn);

__SPRT_END_DECL

#endif // RUNTIME_INCLUDE_SPRT_WRAPPERS_WINDOWS_APP_STARTUP_H_
