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

#ifdef __SPRT_AS_STD
namespace std {
using namespace sprt::_cstdlib_types;
}
#endif

#endif // __cplusplus
#endif // defined(__cplusplus) || __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)


// C++ functions

#if defined(__cplusplus) && __STDC_HOSTED__ == 1
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

// export functions globally
#if __STDC_HOSTED__ == 1 && !defined(__SPRT_BUILD)
using namespace sprt::_cstdlib;
#endif

#ifdef __SPRT_AS_STD
namespace std {
using namespace sprt::_cstdlib;
}
#endif
#endif // __cplusplus


// C functions

#if __STDC_HOSTED__ == 0 || (!defined(__SPRT_BUILD) && !defined(__cplusplus))

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

__SPRT_C_FUNC unsigned short _byteswap_ushort(unsigned short _Number) __SPRT_NOEXCEPT;
__SPRT_C_FUNC unsigned long _byteswap_ulong(unsigned long _Number) __SPRT_NOEXCEPT;
__SPRT_C_FUNC __SPRT_ID(uint64_t) _byteswap_uint64(__SPRT_ID(uint64_t) _Number) __SPRT_NOEXCEPT;

__SPRT_C_FUNC int qsort_s(void *a, size_t b, size_t c,
		int (*cmp)(void *, const void *, const void *), void *ctx) __SPRT_NOEXCEPT;

__SPRT_C_FUNC int getenv_s(size_t *ret, char *buf, __SPRT_ID(rsize_t) bufSize,
		char const *name) __SPRT_NOEXCEPT;

__SPRT_C_FUNC size_t _msize(void *) __SPRT_NOEXCEPT;

__SPRT_C_FUNC __SPRT_ID(wchar_t) * _wgetenv(const __SPRT_ID(wchar_t) * varname) __SPRT_NOEXCEPT;

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

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDLIB_H_
