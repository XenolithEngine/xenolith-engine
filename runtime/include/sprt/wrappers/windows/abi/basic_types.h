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

#ifndef SPRT_WRAPPERS_WINDOWS_ABI_BASIC_TYPES_H_
#define SPRT_WRAPPERS_WINDOWS_ABI_BASIC_TYPES_H_

#include <sprt/c/bits/__sprt_def.h>
#include <sprt/c/bits/__sprt_size_t.h>
#include <sprt/c/bits/__sprt_int64_t.h>
#include <sprt/c/bits/__sprt_uint64_t.h>
#include <sprt/wrappers/windows/__sprt_config.h>
#include <sprt/wrappers/windows/__sprt_intrin.h>

/*
 * Windows calling convention macros
 */

#ifdef _WIN32
#define __SPRT_WINAPI   __stdcall
#else
#define __SPRT_WINAPI
#endif
#define __SPRT_CALLBACK   __SPRT_WINAPI
#define __SPRT_NTAPI
#define __SPRT_BASETYPES
#define __SPRT_APIENTRY

#define WINAPI     __SPRT_WINAPI
#define CALLBACK   __SPRT_CALLBACK
#define NTAPI      __SPRT_NTAPI
#define BASETYPES  __SPRT_BASETYPES
#define APIENTRY   __SPRT_APIENTRY

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
typedef __uint64 UINT64, *PUINT64;
typedef __uint64 ULONG_PTR, *PULONG_PTR, UINT_PTR, *PUINT_PTR;

typedef signed short SHORT, *PSHORT;
typedef signed int INT, *PINT;
typedef signed long LONG, *PLONG, *LPLONG;
typedef __sint64 LONGLONG, *PLONGLONG;
typedef __sint64 LONG64, *PLONG64, QWORD, *PQWORD;
typedef __sint64 INT64, *PINT64;
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

typedef char CCHAR, *PCCHAR;
typedef const char *LPCCH, *PCCH;
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

typedef ULONG_PTR KAFFINITY;
typedef KAFFINITY *PKAFFINITY;

typedef struct _PROC_THREAD_ATTRIBUTE_LIST *PPROC_THREAD_ATTRIBUTE_LIST,
		*LPPROC_THREAD_ATTRIBUTE_LIST;

#define __SPRT_DECLARE_HANDLE(name) typedef HANDLE name
#define DECLARE_HANDLE(name) __SPRT_DECLARE_HANDLE(name)

#if __SPRT_HAS_BUILTIN(__builtin_offsetof)
#define __SPRT_OFFSETOF(type, field)    __builtin_offsetof(type, field)
#else
#define __SPRT_OFFSETOF(type, field)    ((LONG_PTR)&(((type *)0)->field))
#endif

#define __SPRT_FIELD_OFFSET(type, field)    ((LONG)__SPRT_OFFSETOF(type, field))
#define __SPRT_UFIELD_OFFSET(type, field)    ((ULONG)__SPRT_OFFSETOF(type, field))


#ifdef UNICODE
typedef WCHAR TCHAR, *PTCHAR, *LPTCHAR;
#else
typedef CHAR TCHAR, *PTCHAR, *LPTCHAR;
#endif

#if !defined(_M_ARM64EC)

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64
// ARM64 has no __faststorefence (an x86-64 intrinsic); the full-system data memory
// barrier is __dmb(_ARM64_BARRIER_SY), matching MSVC's <winnt.h> MemoryBarrier().
#define __SPRT_MEMORY_BARRIER() __dmb(0xF)
#else
#define __SPRT_MEMORY_BARRIER __faststorefence
#endif

#define MemoryBarrier __SPRT_MEMORY_BARRIER

#endif

#define __SPRT_MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define __SPRT_MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define __SPRT_LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define __SPRT_HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define __SPRT_LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define __SPRT_HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))

#define __SPRT_DECLSPEC_ALIGN(x)   __declspec(align(x))
#define DECLSPEC_ALIGN(x)   __SPRT_DECLSPEC_ALIGN(x)


#endif // SPRT_WRAPPERS_WINDOWS_ABI_BASIC_TYPES_H_
