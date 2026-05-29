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
#endif
