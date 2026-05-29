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

#ifndef SPRT_WRAPPERS_WINDOWS_BASIC_API_H_
#define SPRT_WRAPPERS_WINDOWS_BASIC_API_H_

#include <sprt/wrappers/windows/structures.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/c/__sprt_string.h>

typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;

// clang-format off
#define CP_ACP                    0
#define CP_OEMCP                  1
#define CP_MACCP                  2
#define CP_THREAD_ACP             3
#define CP_SYMBOL                 42
#define CP_UTF8                   65001

#define STD_INPUT_HANDLE    ((DWORD)-10)
#define STD_OUTPUT_HANDLE   ((DWORD)-11)
#define STD_ERROR_HANDLE    ((DWORD)-12)

#define ENABLE_PROCESSED_INPUT              0x0001
#define ENABLE_LINE_INPUT                   0x0002
#define ENABLE_ECHO_INPUT                   0x0004
#define ENABLE_WINDOW_INPUT                 0x0008
#define ENABLE_MOUSE_INPUT                  0x0010
#define ENABLE_INSERT_MODE                  0x0020
#define ENABLE_QUICK_EDIT_MODE              0x0040
#define ENABLE_EXTENDED_FLAGS               0x0080
#define ENABLE_AUTO_POSITION                0x0100
#define ENABLE_VIRTUAL_TERMINAL_INPUT       0x0200

#define ENABLE_PROCESSED_OUTPUT             0x0001
#define ENABLE_WRAP_AT_EOL_OUTPUT           0x0002
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING  0x0004
#define DISABLE_NEWLINE_AUTO_RETURN         0x0008
#define ENABLE_LVB_GRID_WORLDWIDE           0x0010

#define HEAP_NO_SERIALIZE               0x00000001
#define HEAP_GROWABLE                   0x00000002
#define HEAP_GENERATE_EXCEPTIONS        0x00000004
#define HEAP_ZERO_MEMORY                0x00000008
#define HEAP_REALLOC_IN_PLACE_ONLY      0x00000010
#define HEAP_TAIL_CHECKING_ENABLED      0x00000020
#define HEAP_FREE_CHECKING_ENABLED      0x00000040
#define HEAP_DISABLE_COALESCE_ON_FREE   0x00000080
#define HEAP_CREATE_ALIGN_16            0x00010000
#define HEAP_CREATE_ENABLE_TRACING      0x00020000
#define HEAP_CREATE_ENABLE_EXECUTE      0x00040000
#define HEAP_MAXIMUM_TAG                0x0FFF
#define HEAP_PSEUDO_TAG_FLAG            0x8000
#define HEAP_TAG_SHIFT                  18
#define HEAP_CREATE_SEGMENT_HEAP        0x00000100
#define HEAP_CREATE_HARDENED            0x00000200

#define MEM_COMMIT                      0x00001000
#define MEM_RESERVE                     0x00002000
#define MEM_REPLACE_PLACEHOLDER         0x00004000
#define MEM_RESERVE_PLACEHOLDER         0x00040000
#define MEM_RESET                       0x00080000
#define MEM_TOP_DOWN                    0x00100000
#define MEM_WRITE_WATCH                 0x00200000
#define MEM_PHYSICAL                    0x00400000
#define MEM_ROTATE                      0x00800000
#define MEM_DIFFERENT_IMAGE_BASE_OK     0x00800000
#define MEM_RESET_UNDO                  0x01000000
#define MEM_LARGE_PAGES                 0x20000000
#define MEM_4MB_PAGES                   0x80000000
#define MEM_64K_PAGES                   (MEM_LARGE_PAGES | MEM_PHYSICAL)  
#define MEM_UNMAP_WITH_TRANSIENT_BOOST  0x00000001
#define MEM_COALESCE_PLACEHOLDERS       0x00000001
#define MEM_PRESERVE_PLACEHOLDER        0x00000002
#define MEM_DECOMMIT                    0x00004000
#define MEM_RELEASE                     0x00008000
#define MEM_FREE                        0x00010000

#define PAGE_NOACCESS           0x01
#define PAGE_READONLY           0x02
#define PAGE_READWRITE          0x04
#define PAGE_WRITECOPY          0x08
#define PAGE_EXECUTE            0x10
#define PAGE_EXECUTE_READ       0x20
#define PAGE_EXECUTE_READWRITE  0x40
#define PAGE_EXECUTE_WRITECOPY  0x80
#define PAGE_GUARD             0x100
#define PAGE_NOCACHE           0x200
#define PAGE_WRITECOMBINE      0x400

typedef enum _SYSTEM_CPU_SET_INFORMATION_CLASS {
	SystemCpuSetInformationBasic = 0,
	SystemCpuSetInformationGroup = 1
} SYSTEM_CPU_SET_INFORMATION_CLASS, *PSYSTEM_CPU_SET_INFORMATION_CLASS;

/* Token information classes */
typedef enum _TOKEN_INFORMATION_CLASS {
	TokenUser = 1,
	TokenGroups,
	TokenPrivileges,
	TokenOwner,
	TokenPrimaryGroup,
	TokenDefaultDacl,
	TokenSource,
	TokenType,
	TokenImpersonationLevel,
	TokenStatistics,
	TokenRestrictedSids,
	TokenSessionId,
	TokenGroupsAndPrivileges,
	TokenSessionReference,
	TokenSandBoxInert,
	TokenAuditPolicy,
	TokenOrigin,
	TokenElevationType,
	TokenLinkedToken,
	TokenElevation,
	TokenHasRestrictions,
	TokenAccessInformation,
	TokenVirtualizationAllowed,
	TokenVirtualizationEnabled,
	TokenIntegrityLevel,
	TokenUIAccess,
	TokenMandatoryPolicy,
	TokenLogonSid,
	TokenIsAppContainer,
	TokenCapabilities,
	TokenAppContainerSid,
	TokenAppContainerNumber,
	TokenUserClaimAttributes,
	TokenDeviceClaimAttributes,
	TokenRestrictedUserClaimAttributes,
	TokenRestrictedDeviceClaimAttributes,
	TokenDeviceGroups,
	TokenRestrictedDeviceGroups,
	TokenSecurityAttributes,
	TokenIsRestricted,
	TokenProcessTrustLevel,
	TokenPrivateNameSpace,
	TokenSingletonAttributes,
	TokenBnoIsolation,
	TokenChildProcessFlags,
	TokenIsLessPrivilegedAppContainer,
	TokenIsSandboxed,
	TokenIsAppSilo,
	TokenLoggingInformation,
	TokenLearningMode,
	MaxTokenInfoClass // MaxTokenInfoClass should always be the last enum
} TOKEN_INFORMATION_CLASS, *PTOKEN_INFORMATION_CLASS;

typedef enum _WINAPI_PROVIDER {
	WinApiProviderMicrosoft,
	WinApiProviderWine,
	WinApiProviderReactOS,
} WINAPI_PROVIDER;

// clang-format on

#define LHND  (LMEM_MOVEABLE | LMEM_ZEROINIT)
#define LPTR  (LMEM_FIXED | LMEM_ZEROINIT)

typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
	COORD dwSize;
	COORD dwCursorPosition;
	WORD wAttributes;
	SMALL_RECT srWindow;
	COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO, *PCONSOLE_SCREEN_BUFFER_INFO;

__SPRT_BEGIN_DECL

__SPRT_WIN_IMPORT WINAPI HLOCAL LocalAlloc(UINT uFlags, SIZE_T uBytes);

__SPRT_WIN_IMPORT WINAPI HLOCAL LocalFree(HLOCAL hMem);

__SPRT_WIN_IMPORT WINAPI LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);

__SPRT_WIN_IMPORT WINAPI LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem,
		SIZE_T dwBytes);

__SPRT_WIN_IMPORT WINAPI BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);

__SPRT_WIN_IMPORT WINAPI SIZE_T HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);

__SPRT_WIN_IMPORT WINAPI HANDLE HeapCreate(DWORD flOptions, SIZE_T dwInitialSize,
		SIZE_T dwMaximumSize);

__SPRT_WIN_IMPORT WINAPI BOOL HeapDestroy(HANDLE hHeap);

__SPRT_WIN_IMPORT WINAPI BOOL HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);

__SPRT_WIN_IMPORT WINAPI SIZE_T HeapCompact(HANDLE hHeap, DWORD dwFlags);

__SPRT_WIN_IMPORT WINAPI HANDLE GetProcessHeap(VOID);

__SPRT_WIN_IMPORT WINAPI LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize,
		DWORD flAllocationType, DWORD flProtect);

__SPRT_WIN_IMPORT WINAPI BOOL VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect,
		PDWORD lpflOldProtect);

__SPRT_WIN_IMPORT WINAPI BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);

__SPRT_WIN_IMPORT WINAPI DWORD DiscardVirtualMemory(PVOID VirtualAddress, SIZE_T Size);

__SPRT_WIN_IMPORT WINAPI DWORD GetLastError(void);

__SPRT_WIN_IMPORT WINAPI VOID SetLastError(DWORD dwErrCode);

__SPRT_WIN_IMPORT WINAPI BOOL CloseHandle(HANDLE hObject);

__SPRT_WIN_IMPORT WINAPI BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags);

__SPRT_WIN_IMPORT WINAPI BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);

__SPRT_WIN_IMPORT WINAPI BOOL DuplicateHandle(HANDLE hSourceProcess, HANDLE hSourceHandle,
		HANDLE hTargetProcess, PHANDLE lpDuplicateHandle, DWORD dwDesiredAccess,
		BOOL bInheritHandle, DWORD dwOptions);

__SPRT_WIN_IMPORT WINAPI BOOL VirtualLock(LPVOID lpAddress, SIZE_T dwSize);

__SPRT_WIN_IMPORT WINAPI BOOL VirtualUnlock(LPVOID lpAddress, SIZE_T dwSize);

__SPRT_WIN_IMPORT WINAPI DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);

__SPRT_WIN_IMPORT WINAPI BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);

__SPRT_WIN_IMPORT WINAPI DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer,
		DWORD nSize);

__SPRT_WIN_IMPORT WINAPI BOOL SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue);

__SPRT_WIN_IMPORT WINAPI LPWCH GetEnvironmentStringsW();

__SPRT_WIN_IMPORT WINAPI VOID Sleep(DWORD dwMilliseconds);

__SPRT_WIN_IMPORT WINAPI DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable);

__SPRT_WIN_IMPORT WINAPI BOOL WaitOnAddress(volatile VOID *Address, PVOID CompareAddress,
		SIZE_T AddressSize, DWORD dwMilliseconds);

__SPRT_WIN_IMPORT WINAPI VOID WakeByAddressSingle(PVOID Address);

__SPRT_WIN_IMPORT WINAPI VOID WakeByAddressAll(PVOID Address);

__SPRT_WIN_IMPORT WINAPI BOOL IsDebuggerPresent();

__SPRT_WIN_IMPORT WINAPI HANDLE GetStdHandle(DWORD nStdHandle);

__SPRT_WIN_IMPORT WINAPI BOOL GetConsoleScreenBufferInfo(HANDLE hConsoleOutput,
		PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo);

__SPRT_WIN_IMPORT WINAPI BOOL SetConsoleCP(UINT wCodePageID);

__SPRT_WIN_IMPORT WINAPI BOOL SetConsoleOutputCP(UINT wCodePageID);

__SPRT_WIN_IMPORT WINAPI BOOL GetConsoleMode(HANDLE hConsoleHandle, LPDWORD lpMode);

__SPRT_WIN_IMPORT WINAPI BOOL SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);

__SPRT_WIN_IMPORT WINAPI DWORD GetCurrentProcessorNumber(VOID);

__SPRT_WIN_IMPORT WINAPI VOID OutputDebugStringA(LPCSTR lpOutputString);

__SPRT_WIN_IMPORT WINAPI VOID OutputDebugStringW(LPCWSTR lpOutputString);

WINAPI WINAPI_PROVIDER GetWinApiProvider();

SPRT_FORCEINLINE void CopyMemory(LPVOID Destination, const VOID *Source, SIZE_T Length) {
	__sprt_memcpy(Destination, Source, Length);
}

SPRT_FORCEINLINE void FillMemory(LPVOID Destination, SIZE_T Length, BYTE Value) {
	__sprt_memset(Destination, Value, Length);
}

SPRT_FORCEINLINE void ZeroMemory(LPVOID Destination, SIZE_T Length) {
	__sprt_memset(Destination, 0, Length);
}

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_BASIC_API_H_
