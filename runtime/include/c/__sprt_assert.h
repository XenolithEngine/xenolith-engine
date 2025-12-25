/**
Copyright (c) 2025 Stappler Team <admin@stappler.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
**/

#ifndef CORE_RUNTIME_INCLUDE_C___SPRT_ASSERT_H_
#define CORE_RUNTIME_INCLUDE_C___SPRT_ASSERT_H_

#include <c/bits/__sprt_def.h>

#if defined __cplusplus
#define __SPRT_ASSERT_UNUSED(Value) static_cast<void>(Value)
#define __SPRT_ASSERT_TEST(Expr) static_cast<bool>(Expr)
#else
#define __SPRT_ASSERT_UNUSED(Value) (void)(Value)
#define __SPRT_ASSERT_TEST(Expr) (Expr)
#endif

#ifdef NDEBUG

#define assert(Expr)		(__SPRT_ASSERT_UNUSED (0))
#define sprt_passert(Expr, Str)		(__SPRT_ASSERT_UNUSED (0))

#else // NDEBUG

__SPRT_BEGIN_DECL

SPRT_API extern void __sprt_assert_fail(const char *cond, const char *file, unsigned int line,
		const char *fn, const char *text) __SPRT_NOEXCEPT __SPRT_NORETURN;

__SPRT_END_DECL

#define assert(Expr) \
	(__SPRT_ASSERT_TEST(Expr) ? __SPRT_ASSERT_UNUSED(0) : __sprt_assert_fail(#Expr, __FILE__, __LINE__, __SPRT_FUNCTION__, NULL))

#define sprt_passert(Expr, Str) \
	(__SPRT_ASSERT_TEST(Expr) ? __SPRT_ASSERT_UNUSED(0) : __sprt_assert_fail(#Expr, __FILE__, __LINE__, __SPRT_FUNCTION__, Str))

#endif // NDEBUG

#endif // CORE_RUNTIME_INCLUDE_C___SPRT_ASSERT_H_
