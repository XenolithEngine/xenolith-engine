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

#ifndef SPRT_WRAPPERS_WINDOWS_BASIC_TYPES_H_
#define SPRT_WRAPPERS_WINDOWS_BASIC_TYPES_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_size_t.h>

/*
 * Windows calling convention macros
 */

/* Calling convention */
#define WINAPI                      __stdcall
#define CALLBACK                    WINAPI

/*
 * Windows API basic type definitions (windef.h subset)
 */

/* ============================================================ */
/* Basic Types                                                  */
/* ============================================================ */

typedef char CCHAR;
typedef void VOID;
typedef void *HANDLE;
typedef HANDLE *PHANDLE;
typedef void *HMODULE;
typedef HMODULE *PHMODULE;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef DWORD *PDWORD;
typedef BYTE *LPBYTE;

typedef signed long LONG;
typedef unsigned int UINT;

typedef LONG HRESULT;
typedef unsigned long ULONG;
typedef unsigned short USHORT;
typedef unsigned char UCHAR;
typedef unsigned long long ULONGLONG;
typedef unsigned long long ULONG64;
typedef long long LONGLONG;
typedef unsigned __int64 ULONG_PTR;
typedef unsigned __int64 DWORD_PTR;
typedef unsigned long *PULONG;
typedef void *PVOID;
typedef ULONGLONG QWORD;
typedef short SHORT;
typedef float FLOAT;
typedef double DOUBLE;

typedef unsigned __int64 *PULONG_PTR;

typedef unsigned __int64 DWORD64, *PDWORD64;

/* FARPROC - pointer to function returning int and taking variable args */
typedef int (*FARPROC)(void);

/* BOOL type */
typedef int BOOL;

/* BOOLEAN type */
typedef BYTE BOOLEAN;

// clang-format off
#if __cplusplus >= 201103L || !defined(__cplusplus)
/* C++11 or C99+: wchar_t is a built-in type */
typedef wchar_t WCHAR;
#else
/* Pre-C++11: need to define it ourselves */
typedef unsigned short WCHAR;
#endif
// clang-format on

typedef wchar_t *LPWSTR;
typedef const wchar_t *LPCWSTR;
typedef const wchar_t *PCWSTR;

typedef char *LPSTR;
typedef const char *LPCSTR;

typedef void *LPVOID;
typedef const void *LPCVOID;

typedef DWORD *LPDWORD;

typedef __SPRT_ID(size_t) SIZE_T;

#endif // SPRT_WRAPPERS_WINDOWS_BASIC_TYPES_H_
