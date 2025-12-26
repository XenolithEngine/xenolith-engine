/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#ifndef CORE_CORE_SPPLATFORMUNISTD_H_
#define CORE_CORE_SPPLATFORMUNISTD_H_

// Platform-dependent implementation for some unistd.h POSIX utils

#include "stappler-buildconfig.h"
#include "detail/SPPlatformInit.h"

#if WIN32

// Suppress windows warnings for cross-compilation with clang
#if XWIN
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wmicrosoft-include"
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma clang diagnostic ignored "-Wcomment"
#pragma clang diagnostic ignored "-Wunused-value"
#endif

#ifndef _MSC_VER
#warning Not MSC compiler
#endif

#include <direct.h>
#include <errno.h>
#include <intrin.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utime.h>

#include <esent.h>
#include <strsafe.h>

#include <windows.h>
#include <windowsx.h>
#include <winioctl.h>
#include <wincrypt.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellscalingapi.h>
#include <sddl.h>
#include <userenv.h>
#include <winnt.h>

#include <knownfolders.h>
#include <physicalmonitorenumerationapi.h>
#include <processthreadsapi.h>

#include <setupapi.h>
#include <Ntddvdeo.h>
#include <dwmapi.h>

#define WIL_SUPPRESS_EXCEPTIONS 1

#include <winhttp.h>
#include <netlistmgr.h>
#include <wil/result_macros.h>
#include <wil/stl.h>
#include <wil/resource.h>
#include <wil/com.h>
#include <wrl.h>

// suppress common macro leak
#ifdef interface
#undef interface
#endif

#ifdef DELETE
#undef DELETE
#endif

#ifdef uuid_t
#undef uuid_t
#endif

using pid_t = DWORD;

#define PATH_MAX MAX_PATH

#define SP_POSIX_MAPPED_FILES 0

#define R_OK 4 /* Test for read permission.  */
#define W_OK 2 /* Test for write permission.  */
#define F_OK 0 /* Test for existence.  */

#ifndef NTFS_MAX_PATH
#define NTFS_MAX_PATH 32'768
#endif /* NTFS_MAX_PATH */

#ifndef NAME_MAX
#define NAME_MAX 260
#endif /* NAME_MAX */

#else

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>

#define SP_POSIX_MAPPED_FILES _POSIX_MAPPED_FILES

#endif

#if XWIN
#pragma clang diagnostic pop
#endif

#include "detail/SPPlatformCleanup.h"

#endif /* CORE_CORE_SPPLATFORMUNISTD_H_ */
