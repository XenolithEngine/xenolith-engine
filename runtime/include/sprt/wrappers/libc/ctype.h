#ifndef CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_CTYPES_H_
#define CORE_RUNTIME_INCLUDE_SPRT_WRAPPERS_LIBC_CTYPES_H_

#include <sprt/c/__sprt_ctype.h>

/*
Since **Xenolith Runtime** supports only UTF-8 for the primary locale, classifying 8-bit characters
technically does not require access to the locale.

However, if the base libc supports other locales, functions with the `_l` suffix may access them.
*/

#ifdef __cplusplus

#include <sprt/cxx/detail/constexpr.h>

namespace sprt {
inline namespace _cctype {

SPRT_FORCEINLINE constexpr int isalnum(int c) { return __constexpr_isalnum_c(c); }
SPRT_FORCEINLINE constexpr int isalpha(int c) { return __constexpr_isalpha_c(c); }
SPRT_FORCEINLINE constexpr int isblank(int c) { return __constexpr_isblank_c(c); }
SPRT_FORCEINLINE constexpr int iscntrl(int c) { return __constexpr_iscntrl_c(c); }
SPRT_FORCEINLINE constexpr int isdigit(int c) { return __constexpr_isdigit_c(c); }
SPRT_FORCEINLINE constexpr int isgraph(int c) { return __constexpr_isgraph_c(c); }
SPRT_FORCEINLINE constexpr int islower(int c) { return __constexpr_islower_c(c); }
SPRT_FORCEINLINE constexpr int isprint(int c) { return __constexpr_isprint_c(c); }
SPRT_FORCEINLINE constexpr int ispunct(int c) { return __constexpr_ispunct_c(c); }
SPRT_FORCEINLINE constexpr int isspace(int c) { return __constexpr_isspace_c(c); }
SPRT_FORCEINLINE constexpr int isupper(int c) { return __constexpr_isupper_c(c); }
SPRT_FORCEINLINE constexpr int isxdigit(int c) { return __constexpr_isxdigit_c(c); }

SPRT_FORCEINLINE constexpr char tolower_c(char c) { return __constexpr_tolower_c(c); }
SPRT_FORCEINLINE constexpr char16_t tolower_c(char16_t c) { return __constexpr_tolower_c(c); }
SPRT_FORCEINLINE constexpr char32_t tolower_c(char32_t c) { return __constexpr_tolower_c(c); }

SPRT_FORCEINLINE constexpr char toupper_c(char c) { return __constexpr_toupper_c(c); }
SPRT_FORCEINLINE constexpr char16_t toupper_c(char16_t c) { return __constexpr_toupper_c(c); }
SPRT_FORCEINLINE constexpr char32_t toupper_c(char32_t c) { return __constexpr_toupper_c(c); }

} // namespace _cctype
} // namespace sprt

// Export to global namespace
#if !defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1
using namespace sprt::_cctype;
#endif

#ifdef __SPRT_AS_STD
namespace std {
using namespace sprt::_cctype;
}
#endif

#endif // __cplusplus


#if __STDC_HOSTED__ == 0 || (!defined(__SPRT_BUILD) && !defined(__cplusplus))

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int isalnum(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isalnum(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isalpha(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isalpha(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isblank(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isblank(c);
}
#endif

SPRT_UMBRELLA_FUNC
int iscntrl(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iscntrl(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isdigit(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isdigit(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isgraph(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isgraph(c);
}
#endif

SPRT_UMBRELLA_FUNC
int islower(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_islower(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isprint(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isprint(c);
}
#endif

SPRT_UMBRELLA_FUNC
int ispunct(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ispunct(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isspace(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isspace(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isupper(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isupper(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isxdigit(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isxdigit(c);
}
#endif

__SPRT_END_DECL

#endif


#if __STDC_HOSTED__ == 0 || (!defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1)

__SPRT_BEGIN_DECL

SPRT_UMBRELLA_FUNC
int tolower(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tolower(c);
}
#endif

SPRT_UMBRELLA_FUNC
int toupper(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_toupper(c);
}
#endif

SPRT_UMBRELLA_FUNC
int isalnum_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isalnum_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isalpha_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isalpha_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isblank_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isblank_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int iscntrl_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_iscntrl_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isdigit_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isdigit_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isgraph_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isgraph_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int islower_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_islower_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isprint_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isprint_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int ispunct_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_ispunct_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isspace_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isspace_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isupper_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isupper_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isxdigit_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isxdigit_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int tolower_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_tolower_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int toupper_l(int c, __SPRT_ID(locale_t) loc) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_toupper_l(c, loc);
}
#endif

SPRT_UMBRELLA_FUNC
int isascii(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_isascii(c);
}
#endif

SPRT_UMBRELLA_FUNC
int toascii(int c) SPRT_UMBRELLA_END
#if SPRT_UMBRELLA_REQUIRED
{
	return __sprt_toascii(c);
}
#endif

__SPRT_END_DECL

#endif // !defined(__SPRT_BUILD) && __STDC_HOSTED__ == 1

#define _isalnum_l isalnum_l
#define _isalpha_l isalpha_l
#define _isblank_l isblank_l
#define _iscntrl_l iscntrl_l
#define _isdigit_l isdigit_l
#define _isgraph_l isgraph_l
#define _islower_l islower_l
#define _isprint_l isprint_l
#define _ispunct_l ispunct_l
#define _isspace_l isspace_l
#define _isupper_l isupper_l
#define _isxdigit_l isxdigit_l
#define _tolower_l tolower_l
#define _toupper_l toupper_l

#endif
