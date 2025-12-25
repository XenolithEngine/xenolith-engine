
#ifndef INCLUDE_IDN2_H_
#define INCLUDE_IDN2_H_

// IDN2 mimic API

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#ifndef _IDN2_API
# if defined IDN2_BUILDING && defined HAVE_VISIBILITY && HAVE_VISIBILITY
#  define _IDN2_API __attribute__((__visibility__("default")))
# elif defined IDN2_BUILDING && defined _MSC_VER && ! defined IDN2_STATIC
#  define _IDN2_API __declspec(dllexport)
# elif defined _MSC_VER && ! defined IDN2_STATIC
#  define _IDN2_API __declspec(dllimport)
# else
#  define _IDN2_API
# endif
#endif

#define IDN2_VERSION "2.3.2-libuidna"
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

extern _IDN2_API const char* idn2_strerror(int rc);
extern _IDN2_API const char* idn2_strerror_name(int rc);
extern _IDN2_API const char* idn2_check_version(const char *req_version);

extern _IDN2_API void idn2_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_IDN2_H_ */
