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

// Materialized ABI values for <ntstatus.h> (clean names re-exported by
// sprt/wrappers/windows/ntstatus.h).

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_NTSTATUS_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_NTSTATUS_H_

#include <sprt/wrappers/windows/abi/basic_types.h> // NTSTATUS/LONG

// clang-format off
#define __SPRT_STATUS_SUCCESS               ((NTSTATUS) 0x00000000L)
#define __SPRT_STATUS_WX86_SINGLE_STEP      ((NTSTATUS) 0x4000001EL)
#define __SPRT_STATUS_WX86_BREAKPOINT       ((NTSTATUS) 0x4000001FL)
#define __SPRT_STATUS_DELETE_PENDING        ((NTSTATUS) 0xC0000056L)
#define __SPRT_STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS) 0xC0000034L)
#define __SPRT_STATUS_ACCESS_DENIED         ((NTSTATUS) 0xC0000022L)
// clang-format on

#endif // SPRT_WRAPPERS_WINDOWS_ABI_NTSTATUS_H_
