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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_PROCESS_API_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_PROCESS_API_H_


#include <sprt/wrappers/windows/abi/structures.h>
#include <sprt/wrappers/windows/abi/constants.h>

// clang-format off
#define __SPRT_PROCESS_TERMINATE                  (0x0001)
#define __SPRT_PROCESS_CREATE_THREAD              (0x0002)
#define __SPRT_PROCESS_SET_SESSIONID              (0x0004)
#define __SPRT_PROCESS_VM_OPERATION               (0x0008)
#define __SPRT_PROCESS_VM_READ                    (0x0010)
#define __SPRT_PROCESS_VM_WRITE                   (0x0020)
#define __SPRT_PROCESS_DUP_HANDLE                 (0x0040)
#define __SPRT_PROCESS_CREATE_PROCESS             (0x0080)
#define __SPRT_PROCESS_SET_QUOTA                  (0x0100)
#define __SPRT_PROCESS_SET_INFORMATION            (0x0200)
#define __SPRT_PROCESS_QUERY_INFORMATION          (0x0400)
#define __SPRT_PROCESS_SUSPEND_RESUME             (0x0800)
#define __SPRT_PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)
#define __SPRT_PROCESS_SET_LIMITED_INFORMATION    (0x2000)

#define __SPRT_NORMAL_PRIORITY_CLASS             0x00000020
#define __SPRT_IDLE_PRIORITY_CLASS               0x00000040
#define __SPRT_HIGH_PRIORITY_CLASS               0x00000080
#define __SPRT_REALTIME_PRIORITY_CLASS           0x00000100
#define __SPRT_BELOW_NORMAL_PRIORITY_CLASS       0x00004000
#define __SPRT_ABOVE_NORMAL_PRIORITY_CLASS       0x00008000

#define __SPRT_EXTENDED_STARTUPINFO_PRESENT      0x00080000

#define __SPRT_PROC_THREAD_ATTRIBUTE_NUMBER    0x0000FFFF
#define __SPRT_PROC_THREAD_ATTRIBUTE_THREAD    0x00010000  // Attribute may be used with thread creation
#define __SPRT_PROC_THREAD_ATTRIBUTE_INPUT     0x00020000  // Attribute is input only
#define __SPRT_PROC_THREAD_ATTRIBUTE_ADDITIVE  0x00040000  // Attribute may be "accumulated," e.g. bitmasks, counters, etc.

#define __SPRT_STARTF_USESHOWWINDOW       0x00000001
#define __SPRT_STARTF_USESIZE             0x00000002
#define __SPRT_STARTF_USEPOSITION         0x00000004
#define __SPRT_STARTF_USECOUNTCHARS       0x00000008
#define __SPRT_STARTF_USEFILLATTRIBUTE    0x00000010
#define __SPRT_STARTF_RUNFULLSCREEN       0x00000020
#define __SPRT_STARTF_FORCEONFEEDBACK     0x00000040
#define __SPRT_STARTF_FORCEOFFFEEDBACK    0x00000080
#define __SPRT_STARTF_USESTDHANDLES       0x00000100

#define __SPRT_DEBUG_PROCESS                     0x00000001
#define __SPRT_DEBUG_ONLY_THIS_PROCESS           0x00000002
#define __SPRT_CREATE_SUSPENDED                  0x00000004
#define __SPRT_DETACHED_PROCESS                  0x00000008
#define __SPRT_CREATE_NEW_CONSOLE                0x00000010
#define __SPRT_NORMAL_PRIORITY_CLASS             0x00000020
#define __SPRT_IDLE_PRIORITY_CLASS               0x00000040
#define __SPRT_HIGH_PRIORITY_CLASS               0x00000080
#define __SPRT_REALTIME_PRIORITY_CLASS           0x00000100
#define __SPRT_CREATE_NEW_PROCESS_GROUP          0x00000200
#define __SPRT_CREATE_UNICODE_ENVIRONMENT        0x00000400
#define __SPRT_CREATE_SEPARATE_WOW_VDM           0x00000800
#define __SPRT_CREATE_SHARED_WOW_VDM             0x00001000
#define __SPRT_CREATE_FORCEDOS                   0x00002000
#define __SPRT_BELOW_NORMAL_PRIORITY_CLASS       0x00004000
#define __SPRT_ABOVE_NORMAL_PRIORITY_CLASS       0x00008000
#define __SPRT_INHERIT_PARENT_AFFINITY           0x00010000
#define __SPRT_INHERIT_CALLER_PRIORITY           0x00020000    // Deprecated
#define __SPRT_CREATE_PROTECTED_PROCESS          0x00040000
#define __SPRT_EXTENDED_STARTUPINFO_PRESENT      0x00080000
#define __SPRT_PROCESS_MODE_BACKGROUND_BEGIN     0x00100000
#define __SPRT_PROCESS_MODE_BACKGROUND_END       0x00200000
#define __SPRT_CREATE_SECURE_PROCESS             0x00400000
#define __SPRT_CREATE_BREAKAWAY_FROM_JOB         0x01000000
#define __SPRT_CREATE_PRESERVE_CODE_AUTHZ_LEVEL  0x02000000
#define __SPRT_CREATE_DEFAULT_ERROR_MODE         0x04000000
#define __SPRT_CREATE_NO_WINDOW                  0x08000000
#define __SPRT_PROFILE_USER                      0x10000000
#define __SPRT_PROFILE_KERNEL                    0x20000000
#define __SPRT_PROFILE_SERVER                    0x40000000
#define __SPRT_CREATE_IGNORE_SYSTEM_DEFAULT      0x80000000

#define __SPRT_SEE_MASK_NOCLOSEPROCESS    0x00000040   // SHELLEXECUTEINFO.hProcess

#define __SPRT_ProcThreadAttributeValue(Number, Thread, Input, Additive) \
    (((Number) & __SPRT_PROC_THREAD_ATTRIBUTE_NUMBER) | \
     ((Thread != __SPRT_FALSE) ? __SPRT_PROC_THREAD_ATTRIBUTE_THREAD : 0) | \
     ((Input != __SPRT_FALSE) ? __SPRT_PROC_THREAD_ATTRIBUTE_INPUT : 0) | \
     ((Additive != __SPRT_FALSE) ? __SPRT_PROC_THREAD_ATTRIBUTE_ADDITIVE : 0))

#define __SPRT_PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES \
    __SPRT_ProcThreadAttributeValue (ProcThreadAttributeSecurityCapabilities, __SPRT_FALSE, __SPRT_TRUE, __SPRT_FALSE)

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

typedef struct _SHELLEXECUTEINFOW {
	DWORD cbSize; // in, required, sizeof of this structure
	ULONG fMask; // in, SEE_MASK_XXX values
	HANDLE hwnd; // in, optional
	LPCWSTR lpVerb; // in, optional when unspecified the default verb is choosen
	LPCWSTR lpFile; // in, either this value or lpIDList must be specified
	LPCWSTR lpParameters; // in, optional
	LPCWSTR lpDirectory; // in, optional
	int nShow; // in, required
	HINSTANCE hInstApp; // out when SEE_MASK_NOCLOSEPROCESS is specified
	void *lpIDList; // in, valid when SEE_MASK_IDLIST is specified, PCIDLIST_ABSOLUTE, for use with SEE_MASK_IDLIST & SEE_MASK_INVOKEIDLIST
	LPCWSTR lpClass; // in, valid when SEE_MASK_CLASSNAME is specified
	HKEY hkeyClass; // in, valid when SEE_MASK_CLASSKEY is specified
	DWORD dwHotKey; // in, valid when SEE_MASK_HOTKEY is specified
	union {
		HANDLE hIcon; // not used
		HANDLE hMonitor; // in, valid when SEE_MASK_HMONITOR specified
	};
	HANDLE hProcess; // out, valid when SEE_MASK_NOCLOSEPROCESS specified
} SHELLEXECUTEINFOW, *LPSHELLEXECUTEINFOW;

typedef struct _PROCESS_MEMORY_COUNTERS {
	DWORD cb;
	DWORD PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;
typedef PROCESS_MEMORY_COUNTERS *PPROCESS_MEMORY_COUNTERS;


#endif // SPRT_WRAPPERS_WINDOWS_ABI_PROCESS_API_H_
