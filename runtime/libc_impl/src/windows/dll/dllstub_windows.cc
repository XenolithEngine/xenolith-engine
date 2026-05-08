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
	return loader->kernelbase.call<decltype(&CoInitializeSecurity)>(
			loader->combase.CoInitializeSecurity, pSecDesc, cAuthSvc, asAuthSvc, pReserved1,
			dwAuthnLevel, dwImpLevel, pAuthList, dwCapabilities, pReserved3);
}

WINAPI HRESULT CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter, DWORD dwClsContext,
		REFIID riid, LPVOID *ppv) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CoCreateInstance)>(loader->combase.CoCreateInstance,
			rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

WINAPI HRESULT CoSetProxyBlanket(IUnknown *pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc,
		LPCWSTR pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel, void *pAuthIdentity,
		DWORD dwCapabilities) {
	auto loader = sprt::DllLoader::get();
	return loader->kernelbase.call<decltype(&CoSetProxyBlanket)>(loader->combase.CoSetProxyBlanket,
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
	loader->kernelbase.call<decltype(&CopyMemory)>(loader->ntdll.RtlCopyMemory, Destination, Source,
			Length);
}

WINAPI void FillMemory(LPVOID Destination, SIZE_T Length, BYTE Value) {
	auto loader = sprt::DllLoader::get();
	loader->kernelbase.call<decltype(&FillMemory)>(loader->ntdll.RtlFillMemory, Destination, Length,
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

} // extern "C"
