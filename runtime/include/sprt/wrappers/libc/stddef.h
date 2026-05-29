#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDDEF_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDDEF_H_

#include <sprt/c/bits/__sprt_ssize_t.h>
#include "sprt/c/bits/__sprt_wint_t.h"
#include <sprt/c/__sprt_stddef.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifndef NULL
#define NULL __SPRT_NULL
#endif

#ifndef offsetof
#define offsetof(type, member) __SPRT_offsetof(type, member)
#endif
#endif


#if defined(__cplusplus) || __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifdef __cplusplus

namespace sprt {
inline namespace _cstddef {

typedef decltype(nullptr) nullptr_t;

enum class byte : unsigned char {
};

#else

// clang-format off
#if __STDC_VERSION__ >= 202311L
typedef typeof(nullptr) nullptr_t;
#endif
// clang-format on

typedef __SPRT_ID(wchar_t) wchar_t;

#endif

typedef __SPRT_ID(max_align_t) max_align_t;
typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(wint_t) wint_t;
typedef __SPRT_ID(ptrdiff_t) ptrdiff_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(off_t) off64_t;
typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(rsize_t) rsize_t;
typedef __SPRT_ID(errno_t) errno_t;

#ifdef __cplusplus
}
}

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
using namespace sprt::_cstddef;
#endif

#endif // __cplusplus
#endif // defined(__cplusplus) || __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)

#endif // CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDDEF_H_
