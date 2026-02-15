/**
 Copyright (c) 2026 Xenolith Team <admin@stappler.org>

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

#define __SPRT_BUILD 1
#define INITKNOWNFOLDERS
#define INITGUID

#include <sprt/c/__sprt_errno.h>
#include <sprt/c/sys/__sprt_winapi.h>

#include "private/SPRTPrivate.h"

#if __SPRT_CONFIG_HAVE_WINAPI
#include <Windows.h>
#include <userenv.h>
#include <winternl.h>
#include <ntstatus.h>
#include <locale.h>

#include <Shlobj.h>
#include <KnownFolders.h>
#include <sddl.h>
#include <accctrl.h>
#include <aclapi.h>

#include <stdio.h>
#include <io.h>
#include <fcntl.h>

#endif

#include <sprt/runtime/stringview.h>
#include <sprt/runtime/log.h>

#undef __deallocate

#include "private/SPRTSpecific.h"

#if __SPRT_CONFIG_HAVE_WINAPI
extern "C" {

WINBASEAPI NTSTATUS WINAPI NtCreateWaitCompletionPacket(_Out_ PHANDLE WaitCompletionPacketHandle,
		_In_ ACCESS_MASK DesiredAccess, _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes);

WINBASEAPI NTSTATUS WINAPI NtAssociateWaitCompletionPacket(_In_ HANDLE WaitCompletionPacketHandle,
		_In_ HANDLE IoCompletionHandle, _In_ HANDLE TargetObjectHandle, _In_opt_ PVOID KeyContext,
		_In_opt_ PVOID ApcContext, _In_ NTSTATUS IoStatus, _In_ ULONG_PTR IoStatusInformation,
		_Out_opt_ PBOOLEAN AlreadySignaled);

WINBASEAPI NTSTATUS WINAPI NtCancelWaitCompletionPacket(_In_ HANDLE WaitCompletionPacketHandle,
		_In_ BOOLEAN RemoveSignaledPacket);
}
#endif

namespace sprt {

#if __SPRT_CONFIG_HAVE_WINAPI

__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(GetCurrentThreadId)() { return GetCurrentThreadId(); }

__SPRT_C_FUNC __SPRT_ID(uint32_t) __SPRT_ID(GetLastError)() { return GetLastError(); }

__SPRT_C_FUNC int __SPRT_ID(WaitOnAddress)(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds) {
	return WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
}
__SPRT_C_FUNC void __SPRT_ID(WakeByAddressSingle)(void *Address) { WakeByAddressSingle(Address); }

__SPRT_C_FUNC void __SPRT_ID(WakeByAddressAll)(void *Address) { WakeByAddressAll(Address); }

__SPRT_C_FUNC int __SPRT_ID(_LCMapString)(const char16_t *lpLocaleName,
		__SPRT_ID(uint32_t) dwMapFlags, const char16_t *lpSrcStr, int cchSrc, char16_t *lpDestStr,
		int cchDest) {
	return LCMapStringEx((const wchar_t *)lpLocaleName, dwMapFlags, (const wchar_t *)lpSrcStr,
			cchSrc, (wchar_t *)lpDestStr, cchDest, nullptr, nullptr, 0);
}

__SPRT_C_FUNC int __SPRT_ID(_CompareString)(const char16_t *lpLocaleName,
		__SPRT_ID(uint32_t) dwMapFlags, const char16_t *lpString1, int cchCount1,
		const char16_t *lpString2, int cchCount2) {
	return CompareStringEx((const wchar_t *)lpLocaleName, dwMapFlags, (wchar_t *)lpString1,
			cchCount1, (wchar_t *)lpString2, cchCount2, NULL, NULL, 0);
}

__SPRT_C_FUNC int __SPRT_ID(_GetUserDefaultLocaleName)(char16_t *wlocale, int bufSize) {
	return GetUserDefaultLocaleName((wchar_t *)wlocale, bufSize);
}

__SPRT_C_FUNC int __SPRT_ID(_IdnToAscii)(uint32_t dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	return IdnToAscii(dwFlags, (const wchar_t *)lpUnicodeCharStr, cchUnicodeChar,
			(wchar_t *)lpASCIICharStr, cchASCIIChar);
}

__SPRT_C_FUNC int __SPRT_ID(_IdnToUnicode)(uint32_t dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	return IdnToUnicode(dwFlags, (const wchar_t *)lpUnicodeCharStr, cchUnicodeChar,
			(wchar_t *)lpASCIICharStr, cchASCIIChar);
}

__SPRT_C_FUNC __SPRT_ID(uint32_t)
		__SPRT_ID(_GetCurrentDirectory)(__SPRT_ID(uint32_t) nBufferLength, char16_t *lpBuffer) {
	return GetCurrentDirectoryW(nBufferLength, (wchar_t *)lpBuffer);
}
__SPRT_C_FUNC void *__SPRT_ID(_ReportEventAsCompletion)(void *hIOCP, void *hEvent,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		void *lpOverlapped) {

	HANDLE hPacket = NULL;
	HRESULT hr = NtCreateWaitCompletionPacket(&hPacket, GENERIC_ALL, NULL);

	if (SUCCEEDED(hr)) {
		OVERLAPPED_ENTRY completion{};
		completion.dwNumberOfBytesTransferred = dwNumberOfBytesTransferred;
		completion.lpCompletionKey = dwCompletionKey;
		completion.lpOverlapped = (LPOVERLAPPED)lpOverlapped;

		if (!__SPRT_ID(
					_RestartEventCompletion)(hPacket, hIOCP, hEvent, (const void **)&completion)) {
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

__SPRT_C_FUNC int __SPRT_ID(_RestartEventCompletion)(void *hPacket, void *hIOCP, void *hEvent,
		const void **ncompletion) {
	if (!ncompletion) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	auto completion = (const OVERLAPPED_ENTRY *)ncompletion;

	return __SPRT_ID(_RestartEventCompletion2)(hPacket, hIOCP, hEvent,
			completion->dwNumberOfBytesTransferred, completion->lpCompletionKey,
			completion->lpOverlapped);
}

__SPRT_C_FUNC int __SPRT_ID(_RestartEventCompletion2)(void *hPacket, void *hIOCP, void *hEvent,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		void *lpOverlapped) {
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

__SPRT_C_FUNC int __SPRT_ID(_CancelEventCompletion)(void *hPacket, int cancel) {
	HRESULT hr = NtCancelWaitCompletionPacket(hPacket, cancel);
	if (SUCCEEDED(hr)) {
		return TRUE;
	} else {
		SetLastError(hr);
		return FALSE;
	}
}

__SPRT_C_FUNC int __SPRT_ID(_PeekMessage)(__SPRT_ID(winmsg) * lpMsg, void *hWnd,
		unsigned wMsgFilterMin, unsigned wMsgFilterMax, unsigned wRemoveMsg) {
	return PeekMessageW((MSG *)lpMsg, (HWND)hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

__SPRT_C_FUNC int __SPRT_ID(_TranslateMessage)(const __SPRT_ID(winmsg) * lpMsg) {
	return TranslateMessage((const MSG *)lpMsg);
}

__SPRT_C_FUNC int __SPRT_ID(_DispatchMessage)(const __SPRT_ID(winmsg) * lpMsg) {
	return DispatchMessageW((const MSG *)lpMsg);
}

__SPRT_C_FUNC __SPRT_ID(uint32_t) __SPRT_ID(_MsgWaitForMultipleObjectsEx)(
		__SPRT_ID(uint32_t) nCount, void *const *pHandles, __SPRT_ID(uint32_t) dwMilliseconds,
		__SPRT_ID(uint32_t) dwWakeMask, __SPRT_ID(uint32_t) dwFlags) {
	return MsgWaitForMultipleObjectsEx(nCount, pHandles, dwMilliseconds, dwWakeMask, dwFlags);
}

__SPRT_C_FUNC void *__SPRT_ID(_CreateIoCompletionPort)(void *FileHandle,
		void *ExistingCompletionPort, __SPRT_ID(uintptr_t) CompletionKey,
		__SPRT_ID(uint32_t) NumberOfConcurrentThreads) {
	return CreateIoCompletionPort(FileHandle, ExistingCompletionPort, CompletionKey,
			NumberOfConcurrentThreads);
}

__SPRT_C_FUNC int __SPRT_ID(_GetQueuedCompletionStatusEx)(void *CompletionPort,
		__SPRT_ID(overlapped_entry) * lpCompletionPortEntries, unsigned long ulCount,
		unsigned long *ulNumEntriesRemoved, __SPRT_ID(uint32_t) dwMilliseconds, int fAlertable) {
	return GetQueuedCompletionStatusEx(CompletionPort, (LPOVERLAPPED_ENTRY)lpCompletionPortEntries,
			ulCount, ulNumEntriesRemoved, dwMilliseconds, fAlertable);
}

__SPRT_C_FUNC int __SPRT_ID(_PostQueuedCompletionStatus)(void *CompletionPort,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		__SPRT_ID(overlapped) * lpOverlapped) {
	return PostQueuedCompletionStatus(CompletionPort, dwNumberOfBytesTransferred, dwCompletionKey,
			(LPOVERLAPPED)lpOverlapped);
}

__SPRT_C_FUNC int __SPRT_ID(_CloseHandle)(void *hObject) { return CloseHandle(hObject); }

__SPRT_C_FUNC void *__SPRT_ID(_CreateWaitableTimerEx)(
		__SPRT_ID(security_attributes) * lpTimerAttributes, const char16_t *lpTimerName,
		__SPRT_ID(uint32_t) dwFlags, __SPRT_ID(uint32_t) dwDesiredAccess) {
	return CreateWaitableTimerExW((LPSECURITY_ATTRIBUTES)lpTimerAttributes,
			(const wchar_t *)lpTimerName, dwFlags, dwDesiredAccess);
}

__SPRT_C_FUNC int __SPRT_ID(_SetWaitableTimer)(void *hTimer, __SPRT_ID(int64_t) * lpDueTime,
		long lPeriod, __SPRT_ID(timer_apc_routine) pfnCompletionRoutine,
		void *lpArgToCompletionRoutine, int fResume) {
	LARGE_INTEGER _lpDueTime;
	if (lpDueTime) {
		_lpDueTime.QuadPart = *lpDueTime;
	}
	return SetWaitableTimer(hTimer, lpDueTime ? &_lpDueTime : nullptr, lPeriod,
			pfnCompletionRoutine, lpArgToCompletionRoutine, fResume);
}

__SPRT_C_FUNC int __SPRT_ID(_SetWaitableTimerEx)(void *hTimer, __SPRT_ID(int64_t) * lpDueTime,
		long lPeriod, __SPRT_ID(timer_apc_routine) pfnCompletionRoutine,
		void *lpArgToCompletionRoutine, __SPRT_ID(reason_context) * WakeContext,
		unsigned long TolerableDelay) {
	LARGE_INTEGER _lpDueTime;
	if (lpDueTime) {
		_lpDueTime.QuadPart = *lpDueTime;
	}
	return SetWaitableTimerEx(hTimer, lpDueTime ? &_lpDueTime : nullptr, lPeriod,
			pfnCompletionRoutine, lpArgToCompletionRoutine, (PREASON_CONTEXT)WakeContext,
			TolerableDelay);
}

__SPRT_C_FUNC int __SPRT_ID(_CancelWaitableTimer)(void *hTimer) {
	return CancelWaitableTimer(hTimer);
}

#else
__SPRT_C_FUNC __SPRT_ID(pid_t) __SPRT_ID(GetCurrentThreadId)() {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC __SPRT_ID(uint32_t) __SPRT_ID(GetLastError)() {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(WaitOnAddress)(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC void __SPRT_ID(WakeByAddressSingle)(void *Address) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
}

__SPRT_C_FUNC void __SPRT_ID(WakeByAddressAll)(void *Address) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
}

__SPRT_C_FUNC int __SPRT_ID(_LCMapString)(const char16_t *lpLocaleName,
		__SPRT_ID(uint32_t) dwMapFlags, const char16_t *lpSrcStr, int cchSrc, char16_t *lpDestStr,
		int cchDest) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_CompareString)(const char16_t *lpLocaleName,
		__SPRT_ID(uint32_t) dwMapFlags, const char16_t *lpString1, int cchCount1,
		const char16_t *lpString2, int cchCount2) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_GetUserDefaultLocaleName)(char16_t *wlocale, int bufSize) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_IdnToAscii)(uint32_t dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC int __SPRT_ID(_IdnToUnicode)(uint32_t dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return -1;
}

__SPRT_C_FUNC __SPRT_ID(uint32_t)
		__SPRT_ID(_GetCurrentDirectory)(__SPRT_ID(uint32_t) nBufferLength, char *lpBuffer) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC void *__SPRT_ID(_ReportEventAsCompletion)(void *hIOCP, void *hEvent,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		void *lpOverlapped) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return nullptr;
}

__SPRT_C_FUNC int __SPRT_ID(_RestartEventCompletion)(void *hPacket, void *hIOCP, void *hEvent,
		const void **ncompletion) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_RestartEventCompletion2)(void *hPacket, void *hIOCP, void *hEvent,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		void *lpOverlapped) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_CancelEventCompletion)(void *hPacket, int cancel) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_PeekMessage)(__SPRT_ID(winmsg) * lpMsg, void *hWnd,
		unsigned wMsgFilterMin, unsigned wMsgFilterMax, unsigned wRemoveMsg) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_TranslateMessage)(const __SPRT_ID(winmsg) * lpMsg) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_DispatchMessage)(const __SPRT_ID(winmsg) * lpMsg) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC __SPRT_ID(uint32_t) __SPRT_ID(_MsgWaitForMultipleObjectsEx)(
		__SPRT_ID(uint32_t) nCount, void *const *pHandles, __SPRT_ID(uint32_t) dwMilliseconds,
		__SPRT_ID(uint32_t) dwWakeMask, __SPRT_ID(uint32_t) dwFlags) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC void *__SPRT_ID(_CreateIoCompletionPort)(void *FileHandle,
		void *ExistingCompletionPort, __SPRT_ID(uintptr_t) CompletionKey,
		__SPRT_ID(uint32_t) NumberOfConcurrentThreads) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return nullptr;
}

__SPRT_C_FUNC int __SPRT_ID(_GetQueuedCompletionStatusEx)(void *CompletionPort,
		__SPRT_ID(overlapped_entry) * lpCompletionPortEntries, unsigned long ulCount,
		unsigned long *ulNumEntriesRemoved, __SPRT_ID(uint32_t) dwMilliseconds, int fAlertable) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_PostQueuedCompletionStatus)(void *CompletionPort,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		__SPRT_ID(overlapped) * lpOverlapped) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_CloseHandle)(void *hObject) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC void *__SPRT_ID(_CreateWaitableTimerEx)(
		__SPRT_ID(security_attributes) * lpTimerAttributes, const char16_t *lpTimerName,
		__SPRT_ID(uint32_t) dwFlags, __SPRT_ID(uint32_t) dwDesiredAccess) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return nullptr;
}

__SPRT_C_FUNC int __SPRT_ID(_SetWaitableTimer)(void *hTimer, __SPRT_ID(int64_t) * lpDueTime,
		long lPeriod, __SPRT_ID(timer_apc_routine) pfnCompletionRoutine,
		void *lpArgToCompletionRoutine, int fResume) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_SetWaitableTimerEx)(void *hTimer, __SPRT_ID(int64_t) * lpDueTime,
		long lPeriod, __SPRT_ID(timer_apc_routine) pfnCompletionRoutine,
		void *lpArgToCompletionRoutine, __SPRT_ID(reason_context) * WakeContext,
		unsigned long TolerableDelay) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

__SPRT_C_FUNC int __SPRT_ID(_CancelWaitableTimer)(void *hTimer) {
	log::vprint(log::LogType::Info, __SPRT_LOCATION, "rt-libc", __SPRT_FUNCTION__,
			" not available for this platform (__SPRT_CONFIG_HAVE_WINAPI)");
	return 0;
}

#endif

} // namespace sprt

#if __SPRT_CONFIG_HAVE_WINAPI

/*
	AI-Generated
*/

// WSA error code constants (from winsock2.h)
// Note: WSAGetLastError() is used for socket errors, but we map them here for completeness
#ifndef WSAECONNRESET
#define WSAECONNRESET 10'054
#define WSAENETDOWN 10'050
#define WSAENOTCONN 10'057
#define WSAEISCONN 10'056
#define WSAEAFNOSUPPORT 10'047
#define WSAEMSGSIZE 10'040
#define WSAEDESTADDRREQ 10'039
#define WSAENOPROTOOPT 10'042
#define WSAESOCKTNOSUPPORT 10'044
#define WSAEPFNOSUPPORT 10'046
#define WSAEHOSTDOWN 10'064
#define WSAETOOMANYREFS 10'059
#endif

namespace sprt::platform {

int lastErrorToErrno(unsigned long winerr) {
	switch (winerr) {
	// Success
	case ERROR_SUCCESS: return 0;

	// File and path errors
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_BAD_PATHNAME:
	case ERROR_INVALID_DRIVE:
	case ERROR_BAD_NETPATH:
	case ERROR_BAD_NET_NAME:
	case ERROR_FILENAME_EXCED_RANGE:
		return ENOENT; // No such file or directory

	// Permission and access errors
	case ERROR_ACCESS_DENIED:
	case ERROR_LOCK_VIOLATION:
	case ERROR_CANNOT_MAKE:
	case ERROR_NETWORK_ACCESS_DENIED:
		return EACCES; // Permission denied

	// Sharing violation (can be access denied or text file busy)
	case ERROR_SHARING_VIOLATION:
		return ETXTBSY; // Text file busy (sharing violation often indicates file is locked/in use)

	// Invalid handle/file descriptor
	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_ACCESS:
		return EBADF; // Bad file descriptor

	// Memory errors
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
	case ERROR_ARENA_TRASHED:
		return ENOMEM; // Out of memory

	// Invalid parameter/argument
	case ERROR_INVALID_PARAMETER:
	case ERROR_INVALID_DATA:
	case ERROR_BAD_LENGTH:
	case ERROR_INVALID_NAME:
	case ERROR_BAD_FORMAT:
		return EINVAL; // Invalid argument

	// File already exists
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		return EEXIST; // File exists

	// Too many files
	case ERROR_TOO_MANY_OPEN_FILES: return EMFILE; // Too many open files
	case ERROR_NO_PROC_SLOTS:
		return EAGAIN; // Resource temporarily unavailable

	// I/O errors
	case ERROR_WRITE_FAULT:
	case ERROR_READ_FAULT:
	case ERROR_SEEK:
	case ERROR_BAD_COMMAND:
	case ERROR_CRC:
	case ERROR_DISK_CHANGE:
	case ERROR_GEN_FAILURE:
	case ERROR_IO_DEVICE:
	case ERROR_IO_INCOMPLETE:
	case ERROR_BAD_NET_RESP:
	case ERROR_NET_WRITE_FAULT:
		return EIO; // I/O error

	// Not ready (could be I/O error or no medium)
	case ERROR_NOT_READY:
		return ENOMEDIUM; // No medium found (or device not ready)

	// No space
	case ERROR_HANDLE_DISK_FULL:
	case ERROR_DISK_FULL:
		return ENOSPC; // No space left on device

	// Broken pipe
	case ERROR_BROKEN_PIPE:
		return EPIPE; // Broken pipe

	// Device errors
	case ERROR_BAD_UNIT:
	case ERROR_NOT_CONNECTED:
		return ENODEV; // No such device

	// Network errors
	case ERROR_NETWORK_BUSY: return EBUSY; // Device or resource busy
	case ERROR_NETNAME_DELETED: return ENOENT; // No such file or directory
	case ERROR_NETWORK_UNREACHABLE: return ENETUNREACH; // Network is unreachable
	case ERROR_HOST_UNREACHABLE: return EHOSTUNREACH; // Host is unreachable
	case ERROR_CONNECTION_REFUSED: return ECONNREFUSED; // Connection refused
	case ERROR_CONNECTION_ABORTED: return ECONNABORTED; // Connection aborted
	case WSAECONNRESET: return ECONNRESET; // Connection reset by peer
	case WSAENETDOWN:
		return ENETDOWN; // Network is down

	// Timeout
	case ERROR_SEM_TIMEOUT:
	case WAIT_TIMEOUT:
		return ETIMEDOUT; // Connection timed out

	// Socket errors
	case WSAENOTCONN: return ENOTCONN; // Transport endpoint is not connected
	case WSAEISCONN: return EISCONN; // Transport endpoint is already connected
	case ERROR_TOO_MANY_NAMES:
		return EMFILE; // Too many open files

	// Directory errors
	case ERROR_DIRECTORY:
	case ERROR_CURRENT_DIRECTORY: return ENOTDIR; // Not a directory
	case ERROR_DIR_NOT_EMPTY: return ENOTEMPTY; // Directory not empty
	case ERROR_DIRECTORY_NOT_SUPPORTED:
		return ENOSYS; // Function not implemented

	// Is a directory
	case ERROR_CANTOPEN:
		return EISDIR; // Is a directory

	// Operation in progress
	case ERROR_OPERATION_IN_PROGRESS:
		return EINPROGRESS; // Operation now in progress

	// Already in progress
	case ERROR_ALREADY_ASSIGNED:
		return EALREADY; // Operation already in progress

	// Interrupted
	case ERROR_INVALID_AT_INTERRUPT_TIME:
		return EINTR; // Interrupted system call

	// Canceled
	case ERROR_OPERATION_ABORTED:
	case ERROR_CANCELLED:
		return ECANCELED; // Operation canceled

	// Device or resource busy
	case ERROR_BUSY:
	case ERROR_BUSY_DRIVE:
	case ERROR_DEVICE_IN_USE:
		return EBUSY; // Device or resource busy

	// Function not implemented
	case ERROR_CALL_NOT_IMPLEMENTED:
		return ENOSYS; // Function not implemented

	// Address family not supported
	case WSAEAFNOSUPPORT:
		return EAFNOSUPPORT; // Address family not supported by protocol

	// Operation not supported
	case ERROR_NOT_SUPPORTED:
		return EOPNOTSUPP; // Operation not supported on transport endpoint

	// Address already in use
	case ERROR_ADDRESS_ALREADY_ASSOCIATED:
		return EADDRINUSE; // Address already in use

	// Address not available
	case ERROR_ADDRESS_NOT_ASSOCIATED:
		return EADDRNOTAVAIL; // Cannot assign requested address

	// Message too long
	case WSAEMSGSIZE:
		return EMSGSIZE; // Message too long

	// Protocol errors
	case ERROR_PROTOCOL_UNREACHABLE:
		return ENOLINK; // Link has been severed (protocol unreachable)

	// Would block / pending operations
	case ERROR_IO_PENDING:
		return EWOULDBLOCK; // Operation would block

	// Buffer overflow
	case ERROR_INSUFFICIENT_BUFFER: return ENAMETOOLONG; // File name too long
	case ERROR_BUFFER_OVERFLOW:
		return ENAMETOOLONG; // File name too long

	// Semaphore errors
	case ERROR_TOO_MANY_SEMAPHORES: return ENFILE; // Too many open files in system
	case ERROR_EXCL_SEM_ALREADY_OWNED: return EAGAIN; // Resource temporarily unavailable
	case ERROR_SEM_IS_SET: return EAGAIN; // Resource temporarily unavailable
	case ERROR_TOO_MANY_SEM_REQUESTS: return EAGAIN; // Resource temporarily unavailable
	case ERROR_SEM_OWNER_DIED:
		return EOWNERDEAD; // Owner died

	// Mutex errors (using same mapping as semaphore)

	// Not same device
	case ERROR_NOT_SAME_DEVICE:
		return EXDEV; // Cross-device link

	// Read-only file system
	case ERROR_WRITE_PROTECT:
		return EROFS; // Read-only file system (write protected)

	// Range error
	case ERROR_INVALID_ADDRESS:
		return ERANGE; // Numerical result out of range

	// Domain error
	case ERROR_INVALID_DOMAIN_ROLE:
	case ERROR_INVALID_DOMAIN_STATE:
		return EDOM; // Numerical argument out of domain

	// No message available
	case ERROR_NO_DATA:
		return ENOMSG; // No message of desired type

	// Identifier removed
	case ERROR_NO_TOKEN:
		return EIDRM; // Identifier removed

	// Invalid handle state / link number out of range
	case ERROR_INVALID_HANDLE_STATE:
		return ELNRNG; // Link number out of range

	// Medium type wrong - ERROR_BAD_MEDIA_TYPE may not be defined in all Windows versions
	// Using ERROR_BAD_UNIT as alternative for media-related errors

	// Socket operation on non-socket
	case ERROR_INVALID_LEVEL:
		return ENOTSOCK; // Socket operation on non-socket

	// Destination address required
	case WSAEDESTADDRREQ:
		return EDESTADDRREQ; // Destination address required

	// Protocol not available
	case WSAENOPROTOOPT:
		return ENOPROTOOPT; // Protocol not available

	// Socket type not supported
	case WSAESOCKTNOSUPPORT:
		return ESOCKTNOSUPPORT; // Socket type not supported

	// Protocol family not supported
	case WSAEPFNOSUPPORT:
		return EPFNOSUPPORT; // Protocol family not supported

	// Oplock not granted
	case ERROR_OPLOCK_NOT_GRANTED:
		return EACCES; // Permission denied

	// Connection timed out
	case ERROR_TIMEOUT:
		return ETIMEDOUT; // Connection timed out

	// Connection refused
	case ERROR_CONNECTION_UNAVAIL:
		return ECONNREFUSED; // Connection refused

	// Host is down
	case WSAEHOSTDOWN:
		return EHOSTDOWN; // Host is down

	// Too many references
	case WSAETOOMANYREFS:
		return ETOOMANYREFS; // Too many references: cannot splice

	// Stale file handle
	case ERROR_FILE_CORRUPT:
	case ERROR_DISK_CORRUPT:
		return ESTALE; // Stale file handle

	// Default: return generic error
	default: return EINVAL; // Invalid argument (as a generic fallback)
	}
	return EINVAL;
}

static DWORD s_defaultAppContainerCaps[] = {
	SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER,
	SECURITY_CAPABILITY_PICTURES_LIBRARY,
	SECURITY_CAPABILITY_VIDEOS_LIBRARY,
	SECURITY_CAPABILITY_MUSIC_LIBRARY,
	SECURITY_CAPABILITY_DOCUMENTS_LIBRARY,
	SECURITY_CAPABILITY_SHARED_USER_CERTIFICATES,
	SECURITY_CAPABILITY_REMOVABLE_STORAGE,
};

static const KNOWNFOLDERID *s_knownFoldersToAllow[] = {&FOLDERID_Profile, &FOLDERID_Public};

struct StaticInit {
	StaticInit() {
		initResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

		DWORD dwIsAppContainer = 0;
		DWORD dwReturnLength = 0;
		if (GetTokenInformation(GetCurrentThreadEffectiveToken(), TokenIsAppContainer,
					&dwIsAppContainer, sizeof(dwIsAppContainer), &dwReturnLength)) {
			isAppContainer = (dwIsAppContainer != 0);
		}

		::setlocale(LC_ALL, "*.UTF8");

		_setmode(_fileno(stderr), O_BINARY);
		_setmode(_fileno(stdout), O_BINARY);
		_setmode(_fileno(stdin), O_BINARY);

		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

		acquireEffectiveIds();
	}

	~StaticInit() {
		if (!SUCCEEDED(initResult)) {
			CoUninitialize();
		}

		if (containerId) {
			FreeSid(containerId);
		}
	}

	void acquireEffectiveIds() {
		wchar_t tempPath[MAX_PATH];
		wchar_t tempFileName[MAX_PATH];

		UINT uniqueNum = 0;

		if (GetTempPathW(MAX_PATH, tempPath) == 0) {
			return;
		}

		if (GetTempFileNameW(tempPath, L"euidgid", uniqueNum, tempFileName) == 0) {
			return;
		}

		// Open the file with FILE_FLAG_DELETE_ON_CLOSE
		HANDLE hTempFile = CreateFileW(tempFileName, GENERIC_READ | GENERIC_WRITE,
				0, // No sharing
				NULL,
				OPEN_EXISTING, // The file was created by GetTempFileName
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);

		if (hTempFile == INVALID_HANDLE_VALUE) {
			return;
		}

		PSID owner_sid = nullptr;
		PSID group_sid = nullptr;

		if (GetSecurityInfo(hTempFile, SE_FILE_OBJECT,
					OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, &owner_sid, &group_sid,
					NULL, NULL, NULL)
				== ERROR_SUCCESS) {
			euid = platform::getRid(owner_sid);
			egid = platform::getRid(group_sid);
		}

		// The file is automatically deleted when the handle is closed.
		CloseHandle(hTempFile);
	}

	HRESULT initResult;
	PSID containerId = nullptr;
	bool isAppContainer = false;

	uint32_t euid = 0;
	uint32_t egid = 0;
};

static StaticInit s_staticInit __attribute__((used));

uint32_t getEffectiveFileUid() { return s_staticInit.euid; }
uint32_t getEffectiveFileGid() { return s_staticInit.egid; }

bool isAppContainer() { return s_staticInit.isAppContainer; }

void destroyAppContainer(StringView appConfigBundleName) {
	char16_t profileName[64] = {0};
	unicode::toUtf16(profileName, 63, appConfigBundleName);

	auto hr = ::DeleteAppContainerProfile((const wchar_t *)profileName);
	if (!SUCCEEDED(hr)) {
		log::vpwarn(__SPRT_LOCATION, "rt-libc",
				"Fail to delete app profile: ", appConfigBundleName);
	}
}

static bool allowNamedObjectAccess(PSID appContainerSid, PWSTR name, SE_OBJECT_TYPE type,
		ACCESS_MASK accessMask) {
	PACL oldAcl, newAcl = nullptr;
	DWORD status;
	EXPLICIT_ACCESSW access;
	do {
		access.grfAccessMode = GRANT_ACCESS;
		access.grfAccessPermissions = accessMask;
		access.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
		access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		access.Trustee.pMultipleTrustee = nullptr;
		access.Trustee.ptstrName = (PWSTR)appContainerSid;
		access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		access.Trustee.TrusteeType = TRUSTEE_IS_GROUP;

		status = ::GetNamedSecurityInfoW(name, type, DACL_SECURITY_INFORMATION, nullptr, nullptr,
				&oldAcl, nullptr, nullptr);
		if (status != ERROR_SUCCESS) {
			return false;
		}

		status = ::SetEntriesInAclW(1, &access, oldAcl, &newAcl);
		if (status != ERROR_SUCCESS) {
			return false;
		}

		status = ::SetNamedSecurityInfoW(name, type, DACL_SECURITY_INFORMATION, nullptr, nullptr,
				newAcl, nullptr);
		if (status != ERROR_SUCCESS) {
			break;
		}
	} while (false);

	if (newAcl) {
		::LocalFree(newAcl);
	}

	return status == ERROR_SUCCESS;
}

bool initAppContainer(StringView appConfigBundleName, StringView appConfigName) {
	char16_t profileName[64] = {0};
	char16_t publicName[512] = {0};
	const wchar_t *desc = L"Stappler Application";

	unicode::toUtf16(profileName, 63, appConfigBundleName);
	unicode::toUtf16(publicName, 511, appConfigName);

	HRESULT hr = ::CreateAppContainerProfile((const wchar_t *)profileName,
			(const wchar_t *)publicName, desc, nullptr, 0, &s_staticInit.containerId);
	if (!SUCCEEDED(hr)) {
		if (hr == E_ACCESSDENIED) {
			log::vpwarn(__SPRT_LOCATION, "rt-libc",
					"Fail to create temporary profile: E_ACCESSDENIED");
		} else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
			hr = ::DeriveAppContainerSidFromAppContainerName((const wchar_t *)profileName,
					&s_staticInit.containerId);
			if (!SUCCEEDED(hr)) {
				log::vpwarn(__SPRT_LOCATION, "rt-libc",
						"Fail to create temporary profile: ERROR_ALREADY_EXISTS");
			}
			return true;
		} else if (hr == E_INVALIDARG) {
			log::vpwarn(__SPRT_LOCATION, "rt-libc",
					"Fail to create temporary profile: E_INVALIDARG");
		}
		return false;
	} else {
		wchar_t *commonDirPath = nullptr;
		for (auto &it : s_knownFoldersToAllow) {
			SHGetKnownFolderPath(*it, 0, nullptr, &commonDirPath);
			if (commonDirPath) {
				allowNamedObjectAccess(s_staticInit.containerId, commonDirPath, SE_FILE_OBJECT,
						FILE_GENERIC_READ);
				CoTaskMemFree(commonDirPath);
				commonDirPath = nullptr;
			}
		}
		return true;
	}
}

bool runSelfInContainer(int &resultCode) {
	// run self in app container
	constexpr auto capsCount = sizeof(s_defaultAppContainerCaps) / sizeof(DWORD);

	SID_AND_ATTRIBUTES capsAttrs[capsCount];
	SID_IDENTIFIER_AUTHORITY authority = SECURITY_APP_PACKAGE_AUTHORITY;

	int i = 0;
	for (auto &it : s_defaultAppContainerCaps) {
		if (!AllocateAndInitializeSid(&authority, SECURITY_BUILTIN_CAPABILITY_RID_COUNT,
					SECURITY_CAPABILITY_BASE_RID, it, 0, 0, 0, 0, 0, 0, &capsAttrs[i].Sid)) {
			log::vpwarn(__SPRT_LOCATION, "rt-libc", "Fail to allocate capability SID");
		}
		capsAttrs[i].Attributes = SE_GROUP_ENABLED;
		++i;
	}

	// Run self in container
	SECURITY_CAPABILITIES sc;
	sc.AppContainerSid = s_staticInit.containerId;
	sc.Capabilities = nullptr;
	sc.CapabilityCount = 0;
	sc.Reserved = 0;
	sc.Capabilities = capsAttrs;
	sc.CapabilityCount = capsCount;

	STARTUPINFOEXW si;
	memset(&si, 0, sizeof(STARTUPINFOEXW));
	si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(PROCESS_INFORMATION));

	SIZE_T AttributesSize;
	InitializeProcThreadAttributeList(NULL, 1, NULL, &AttributesSize);
	si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			AttributesSize);
	InitializeProcThreadAttributeList(si.lpAttributeList, 1, NULL, &AttributesSize);

	if (!::UpdateProcThreadAttribute(si.lpAttributeList, 0,
				PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &sc, sizeof(SECURITY_CAPABILITIES),
				nullptr, nullptr)) {
		log::vperror(__SPRT_LOCATION, "rt-libc",
				"Fail to update proc attributes for AppContainer: ",
				status::lastErrorToStatus(GetLastError()));
		return false;
	}

	wchar_t fullpath[32'768] = {0};
	fullpath[GetModuleFileNameW(NULL, fullpath, 32'768)] = 0;

	auto commandLine = GetCommandLineW();

	BOOL created = ::CreateProcessW(fullpath, commandLine, nullptr, nullptr, TRUE,
			EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, (LPSTARTUPINFOW)&si, &pi);

	if (created) {
		WaitForSingleObject(pi.hProcess, INFINITE);

		DWORD code = 0;
		GetExitCodeProcess(pi.hProcess, &code);

		resultCode = code;
	} else {
		auto lastError = GetLastError();
		log::vperror(__SPRT_LOCATION, "rt-libc",
				"Fail to create AppContainer process: ", status::lastErrorToStatus(lastError));
		resultCode = -1'024;
	}

	DeleteProcThreadAttributeList(si.lpAttributeList);
	for (auto &it : capsAttrs) { FreeSid(it.Sid); }

	return false;
}

bool getAppPath(const callback<void(StringView)> &cb) {
	wchar_t fullpath[32'768] = {0};
	auto n = GetModuleFileNameW(NULL, fullpath, 32'768);

	if (n == 0) {
		return false;
	}

	unicode::toUtf8([&](StringView str) {
		auto len =
				__sprt_fpath_to_posix(str.data(), str.size(), (char *)str.data(), str.size() + 1);
		cb(StringView(str.data(), len));
	}, WideStringView((char16_t *)fullpath, n));
	return true;
}

bool getHomePath(const callback<void(StringView)> &cb) {
	PWSTR pPath = NULL;
	// Use FOLDERID_Documents to get the path to the Documents folder
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &pPath);
	if (SUCCEEDED(hr)) {
		unicode::toUtf8([&](StringView str) {
			auto len = __sprt_fpath_to_posix(str.data(), str.size(), (char *)str.data(),
					str.size() + 1);
			cb(StringView(str.data(), len));
		}, WideStringView((char16_t *)pPath));
		CoTaskMemFree(pPath); // Free memory allocated by the function
		return true;
	}
	return false;
}

bool getMachineId(const callback<void(StringView)> &cb) {
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey)
			!= ERROR_SUCCESS) {
		return false;
	}

	DWORD type;
	DWORD dataSize = 0;
	// Call RegGetValue once to get the buffer size
	LONG result =
			RegGetValueW(hKey, nullptr, L"MachineGuid", RRF_RT_REG_SZ, &type, nullptr, &dataSize);

	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Allocate a buffer of the required size
	auto wbuf = __sprt_typed_malloca(wchar_t, dataSize + 1);
	result = RegGetValueW(hKey, nullptr, L"MachineGuid", RRF_RT_REG_SZ, &type, wbuf, &dataSize);

	if (result == ERROR_SUCCESS) {
		unicode::toUtf8(cb, WideStringView((char16_t *)wbuf, dataSize));
	}

	RegCloseKey(hKey);

	__sprt_freea(wbuf);
	return false;
}

} // namespace sprt::platform

#undef interface

#include <sprt/runtime/filesystem/lookup.h>
#include <sprt/runtime/filesystem/filepath.h>
#include "private/SPRTFilesystem.h"

namespace sprt::filesystem::detail {

struct KnownFolderInfo {
	const KNOWNFOLDERID *folder;
	LocationCategory category;
	LookupFlags flags;
};

static KnownFolderInfo s_defaultKnownFolders[] = {
	KnownFolderInfo{&FOLDERID_AppDataDesktop, LocationCategory::UserDesktop, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_Desktop, LocationCategory::UserDesktop, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDesktop, LocationCategory::UserDesktop, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Pictures, LocationCategory::UserPictures, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicPictures, LocationCategory::UserPictures, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Videos, LocationCategory::UserVideos, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicVideos, LocationCategory::UserVideos, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Music, LocationCategory::UserMusic, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicMusic, LocationCategory::UserMusic, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Downloads, LocationCategory::UserDownload, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDownloads, LocationCategory::UserDownload, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Documents, LocationCategory::UserDocuments, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_PublicDocuments, LocationCategory::UserDocuments,
		LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Profile, LocationCategory::UserHome, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_Public, LocationCategory::UserHome, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_Fonts, LocationCategory::Fonts, LookupFlags::Shared},
	KnownFolderInfo{&FOLDERID_InternetCache, LocationCategory::CommonData, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_LocalAppData, LocationCategory::CommonData, LookupFlags::Private},
	KnownFolderInfo{&FOLDERID_RoamingAppData, LocationCategory::CommonData, LookupFlags::Public},
	KnownFolderInfo{&FOLDERID_ProgramData, LocationCategory::CommonData, LookupFlags::Shared},
};

StringView _readEnvExt(memory::pool_t *pool, StringView key) {
	if (key == "EXEC_DIR") {
		return filepath::root(platform::getExecPath()).pdup(pool);
	} else if (key == "CWD") {
		StringView ret;
		getCurrentDir([&](StringView path) { ret = path.pdup(pool); });
		return ret;
	} else {
		char *buf = nullptr;
		size_t size = 0;
		if (_dupenv_s(&buf, &size, key.data()) == 0) {
			return StringView(buf, size).pdup(pool);
		}
	}
	return StringView();
}

static void processKnownDir(LookupData &data, const KnownFolderInfo &info, IKnownFolder *dir) {
	KNOWNFOLDER_DEFINITION def;
	dir->GetFolderDefinition(&def);

	wchar_t *pathAppWide = nullptr;

	auto dirFlagsAppWide = KF_FLAG_DONT_UNEXPAND | KF_FLAG_NO_ALIAS
			| KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET | KF_FLAG_CREATE;

	dir->GetPath(dirFlagsAppWide, &pathAppWide);

	if (pathAppWide) {
		unicode::toUtf8([&](StringView uPath) {
			auto len = __sprt_fpath_to_posix(uPath.data(), uPath.size(), (char *)uPath.data(),
					uPath.size() + 1);


			auto &res = data._resourceLocations[toInt(info.category)];

			res.paths.emplace_back(LocationInfo{
				.path = StringView(uPath.data(), len).pdup(data._pool),
				.lookupType = info.flags,
				.locationFlags = LocationFlags::Locateable,
				.interface = getDefaultInterface(),
			});
			res.init = true;
		}, WideStringView((const char16_t *)pathAppWide));

		CoTaskMemFree(pathAppWide);
	}
}

static void defineAppPathFromCommon(LookupData &data, StringView bundleName) {
	// init with CommonData and CommonCache paths
	auto makeLocation = [&](LocationCategory cat, StringView root, StringView subname) {
		auto &res = data._resourceLocations[toInt(cat)];
		filepath::merge([&](StringView path) {
			res.paths.emplace_back(LocationInfo{
				.path = path.pdup(data._pool),
				.lookupType = LookupFlags::Private | LookupFlags::Writable,
				.locationFlags = LocationFlags::Locateable,
				.interface = getDefaultInterface(),
			});
		}, root, bundleName, subname);
	};

	auto commonData = &data._resourceLocations[toInt(LocationCategory::CommonData)];
	auto commonCache = &data._resourceLocations[toInt(LocationCategory::CommonCache)];

	if (commonCache->paths.empty()) {
		commonCache = commonData;
	}

	makeLocation(LocationCategory::AppData, commonData->paths.front().path, "Data");
	makeLocation(LocationCategory::AppConfig, commonData->paths.front().path, "Config");
	makeLocation(LocationCategory::AppState, commonData->paths.front().path, "State");
	makeLocation(LocationCategory::AppCache, commonCache->paths.front().path, "Cache");
	makeLocation(LocationCategory::AppRuntime, commonCache->paths.front().path, "Runtime");
}

static memory::dynstring getAppContainerPath(PSID sid) {
	memory::dynstring ret;
	PWSTR str = nullptr, path = nullptr;
	::ConvertSidToStringSidW(sid, &str);

	if (SUCCEEDED(::GetAppContainerFolderPath(str, &path))) {
		unicode::toUtf8([&](StringView str) {
			ret.resize(str.size() + 1);
			ret.resize(__sprt_fpath_to_posix(str.data(), str.size(), ret.data(), ret.size()));
		}, WideStringView((const char16_t *)path));
		::CoTaskMemFree(path);
	}
	::LocalFree(str);
	return ret;
}

static memory::dynstring s_containerPath;

void _initSystemPaths(LookupData &data) {
	auto exeecPath = platform::getExecPath();
	auto defaultInterface = getDefaultInterface();

	CLSID _CLSID_KnownFolderManager;
	CLSIDFromString(L"4df0c730-df9d-4ae3-9153-aa6b82e9795a", &_CLSID_KnownFolderManager);

	IID _IID_IKnownFolderManager;
	IIDFromString(L"8BE2D872-86AA-4d47-B776-32CCA40C7018", &_IID_IKnownFolderManager);

	IKnownFolderManager *manager = nullptr;
	auto hr = CoCreateInstance(_CLSID_KnownFolderManager, nullptr, CLSCTX_INPROC_SERVER,
			_IID_IKnownFolderManager, (void **)&manager);
	if (SUCCEEDED(hr)) {
		IKnownFolder *pKnownFolder = nullptr;
		for (auto &it : s_defaultKnownFolders) {
			hr = manager->GetFolder(*it.folder, &pKnownFolder);
			if (SUCCEEDED(hr)) {
				processKnownDir(data, it, pKnownFolder);
				pKnownFolder->Release();
			}
		}
		manager->Release();
	}

	auto &appConfig = getAppConfig();

	if (!appConfig.bundlePath.empty()) {
		auto &bundledLoc = data._resourceLocations[toInt(LocationCategory::Bundled)];

		appConfig.bundlePath.split<StringView::Chars<':'>>([&](StringView str) {
			auto value = readVariable(data._pool, str);
			if (!value.empty()) {
				bundledLoc.paths.emplace_back(LocationInfo{
					value,
					LookupFlags::Private,
					LocationFlags::Locateable,
					defaultInterface,
				});
			}
		});
	}

	auto pathEnv = __sprt_getenv("PATH");
	if (pathEnv) {
		auto &res = data._resourceLocations[toInt(LocationCategory::Exec)];
		StringView(pathEnv).split<StringView::Chars<':'>>([&](StringView value) {
			res.paths.emplace_back(LocationInfo{
				value.pdup(data._pool),
				LookupFlags::Shared,
				LocationFlags::Locateable,
				defaultInterface,
			});
		});
		res.init = true;
	}

	if (appConfig.pathScheme == AppLocationScheme::ExecutableRelative) {
		auto rootPath = filepath::root(exeecPath);
		data.initAppPaths(rootPath);
	} else if (appConfig.pathScheme == AppLocationScheme::SystemRelative) {
		defineAppPathFromCommon(data, appConfig.bundleName);
	} else if (appConfig.pathScheme == AppLocationScheme::ContainerRelative) {
		// first, determine app container path, then - set paths within it

		s_containerPath = getAppContainerPath(sprt::platform::s_staticInit.containerId);

		if (!s_containerPath.empty()) {
			data.initAppPaths(s_containerPath);
		} else {
			defineAppPathFromCommon(data, appConfig.bundleName);
		}
	} else if (appConfig.pathScheme == AppLocationScheme::ForceContainer) {
		// CommonData and CommonCache already within container
		DWORD returnLength = 0;
		TOKEN_APPCONTAINER_INFORMATION info = {nullptr};
		::GetTokenInformation(::GetCurrentThreadEffectiveToken(), TokenAppContainerSid, &info,
				sizeof(TOKEN_APPCONTAINER_INFORMATION), &returnLength);
		if (info.TokenAppContainer) {
			s_containerPath = getAppContainerPath(info.TokenAppContainer);
		} else {
			s_containerPath = getAppContainerPath(sprt::platform::s_staticInit.containerId);
		}

		if (!s_containerPath.empty()) {
			data.initAppPaths(s_containerPath);
		} else {
			defineAppPathFromCommon(data, appConfig.bundleName);
		}
	}
}

void _termSystemPaths(LookupData &data) { }

} // namespace sprt::filesystem::detail

#endif
