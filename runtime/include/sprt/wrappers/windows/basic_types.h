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
#include <sprt/c/bits/__sprt_int64_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>

// Some developers expects, that wchar.h functions will be defined when windows headers are included;
// When we build SPRT itself, it should not be defined
#if __STDC_HOSTED__ == 1 && !defined(__SPRT_BUILD)
#include <wchar.h>
#endif

/*
 * Windows calling convention macros
 */

#ifdef _WIN32
#define WINAPI   __stdcall
#else
#define WINAPI
#endif
#define CALLBACK  WINAPI

#define BASETYPES
#define APIENTRY

/*
 * Windows API basic type definitions (windef.h subset)
 */

/* ============================================================ */
/* Basic Types                                                  */
/* ============================================================ */

#ifndef _WIN32
typedef __SPRT_ID(int64_t) __int64;
typedef __SPRT_ID(uint64_t) __uint64;
typedef __SPRT_ID(int64_t) __sint64;
#else
typedef unsigned __int64 __uint64;
typedef signed __int64 __sint64;
#endif


typedef void *HANDLE;
typedef HANDLE *PHANDLE, *LPHANDLE;

typedef HANDLE HMODULE, HINSTANCE, HKEY, *PHMODULE;

typedef void VOID, *PVOID, *LPVOID;

typedef char CHAR, *PCHAR;
typedef unsigned char BYTE, *PBYTE, *LPBYTE, UCHAR, *PUCHAR;

typedef unsigned short WORD, *PWORD;
typedef unsigned long DWORD, *PDWORD, *LPDWORD;
typedef __uint64 DWORDLONG, *PDWORDLONG;
typedef __uint64 DWORD_PTR, *PDWORD_PTR;
typedef __uint64 DWORD64, *PDWORD64;


typedef unsigned short USHORT, *PUSHORT;
typedef unsigned int UINT, *PUINT;
typedef unsigned long ULONG, *PULONG;
typedef __uint64 ULONGLONG, *PULONGLONG;
typedef __uint64 ULONG64, *PULONG64;
typedef __uint64 ULONG_PTR, *PULONG_PTR, UINT_PTR, *PUINT_PTR;

typedef signed short SHORT, *PSHORT;
typedef signed int INT, *PINT;
typedef signed long LONG, *PLONG, *LPLONG;
typedef __sint64 LONGLONG, *PLONGLONG;
typedef __sint64 LONG64, *PLONG64, QWORD, *PQWORD;
typedef __sint64 LONG_PTR, *PLONG_PTR, INT_PTR, *PINT_PTR;

typedef float FLOAT, *PFLOAT;
typedef double DOUBLE, *PDOUBLE;

/* BOOL type */
typedef int BOOL, *LPBOOL;

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

typedef char CCHAR, *PCCHAR, *LPCCH;
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

#define DECLARE_HANDLE(name) typedef HANDLE name

#ifdef __has_builtin
#if __has_builtin(__builtin_offsetof)
#define FIELD_OFFSET(type, field)    ((LONG)__builtin_offsetof(type, field))
#define UFIELD_OFFSET(type, field)    ((ULONG)__builtin_offsetof(type, field))
#endif
#endif

#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field)    ((LONG)(LONG_PTR)&(((type *)0)->field))
#define UFIELD_OFFSET(type, field)    ((ULONG)(LONG_PTR)&(((type *)0)->field))
#endif


#ifdef UNICODE
typedef WCHAR TCHAR, *PTCHAR, *LPTCHAR;
#else
typedef CHAR TCHAR, *PTCHAR, *LPTCHAR;
#endif

__SPRT_C_FUNC void __faststorefence(void);

#pragma intrinsic(__faststorefence)

#if !defined(_M_ARM64EC)

#define MemoryBarrier __faststorefence

#endif

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))

#endif // SPRT_WRAPPERS_WINDOWS_BASIC_TYPES_H_
