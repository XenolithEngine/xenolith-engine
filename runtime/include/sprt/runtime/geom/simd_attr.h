/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_ATTR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_ATTR_H_

#define SP_SIMD_DEBUG 0

#if SP_SIMD_DEBUG
#define SP_ATTR_OPTIMIZE_FN
#define SP_ATTR_OPTIMIZE_INLINE_FN
#else
// mostly to debug SIMD in ASM, maybe disable for NDEBUG?
#if __clang__
#define SP_ATTR_OPTIMIZE_FN
#define SP_ATTR_OPTIMIZE_INLINE_FN __attribute__((always_inline))
#else
#define SP_ATTR_OPTIMIZE_FN __attribute__((optimize(3)))
#define SP_ATTR_OPTIMIZE_INLINE_FN __attribute__((optimize(3),always_inline))
#endif
#endif

#define SP_GEOM_DEFAULT_SIMD_SSE 1
#define SP_GEOM_DEFAULT_SIMD_NEON 2
#define SP_GEOM_DEFAULT_SIMD_NEON64 3

#if __SSE__
#define SP_GEOM_DEFAULT_SIMD SP_GEOM_DEFAULT_SIMD_SSE
#define SP_GEOM_DEFAULT_SIMD_NAMESPACE sse
#elif __aarch64__
#define SP_GEOM_DEFAULT_SIMD SP_GEOM_DEFAULT_SIMD_NEON64
#define SP_GEOM_DEFAULT_SIMD_NAMESPACE neon64
#elif __arm__
#define SP_GEOM_DEFAULT_SIMD SP_GEOM_DEFAULT_SIMD_NEON
#define SP_GEOM_DEFAULT_SIMD_NAMESPACE neon
#else
#define SP_GEOM_DEFAULT_SIMD SP_GEOM_DEFAULT_SIMD_SSE
#define SP_GEOM_DEFAULT_SIMD_NAMESPACE sse
#endif

#endif // RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_ATTR_H_
