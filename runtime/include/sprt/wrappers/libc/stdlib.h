#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDLIB_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDLIB_H_

#include <sprt/c/__sprt_stdlib.h>
#include <sprt/c/__sprt_wchar.h>
#include <sprt/c/bits/__sprt_uint64_t.h>
#include <sprt/wrappers/libc/stddef.h>


// Definitions

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifndef NULL
#define NULL __SPRT_NULL
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifdef __sprt_malloca
#define _malloca(Sz) __sprt_malloca(Sz)
#define _freea(Ptr) __sprt_freea(Ptr)
#endif

#define RAND_MAX  __SPRT_RAND_MAX

#define MB_CUR_MAX __SPRT_ID(__ctype_get_mb_cur_max)()

#endif // __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)

#ifndef _countof
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif


#if __STDC_HOSTED__ == 0
__SPRT_C_FUNC size_t __ctype_get_mb_cur_max() __SPRT_NOEXCEPT;
#endif


// Types

#if defined(__cplusplus) || __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifdef __cplusplus
namespace sprt {
inline namespace _cstdlib_types {
#endif

typedef __SPRT_ID(div_t) div_t;
typedef __SPRT_ID(ldiv_t) ldiv_t;
typedef __SPRT_ID(lldiv_t) lldiv_t;

#ifdef __cplusplus
}
}

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
using namespace sprt::_cstdlib_types;
#endif

#endif // __cplusplus
#endif // defined(__cplusplus) || __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)


// C++ functions
//
// Suppressed entirely UNDER libc++ (_LIBCPP_VERSION): libc++ owns the hosted C++
// surface, and these sprt::_cstdlib copies — inline-namespace members of `sprt` —
// would otherwise clash for any code written inside `namespace sprt`:
//  - overload sets duplicated against math.h's sprt::_cmath (abs -> ambiguous);
//  - non-overloadable names found by scope lookup in `sprt` AND by ADL at global
//    scope as the distinct extern-C definition (free(dirent*) -> ambiguous).
// Nothing references the sprt::-qualified spellings of these functions; the
// extern-C block below provides the C names libc++ builds on.
#if defined(__cplusplus) && __STDC_HOSTED__ == 1 && !defined(_LIBCPP_VERSION)
namespace sprt {
inline namespace _cstdlib {

#define SPRT_FUNC_BEGIN SPRT_FORCEINLINE
#define SPRT_FUNC_END SPRT_NOEXCEPT
#define SPRT_FUNC_END_EXCEPT
#define SPRT_FUNC_BODY 1

#include <sprt/wrappers/libc/stdlib_impl.h>

#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

} // namespace _cstdlib
} // namespace sprt

// export functions globally — but NOT when reached through libc++, which owns the
// C++ abs/div overloads and re-exports its own globally; a second, equal-rank set
// makes abs()/div() ambiguous (same issue as <math.h>). See math.h for the model.
#if __STDC_HOSTED__ == 1 && !defined(__SPRT_BUILD) && !defined(_LIBCPP_VERSION)
using namespace sprt::_cstdlib;
#endif
#endif // __cplusplus


// C functions
//
// Also emitted (as extern-C) in the hosted C++ path UNDER libc++: libc++ supplies the
// abs/div overloads but relies on the C library for the C names (labs/llabs/...), and
// its _LIBCPP_PREFERRED_OVERLOADs dominate the C-linkage decls without ambiguity.
#if __STDC_HOSTED__ == 0 || (!defined(__SPRT_BUILD) && !defined(__cplusplus)) \
		|| (defined(_LIBCPP_VERSION) && !defined(__SPRT_BUILD))

__SPRT_BEGIN_DECL

#define SPRT_FUNC_BEGIN SPRT_UMBRELLA_FUNC
#define SPRT_FUNC_END SPRT_UMBRELLA_END
#define SPRT_FUNC_END_EXCEPT SPRT_UMBRELLA_END_EXCEPT
#define SPRT_FUNC_BODY SPRT_UMBRELLA_REQUIRED

#include <sprt/wrappers/libc/stdlib_impl.h>

#undef SPRT_FUNC_BEGIN
#undef SPRT_FUNC_END
#undef SPRT_FUNC_BODY

__SPRT_END_DECL

#endif


#if !defined(__SPRT_BUILD) || __STDC_HOSTED__ == 0
__SPRT_BEGIN_DECL

/*
	WARNING: use aligned_free to safely free aligned memory.
	It's MSVC requirement, but safe to follow this rule everywhere
*/
SPRT_UMBRELLA_FUNC
int posix_memalign(void **ptr, size_t size, size_t align) SPRT_UMBRELLA_END_EXCEPT
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_posix_memalign(ptr, size, align);
}
#endif

__SPRT_END_DECL
#endif


// CRT extensions

__SPRT_BEGIN_DECL


// MSVC CRT byte-swap intrinsics. Gated to Windows because _byteswap_ulong operates
// on a 32-bit unsigned long, which only holds under the LLP64 model (every Windows
// arch); on LP64 unsigned long is 64-bit and the signature would not match MSVC.
// The implementation lives in libc_wrapper/c/stdlib/byteswap.cc. (The previous
// `__SPRT_ARCH_ID == __SPRT_ARCH_NAME_X86_64` guard compared a number to the token
// x86_64_sprt, so it was false on every arch -- breaking aarch64-windows.)
#if SPRT_WINDOWS

unsigned short _byteswap_ushort(unsigned short _Number);
unsigned long _byteswap_ulong(unsigned long _Number);
__SPRT_ID(uint64_t) _byteswap_uint64(__SPRT_ID(uint64_t) _Number);

#endif

SPRT_API int qsort_s(void *a, size_t b, size_t c, int (*cmp)(void *, const void *, const void *),
		void *ctx) __SPRT_NOEXCEPT;

SPRT_API int getenv_s(size_t *ret, char *buf, __SPRT_ID(rsize_t) bufSize,
		char const *name) __SPRT_NOEXCEPT;

SPRT_API size_t _msize(void *) __SPRT_NOEXCEPT;

SPRT_API __SPRT_ID(wchar_t) * _wgetenv(const __SPRT_ID(wchar_t) * varname) __SPRT_NOEXCEPT;

SPRT_API int _putenv_s(const char *name, const char *value) __SPRT_NOEXCEPT;

SPRT_API int _wputenv_s(const __SPRT_ID(wchar_t) * name,
		const __SPRT_ID(wchar_t) * value) __SPRT_NOEXCEPT;

SPRT_API extern __SPRT_ID(wchar_t) * *_wenviron;

SPRT_API extern int __argc;
SPRT_API extern char **__argv;

#if SPRT_WINDOWS

// MSVC CRT crash-UI controls. sprt has no CRT abort dialog / report-fault UI, so
// these accept and ignore their flags and report the previous value (0).
// Inline no-ops: there is no runtime backend to link against.
#define _WRITE_ABORT_MSG 0x1
#define _CALL_REPORTFAULT 0x2

#define _OUT_TO_DEFAULT 0
#define _OUT_TO_STDERR 1
#define _OUT_TO_MSGBOX 2
#define _REPORT_ERRMODE 3

static inline unsigned _set_abort_behavior(unsigned _Flags, unsigned _Mask) {
	(void)_Flags;
	(void)_Mask;
	return 0;
}

static inline int _set_error_mode(int _Mode) {
	(void)_Mode;
	return 0;
}

// MSVC's radix conversions. Code that formats an integer by hand tends to reach for these
// behind a _MSC_VER test and never see the portable branch next to it
//
// Digits come out least-significant first and are reversed in place. Following MSVC: a
// radix other than 10 makes the value unsigned, so only base 10 can produce a sign; a bad
// argument yields EINVAL and a buffer too small yields ERANGE with the buffer emptied.
// The values are spelled out because <errno.h> is not pulled in this early.
static inline int __sprt_radix_to_str(unsigned long long _Value, char *_Buf, size_t _Size,
		int _Radix, int _Negative) {
	char _Digits[66];
	size_t _Len = 0;
	size_t _I;

	if (!_Buf || _Size == 0 || _Radix < 2 || _Radix > 36) {
		if (_Buf && _Size) {
			_Buf[0] = '\0';
		}
		return 22; // EINVAL
	}

	do {
		unsigned _Digit = (unsigned)(_Value % (unsigned long long)_Radix);
		_Digits[_Len++] = (char)(_Digit < 10 ? '0' + _Digit : 'a' + (_Digit - 10));
		_Value /= (unsigned long long)_Radix;
	} while (_Value);

	if (_Negative) {
		_Digits[_Len++] = '-';
	}

	if (_Len + 1 > _Size) {
		_Buf[0] = '\0';
		return 34; // ERANGE
	}

	for (_I = 0; _I < _Len; ++_I) { _Buf[_I] = _Digits[_Len - 1 - _I]; }
	_Buf[_Len] = '\0';
	return 0;
}

static inline int _i64toa_s(long long _Value, char *_Buf, size_t _Size, int _Radix) {
	if (_Radix == 10 && _Value < 0) {
		return __sprt_radix_to_str((unsigned long long)(-(_Value + 1)) + 1u, _Buf, _Size, _Radix,
				1);
	}
	return __sprt_radix_to_str((unsigned long long)_Value, _Buf, _Size, _Radix, 0);
}

static inline int _ui64toa_s(unsigned long long _Value, char *_Buf, size_t _Size, int _Radix) {
	return __sprt_radix_to_str(_Value, _Buf, _Size, _Radix, 0);
}

static inline int _itoa_s(int _Value, char *_Buf, size_t _Size, int _Radix) {
	if (_Radix == 10) {
		return _i64toa_s((long long)_Value, _Buf, _Size, _Radix);
	}
	return __sprt_radix_to_str((unsigned long long)(unsigned int)_Value, _Buf, _Size, _Radix, 0);
}

static inline int _ltoa_s(long _Value, char *_Buf, size_t _Size, int _Radix) {
	if (_Radix == 10) {
		return _i64toa_s((long long)_Value, _Buf, _Size, _Radix);
	}
	return __sprt_radix_to_str((unsigned long long)(unsigned long)_Value, _Buf, _Size, _Radix, 0);
}

static inline int _ultoa_s(unsigned long _Value, char *_Buf, size_t _Size, int _Radix) {
	return __sprt_radix_to_str((unsigned long long)_Value, _Buf, _Size, _Radix, 0);
}

// The pre-Secure-CRT spellings, which take no size and so cannot report anything.
static inline char *_itoa(int _Value, char *_Buf, int _Radix) {
	_itoa_s(_Value, _Buf, (size_t)-1, _Radix);
	return _Buf;
}

static inline char *_ltoa(long _Value, char *_Buf, int _Radix) {
	_ltoa_s(_Value, _Buf, (size_t)-1, _Radix);
	return _Buf;
}

static inline char *_ultoa(unsigned long _Value, char *_Buf, int _Radix) {
	_ultoa_s(_Value, _Buf, (size_t)-1, _Radix);
	return _Buf;
}

static inline char *_i64toa(long long _Value, char *_Buf, int _Radix) {
	_i64toa_s(_Value, _Buf, (size_t)-1, _Radix);
	return _Buf;
}

static inline char *_ui64toa(unsigned long long _Value, char *_Buf, int _Radix) {
	_ui64toa_s(_Value, _Buf, (size_t)-1, _Radix);
	return _Buf;
}

#endif

__SPRT_END_DECL

#define _strtol_l strtol_l
#define _strtoll_l strtoll_l
#define _strtoul_l strtoul_l
#define _strtoull_l strtoull_l
#define _strtof_l strtof_l
#define _strtod_l strtod_l
#define _strtold_l strtold_l
#define _strtoi64_l strtoll_l
#define _strtoui64_l strtoull_l

// MSVC declares errno in <stdlib.h> (not only in <errno.h>); mirror that on Windows so
// third-party code that reaches errno through <stdlib.h> alone — e.g. llvm compiler-rt's
// profile lib on its _WIN32 path — resolves it. Same expansion as <errno.h>.
#if SPRT_WINDOWS
#include <sprt/c/__sprt_errno.h>
#ifndef errno
#define errno __sprt_errno
#endif
#endif

// clang-format off
#if SPRT_WINDOWS && defined(_LIBCPP_VERSION)
#ifndef _sys_nerr
#define _sys_nerr 4096
#endif
#endif
// clang-format on

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDLIB_H_
