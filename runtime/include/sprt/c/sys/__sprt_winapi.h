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

#ifndef CORE_RUNTIME_INCLUDE_C_SYS___SPRT_WINAPI_H_
#define CORE_RUNTIME_INCLUDE_C_SYS___SPRT_WINAPI_H_

#include <sprt/c/cross/__sprt_config.h>
#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/bits/__sprt_uint32_t.h>
#include <sprt/c/bits/__sprt_int64_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>
#include <sprt/c/bits/__sprt_uintptr_t.h>
#include <sprt/c/bits/__sprt_intptr_t.h>
#include <sprt/c/bits/__sprt_size_t.h>

/*
	This defines some WinAPI umbrella methods available for use in the Stappler Runtime

	Methods are defined in libc style, without WinAPI-specific variable types.
*/

// clang-format off
#define __SPRT_WINAPI_INFINITE 0xFFFFFFFF
// clang-format on

#define __SPRT_INVALID_HANDLE_VALUE ((void *)-1)

typedef struct __SPRT_ID(overlapped) {
	__SPRT_ID(uintptr_t) Internal;
	__SPRT_ID(uintptr_t) InternalHigh;
	union {
		struct {
			__SPRT_ID(uint32_t) Offset;
			__SPRT_ID(uint32_t) OffsetHigh;
		};
		void *Pointer;
	};

	void *hEvent;
} __SPRT_ID(overlapped);

typedef struct __SPRT_ID(overlapped_entry) {
	__SPRT_ID(uintptr_t) lpCompletionKey;
	__SPRT_ID(overlapped) * lpOverlapped;
	__SPRT_ID(uintptr_t) Internal;
	__SPRT_ID(uint32_t) dwNumberOfBytesTransferred;
} __SPRT_ID(overlapped_entry);


typedef struct __SPRT_ID(security_attributes) {
	__SPRT_ID(uint32_t) nLength;
	void *lpSecurityDescriptor;
	int bInheritHandle;
} __SPRT_ID(security_attributes);

typedef struct __SPRT_ID(reason_context) {
	unsigned long Version;
	__SPRT_ID(uint32_t) Flags;
	union {
		struct {
			void *LocalizedReasonModule;
			unsigned long LocalizedReasonId;
			unsigned long ReasonStringCount;
			char16_t *ReasonStrings;
		} Detailed;

		char16_t *SimpleReasonString;
	} Reason;
} __SPRT_ID(reason_context);

typedef struct __SPRT_ID(winmsg) {
	void *hwnd;
	unsigned message;
	__SPRT_ID(uintptr_t) wParam;
	__SPRT_ID(intptr_t) lParam;
	__SPRT_ID(uint32_t) time;
	long ptX;
	long ptY;
} __SPRT_ID(winmsg);

typedef void (*__SPRT_ID(timer_apc_routine))(void *, unsigned long, unsigned long);

#if __SPRT_CONFIG_HAVE_WINAPI || __SPRT_CONFIG_DEFINE_UNAVAILABLE_FUNCTIONS

__SPRT_BEGIN_DECL

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(pid_t) __SPRT_ID(GetCurrentThreadId)();

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(uint32_t) __SPRT_ID(GetLastError)();

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(WaitOnAddress)(volatile void *Address, void *CompareAddress,
		__SPRT_ID(size_t) AddressSize, __SPRT_ID(uint32_t) dwMilliseconds);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void __SPRT_ID(WakeByAddressSingle)(void *Address);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void __SPRT_ID(WakeByAddressAll)(void *Address);

// clang-format off
#define __SPRT_LOCALE_NAME_USER_DEFAULT            nullptr
#define __SPRT_LOCALE_NAME_INVARIANT               u""
#define __SPRT_LOCALE_NAME_SYSTEM_DEFAULT          u"!x-sys-default-locale"

#define __SPRT_LCMAP_LOWERCASE           0x00000100  // lower case letters
#define __SPRT_LCMAP_UPPERCASE           0x00000200  // UPPER CASE LETTERS
#define __SPRT_LCMAP_TITLECASE           0x00000300  // Title Case Letters
#define __SPRT_LCMAP_SORTKEY             0x00000400  // WC sort key (normalize)
#define __SPRT_LCMAP_BYTEREV             0x00000800  // byte reversal
#define __SPRT_LCMAP_HIRAGANA            0x00100000  // map katakana to hiragana
#define __SPRT_LCMAP_KATAKANA            0x00200000  // map hiragana to katakana
#define __SPRT_LCMAP_HALFWIDTH           0x00400000  // map double byte to single byte
#define __SPRT_LCMAP_FULLWIDTH           0x00800000  // map single byte to double byte
#define __SPRT_LCMAP_LINGUISTIC_CASING   0x01000000  // use linguistic rules for casing
#define __SPRT_LCMAP_SIMPLIFIED_CHINESE  0x02000000  // map traditional chinese to simplified chinese
#define __SPRT_LCMAP_TRADITIONAL_CHINESE 0x04000000  // map simplified chinese to traditional chinese

#define __SPRT_NORM_IGNORECASE           0x00000001  // ignore case
#define __SPRT_NORM_IGNORENONSPACE       0x00000002  // ignore nonspacing chars
#define __SPRT_NORM_IGNORESYMBOLS        0x00000004  // ignore symbols

#define __SPRT_LINGUISTIC_IGNORECASE     0x00000010  // linguistically appropriate 'ignore case'
#define __SPRT_LINGUISTIC_IGNOREDIACRITIC 0x00000020  // linguistically appropriate 'ignore nonspace'

#define __SPRT_NORM_IGNOREKANATYPE       0x00010000  // ignore kanatype
#define __SPRT_NORM_IGNOREWIDTH          0x00020000  // ignore width
#define __SPRT_NORM_LINGUISTIC_CASING    0x08000000  // use linguistic rules for casing

#define __SPRT_SORT_STRINGSORT           0x00001000  // use string sort method
#define __SPRT_SORT_DIGITSASNUMBERS      0x00000008  // use digits as numbers sort method

#define __SPRT_CSTR_LESS_THAN            1           // string 1 less than string 2
#define __SPRT_CSTR_EQUAL                2           // string 1 equal to string 2
#define __SPRT_CSTR_GREATER_THAN         3           // string 1 greater than string 2
// clang-format on

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_LCMapString)(const char16_t *lpLocaleName, __SPRT_ID(uint32_t) dwMapFlags,
		const char16_t *lpSrcStr, int cchSrc, char16_t *lpDestStr, int cchDest);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_CompareString)(const char16_t *lpLocaleName, __SPRT_ID(uint32_t) dwMapFlags,
		const char16_t *lpString1, int cchCount1, const char16_t *lpString2, int cchCount2);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_GetUserDefaultLocaleName)(char16_t *wlocale, int bufSize);


#define __SPRT_IDN_ALLOW_UNASSIGNED        0x01  // Allow unassigned "query" behavior per RFC 3454
#define __SPRT_IDN_USE_STD3_ASCII_RULES    0x02  // Enforce STD3 ASCII restrictions for legal characters
#define __SPRT_IDN_EMAIL_ADDRESS           0x04  // Enable EAI algorithmic fallback for email local parts behavior
#define __SPRT_IDN_RAW_PUNYCODE            0x08  // Disable validation and mapping of punycode.

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_IdnToAscii)(__SPRT_ID(uint32_t) dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_IdnToUnicode)(__SPRT_ID(uint32_t) dwFlags, const char16_t *lpUnicodeCharStr,
		int cchUnicodeChar, char16_t *lpASCIICharStr, int cchASCIIChar);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(uint32_t)
		__SPRT_ID(_GetCurrentDirectory)(__SPRT_ID(uint32_t) nBufferLength, char16_t *lpBuffer);


// ReportEventAsCompletion
//  - associates Event with I/O Completion Port and requests a completion packet when signalled
//  - parameters order modelled after PostQueuedCompletionStatus
//  - parameters: hIOCP - handle to I/O Completion Port
//                hEvent - handle to Event, Semaphore, Thread or Process
//                       - NOTE: Mutex is not supported, it makes no sense in this context
//                dwNumberOfBytesTransferred - user-specified value, provided back by GetQueuedCompletionStatus(Ex)
//                dwCompletionKey - user-specified value, provided back by GetQueuedCompletionStatus(Ex)
//                lpOverlapped - user-specified value, provided back by GetQueuedCompletionStatus(Ex)
//  - returns: I/O Packet HANDLE for the association
//             NULL on failure, call GetLastError () for details
//              - ERROR_INVALID_PARAMETER -
//              - ERROR_INVALID_HANDLE - provided hEvent is not supported by this API
//              - otherwise internal HRESULT is forwarded
//  - call CloseHandle to free the returned I/O Packet HANDLE when no longer needed
//
__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void *__SPRT_ID(_ReportEventAsCompletion)(void *hIOCP, void *hEvent,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		void *lpOverlapped);

// RestartEventCompletion
//  - use to wait again, after the event completion was consumed by GetQueuedCompletionStatus(Ex)
//  - parameters: hPacket - is HANDLE returned by 'ReportEventAsCompletion'
//                hIOCP - handle to I/O Completion Port
//                hEvent - handle to the Event object
//                oEntry - pointer to data provided back by GetQueuedCompletionStatus(Ex)
//  - returns: TRUE on success
//             FALSE on failure, call GetLastError () for details (TBD)
//
__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(
		_RestartEventCompletion)(void *hPacket, void *hIOCP, void *hEvent, const void **oEntry);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_RestartEventCompletion2)(void *hPacket, void *hIOCP, void *hEvent,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		void *lpOverlapped);

// CancelEventCompletion
//  - stops the Event from completing into the I/O Completion Port
//  - call CloseHandle to free the I/O Packet HANDLE when no longer needed
//  - parameters: hPacket - is HANDLE returned by 'ReportEventAsCompletion'
//                cancel - if TRUE, if already signalled, the completion packet is removed from queue
//  - returns: TRUE on success
//             FALSE on failure, call GetLastError () for details (TBD)
//
__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_CancelEventCompletion)(void *hPacket, int cancel);

#define __SPRT_PM_NOREMOVE         0x0000
#define __SPRT_PM_REMOVE           0x0001
#define __SPRT_PM_NOYIELD          0x0002

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_PeekMessage)(__SPRT_ID(winmsg) * lpMsg, void *hWnd, unsigned wMsgFilterMin,
		unsigned wMsgFilterMax, unsigned wRemoveMsg);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_TranslateMessage)(const __SPRT_ID(winmsg) * lpMsg);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_DispatchMessage)(const __SPRT_ID(winmsg) * lpMsg);


#define __SPRT_MWMO_WAITALL        0x0001
#define __SPRT_MWMO_ALERTABLE      0x0002
#define __SPRT_MWMO_INPUTAVAILABLE 0x0004

#define __SPRT_QS_KEY              0x0001
#define __SPRT_QS_MOUSEMOVE        0x0002
#define __SPRT_QS_MOUSEBUTTON      0x0004
#define __SPRT_QS_POSTMESSAGE      0x0008
#define __SPRT_QS_TIMER            0x0010
#define __SPRT_QS_PAINT            0x0020
#define __SPRT_QS_SENDMESSAGE      0x0040
#define __SPRT_QS_HOTKEY           0x0080
#define __SPRT_QS_ALLPOSTMESSAGE   0x0100
#define __SPRT_QS_RAWINPUT         0x0400
#define __SPRT_QS_TOUCH            0x0800
#define __SPRT_QS_POINTER          0x1000
#define __SPRT_QS_MOUSE (__SPRT_QS_MOUSEMOVE|__SPRT_QS_MOUSEBUTTON)
#define __SPRT_QS_INPUT (__SPRT_QS_MOUSE|__SPRT_QS_KEY|__SPRT_QS_RAWINPUT|__SPRT_QS_TOUCH|__SPRT_QS_POINTER)
#define __SPRT_QS_ALLEVENTS (__SPRT_QS_INPUT|__SPRT_QS_POSTMESSAGE|__SPRT_QS_TIMER|__SPRT_QS_PAINT|__SPRT_QS_HOTKEY)
#define __SPRT_QS_ALLINPUT (__SPRT_QS_INPUT|__SPRT_QS_POSTMESSAGE|__SPRT_QS_TIMER|__SPRT_QS_PAINT|__SPRT_QS_HOTKEY|__SPRT_QS_SENDMESSAGE)

#define __SPRT_WAIT_TIMEOUT 258L

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API __SPRT_ID(uint32_t) __SPRT_ID(_MsgWaitForMultipleObjectsEx)(__SPRT_ID(uint32_t) nCount,
		void *const *pHandles, __SPRT_ID(uint32_t) dwMilliseconds, __SPRT_ID(uint32_t) dwWakeMask,
		__SPRT_ID(uint32_t) dwFlags);


__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void *__SPRT_ID(_CreateIoCompletionPort)(void *FileHandle, void *ExistingCompletionPort,
		__SPRT_ID(uintptr_t) CompletionKey, __SPRT_ID(uint32_t) NumberOfConcurrentThreads);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_GetQueuedCompletionStatusEx)(void *CompletionPort,
		__SPRT_ID(overlapped_entry) * lpCompletionPortEntries, unsigned long ulCount,
		unsigned long *ulNumEntriesRemoved, __SPRT_ID(uint32_t) dwMilliseconds, int fAlertable);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_PostQueuedCompletionStatus)(void *CompletionPort,
		__SPRT_ID(uint32_t) dwNumberOfBytesTransferred, __SPRT_ID(uintptr_t) dwCompletionKey,
		__SPRT_ID(overlapped) * lpOverlapped);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_CloseHandle)(void *hObject);

// clang-format off
#define __SPRT_CREATE_WAITABLE_TIMER_MANUAL_RESET  0x00000001
#define __SPRT_CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002

#define __SPRT_SYNCHRONIZE                      (0x00100000L)
#define __SPRT_STANDARD_RIGHTS_REQUIRED         (0x000F0000L)
// clang-format on

#define __SPRT_TIMER_QUERY_STATE       0x0001
#define __SPRT_TIMER_MODIFY_STATE      0x0002
#define __SPRT_TIMER_ALL_ACCESS (__SPRT_STANDARD_RIGHTS_REQUIRED|__SPRT_SYNCHRONIZE|__SPRT_TIMER_QUERY_STATE|__SPRT_TIMER_MODIFY_STATE)


__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API void *__SPRT_ID(_CreateWaitableTimerEx)(__SPRT_ID(security_attributes) * lpTimerAttributes,
		const char16_t *lpTimerName, __SPRT_ID(uint32_t) dwFlags,
		__SPRT_ID(uint32_t) dwDesiredAccess);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_SetWaitableTimer)(void *hTimer, __SPRT_ID(int64_t) * lpDueTime,
		long lPeriod, __SPRT_ID(timer_apc_routine) pfnCompletionRoutine,
		void *lpArgToCompletionRoutine, int fResume);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_SetWaitableTimerEx)(void *hTimer, __SPRT_ID(int64_t) * lpDueTime,
		long lPeriod, __SPRT_ID(timer_apc_routine) pfnCompletionRoutine,
		void *lpArgToCompletionRoutine, __SPRT_ID(reason_context) * WakeContext,
		unsigned long TolerableDelay);

__SPRT_CONFIG_HAVE_WINAPI_NOTICE
SPRT_API int __SPRT_ID(_CancelWaitableTimer)(void *hTimer);

__SPRT_END_DECL

#endif

#endif
