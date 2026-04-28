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

#ifndef SPRT_WRAPPERS_WINDOWS_PROCESS_API_H_
#define SPRT_WRAPPERS_WINDOWS_PROCESS_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>

#define PROCESS_TERMINATE                  (0x0001)
#define PROCESS_CREATE_THREAD              (0x0002)
#define PROCESS_SET_SESSIONID              (0x0004)
#define PROCESS_VM_OPERATION               (0x0008)
#define PROCESS_VM_READ                    (0x0010)
#define PROCESS_VM_WRITE                   (0x0020)
#define PROCESS_DUP_HANDLE                 (0x0040)
#define PROCESS_CREATE_PROCESS             (0x0080)
#define PROCESS_SET_QUOTA                  (0x0100)
#define PROCESS_SET_INFORMATION            (0x0200)
#define PROCESS_QUERY_INFORMATION          (0x0400)
#define PROCESS_SUSPEND_RESUME             (0x0800)
#define PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)
#define PROCESS_SET_LIMITED_INFORMATION    (0x2000)

#define NORMAL_PRIORITY_CLASS             0x0000'0020
#define IDLE_PRIORITY_CLASS               0x0000'0040
#define HIGH_PRIORITY_CLASS               0x0000'0080
#define REALTIME_PRIORITY_CLASS           0x0000'0100
#define BELOW_NORMAL_PRIORITY_CLASS       0x0000'4000
#define ABOVE_NORMAL_PRIORITY_CLASS       0x0000'8000

__SPRT_BEGIN_DECL

/**
 * Retrieves a handle to the current process object.
 * @return Pseudo-handle for the calling process (PROCESS_ALL_ACCESS access).
 * @see https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocess
 */
WINAPI HANDLE GetCurrentProcess(void);

/**
 * Retrieves the identifier of the current process.
 * @return Process ID of the calling process.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocessid
 */
WINAPI DWORD GetCurrentProcessId(void);

WINAPI HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);

WINAPI DWORD GetPriorityClass(HANDLE hProcess);

WINAPI VOID YieldProcessor();

__SPRT_END_DECL

#endif
