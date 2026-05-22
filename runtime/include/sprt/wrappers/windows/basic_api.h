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

HLOCAL LocalAlloc(UINT uFlags, SIZE_T uBytes);
HLOCAL LocalFree(HLOCAL hMem);

WINAPI LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);

WINAPI LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes);

WINAPI BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);

WINAPI SIZE_T HeapSize(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);

WINAPI HANDLE HeapCreate(DWORD flOptions, SIZE_T dwInitialSize, SIZE_T dwMaximumSize);

WINAPI BOOL HeapDestroy(HANDLE hHeap);

WINAPI BOOL HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem);

WINAPI SIZE_T HeapCompact(HANDLE hHeap, DWORD dwFlags);

WINAPI HANDLE GetProcessHeap(VOID);

WINAPI LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType,
		DWORD flProtect);

WINAPI BOOL VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect,
		PDWORD lpflOldProtect);

WINAPI BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);

WINAPI DWORD DiscardVirtualMemory(PVOID VirtualAddress, SIZE_T Size);

/* ---- Error Handling (errhandlingapi.h) ---- */
/**
 * Retrieves the calling threads last-error code value.
 * The last-error code is maintained per thread.
 * @return Last-error code, or zero if no error occurred.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror
 */
WINAPI DWORD GetLastError(void);

/**
 * Sets the last-error code for the calling thread.
 * @param dwErrCode The error code to set (0-4294967295).
 * @see https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-setlasterror
 */
WINAPI VOID SetLastError(DWORD dwErrCode);

/**
 * Closes an open object handle.
 * @param hObject Handle to the object to close.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle
 */
WINAPI BOOL CloseHandle(HANDLE hObject);

/**
 * Retrieves or sets the handle information for a thread object.
 * @param hObject Handle to the object.
 * @param lpdwFlags Pointer to the current flags.
 * @param dwFlags New flag values.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-gethandleinformation
 */
WINAPI BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags);

/**
 * Sets the handle information for a thread object.
 * @param hObject Handle to the object.
 * @param dwMask Mask specifying which flags to modify.
 * @param dwFlags New flag values.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-sethandleinformation
 */
WINAPI BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);

/**
 * Duplicates a handle to another process or creates a duplicate for the current process.
 * @param hSourceProcess Handle to the source process.
 * @param hSourceHandle Handle to duplicate.
 * @param hTargetProcess Handle to the target process.
 * @param lpDuplicateHandle Pointer to receive the duplicated handle.
 * @param dwDesiredAccess Desired access for the new handle.
 * @param bInheritHandle TRUE if inheritable.
 * @param dwOptions Duplication options.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-duplicatehandle
 */
WINAPI BOOL DuplicateHandle(HANDLE hSourceProcess, HANDLE hSourceHandle, HANDLE hTargetProcess,
		PHANDLE lpDuplicateHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);

/**
 * Locks a region of virtual memory in physical RAM, preventing the system from swapping it to disk.
 * @param lpAddress Base address of the memory region to lock.
 * @param dwSize Size of the memory region in bytes.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtuallock
 */
WINAPI BOOL VirtualLock(LPVOID lpAddress, SIZE_T dwSize);

/**
 * Unlocks a region of virtual memory that was previously locked with VirtualLock.
 * @param lpAddress Base address of the memory region to unlock.
 * @param dwSize Size of the memory region in bytes.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualunlock
 */
WINAPI BOOL VirtualUnlock(LPVOID lpAddress, SIZE_T dwSize);

WINAPI DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);

WINAPI BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);

WINAPI DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);

WINAPI BOOL SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue);

WINAPI VOID Sleep(DWORD dwMilliseconds);

WINAPI DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable);

WINAPI BOOL WaitOnAddress(volatile VOID *Address, PVOID CompareAddress, SIZE_T AddressSize,
		DWORD dwMilliseconds);

WINAPI VOID WakeByAddressSingle(PVOID Address);

WINAPI VOID WakeByAddressAll(PVOID Address);

WINAPI BOOL IsDebuggerPresent();

WINAPI HANDLE GetStdHandle(DWORD nStdHandle);

WINAPI BOOL GetConsoleScreenBufferInfo(HANDLE hConsoleOutput,
		PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo);

WINAPI BOOL SetConsoleCP(UINT wCodePageID);

WINAPI BOOL SetConsoleOutputCP(UINT wCodePageID);

WINAPI BOOL GetConsoleMode(HANDLE hConsoleHandle, LPDWORD lpMode);

WINAPI BOOL SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);

WINAPI DWORD GetCurrentProcessorNumber(VOID);

WINAPI VOID OutputDebugStringA(LPCSTR lpOutputString);

WINAPI VOID OutputDebugStringW(LPCWSTR lpOutputString);

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_BASIC_API_H_
