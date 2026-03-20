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
		__SPRT_ID(_GetCurrentDirectory)(__SPRT_ID(uint32_t) nBufferLength, char16_t *lpBuffer) {
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
