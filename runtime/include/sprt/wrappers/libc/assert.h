#include <sprt/c/__sprt_assert.h>

#if __STDC_HOSTED__ == 0 || !defined(__SPRT_BUILD)
#ifndef assert
#ifdef NDEBUG
#define assert(Expr)		(__SPRT_ASSERT_UNUSED (0))
#else
#define assert(Expr) \
	(__SPRT_ASSERT_TEST(Expr) ? __SPRT_ASSERT_UNUSED(0) : __sprt_assert_fail(#Expr, __FILE__, __LINE__, __SPRT_FUNCTION__, __SPRT_NULL))
#endif
#endif

// ISO C11 <assert.h> defines static_assert as a macro for _Static_assert. In
// C++ and in C23 it is a keyword, so it is not (re)defined there.
#if !defined(__cplusplus) && !defined(static_assert) \
		&& (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#define static_assert _Static_assert
#endif
#endif
