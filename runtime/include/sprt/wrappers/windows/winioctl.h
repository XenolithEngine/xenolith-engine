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

// Minimal sprt <winioctl.h>: the device-IO control surface the ported libc++ actually
// uses. Only libc++'s <filesystem> backend reaches for this header (src/libcxx/filesystem/
// posix_compat.h), and only for reparse-point (symlink / mount-point) resolution: the
// FSCTL_GET_REPARSE_POINT control code, the reparse-buffer size cap, the relative-symlink
// flag, and DeviceIoControl itself. The reparse *tags* (IO_REPARSE_TAG_SYMLINK, ...) and
// FILE_FLAG_OPEN_REPARSE_POINT already live in <file_api.h>, reached through <windows.h>.

#ifndef SPRT_WRAPPERS_WINDOWS_WINIOCTL_H_
#define SPRT_WRAPPERS_WINDOWS_WINIOCTL_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/wrappers/windows/abi/winioctl.h>

/* Clean public names (materialized __SPRT_ values live in abi/winioctl.h) */
#define FSCTL_GET_REPARSE_POINT          __SPRT_FSCTL_GET_REPARSE_POINT
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE __SPRT_MAXIMUM_REPARSE_DATA_BUFFER_SIZE
#define SYMLINK_FLAG_RELATIVE            __SPRT_SYMLINK_FLAG_RELATIVE

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI BOOL DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode,
		LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize,
		LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_WINIOCTL_H_
