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

// Materialized ABI values for <crtdbg.h> (clean names re-exported by
// sprt/wrappers/windows/crtdbg.h).

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_CRTDBG_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_CRTDBG_H_

// clang-format off
#define __SPRT__CRT_WARN   0
#define __SPRT__CRT_ERROR  1
#define __SPRT__CRT_ASSERT 2
#define __SPRT__CRT_ERRCNT 3

#define __SPRT__CRTDBG_MODE_FILE    0x1
#define __SPRT__CRTDBG_MODE_DEBUG   0x2
#define __SPRT__CRTDBG_MODE_WNDW    0x4
#define __SPRT__CRTDBG_REPORT_MODE  (-1)
#define __SPRT__CRTDBG_HFILE_ERROR  ((void *) -2)
#define __SPRT__CRTDBG_FILE_STDOUT  ((void *) -4)
#define __SPRT__CRTDBG_FILE_STDERR  ((void *) -5)

#define __SPRT__CRTDBG_ALLOC_MEM_DF       0x01
#define __SPRT__CRTDBG_DELAY_FREE_MEM_DF  0x02
#define __SPRT__CRTDBG_CHECK_ALWAYS_DF    0x04
#define __SPRT__CRTDBG_RESERVED_DF        0x08
#define __SPRT__CRTDBG_CHECK_CRT_DF       0x10
#define __SPRT__CRTDBG_LEAK_CHECK_DF      0x20
#define __SPRT__CRTDBG_REPORT_FLAG        (-1)
// clang-format on

typedef int(__cdecl *_CRT_REPORT_HOOK)(int, char *, int *);

#endif // SPRT_WRAPPERS_WINDOWS_ABI_CRTDBG_H_
