#ifndef RUNTIME_INCLUDE_SPRT_COMPAT_IDN2_H_
#define RUNTIME_INCLUDE_SPRT_COMPAT_IDN2_H_

// The libidn2 C API, as implemented by the runtime's own UTS-46 engine
// (runtime/src/idn/SPRuntimeIdn2Api.cpp).
//
// This exists so that code written against libidn2 - cURL, above all - links
// against the engine without a source change and without a second IDNA
// implementation in the binary. It is the ABI only: for new code use
// <sprt/runtime/utils/idn.h>, which reports errors as a Status and does not make
// the caller free the result.
//
// The header must stay a plain C header that includes no sprt header of its own: the
// toolchains install it into each sysroot as `idn2.h` for third-party configure
// scripts to find, and whatever includes it there has no sprt include path. It does
// read the runtime's build-mode macros - see the linkage block below.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Linkage. These functions are not a separate libidn2 - they are part of the
// runtime, defined with SPRT_GLOBAL in runtime/src/idn/SPRuntimeIdn2Api.cpp - so a
// declaration has to be decorated the way the runtime was actually built, and the
// conditions below are the ones <sprt/c/bits/__sprt_def.h> keys SPRT_GLOBAL on:
//
//   SPRT_BUILD_RUNTIME + SPRT_BUILD_SHARED_RUNTIME - building sprt.dll: dllexport
//   SPRT_BUILD_RUNTIME                             - building the static runtime: nothing
//   SPRT_SHARED_RUNTIME                            - consumer of sprt.dll: dllimport
//   none of them                                   - consumer of the static runtime: nothing
//
// They are repeated by hand rather than taken from __sprt_def.h because this header is
// installed into each sysroot as a plain C `idn2.h` and gets included by third-party code
// that has no sprt include path.
//
// libidn2's own scheme, which used to be copied here verbatim, describes a different
// build entirely and answered wrong on Windows in both directions: `_MSC_VER &&
// !IDN2_STATIC` is true for every clang windows-msvc target, so the default (static)
// sysroot had cURL call through __imp_idn2_*, which the static runtime does not define,
// while the runtime's own definitions silently acquired dllexport from the mismatched
// declaration (-Winconsistent-dllimport) and landed in the export directory of every
// image that linked them. IDN2_BUILDING, IDN2_STATIC and HAVE_VISIBILITY are therefore
// not consulted at all; predefine _IDN2_API to override.
#ifndef _IDN2_API

#if defined(_WIN32) || defined(_WIN64)

#ifdef __GNUC__
#define __IDN2_DLLEXPORT __attribute__((dllexport))
#define __IDN2_DLLIMPORT __attribute__((dllimport))
#else
#define __IDN2_DLLEXPORT __declspec(dllexport)
#define __IDN2_DLLIMPORT __declspec(dllimport)
#endif

#if defined(SPRT_BUILD_RUNTIME) && defined(SPRT_BUILD_SHARED_RUNTIME)
#define _IDN2_API __IDN2_DLLEXPORT
#elif !defined(SPRT_BUILD_RUNTIME) && defined(SPRT_SHARED_RUNTIME)
#define _IDN2_API __IDN2_DLLIMPORT
#else
#define _IDN2_API
#endif

#elif defined(__GNUC__) || defined(__clang__)
// Every non-Windows branch of SPRT_GLOBAL is default visibility, including the runtime's
// own build: a caller compiled with -fvisibility=hidden must still bind to the definition
// across an ELF/Mach-O shared object boundary.
#define _IDN2_API __attribute__((__visibility__("default")))
#else
#define _IDN2_API
#endif

#endif /* _IDN2_API */

#define IDN2_VERSION "2.3.2-xenolith"
// No digit separator: this header is C, where ' is not one (the shim this replaces spelled
// the same value 0x02030002) and clang reports an unterminated character constant.
#define IDN2_VERSION_NUMBER 0x02030002
#define IDN2_LABEL_MAX_LENGTH 63
#define IDN2_DOMAIN_MAX_LENGTH 255

typedef enum {
	IDN2_NFC_INPUT = 1,
	IDN2_ALABEL_ROUNDTRIP = 2,
	IDN2_TRANSITIONAL = 4,
	IDN2_NONTRANSITIONAL = 8,
	IDN2_ALLOW_UNASSIGNED = 16,
	IDN2_USE_STD3_ASCII_RULES = 32,
	IDN2_NO_TR46 = 64,
	IDN2_NO_ALABEL_ROUNDTRIP = 128
} idn2_flags;

/* IDNA2008 with UTF-8 encoded inputs. */

extern _IDN2_API int idn2_lookup_u8(const uint8_t *src, uint8_t **lookupname, int flags);
extern _IDN2_API int idn2_lookup_ul(const char *src, char **lookupname, int flags);
extern _IDN2_API int idn2_to_unicode_8z8z(const char *src, char **lookupname, int flags);

typedef enum {
	IDN2_OK = 0,
	IDN2_MALLOC = -100,
	IDN2_NO_CODESET = -101,
	IDN2_ICONV_FAIL = -102,
	IDN2_ENCODING_ERROR = -200,
	IDN2_NFC = -201,
	IDN2_PUNYCODE_BAD_INPUT = -202,
	IDN2_PUNYCODE_BIG_OUTPUT = -203,
	IDN2_PUNYCODE_OVERFLOW = -204,
	IDN2_TOO_BIG_DOMAIN = -205,
	IDN2_TOO_BIG_LABEL = -206,
	IDN2_INVALID_ALABEL = -207,
	IDN2_UALABEL_MISMATCH = -208,
	IDN2_INVALID_FLAGS = -209,
	IDN2_NOT_NFC = -300,
	IDN2_2HYPHEN = -301,
	IDN2_HYPHEN_STARTEND = -302,
	IDN2_LEADING_COMBINING = -303,
	IDN2_DISALLOWED = -304,
	IDN2_CONTEXTJ = -305,
	IDN2_CONTEXTJ_NO_RULE = -306,
	IDN2_CONTEXTO = -307,
	IDN2_CONTEXTO_NO_RULE = -308,
	IDN2_UNASSIGNED = -309,
	IDN2_BIDI = -310,
	IDN2_DOT_IN_LABEL = -311,
	IDN2_INVALID_TRANSITIONAL = -312,
	IDN2_INVALID_NONTRANSITIONAL = -313,
	IDN2_ALABEL_ROUNDTRIP_FAILED = -314,
} idn2_rc;

typedef enum {
	IDNA_SUCCESS = IDN2_OK,
	IDNA_STRINGPREP_ERROR = IDN2_ENCODING_ERROR,
	IDNA_PUNYCODE_ERROR = IDN2_PUNYCODE_BAD_INPUT,
	IDNA_CONTAINS_NON_LDH = IDN2_ENCODING_ERROR,
	IDNA_CONTAINS_LDH = IDNA_CONTAINS_NON_LDH,
	IDNA_CONTAINS_MINUS = IDN2_ENCODING_ERROR,
	IDNA_INVALID_LENGTH = IDN2_DISALLOWED,
	IDNA_NO_ACE_PREFIX = IDN2_ENCODING_ERROR,
	IDNA_ROUNDTRIP_VERIFY_ERROR = IDN2_ENCODING_ERROR,
	IDNA_CONTAINS_ACE_PREFIX = IDN2_ENCODING_ERROR,
	IDNA_ICONV_ERROR = IDN2_ENCODING_ERROR,
	IDNA_MALLOC_ERROR = IDN2_MALLOC,
	IDNA_DLOPEN_ERROR = IDN2_MALLOC
} Idna_rc;

extern _IDN2_API const char *idn2_strerror(int rc);
extern _IDN2_API const char *idn2_strerror_name(int rc);
extern _IDN2_API const char *idn2_check_version(const char *req_version);

extern _IDN2_API void idn2_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* RUNTIME_INCLUDE_SPRT_COMPAT_IDN2_H_ */
