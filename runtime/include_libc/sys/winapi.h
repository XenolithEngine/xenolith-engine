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

#ifndef CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_
#define CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_

#include <sprt/c/sys/__sprt_winapi.h>

typedef __SPRT_ID(overlapped_entry) overlapped_entry;
typedef __SPRT_ID(winmsg) winmsg;
typedef __SPRT_ID(security_attributes) security_attributes;

#if __SPRT_CONFIG_HAVE_WINAPI || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

#define _WINAPI_INFINITE __SPRT_WINAPI_INFINITE
#define INVALID_HANDLE_VALUE __SPRT_INVALID_HANDLE_VALUE

__SPRT_BEGIN_DECL

SPRT_FORCEINLINE __SPRT_ID(pid_t) _GetCurrentThreadId() { return __sprt_GetCurrentThreadId(); }

SPRT_FORCEINLINE __SPRT_ID(uint32_t) _GetLastError() { return __sprt_GetLastError(); }

SPRT_FORCEINLINE int _WaitOnAddress(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds) {
	return __sprt_WaitOnAddress(Address, CompareAddress, AddressSize, dwMilliseconds);
}

SPRT_FORCEINLINE void _WakeByAddressSingle(void *Address) { __sprt_WakeByAddressSingle(Address); }

SPRT_FORCEINLINE void _WakeByAddressAll(void *Address) { __sprt_WakeByAddressAll(Address); }


#define LOCALE_NAME_USER_DEFAULT __SPRT_LOCALE_NAME_USER_DEFAULT
#define LOCALE_NAME_INVARIANT __SPRT_LOCALE_NAME_INVARIANT
#define LOCALE_NAME_SYSTEM_DEFAULT __SPRT_LOCALE_NAME_SYSTEM_DEFAULT

#define LCMAP_LOWERCASE __SPRT_LCMAP_LOWERCASE
#define LCMAP_UPPERCASE __SPRT_LCMAP_UPPERCASE
#define LCMAP_TITLECASE __SPRT_LCMAP_TITLECASE
#define LCMAP_SORTKEY __SPRT_LCMAP_SORTKEY
#define LCMAP_BYTEREV __SPRT_LCMAP_BYTEREV
#define LCMAP_HIRAGANA __SPRT_LCMAP_HIRAGANA
#define LCMAP_KATAKANA __SPRT_LCMAP_KATAKANA
#define LCMAP_HALFWIDTH __SPRT_LCMAP_HALFWIDTH
#define LCMAP_FULLWIDTH __SPRT_LCMAP_FULLWIDTH
#define LCMAP_LINGUISTIC_CASING __SPRT_LCMAP_LINGUISTIC_CASING
#define LCMAP_SIMPLIFIED_CHINESE __SPRT_LCMAP_SIMPLIFIED_CHINESE
#define LCMAP_TRADITIONAL_CHINESE __SPRT_LCMAP_TRADITIONAL_CHINESE

#define NORM_IGNORECASE __SPRT_NORM_IGNORECASE
#define NORM_IGNORENONSPACE __SPRT_NORM_IGNORENONSPACE
#define NORM_IGNORESYMBOLS __SPRT_NORM_IGNORESYMBOLS

#define LINGUISTIC_IGNORECASE __SPRT_LINGUISTIC_IGNORECASE
#define LINGUISTIC_IGNOREDIACRITIC __SPRT_LINGUISTIC_IGNOREDIACRITIC

#define NORM_IGNOREKANATYPE __SPRT_NORM_IGNOREKANATYPE
#define NORM_IGNOREWIDTH __SPRT_NORM_IGNOREWIDTH
#define NORM_LINGUISTIC_CASING __SPRT_NORM_LINGUISTIC_CASING

#define SORT_STRINGSORT __SPRT_SORT_STRINGSORT
#define SORT_DIGITSASNUMBERS __SPRT_SORT_DIGITSASNUMBERS

#define CSTR_LESS_THAN __SPRT_CSTR_LESS_THAN
#define CSTR_EQUAL __SPRT_CSTR_EQUAL
#define CSTR_GREATER_THAN __SPRT_CSTR_GREATER_THAN

SPRT_FORCEINLINE int _LCMapString(const char16_t *lpLocaleName, __SPRT_ID(uint32_t) dwMapFlags,
		const char16_t *lpSrcStr, int cchSrc, char16_t *lpDestStr, int cchDest) {
	return __sprt__LCMapString(lpLocaleName, dwMapFlags, lpSrcStr, cchSrc, lpDestStr, cchDest);
}

SPRT_FORCEINLINE int _CompareString(const char16_t *lpLocaleName, __SPRT_ID(uint32_t) dwMapFlags,
		const char16_t *lpString1, int cchCount1, const char16_t *lpString2, int cchCount2) {
	return __sprt__CompareString(lpLocaleName, dwMapFlags, lpString1, cchCount1, lpString2,
			cchCount2);
}

SPRT_FORCEINLINE int _GetUserDefaultLocaleName(char16_t *wlocale, int bufSize) {
	return __sprt__GetUserDefaultLocaleName(wlocale, bufSize);
}


#define IDN_ALLOW_UNASSIGNED __SPRT_IDN_ALLOW_UNASSIGNED
#define IDN_USE_STD3_ASCII_RULES __SPRT_IDN_USE_STD3_ASCII_RULES
#define IDN_EMAIL_ADDRESS __SPRT_IDN_EMAIL_ADDRESS
#define IDN_RAW_PUNYCODE __SPRT_IDN_RAW_PUNYCODE

SPRT_FORCEINLINE int _IdnToAscii(__SPRT_ID(uint32_t) dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	return __sprt__IdnToAscii(dwFlags, lpUnicodeCharStr, cchUnicodeChar, lpASCIICharStr,
			cchASCIIChar);
}

SPRT_FORCEINLINE int _IdnToUnicode(__SPRT_ID(uint32_t) dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar) {
	return __sprt__IdnToUnicode(dwFlags, lpUnicodeCharStr, cchUnicodeChar, lpASCIICharStr,
			cchASCIIChar);
}

SPRT_FORCEINLINE __SPRT_ID(uint32_t)
		_GetCurrentDirectory(__SPRT_ID(uint32_t) nBufferLength, char16_t *lpBuffer) {
	return __sprt__GetCurrentDirectory(nBufferLength, lpBuffer);
}

SPRT_FORCEINLINE void *_ReportEventAsCompletion(void *hIOCP, void *hEvent,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		void *lpOverlapped) {
	return __sprt__ReportEventAsCompletion(hIOCP, hEvent, dwNumberOfBytesTransferred,
			dwCompletionKey, lpOverlapped);
}

SPRT_FORCEINLINE int _RestartEventCompletion(void *hPacket, void *hIOCP, void *hEvent,
		const void **oEntry) {
	return __sprt__RestartEventCompletion(hPacket, hIOCP, hEvent, oEntry);
}

SPRT_FORCEINLINE int _RestartEventCompletion2(void *hPacket, void *hIOCP, void *hEvent,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		void *lpOverlapped) {
	return __sprt__RestartEventCompletion2(hPacket, hIOCP, hEvent, dwNumberOfBytesTransferred,
			dwCompletionKey, lpOverlapped);
}

SPRT_FORCEINLINE int _CancelEventCompletion(void *hPacket, int cancel) {
	return __sprt__CancelEventCompletion(hPacket, cancel);
}

#define PM_NOREMOVE __SPRT_PM_NOREMOVE
#define PM_REMOVE __SPRT_PM_REMOVE
#define PM_NOYIELD __SPRT_PM_NOYIELD

SPRT_FORCEINLINE int _PeekMessage(__SPRT_ID(winmsg) * lpMsg, void *hWnd, unsigned wMsgFilterMin,
		unsigned wMsgFilterMax, unsigned wRemoveMsg) {
	return __sprt__PeekMessage(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
}

SPRT_FORCEINLINE int _TranslateMessage(const __SPRT_ID(winmsg) * lpMsg) {
	return __sprt__TranslateMessage(lpMsg);
}

SPRT_FORCEINLINE int _DispatchMessage(const __SPRT_ID(winmsg) * lpMsg) {
	return __sprt__DispatchMessage(lpMsg);
}


#define MWMO_WAITALL __SPRT_MWMO_WAITALL
#define MWMO_ALERTABLE __SPRT_MWMO_ALERTABLE
#define MWMO_INPUTAVAILABLE __SPRT_MWMO_INPUTAVAILABLE

#define QS_KEY __SPRT_QS_KEY
#define QS_MOUSEMOVE __SPRT_QS_MOUSEMOVE
#define QS_MOUSEBUTTON __SPRT_QS_MOUSEBUTTON
#define QS_POSTMESSAGE __SPRT_QS_POSTMESSAGE
#define QS_TIMER __SPRT_QS_TIMER
#define QS_PAINT __SPRT_QS_PAINT
#define QS_SENDMESSAGE __SPRT_QS_SENDMESSAGE
#define QS_HOTKEY __SPRT_QS_HOTKEY
#define QS_ALLPOSTMESSAGE __SPRT_QS_ALLPOSTMESSAGE
#define QS_RAWINPUT __SPRT_QS_RAWINPUT
#define QS_TOUCH __SPRT_QS_TOUCH
#define QS_POINTER __SPRT_QS_POINTER
#define QS_MOUSE __SPRT_QS_MOUSE
#define QS_INPUT __SPRT_QS_INPUT
#define QS_ALLEVENTS __SPRT_QS_ALLEVENTS
#define QS_ALLINPUT __SPRT_QS_ALLINPUT

#define WAIT_TIMEOUT __SPRT_WAIT_TIMEOUT

SPRT_FORCEINLINE __SPRT_ID(uint32_t) _MsgWaitForMultipleObjectsEx(__SPRT_ID(uint32_t) nCount,
		void *const *pHandles, __SPRT_ID(uint32_t) dwMilliseconds, __SPRT_ID(uint32_t) dwWakeMask,
		__SPRT_ID(uint32_t) dwFlags) {
	return __sprt__MsgWaitForMultipleObjectsEx(nCount, pHandles, dwMilliseconds, dwWakeMask,
			dwFlags);
}

SPRT_FORCEINLINE void *_CreateIoCompletionPort(void *FileHandle, void *ExistingCompletionPort,
		__SPRT_ID(uintptr_t) CompletionKey, __SPRT_ID(uint32_t) NumberOfConcurrentThreads) {
	return __sprt__CreateIoCompletionPort(FileHandle, ExistingCompletionPort, CompletionKey,
			NumberOfConcurrentThreads);
}

SPRT_FORCEINLINE int _GetQueuedCompletionStatusEx(void *CompletionPort,
		__SPRT_ID(overlapped_entry) * lpCompletionPortEntries, unsigned long ulCount,
		unsigned long *ulNumEntriesRemoved, __SPRT_ID(uint32_t) dwMilliseconds, int fAlertable) {
	return __sprt__GetQueuedCompletionStatusEx(CompletionPort, lpCompletionPortEntries, ulCount,
			ulNumEntriesRemoved, dwMilliseconds, fAlertable);
}

SPRT_FORCEINLINE int _PostQueuedCompletionStatus(void *CompletionPort,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		__SPRT_ID(overlapped) * lpOverlapped) {
	return __sprt__PostQueuedCompletionStatus(CompletionPort, dwNumberOfBytesTransferred,
			dwCompletionKey, lpOverlapped);
}

SPRT_FORCEINLINE int _CloseHandle(void *hObject) { return __sprt__CloseHandle(hObject); }

#define CREATE_WAITABLE_TIMER_MANUAL_RESET __SPRT_CREATE_WAITABLE_TIMER_MANUAL_RESET
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION __SPRT_CREATE_WAITABLE_TIMER_HIGH_RESOLUTION

#define SYNCHRONIZE __SPRT_SYNCHRONIZE
#define STANDARD_RIGHTS_REQUIRED __SPRT_STANDARD_RIGHTS_REQUIRED

#define TIMER_QUERY_STATE __SPRT_TIMER_QUERY_STATE
#define TIMER_MODIFY_STATE __SPRT_TIMER_MODIFY_STATE
#define TIMER_ALL_ACCESS __SPRT_TIMER_ALL_ACCESS

SPRT_FORCEINLINE void *_CreateWaitableTimerEx(__SPRT_ID(security_attributes) * lpTimerAttributes,
		const char16_t *lpTimerName, __SPRT_ID(uint32_t) dwFlags,
		__SPRT_ID(uint32_t) dwDesiredAccess) {
	return __sprt__CreateWaitableTimerEx(lpTimerAttributes, lpTimerName, dwFlags, dwDesiredAccess);
}

SPRT_FORCEINLINE int _SetWaitableTimer(void *hTimer, __SPRT_ID(int64_t) * lpDueTime, long lPeriod,
		__SPRT_ID(timer_apc_routine) pfnCompletionRoutine, void *lpArgToCompletionRoutine,
		int fResume) {
	return __sprt__SetWaitableTimer(hTimer, lpDueTime, lPeriod, pfnCompletionRoutine,
			lpArgToCompletionRoutine, fResume);
}

SPRT_FORCEINLINE int _SetWaitableTimerEx(void *hTimer, __SPRT_ID(int64_t) * lpDueTime, long lPeriod,
		__SPRT_ID(timer_apc_routine) pfnCompletionRoutine, void *lpArgToCompletionRoutine,
		__SPRT_ID(reason_context) * WakeContext, unsigned long TolerableDelay) {
	return __sprt__SetWaitableTimerEx(hTimer, lpDueTime, lPeriod, pfnCompletionRoutine,
			lpArgToCompletionRoutine, WakeContext, TolerableDelay);
}

SPRT_FORCEINLINE int _CancelWaitableTimer(void *hTimer) {
	return __sprt__CancelWaitableTimer(hTimer);
}

__SPRT_END_DECL

#endif

#endif // CORE_RUNTIME_INCLUDE_LIBC_SYS_WINAPI_H_
