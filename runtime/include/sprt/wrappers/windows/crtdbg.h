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

// <crtdbg.h>: the MSVC debug-CRT report hooks. This freestanding runtime has no debug
// heap, so the report machinery is compiled as inline no-ops (as the real CRT does when
// _DEBUG is off). llvm's Signals.inc installs a report hook to suppress message boxes;
// with no debug reports there is nothing to suppress, so the no-op is correct.

#ifndef SPRT_WRAPPERS_WINDOWS_CRTDBG_H_
#define SPRT_WRAPPERS_WINDOWS_CRTDBG_H_

#include <sprt/wrappers/windows/abi/crtdbg.h>

/* Clean public names (materialized __SPRT_ values / _CRT_REPORT_HOOK live in abi/crtdbg.h) */
#define _CRT_WARN __SPRT__CRT_WARN
#define _CRT_ERROR __SPRT__CRT_ERROR
#define _CRT_ASSERT __SPRT__CRT_ASSERT
#define _CRT_ERRCNT __SPRT__CRT_ERRCNT

#define _CRTDBG_MODE_FILE __SPRT__CRTDBG_MODE_FILE
#define _CRTDBG_MODE_DEBUG __SPRT__CRTDBG_MODE_DEBUG
#define _CRTDBG_MODE_WNDW __SPRT__CRTDBG_MODE_WNDW
#define _CRTDBG_REPORT_MODE __SPRT__CRTDBG_REPORT_MODE
#define _CRTDBG_HFILE_ERROR __SPRT__CRTDBG_HFILE_ERROR
#define _CRTDBG_FILE_STDOUT __SPRT__CRTDBG_FILE_STDOUT
#define _CRTDBG_FILE_STDERR __SPRT__CRTDBG_FILE_STDERR

#ifdef __cplusplus
extern "C" {
#endif

static inline _CRT_REPORT_HOOK _CrtSetReportHook(_CRT_REPORT_HOOK _Hook) {
	(void) _Hook;
	return 0;
}
static inline int _CrtSetReportMode(int _ReportType, int _ReportMode) {
	(void) _ReportType;
	(void) _ReportMode;
	return 0;
}
static inline void *_CrtSetReportFile(int _ReportType, void *_ReportFile) {
	(void) _ReportType;
	return _ReportFile;
}

#ifdef __cplusplus
}
#endif

#endif // SPRT_WRAPPERS_WINDOWS_CRTDBG_H_
