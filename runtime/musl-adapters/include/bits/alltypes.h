#ifndef RUNTIME_MUSL_ADAPTERS_INCLUDE_BITS_ALLTYPES_H_
#define RUNTIME_MUSL_ADAPTERS_INCLUDE_BITS_ALLTYPES_H_

#include <sprt/c/cross/__sprt_sysid.h>
#include <sprt/c/cross/__sprt_locale.h>
#include <sprt/c/__sprt_stdint.h>
#include <sprt/c/__sprt_float.h>
#include <sprt/c/bits/__sprt_ssize_t.h>

typedef __SPRT_ID(intmax_t) intmax_t;
typedef __SPRT_ID(intptr_t) intptr_t;
typedef __SPRT_ID(int8_t) int8_t;
typedef __SPRT_ID(int16_t) int16_t;
typedef __SPRT_ID(int32_t) int32_t;
typedef __SPRT_ID(int64_t) int64_t;

typedef __SPRT_ID(uintmax_t) uintmax_t;
typedef __SPRT_ID(uintptr_t) uintptr_t;
typedef __SPRT_ID(uint8_t) uint8_t;
typedef __SPRT_ID(uint16_t) uint16_t;
typedef __SPRT_ID(uint32_t) uint32_t;
typedef __SPRT_ID(uint64_t) uint64_t;

typedef __SPRT_ID(float_t) float_t;
typedef __SPRT_ID(double_t) double_t;

typedef __SPRT_ID(size_t) size_t;
typedef __SPRT_ID(off_t) off_t;
typedef __SPRT_ID(ssize_t) ssize_t;
typedef __SPRT_ID(locale_t) locale_t;
typedef __SPRT_ID(uid_t) uid_t;
typedef __SPRT_ID(pid_t) pid_t;
typedef __SPRT_ID(gid_t) gid_t;

// Musl expects 32-bit wchar_t
typedef __SPRT_ID(uint32_t) wchar_t;
typedef unsigned long wctype_t;

typedef struct __mbstate_t {
	unsigned __opaque1, __opaque2;
} mbstate_t;

typedef struct _IO_FILE FILE;

typedef __builtin_va_list __isoc_va_list;

#endif // RUNTIME_MUSL_ADAPTERS_INCLUDE_BITS_ALLTYPES_H_
