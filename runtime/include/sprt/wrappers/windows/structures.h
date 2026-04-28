/**
 * Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 **/

#ifndef SPRT_WRAPPERS_WINDOWS_STRUCTURES_H_
#define SPRT_WRAPPERS_WINDOWS_STRUCTURES_H_

#include <sprt/wrappers/windows/basic_types.h>

/*
 * Windows common structures (windef.h, winnt.h subset)
 */

/* OVERLAPPED structure for async I/O */
typedef struct _OVERLAPPED {
	ULONG_PTR Internal;
	ULONG_PTR InternalHigh;
	union {
		struct {
			DWORD Offset;
			DWORD OffsetHigh;
		};
		PVOID Pointer;
	};
	HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

/* LARGE_INTEGER union for file positioning */
typedef union _LARGE_INTEGER {
	struct {
		DWORD LowPart;
		LONG HighPart;
	} DUMMYSTRUCTNAME;
	struct {
		DWORD LowPart;
		LONG HighPart;
	} u;
	LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

/* FILETIME structure */
typedef struct _FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

/* Security attributes structure */
typedef struct _SECURITY_ATTRIBUTES {
	DWORD nLength;
	LPVOID lpSecurityDescriptor;
	BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

#endif // SPRT_WRAPPERS_WINDOWS_STRUCTURES_H_
