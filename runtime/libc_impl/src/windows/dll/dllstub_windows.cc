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

#include <sprt/wrappers/windows/windows.h>
#include <sprt/wrappers/windows/com_api.h>

#include "dllloader.h"

extern "C" {

/* ============================================================ */
/* Synchronization API (synchapi.h)                             */
/* ============================================================ */

WINAPI DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&WaitForSingleObject)>(
			loader->kernelbase.WaitForSingleObject, hHandle, dwMilliseconds);
}

WINAPI DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&WaitForSingleObjectEx)>(
			loader->kernelbase.WaitForSingleObjectEx, hHandle, dwMilliseconds, bAlertable);
}

/* ============================================================ */
/* Security API (securitybaseapi.h)                             */
/* ============================================================ */

WINAPI BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&OpenProcessToken)>(loader->advapi32.OpenProcessToken,
			ProcessHandle, DesiredAccess, TokenHandle);
}

WINAPI BOOL GetTokenInformation(HANDLE TokenHandle, TOKEN_INFORMATION_CLASS TokenInformationClass,
		LPVOID TokenInformation, DWORD TokenInformationLength, PDWORD ReturnLength) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&GetTokenInformation)>(
			loader->advapi32.GetTokenInformation, TokenHandle, TokenInformationClass,
			TokenInformation, TokenInformationLength, ReturnLength);
}

/* ============================================================ */
/* Process Snapshot (tlhelp32.h)                                */
/* ============================================================ */

WINAPI BOOL Process32FirstW(HANDLE hSnapshot, LPPROCESSENTRY32W lpEntry) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&Process32FirstW)>(loader->kernel32.Process32FirstW,
			hSnapshot, lpEntry);
}

WINAPI BOOL Process32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&Process32NextW)>(loader->kernel32.Process32NextW,
			hSnapshot, lppe);
}

/* ============================================================ */
/* User Environment (winbase.h)                                 */
/* ============================================================ */

WINAPI BOOL GetUserNameW(LPWSTR lpBuffer, LPDWORD lpnSize) {
	auto loader = sprt::DllLoader::get();
	return loader->advapi32.call<decltype(&GetUserNameW)>(loader->advapi32.GetUserNameW, lpBuffer,
			lpnSize);
}

/* ============================================================ */
/* Timer Services (winbase.h)                                   */
/* ============================================================ */

WINAPI HANDLE CreateWaitableTimerW(LPSECURITY_ATTRIBUTES lpAttributes, BOOL bManualReset,
		LPCWSTR lpTimerName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CreateWaitableTimerW)>(
			loader->kernel32.CreateWaitableTimerW, lpAttributes, bManualReset, lpTimerName);
}

WINAPI HANDLE CreateWaitableTimerExW(LPSECURITY_ATTRIBUTES lpTimerAttributes, LPCWSTR lpTimerName,
		DWORD dwFlags, DWORD dwDesiredAccess) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CreateWaitableTimerExW)>(
			loader->kernelbase.CreateWaitableTimerExW, lpTimerAttributes, lpTimerName, dwFlags,
			dwDesiredAccess);
}

WINAPI BOOL SetWaitableTimer(HANDLE hTimer, const LARGE_INTEGER *ftDueTime, LONG dwPeriod,
		PTIMERAPCROUTINE lpCompletionRoutine, LPVOID lpArgument, BOOL fResume) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&SetWaitableTimer)>(loader->kernel32.SetWaitableTimer,
			hTimer, ftDueTime, dwPeriod, lpCompletionRoutine, lpArgument, fResume);
}

WINAPI BOOL SetWaitableTimerEx(HANDLE hTimer, const LARGE_INTEGER *lpDueTime, LONG lPeriod,
		PTIMERAPCROUTINE pfnCompletionRoutine, LPVOID lpArgToCompletionRoutine,
		PREASON_CONTEXT WakeContext, ULONG TolerableDelay) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetWaitableTimerEx)>(
			loader->kernelbase.SetWaitableTimerEx, hTimer, lpDueTime, lPeriod, pfnCompletionRoutine,
			lpArgToCompletionRoutine, WakeContext, TolerableDelay);
}

WINAPI BOOL CancelWaitableTimer(HANDLE hTimer) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CancelWaitableTimer)>(
			loader->kernel32.CancelWaitableTimer, hTimer);
}

/* ============================================================ */
/* PSAPI (psapi.h)                                              */
/* ============================================================ */

WINAPI BOOL EnumProcessModules(HANDLE hProcess, HMODULE *lphModule, DWORD cb, LPDWORD lpcbNeeded) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&EnumProcessModules)>(
			loader->kernelbase.EnumProcessModules, hProcess, lphModule, cb, lpcbNeeded);
}

WINAPI BOOL QueryWorkingSetEx(HANDLE hProcess, PVOID pWsInfo, DWORD cb) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&QueryWorkingSetEx)>(
			loader->kernelbase.QueryWorkingSetEx, hProcess, pWsInfo, cb);
}

/* ============================================================ */
/* Registry API (winreg.h)                                      */
/* ============================================================ */

WINAPI LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, DWORD samDesired,
		PHANDLE phkResult) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RegOpenKeyExW)>(loader->kernel32.RegOpenKeyExW, hKey,
			lpSubKey, Reserved, samDesired, phkResult);
}

WINAPI LSTATUS RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags,
		LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RegGetValueW)>(loader->kernel32.RegGetValueW, hkey,
			lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

WINAPI LSTATUS RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName, LPDWORD lpcchValueName,
		LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RegEnumValueW)>(loader->kernel32.RegEnumValueW, hKey,
			dwIndex, lpValueName, lpcchValueName, lpReserved, lpType, lpData, lpcbData);
}

WINAPI LONG RegCloseKey(HKEY hKey) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&RegCloseKey)>(loader->kernel32.RegCloseKey, hKey);
}

/* ============================================================ */
/* COM/CoInitialize API (combaseapi.h)                          */
/* ============================================================ */

WINAPI HRESULT CoInitializeSecurity(PSECURITY_DESCRIPTOR pSecDesc, LONG cAuthSvc,
		SOLE_AUTHENTICATION_SERVICE *asAuthSvc, void *pReserved1, DWORD dwAuthnLevel,
		DWORD dwImpLevel, void *pAuthList, DWORD dwCapabilities, void *pReserved3) {
	auto loader = sprt::DllLoader::get();
	return loader->combase.call<decltype(&CoInitializeSecurity)>(
			loader->combase.CoInitializeSecurity, pSecDesc, cAuthSvc, asAuthSvc, pReserved1,
			dwAuthnLevel, dwImpLevel, pAuthList, dwCapabilities, pReserved3);
}

WINAPI HRESULT CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter, DWORD dwClsContext,
		REFIID riid, LPVOID *ppv) {
	auto loader = sprt::DllLoader::get();
	return loader->combase.call<decltype(&CoCreateInstance)>(loader->combase.CoCreateInstance,
			rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

WINAPI HRESULT CoSetProxyBlanket(IUnknown *pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc,
		LPCWSTR pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel, void *pAuthIdentity,
		DWORD dwCapabilities) {
	auto loader = sprt::DllLoader::get();
	return loader->combase.call<decltype(&CoSetProxyBlanket)>(loader->combase.CoSetProxyBlanket,
			pProxy, dwAuthnSvc, dwAuthzSvc, pServerPrincName, dwAuthnLevel, dwImpLevel,
			pAuthIdentity, dwCapabilities);
}

/* ============================================================ */
/* System Information API (winbase.h, sysinfoapi.h)             */
/* ============================================================ */

WINAPI BOOL GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetComputerNameW)>(loader->kernel32.GetComputerNameW,
			lpBuffer, nSize);
}

WINAPI void GetNativeSystemInfo(LPSYSTEM_INFO pSystemInfo) {
	auto loader = sprt::DllLoader::get();
	loader->kernelbase.call<decltype(&GetNativeSystemInfo)>(loader->kernelbase.GetNativeSystemInfo,
			pSystemInfo);
}

/* ============================================================ */
/* Memory Functions (memoryapi.h, winbase.h)                    */
/* ============================================================ */

WINAPI void CopyMemory(LPVOID Destination, const VOID *Source, SIZE_T Length) {
	auto loader = sprt::DllLoader::get();
	loader->ntdll.call<decltype(&CopyMemory)>(loader->ntdll.RtlCopyMemory, Destination, Source,
			Length);
}

WINAPI void FillMemory(LPVOID Destination, SIZE_T Length, BYTE Value) {
	auto loader = sprt::DllLoader::get();
	loader->ntdll.call<decltype(&FillMemory)>(loader->ntdll.RtlFillMemory, Destination, Length,
			Value);
}

WINAPI void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo) {
	auto loader = sprt::DllLoader::get();
	loader->kernelbase.call<decltype(&GetSystemInfo)>(loader->kernelbase.GetSystemInfo,
			lpSystemInfo);
}

WINAPI DWORD GetActiveProcessorCount(WORD GroupNumber) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetActiveProcessorCount)>(
			loader->kernel32.GetActiveProcessorCount, GroupNumber);
}

WINAPI BOOL GetLogicalProcessorInformation(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer,
		PDWORD ReturnedLength) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetLogicalProcessorInformation)>(
			loader->kernel32.GetLogicalProcessorInformation, Buffer, ReturnedLength);
}

WINAPI BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX lpBuffer) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GlobalMemoryStatusEx)>(
			loader->kernel32.GlobalMemoryStatusEx, lpBuffer);
}

WINAPI BOOL GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetComputerNameExW)>(loader->kernel32.GetComputerNameExW,
			NameType, lpBuffer, nSize);
}

WINAPI UINT SetErrorMode(UINT uMode) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&SetErrorMode)>(loader->kernelbase.SetErrorMode, uMode);
}

/* ============================================================ */
/* I/O Completion Ports (winioctl.h)                            */
/* ============================================================ */

WINAPI HANDLE CreateIoCompletionPort(HANDLE FileHandle, HANDLE ExistingCompletionPort,
		ULONG_PTR CompletionKey, DWORD NumberOfConcurrentThreads) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CreateIoCompletionPort)>(
			loader->kernel32.CreateIoCompletionPort, FileHandle, ExistingCompletionPort,
			CompletionKey, NumberOfConcurrentThreads);
}

WINAPI BOOL GetQueuedCompletionStatusEx(HANDLE CompletionPort,
		LPOVERLAPPED_ENTRY lpCompletionPortEntries, ULONG ulCount, PULONG ulNumEntriesRemoved,
		DWORD dwMilliseconds, BOOL fAlertable) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetQueuedCompletionStatusEx)>(
			loader->kernel32.GetQueuedCompletionStatusEx, CompletionPort, lpCompletionPortEntries,
			ulCount, ulNumEntriesRemoved, dwMilliseconds, fAlertable);
}

WINAPI BOOL PostQueuedCompletionStatus(HANDLE CompletionPort, DWORD dwNumberOfBytesTransferred,
		ULONG_PTR dwCompletionKey, LPOVERLAPPED lpOverlapped) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&PostQueuedCompletionStatus)>(
			loader->kernel32.PostQueuedCompletionStatus, CompletionPort, dwNumberOfBytesTransferred,
			dwCompletionKey, lpOverlapped);
}

/* ============================================================ */
/* NT Functions (ntdll.h)                                       */
/* ============================================================ */

WINAPI BOOL NtCompletionPacketAvailable() {
	auto loader = sprt::DllLoader::get();
	if (!loader->ntdll.NtCreateWaitCompletionPacket.fn) {
		if (!loader->ntdll.load(&loader->ntdll.NtCreateWaitCompletionPacket)) {
			return FALSE;
		}
	}
	if (!loader->ntdll.NtAssociateWaitCompletionPacket.fn) {
		if (!loader->ntdll.load(&loader->ntdll.NtAssociateWaitCompletionPacket)) {
			return FALSE;
		}
	}
	if (!loader->ntdll.NtCancelWaitCompletionPacket.fn) {
		if (!loader->ntdll.load(&loader->ntdll.NtCancelWaitCompletionPacket)) {
			return FALSE;
		}
	}
	return TRUE;
}

WINAPI NTSTATUS NtCreateWaitCompletionPacket(PHANDLE WaitCompletionPacketHandle,
		ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&NtCreateWaitCompletionPacket)>(
			loader->ntdll.NtCreateWaitCompletionPacket, WaitCompletionPacketHandle, DesiredAccess,
			ObjectAttributes);
}

WINAPI NTSTATUS NtAssociateWaitCompletionPacket(HANDLE WaitCompletionPacketHandle,
		HANDLE IoCompletionHandle, HANDLE TargetObjectHandle, PVOID KeyContext, PVOID ApcContext,
		NTSTATUS IoStatus, ULONG_PTR IoStatusInformation, PBOOLEAN AlreadySignaled) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&NtAssociateWaitCompletionPacket)>(
			loader->ntdll.NtAssociateWaitCompletionPacket, WaitCompletionPacketHandle,
			IoCompletionHandle, TargetObjectHandle, KeyContext, ApcContext, IoStatus,
			IoStatusInformation, AlreadySignaled);
}

WINAPI NTSTATUS NtCancelWaitCompletionPacket(HANDLE WaitCompletionPacketHandle,
		BOOLEAN RemoveSignaledPacket) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&NtCancelWaitCompletionPacket)>(
			loader->ntdll.NtCancelWaitCompletionPacket, WaitCompletionPacketHandle,
			RemoveSignaledPacket);
}

WINAPI NTSTATUS NtClose(HANDLE Handle) {
	auto loader = sprt::DllLoader::get();
	return loader->ntdll.call<decltype(&NtClose)>(loader->ntdll.NtClose, Handle);
}

/* ============================================================ */
/* String/NLS Functions (winnls.h)                              */
/* ============================================================ */

WINAPI int LCMapStringEx(LPCWSTR lpLocaleName, DWORD dwMapFlags, LPCWSTR lpSrcStr, int cchSrc,
		LPWSTR lpDestStr, int cchDest, LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved,
		LPARAM sortHandle) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&LCMapStringEx)>(loader->kernel32.LCMapStringEx,
			lpLocaleName, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest, lpVersionInformation,
			lpReserved, sortHandle);
}

WINAPI int CompareStringEx(LPCWSTR lpLocaleName, DWORD dwCmpFlags, LPCWCH lpString1, int cchCount1,
		LPCWCH lpString2, int cchCount2, LPNLSVERSIONINFO lpVersionInformation, LPVOID lpReserved,
		LPARAM lParam) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&CompareStringEx)>(loader->kernel32.CompareStringEx,
			lpLocaleName, dwCmpFlags, lpString1, cchCount1, lpString2, cchCount2,
			lpVersionInformation, lpReserved, lParam);
}

WINAPI int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte,
		LPWSTR lpWideCharStr, int cchWideChar) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&MultiByteToWideChar)>(
			loader->kernel32.MultiByteToWideChar, CodePage, dwFlags, lpMultiByteStr, cbMultiByte,
			lpWideCharStr, cchWideChar);
}

WINAPI int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar,
		LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&WideCharToMultiByte)>(
			loader->kernel32.WideCharToMultiByte, CodePage, dwFlags, lpWideCharStr, cchWideChar,
			lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar);
}

WINAPI int GetLocaleInfoEx(LPCWSTR lpLocaleName, LCTYPE LCType, LPWSTR lpLCData, int cchData) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetLocaleInfoEx)>(loader->kernel32.GetLocaleInfoEx,
			lpLocaleName, LCType, lpLCData, cchData);
}

WINAPI BOOL IsValidLocaleName(LPCWSTR lpLocaleName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&IsValidLocaleName)>(loader->kernel32.IsValidLocaleName,
			lpLocaleName);
}

WINAPI int GetUserDefaultLocaleName(LPWSTR lpLocaleName, int cchLocaleName) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetUserDefaultLocaleName)>(
			loader->kernel32.GetUserDefaultLocaleName, lpLocaleName, cchLocaleName);
}

WINAPI int IdnToAscii(DWORD dwFlags, LPCWSTR lpUnicodeCharStr, int cchUnicodeChar,
		LPWSTR lpASCIICharStr, int cchASCIIChar) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&IdnToAscii)>(loader->kernel32.IdnToAscii, dwFlags,
			lpUnicodeCharStr, cchUnicodeChar, lpASCIICharStr, cchASCIIChar);
}

WINAPI int IdnToUnicode(DWORD dwFlags, LPCWSTR lpASCIICharStr, int cchASCIIChar,
		LPWSTR lpUnicodeCharStr, int cchUnicodeChar) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&IdnToUnicode)>(loader->kernel32.IdnToUnicode, dwFlags,
			lpASCIICharStr, cchASCIIChar, lpUnicodeCharStr, cchUnicodeChar);
}

WINAPI HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit) {
	auto loader = sprt::DllLoader::get();
	return loader->combase.call<decltype(&CoInitializeEx)>(loader->combase.CoInitializeEx,
			pvReserved, dwCoInit);
}

WINAPI void CoUninitialize() {
	auto loader = sprt::DllLoader::get();
	loader->combase.call<decltype(&CoUninitialize)>(loader->combase.CoUninitialize);
}

WINAPI void CoTaskMemFree(LPVOID pv) {
	auto loader = sprt::DllLoader::get();
	loader->combase.call<decltype(&CoTaskMemFree)>(loader->combase.CoTaskMemFree, pv);
}

WINAPI HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken,
		PWSTR *ppszPath) {
	auto loader = sprt::DllLoader::get();
	return loader->shell32.call<decltype(&SHGetKnownFolderPath)>(
			loader->shell32.SHGetKnownFolderPath, rfid, dwFlags, hToken, ppszPath);
}

WINAPI int RestartEventCompletion2(void *hPacket, void *hIOCP, void *hEvent,
		DWORD dwNumberOfBytesTransferred, UINT_PTR dwCompletionKey, void *lpOverlapped) {
	HRESULT hr = NtAssociateWaitCompletionPacket(hPacket, hIOCP, hEvent, (PVOID)dwCompletionKey,
			(PVOID)lpOverlapped, 0, dwNumberOfBytesTransferred, NULL);
	if (SUCCEEDED(hr)) {
		return TRUE;
	} else {
		switch (hr) {
		case STATUS_NO_MEMORY: SetLastError(ERROR_OUTOFMEMORY); break;
		case STATUS_INVALID_HANDLE: // not valid handle passed for hIOCP
		case STATUS_OBJECT_TYPE_MISMATCH: // incorrect handle passed for hIOCP
		case STATUS_INVALID_PARAMETER_1:
		case STATUS_INVALID_PARAMETER_2: SetLastError(ERROR_INVALID_PARAMETER); break;
		case STATUS_INVALID_PARAMETER_3:
			if (hEvent) {
				SetLastError(ERROR_INVALID_HANDLE);
			} else {
				SetLastError(ERROR_INVALID_PARAMETER);
			}
			break;
		default: SetLastError(hr);
		}
		return FALSE;
	}
}

WINAPI int RestartEventCompletion(void *hPacket, void *hIOCP, void *hEvent,
		const void **ncompletion) {
	if (!ncompletion) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	auto completion = (const OVERLAPPED_ENTRY *)ncompletion;

	return RestartEventCompletion2(hPacket, hIOCP, hEvent, completion->dwNumberOfBytesTransferred,
			completion->lpCompletionKey, completion->lpOverlapped);
}

WINAPI int CancelEventCompletion(void *hPacket, int cancel) {
	HRESULT hr = NtCancelWaitCompletionPacket(hPacket, cancel);
	if (SUCCEEDED(hr)) {
		return TRUE;
	} else {
		SetLastError(hr);
		return FALSE;
	}
}

WINAPI void *ReportEventAsCompletion(void *hIOCP, void *hEvent, DWORD dwNumberOfBytesTransferred,
		UINT_PTR dwCompletionKey, void *lpOverlapped) {

	HANDLE hPacket = NULL;
	HRESULT hr = NtCreateWaitCompletionPacket(&hPacket, GENERIC_ALL, NULL);

	if (SUCCEEDED(hr)) {
		OVERLAPPED_ENTRY completion{};
		completion.dwNumberOfBytesTransferred = dwNumberOfBytesTransferred;
		completion.lpCompletionKey = dwCompletionKey;
		completion.lpOverlapped = (LPOVERLAPPED)lpOverlapped;

		if (!RestartEventCompletion(hPacket, hIOCP, hEvent, (const void **)&completion)) {
			NtClose(hPacket);
			hPacket = NULL;
		}
	} else {
		switch (hr) {
		case STATUS_NO_MEMORY: SetLastError(ERROR_OUTOFMEMORY); break;
		default: SetLastError(hr);
		}
	}
	return hPacket;
}

WINAPI BOOL GetVersionExA(LPOSVERSIONINFOA lpVersionInformation) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetVersionExA)>(loader->kernel32.GetVersionExA,
			lpVersionInformation);
}

WINAPI BOOL GetVersionExW(LPOSVERSIONINFOW lpVersionInformation) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetVersionExW)>(loader->kernel32.GetVersionExW,
			lpVersionInformation);
}

WINAPI UINT GetACP(void) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetACP)>(loader->kernel32.GetACP);
}

WINAPI BOOL ReadConsoleA(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead,
		LPDWORD lpNumberOfCharsRead, PCONSOLE_READCONSOLE_CONTROL pInputControl) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&ReadConsoleA)>(loader->kernel32.ReadConsoleA,
			hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
}

WINAPI BOOL ReadConsoleW(HANDLE hConsoleInput, LPVOID lpBuffer, DWORD nNumberOfCharsToRead,
		LPDWORD lpNumberOfCharsRead, PCONSOLE_READCONSOLE_CONTROL pInputControl) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&ReadConsoleW)>(loader->kernel32.ReadConsoleW,
			hConsoleInput, lpBuffer, nNumberOfCharsToRead, lpNumberOfCharsRead, pInputControl);
}

WINAPI UINT GetSystemDirectoryA(LPSTR lpBuffer, UINT uSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetSystemDirectoryA)>(
			loader->kernel32.GetSystemDirectoryA, lpBuffer, uSize);
}

WINAPI UINT GetSystemDirectoryW(LPWSTR lpBuffer, UINT uSize) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&GetSystemDirectoryW)>(
			loader->kernel32.GetSystemDirectoryW, lpBuffer, uSize);
}

WINAPI DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll,
		DWORD dwMilliseconds) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&WaitForMultipleObjects)>(
			loader->kernel32.WaitForMultipleObjects, nCount, lpHandles, bWaitAll, dwMilliseconds);
}

WINAPI BOOL PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize,
		LPDWORD lpBytesRead, LPDWORD lpTotalBytesAvail, LPDWORD lpBytesLeftThisMessage) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&PeekNamedPipe)>(loader->kernel32.PeekNamedPipe,
			hNamedPipe, lpBuffer, nBufferSize, lpBytesRead, lpTotalBytesAvail,
			lpBytesLeftThisMessage);
}

WINAPI void VariantInit(VARIANTARG *pvarg) {
	auto loader = sprt::DllLoader::get();
	loader->oleaut32.call<decltype(&VariantInit)>(loader->oleaut32.VariantInit, pvarg);
}

WINAPI HRESULT VariantClear(VARIANTARG *pvarg) {
	auto loader = sprt::DllLoader::get();
	return loader->oleaut32.call<decltype(&VariantClear)>(loader->oleaut32.VariantClear, pvarg);
}

WINAPI HRESULT VariantCopy(VARIANTARG *pvargDest, const VARIANTARG *pvargSrc) {
	auto loader = sprt::DllLoader::get();
	return loader->oleaut32.call<decltype(&VariantCopy)>(loader->oleaut32.VariantCopy, pvargDest,
			pvargSrc);
}

WINAPI void SysFreeString(BSTR bstrString) {
	auto loader = sprt::DllLoader::get();
	loader->oleaut32.call<decltype(&SysFreeString)>(loader->oleaut32.SysFreeString, bstrString);
}

WINAPI HRESULT StringFromCLSID(REFCLSID rclsid, LPOLESTR *lplpsz) {
	auto loader = sprt::DllLoader::get();
	return loader->combase.call<decltype(&StringFromCLSID)>(loader->combase.StringFromCLSID, rclsid,
			lplpsz);
}

WINAPI HRESULT CLSIDFromString(LPCOLESTR lpsz, LPCLSID pclsid) {
	auto loader = sprt::DllLoader::get();
	return loader->combase.call<decltype(&CLSIDFromString)>(loader->combase.CLSIDFromString, lpsz,
			pclsid);
}

WINAPI HRESULT IIDFromString(LPCOLESTR lpsz, LPIID lpiid) {
	auto loader = sprt::DllLoader::get();
	return loader->combase.call<decltype(&IIDFromString)>(loader->combase.IIDFromString, lpsz,
			lpiid);
}

WINAPI ULONGLONG VerSetConditionMask(ULONGLONG ConditionMask, DWORD TypeMask, BYTE Condition) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&VerSetConditionMask)>(
			loader->kernel32.VerSetConditionMask, ConditionMask, TypeMask, Condition);
}

WINAPI BOOL VerifyVersionInfoW(LPOSVERSIONINFOEXW lpVersionInformation, DWORD dwTypeMask,
		DWORDLONG dwlConditionMask) {
	auto loader = sprt::DllLoader::get();
	return loader->kernel32.call<decltype(&VerifyVersionInfoW)>(loader->kernel32.VerifyVersionInfoW,
			lpVersionInformation, dwTypeMask, dwlConditionMask);
}


} // extern "C"
