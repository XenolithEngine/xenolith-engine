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

__SPRT_BEGIN_DECL

#define LHND  (LMEM_MOVEABLE | LMEM_ZEROINIT)
#define LPTR  (LMEM_FIXED | LMEM_ZEROINIT)

HLOCAL LocalAlloc(UINT uFlags, SIZE_T uBytes);
HLOCAL LocalFree(HLOCAL hMem);

/* ---- Error Handling (errhandlingapi.h) ---- */
/**
 * Retrieves the calling thread's last-error code value.
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

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_BASIC_API_H_
