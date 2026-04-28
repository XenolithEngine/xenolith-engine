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

#ifndef SPRT_WRAPPERS_WINDOWS_WINDOWS_H_
#define SPRT_WRAPPERS_WINDOWS_WINDOWS_H_

#include <sprt/wrappers/windows/complex_types.h>
#include <sprt/wrappers/windows/constants.h>
#include <sprt/wrappers/windows/basic_api.h>
#include <sprt/wrappers/windows/file_api.h>
#include <sprt/wrappers/windows/thread_api.h>

/* ============================================================ */
/* WinAPI Function Declarations                                 */
/* Based on Microsoft documentation: https://docs.microsoft.com/ */
/* ============================================================ */

typedef struct _SYSTEM_INFO {
	union {
		DWORD dwOemId; // Obsolete field...do not use
		struct {
			WORD wProcessorArchitecture;
			WORD wReserved;
		};
	};
	DWORD dwPageSize;
	LPVOID lpMinimumApplicationAddress;
	LPVOID lpMaximumApplicationAddress;
	DWORD_PTR dwActiveProcessorMask;
	DWORD dwNumberOfProcessors;
	DWORD dwProcessorType;
	DWORD dwAllocationGranularity;
	WORD wProcessorLevel;
	WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

__SPRT_BEGIN_DECL

/**
 * Waits for the specified object to be signaled or for the time-out interval to elapse.
 * @param hHandle Handle to the object.
 * @param dwMilliseconds Time-out interval in milliseconds (INFINITE = wait forever).
 * @return One of WAIT_OBJECT_0, WAIT_ABANDONED, WAIT_TIMEOUT, or WAIT_FAILED.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitsingleobject
 */
WINAPI DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);

/**
 * Duplicates an object handle.
 * @param hSourceProcessHandle Handle to the source process.
 * @param hSourceHandle Handle to duplicate.
 * @param hTargetProcessHandle Handle to target process.
 * @param lpTargetHandle Pointer to receive duplicated handle.
 * @param dwDesiredAccess Desired access for new handle.
 * @param bInheritHandle TRUE if new handle is inheritable.
 * @param dwOptions Duplication options.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle
 */
WINAPI BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
		HANDLE hTargetProcessHandle, PHANDLE lpTargetHandle, DWORD dwDesiredAccess,
		BOOL bInheritHandle, DWORD dwOptions);

/**
 * Opens the access token associated with a process.
 * @param ProcessHandle Handle to a process object.
 * @param DesiredAccess Access mode for the token.
 * @param TokenHandle Pointer to receive opened token handle.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-openprocesstoken
 */
WINAPI BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle);

/**
 * Retrieves specified information about a security token.
 * @param TokenHandle Handle to a token object.
 * @param TokenInformationClass Type of information to retrieve.
 * @param TokenInformation Buffer for returned information.
 * @param TokenInformationLength Size of the buffer in bytes.
 * @param ReturnLength Pointer to receive actual bytes returned.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-gettokeninformation
 */
WINAPI BOOL GetTokenInformation(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass,
		LPVOID TokenInformation, DWORD TokenInformationLength, PDWORD ReturnLength);

/**
 * Retrieves a handle to the effective token for the current thread.
 * @return Handle to the effective access token, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-getcurrentthreadeffectivetoken
 */
WINAPI HANDLE GetCurrentThreadEffectiveToken(void);

/* ---- Synchronization (synchapi.h) ---- */
/**
 * Waits for a specified amount of time while another thread changes the value at a given address.
 * @param Address Pointer to the memory location to watch.
 * @param CompareAddress Pointer to the value to compare against.
 * @param AddressSize Size in bytes of the memory region (1, 2, 4, or 8).
 * @param dwMilliseconds Time-out interval in milliseconds.
 * @return TRUE if address changed, FALSE on timeout/failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress
 */
WINAPI BOOL WaitOnAddress(volatile void *Address, const void *CompareAddress, SIZE_T AddressSize,
		DWORD dwMilliseconds);

/* ---- Process Snapshot (tlhelp32.h) ---- */
/**
 * Retrieves information about the first process encountered in a system snapshot.
 * @param hSnapshot Handle to the snapshot returned by CreateToolhelp32Snapshot.
 * @param lpEntry Pointer to PROCESSENTRY32 structure.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-process32firstw
 */
WINAPI BOOL Process32FirstW(HANDLE hSnapshot, LPPROCESSENTRY32W lpEntry);

/* ---- User Environment (winbase.h) ---- */
/**
 * Retrieves the user name associated with the current thread of execution.
 * @param lpBuffer Buffer to receive the user name.
 * @param lpnSize On input, size of buffer in characters; on output, characters written.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getusernamew
 */
WINAPI BOOL GetUserNameW(LPWSTR lpBuffer, LPDWORD lpnSize);

/* ---- Memory Management (memoryapi.h) ---- */

/* ---- Timer Services (winbase.h) ---- */
/**
 * Creates a waitable timer object.
 * @param lpAttributes Security attributes (NULL = default).
 * @param bManualReset TRUE for manual reset, FALSE for automatic reset.
 * @param lpTimerName Name of the timer object.
 * @return Handle to the new timer object, or NULL on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-creatwaitabletimerw
 */
WINAPI HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpAttributes, BOOL bManualReset,
		LPCWSTR lpTimerName);

/* ---- PSAPI (psapi.h) ---- */
/**
 * Retrieves a list of module handles for the specified process.
 * @param hProcess Handle to the process.
 * @param lphModule Array to receive module handles.
 * @param cb Size of the array in bytes.
 * @param lpcbNeeded Pointer to receive bytes needed.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-enumprocessmodules
 */
WINAPI BOOL EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded);

/**
 * Extended working set query for memory monitoring.
 * @param hProcess Handle to the process.
 * @param pWsInfo Pointer to working set information buffer.
 * @param cb Size of the buffer in bytes.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-queryworkingsetchain
 */
WINAPI BOOL QueryWorkingSetEx(HANDLE hProcess, PVOID pWsInfo, DWORD cb);

/* ---- Multimedia Timer Services (mmsystem.h) ---- */
/**
 * Sets the resolution of the timer interrupt.
 * @param uPeriod Timer period in milliseconds (1-255).
 * @return Error code if an error occurs, otherwise zero.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/mmsystem/nf-mmsystem-timebeginperiod
 */
WINAPI UINT timeBeginPeriod(UINT uPeriod);

/**
 * Restores the resolution of the timer interrupt to its default setting.
 * @param uPeriod Timer period previously set by timeBeginPeriod.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/mmsystem/nf-mmsystem-timeendperiod
 */
WINAPI VOID timeEndPeriod(UINT uPeriod);

/* ============================================================ */
/* Registry API (winreg.h)                                      */
/* ============================================================ */

/**
 * Opens the specified registry key. If the key does not exist, it is created.
 * @param hKey Handle to an open key, or one of the following predefined values:
 *             HKEY_CLASSES_ROOT, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, etc.
 * @param lpSubKey Null-terminated string specifying the name of the subkey to open.
 * @param Reserved Reserved; must be zero.
 * @param samDesired Access mode for the key.
 * @param phkResult Pointer to a handle that identifies the opened key.
 * @return ERROR_SUCCESS if successful, or a Win32 error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexw
 */
WINAPI LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, DWORD samDesired,
		PHANDLE phkResult);

/**
 * Retrieves data from a specified registry value.
 * @param hKey Handle to an open key, or one of the predefined values.
 * @param lpValueName Null-terminated string specifying the name of the registry value.
 * @param dwFlags Flags controlling retrieval (e.g., RRF_RT_REG_SZ).
 * @param lpType Pointer to a variable that receives the type of data stored.
 * @param lpData Buffer that receives the copied value.
 * @param lpcbData Pointer to a variable specifying the size of the buffer in bytes.
 * @return ERROR_SUCCESS if successful, or a Win32 error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-reggetvaluew
 */
WINAPI LONG RegGetValueW(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValueName, DWORD dwFlags,
		LPDWORD lpType, PVOID lpData, PDWORD lpcbData);

/**
 * Closes a registry key handle.
 * @param hKey Handle to the registry key to close.
 * @return ERROR_SUCCESS if successful, or a Win32 error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regclosekey
 */
WINAPI LONG RegCloseKey(HKEY hKey);

/* ============================================================ */
/* COM/CoInitialize API (combaseapi.h)                          */
/* ============================================================ */

/**
 * Sets the security levels on a process for incoming calls.
 * @param pSecDesc Pointer to a security descriptor (NULL = default).
 * @param cAuthSvc Security services array (NULL = none).
 * @param asAuthSvc Array of authentication service structures.
 * @param pReserved1 Reserved; must be NULL.
 * @param dwAuthnLevel Authentication level for incoming calls.
 * @param dwImpLevel Impersonation level for incoming calls.
 * @param pAuthList Pointer to an array of authentication identities.
 * @param dwCapabilities Capability flags (e.g., EOAC_NONE).
 * @param pReserved2 Reserved; must be NULL.
 * @return S_OK on success, or an HRESULT error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializesecurity
 */
WINAPI HRESULT CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
		SOLE_AUTHENTICATION_SERVICE *asAuthSvc, void *pReserved1, DWORD dwAuthnLevel,
		DWORD dwImpLevel, void *pAuthList, DWORD dwCapabilities, void *pReserved3);

/**
 * Creates an instance of a COM object.
 * @param rclsid Class identifier (CLSID) for the object to create.
 * @param pUnkOuter Pointer to IUnknown interface of aggregated object (NULL if not aggregating).
 * @param dwClsContext Context in which to run the code.
 * @param riid Interface identifier (IID) requested from the new object.
 * @param ppv Address of pointer variable that receives the interface pointer.
 * @return S_OK on success, or an HRESULT error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-ocreateinstance
 */
WINAPI HRESULT CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter, DWORD dwClsContext,
		REFIID riid, LPVOID *ppv);

/**
 * Sets security capabilities on a proxy.
 * @param pProxy Pointer to the proxy to set capabilities on.
 * @param dwAuthnSvc Authentication service to use for the proxy.
 * @param dwAuthzSvc Authorization service to use for the proxy.
 * @param pServerPrincName Server principal name.
 * @param dwAuthnLevel Authentication level for calls through the proxy.
 * @param dwImpLevel Impersonation level for the proxy.
 * @param pAuthIdentity Client identity.
 * @param dwCapabilities Capability flags (e.g., EOAC_NONE).
 * @return S_OK on success, or an HRESULT error code otherwise.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cosetproxyblanket
 */
WINAPI HRESULT CoSetProxyBlanket(IUnknown *pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc,
		LPCWSTR pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel, void *pAuthIdentity,
		DWORD dwCapabilities);

/* ============================================================ */
/* System Information API (winbase.h, sysinfoapi.h)             */
/* ============================================================ */

/**
 * Retrieves the name of the local computer.
 * @param lpBuffer Buffer to receive the computer name.
 * @param nSize On input, size of buffer in characters; on output, characters written.
 * @return TRUE on success, FALSE on failure.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getcomputernamew
 */
WINAPI BOOL GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize);

/**
 * Retrieves information about the processor architecture and other system details.
 * @param pSystemInfo Pointer to a SYSTEM_INFO structure that receives the information.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getnativesysteminfo
 */
WINAPI void GetNativeSystemInfo(LPSYSTEM_INFO pSystemInfo);

/* ============================================================ */
/* Memory Functions (memoryapi.h, winbase.h)                    */
/* ============================================================ */

/**
 * Copies a block of memory from one location to another.
 * @param Destination Pointer to the destination block of memory.
 * @param Source Pointer to the source block of memory.
 * @param Length Number of bytes to copy.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-copymemory
 */
WINAPI void CopyMemory(LPVOID Destination, const VOID *Source, SIZE_T Length);

/**
 * Sets a block of memory to a specified value.
 * @param Destination Pointer to the destination block of memory.
 * @param Value The value to set (typically 0 for ZeroMemory).
 * @param Length Number of bytes to set.
 * @see https://docs.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-fillmemory
 */
WINAPI void FillMemory(LPVOID Destination, SIZE_T Length, BYTE Value);

/* RtlZeroMemory is the underlying implementation for ZeroMemory macro */
#define RtlZeroMemory(Destination, Length) \
    FillMemory((Destination), (Length), 0)

__SPRT_END_DECL

#endif // SPRT_WRAPPERS_WINDOWS_WINDOWS_H_
