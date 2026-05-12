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

typedef void *HANDLE;
typedef HANDLE *PHANDLE;

typedef HANDLE HMODULE, HINSTANCE, HKEY, *PHMODULE;

typedef void VOID, *PVOID, *LPVOID;

typedef char CHAR, *PCHAR;
typedef unsigned char BYTE, *PBYTE, *LPBYTE, UCHAR, *PUCHAR;

typedef unsigned short WORD, *PWORD;
typedef unsigned long DWORD, *PDWORD, *LPDWORD;
typedef unsigned __int64 DWORDLONG, *PDWORDLONG;
typedef unsigned __int64 DWORD_PTR, *PDWORD_PTR;
typedef unsigned __int64 DWORD64, *PDWORD64;


typedef unsigned short USHORT, *PUSHORT;
typedef unsigned int UINT, *PUINT;
typedef unsigned long ULONG, *PULONG;
typedef unsigned __int64 ULONGLONG, *PULONGLONG;
typedef unsigned __int64 ULONG64, *PULONG64;
typedef unsigned __int64 ULONG_PTR, *PULONG_PTR, UINT_PTR, *PUINT_PTR;

typedef signed short SHORT, *PSHORT;
typedef signed int INT, *PINT;
typedef signed long LONG, *PLONG;
typedef signed __int64 LONGLONG, *PLONGLONG;
typedef signed __int64 LONG64, *PLONG64, QWORD, *PQWORD;
typedef signed __int64 LONG_PTR, *PLONG_PTR, INT_PTR, *PINT_PTR;

typedef float FLOAT, *PFLOAT;
typedef double DOUBLE, *PDOUBLE;

/* BOOL type */
typedef int BOOL;

/* BOOLEAN type */
typedef BYTE BOOLEAN;
typedef BOOLEAN *PBOOLEAN;

/* FARPROC - pointer to function returning int and taking variable args */
typedef int (*FARPROC)(void);


// clang-format off
#if defined(__cplusplus)
typedef wchar_t WCHAR, *PWCHAR;
#else
/* Pre-C++11: need to define it ourselves */
typedef unsigned short WCHAR, *PWCHAR;
#endif
// clang-format on

typedef WCHAR *PWSTR, *LPWSTR, *LPWCH, *PWCH;
typedef const WCHAR *PCWSTR, *LPCWSTR, *LPCWCH, *PCWCH;

typedef char *PSTR, *LPSTR;
typedef const char *PCSTR, *LPCSTR;

typedef const void *PCVOID, *LPCVOID;

typedef __SPRT_ID(size_t) SIZE_T, *PSIZE_T;

typedef char CCHAR, *PCCHAR;
typedef short CSHORT, *PCSHORT;
typedef ULONG CLONG, *PCLONG;

typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

typedef DWORD HRESULT;
typedef DWORD NTSTATUS;
typedef DWORD LSTATUS;

typedef DWORD ACCESS_MASK;
typedef ACCESS_MASK REGSAM;

typedef WORD ATOM;

typedef DWORD SECURITY_INFORMATION, *PSECURITY_INFORMATION;

typedef SHORT VARIANT_BOOL;
typedef unsigned short VARTYPE;

typedef WCHAR *BSTR;

typedef struct _PROC_THREAD_ATTRIBUTE_LIST *PPROC_THREAD_ATTRIBUTE_LIST,
		*LPPROC_THREAD_ATTRIBUTE_LIST;


#endif // SPRT_WRAPPERS_WINDOWS_BASIC_TYPES_H_
