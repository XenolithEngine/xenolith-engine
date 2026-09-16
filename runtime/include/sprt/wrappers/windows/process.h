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

#ifndef SPRT_WRAPPERS_WINDOWS_PROCESS_H_
#define SPRT_WRAPPERS_WINDOWS_PROCESS_H_

#include <sprt/wrappers/windows/complex_types.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/wrappers/windows/process_api.h>
#include <sprt/wrappers/windows/__sprt_threads.h>

/*
	CRT half of <process.h>.

	This header answers a plain `#include <process.h>`, so it has to carry what MSVC puts
	there and Windows code reaches for without including <unistd.h>: _beginthreadex (via
	__sprt_threads.h above) and the process id. Both names, because MSVC declares _getpid
	and, unless _CRT_NO_NONSTDC_NAMES is set, the bare getpid as well - the one LLDB's
	Log.cpp uses on Windows.

	Declared here rather than by pulling in the whole sprt <unistd.h>, which would drag the
	POSIX libc into every Win32 translation unit. The guard is shared with that header so
	whichever is included first wins and the other stays quiet.
*/

#include <sprt/c/__sprt_unistd.h>

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
__SPRT_ID(pid_t)
_getpid(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(getpid)();
}
#endif

#ifndef __SPRT_GETPID_DEFINED
#define __SPRT_GETPID_DEFINED
SPRT_UMBRELLA_FUNC
__SPRT_ID(pid_t)
getpid(void) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __SPRT_ID(getpid)();
}
#endif
#endif // __SPRT_GETPID_DEFINED

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_PROCESS_H_
