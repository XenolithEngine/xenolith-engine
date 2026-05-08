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

// clang-format off
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

#define NORMAL_PRIORITY_CLASS             0x00000020
#define IDLE_PRIORITY_CLASS               0x00000040
#define HIGH_PRIORITY_CLASS               0x00000080
#define REALTIME_PRIORITY_CLASS           0x00000100
#define BELOW_NORMAL_PRIORITY_CLASS       0x00004000
#define ABOVE_NORMAL_PRIORITY_CLASS       0x00008000

#define EXTENDED_STARTUPINFO_PRESENT      0x00080000

#define PROC_THREAD_ATTRIBUTE_NUMBER    0x0000FFFF
#define PROC_THREAD_ATTRIBUTE_THREAD    0x00010000  // Attribute may be used with thread creation
#define PROC_THREAD_ATTRIBUTE_INPUT     0x00020000  // Attribute is input only
#define PROC_THREAD_ATTRIBUTE_ADDITIVE  0x00040000  // Attribute may be "accumulated," e.g. bitmasks, counters, etc.

#define ProcThreadAttributeValue(Number, Thread, Input, Additive) \
    (((Number) & PROC_THREAD_ATTRIBUTE_NUMBER) | \
     ((Thread != FALSE) ? PROC_THREAD_ATTRIBUTE_THREAD : 0) | \
     ((Input != FALSE) ? PROC_THREAD_ATTRIBUTE_INPUT : 0) | \
     ((Additive != FALSE) ? PROC_THREAD_ATTRIBUTE_ADDITIVE : 0))

#define PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES \
    ProcThreadAttributeValue (ProcThreadAttributeSecurityCapabilities, FALSE, TRUE, FALSE)

// clang-format on

typedef enum _PROC_THREAD_ATTRIBUTE_NUM {
	ProcThreadAttributeParentProcess = 0,
	ProcThreadAttributeHandleList = 2,
	ProcThreadAttributeGroupAffinity = 3,
	ProcThreadAttributePreferredNode = 4,
	ProcThreadAttributeIdealProcessor = 5,
	ProcThreadAttributeUmsThread = 6,
	ProcThreadAttributeMitigationPolicy = 7,
	ProcThreadAttributeSecurityCapabilities = 9,
	ProcThreadAttributeProtectionLevel = 11,
	ProcThreadAttributeJobList = 13,
	ProcThreadAttributeChildProcessPolicy = 14,
	ProcThreadAttributeAllApplicationPackagesPolicy = 15,
	ProcThreadAttributeWin32kFilter = 16,
	ProcThreadAttributeSafeOpenPromptOriginClaim = 17,
	ProcThreadAttributeDesktopAppPolicy = 18,
	ProcThreadAttributePseudoConsole = 22,
	ProcThreadAttributeMitigationAuditPolicy = 24,
	ProcThreadAttributeMachineType = 25,
	ProcThreadAttributeComponentFilter = 26,
	ProcThreadAttributeEnableOptionalXStateFeatures = 27,
	ProcThreadAttributeTrustedApp = 29,
	ProcThreadAttributeSveVectorLength = 30,
} PROC_THREAD_ATTRIBUTE_NUM;

typedef struct _PROCESS_INFORMATION {
	HANDLE hProcess;
	HANDLE hThread;
	DWORD dwProcessId;
	DWORD dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef struct _STARTUPINFOW {
	DWORD cb;
	LPWSTR lpReserved;
	LPWSTR lpDesktop;
	LPWSTR lpTitle;
	DWORD dwX;
	DWORD dwY;
	DWORD dwXSize;
	DWORD dwYSize;
	DWORD dwXCountChars;
	DWORD dwYCountChars;
	DWORD dwFillAttribute;
	DWORD dwFlags;
	WORD wShowWindow;
	WORD cbReserved2;
	LPBYTE lpReserved2;
	HANDLE hStdInput;
	HANDLE hStdOutput;
	HANDLE hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;

typedef struct _STARTUPINFOEXW {
	STARTUPINFOW StartupInfo;
	LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;
} STARTUPINFOEXW, *LPSTARTUPINFOEXW;

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

/**
 * Retrieves the termination status of the specified process.
 * @param hProcess Handle to the process.
 * @param lpExitCode Pointer to a variable that receives the termination status code.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess
 */
WINAPI BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode);

WINAPI DWORD GetPriorityClass(HANDLE hProcess);

WINAPI VOID YieldProcessor();

WINAPI BOOL InitializeProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
		DWORD dwAttributeCount, DWORD dwFlags, PSIZE_T lpSize);

WINAPI BOOL UpdateProcThreadAttribute(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList, DWORD dwFlags,
		DWORD_PTR Attribute, PVOID lpValue, SIZE_T cbSize, PVOID lpPreviousValue,
		PSIZE_T lpReturnSize);

WINAPI VOID DeleteProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList);

WINAPI LPWSTR GetCommandLineW(VOID);

WINAPI BOOL CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
		LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
		BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
		LPPROCESS_INFORMATION lpProcessInformation);

__SPRT_END_DECL

#endif
