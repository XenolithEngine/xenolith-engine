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

// ABI-materialized __SPRT_* values for the device-IO control surface the ported libc++
// filesystem backend uses (reparse-point / symlink resolution). Values are validated
// against the Windows SDK in tests/libc/windows-abi/check-winioctl.cpp.

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_WINIOCTL_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_WINIOCTL_H_

// clang-format off

// CTL_CODE(FILE_DEVICE_FILE_SYSTEM=0x9, 42, METHOD_BUFFERED=0, FILE_ANY_ACCESS=0)
#define __SPRT_FSCTL_GET_REPARSE_POINT          0x000900A8

// 16 * 1024
#define __SPRT_MAXIMUM_REPARSE_DATA_BUFFER_SIZE 16384

// Relative-target flag in a symlink reparse buffer. Defined only in the driver header
// ntifs.h, NOT in the user-mode Windows SDK, so it has no SPRT_CONST parity check; the
// value is a stable part of the reparse-point ABI.
#define __SPRT_SYMLINK_FLAG_RELATIVE            0x00000001

// clang-format on

#endif // SPRT_WRAPPERS_WINDOWS_ABI_WINIOCTL_H_
