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

// <ntstatus.h>: a minimal subset of NTSTATUS codes. llvm's ErrorHandling.cpp checks
// STATUS_DELETE_PENDING against RtlGetLastNtStatus().

#ifndef SPRT_WRAPPERS_WINDOWS_NTSTATUS_H_
#define SPRT_WRAPPERS_WINDOWS_NTSTATUS_H_

#include <sprt/wrappers/windows/abi/basic_types.h> // NTSTATUS/LONG
#include <sprt/wrappers/windows/abi/ntstatus.h>

// ntdll calling-convention markers (winternl.h). x64 has a single calling
// convention, so NTAPI is empty; NTSYSAPI is an import marker (left empty here).
#ifndef NTSYSAPI
#define NTSYSAPI
#endif
#ifndef NTAPI
#define NTAPI
#endif

/* Clean public names (materialized __SPRT_ values live in abi/ntstatus.h) */
// clang-format off
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS __SPRT_STATUS_SUCCESS
#endif
#ifndef STATUS_WX86_SINGLE_STEP
#define STATUS_WX86_SINGLE_STEP __SPRT_STATUS_WX86_SINGLE_STEP
#define STATUS_WX86_BREAKPOINT __SPRT_STATUS_WX86_BREAKPOINT
#endif
#ifndef STATUS_DELETE_PENDING
#define STATUS_DELETE_PENDING __SPRT_STATUS_DELETE_PENDING
#endif
#ifndef STATUS_OBJECT_NAME_NOT_FOUND
#define STATUS_OBJECT_NAME_NOT_FOUND __SPRT_STATUS_OBJECT_NAME_NOT_FOUND
#endif
#ifndef STATUS_ACCESS_DENIED
#define STATUS_ACCESS_DENIED __SPRT_STATUS_ACCESS_DENIED
#endif
// clang-format on

#endif // SPRT_WRAPPERS_WINDOWS_NTSTATUS_H_
