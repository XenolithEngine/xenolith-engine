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

#ifndef SPRT_WRAPPERS_WINDOWS_DBGHELP_H_
#define SPRT_WRAPPERS_WINDOWS_DBGHELP_H_

#include <sprt/wrappers/windows/abi/dbghelp.h>

// ---- SymSetOptions flags --------------------------------------------------
#define SYMOPT_DEFERRED_LOADS __SPRT_SYMOPT_DEFERRED_LOADS
#define SYMOPT_LOAD_LINES __SPRT_SYMOPT_LOAD_LINES

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI BOOL MiniDumpWriteDump(HANDLE hProcess, DWORD ProcessId, HANDLE hFile,
		MINIDUMP_TYPE DumpType, PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
		PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
		PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_DBGHELP_H_
