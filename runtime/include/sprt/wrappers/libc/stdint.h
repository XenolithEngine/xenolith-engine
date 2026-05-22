#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDINT_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_STDINT_H_

#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/__sprt_signal.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#define INT8_C(V) __SPRT_INT8_C(V)
#define INT16_C(V) __SPRT_INT16_C(V)
#define INT32_C(V) __SPRT_INT32_C(V)
#define INT64_C(V) __SPRT_INT64_C(V)
#define UINT8_C(V) __SPRT_UINT8_C(V)
#define UINT16_C(V) __SPRT_UINT16_C(V)
#define UINT32_C(V) __SPRT_UINT32_C(V)
#define UINT64_C(V) __SPRT_UINT64_C(V)

#define INT8_MAX __SPRT_INT8_MAX
#define INT16_MAX __SPRT_INT16_MAX
#define INT32_MAX __SPRT_INT32_MAX
#define INT64_MAX __SPRT_INT64_MAX
#define UINT8_MAX __SPRT_UINT8_MAX
#define UINT16_MAX __SPRT_UINT16_MAX
#define UINT32_MAX __SPRT_UINT32_MAX
#define UINT64_MAX __SPRT_UINT64_MAX

#define INT8_MIN (-1-INT8_MAX)
#define INT16_MIN (-1-INT16_MAX)
#define INT32_MIN (-1-INT32_MAX)
#define INT64_MIN (-1-INT64_MAX)
#define UINT8_MIN 0
#define UINT16_MIN 0
#define UINT32_MIN 0
#define UINT64_MIN 0

#define INTMAX_MIN      __SPRT_INTMAX_MIN
#define INTMAX_MAX      __SPRT_INTMAX_MAX
#define UINTMAX_MAX     __SPRT_UINTMAX_MAX

#define INTPTR_MIN       __SPRT_INTPTR_MIN
#define INTPTR_MAX       __SPRT_INTPTR_MAX
#define UINTPTR_MAX      __SPRT_UINTPTR_MAX

#define PTRDIFF_MIN    __SPRT_PTRDIFF_MIN
#define PTRDIFF_MAX    __SPRT_PTRDIFF_MAX
#define SIZE_MAX       __SPRT_SIZE_MAX

#define WCHAR_MIN       __SPRT_WCHAR_MIN
#define WCHAR_MAX       __SPRT_WCHAR_MAX

#define WINT_MIN        __SPRT_WINT_MIN
#define WINT_MAX        __SPRT_WINT_MAX

#define SIG_ATOMIC_MIN   __SPRT_SIG_ATOMIC_MIN
#define SIG_ATOMIC_MAX   __SPRT_SIG_ATOMIC_MAX
#endif


#if defined(__cplusplus) || __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifdef __cplusplus
namespace sprt {
inline namespace _cstdint {
#endif

typedef __SPRT_ID(intmax_t) intmax_t;
typedef __SPRT_ID(intptr_t) intptr_t;
typedef __SPRT_ID(int8_t) int8_t;
typedef __SPRT_ID(int16_t) int16_t;
typedef __SPRT_ID(int32_t) int32_t;
typedef __SPRT_ID(int64_t) int64_t;
typedef __SPRT_ID(int_fast8_t) int_fast8_t;
typedef __SPRT_ID(int_fast16_t) int_fast16_t;
typedef __SPRT_ID(int_fast32_t) int_fast32_t;
typedef __SPRT_ID(int_fast64_t) int_fast64_t;
typedef __SPRT_ID(int_least8_t) int_least8_t;
typedef __SPRT_ID(int_least16_t) int_least16_t;
typedef __SPRT_ID(int_least32_t) int_least32_t;
typedef __SPRT_ID(int_least64_t) int_least64_t;

typedef __SPRT_ID(uintmax_t) uintmax_t;
typedef __SPRT_ID(uintptr_t) uintptr_t;
typedef __SPRT_ID(uint8_t) uint8_t;
typedef __SPRT_ID(uint16_t) uint16_t;
typedef __SPRT_ID(uint32_t) uint32_t;
typedef __SPRT_ID(uint64_t) uint64_t;
typedef __SPRT_ID(uint_fast8_t) uint_fast8_t;
typedef __SPRT_ID(uint_fast16_t) uint_fast16_t;
typedef __SPRT_ID(uint_fast32_t) uint_fast32_t;
typedef __SPRT_ID(uint_fast64_t) uint_fast64_t;
typedef __SPRT_ID(uint_least8_t) uint_least8_t;
typedef __SPRT_ID(uint_least16_t) uint_least16_t;
typedef __SPRT_ID(uint_least32_t) uint_least32_t;
typedef __SPRT_ID(uint_least64_t) uint_least64_t;

#ifdef __cplusplus
static_assert(sizeof(uint8_t) == 1, "Invalid int length");
static_assert(sizeof(int8_t) == 1, "Invalid int length");
static_assert(sizeof(uint16_t) == 2, "Invalid int length");
static_assert(sizeof(int16_t) == 2, "Invalid int length");
static_assert(sizeof(uint32_t) == 4, "Invalid int length");
static_assert(sizeof(int32_t) == 4, "Invalid int length");
static_assert(sizeof(uint64_t) == 8, "Invalid int length");
static_assert(sizeof(uint64_t) == 8, "Invalid int length");
}
}

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
using namespace sprt::_cstdint;
#endif

#endif // __cplusplus
#endif // defined(__cplusplus) || __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)

#endif
