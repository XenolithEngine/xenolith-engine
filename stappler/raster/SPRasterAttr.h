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

#ifndef STAPPLER_RASTER_SPRASTERATTR_H_
#define STAPPLER_RASTER_SPRASTERATTR_H_

// Attributes of the pixel loops, in the spirit of runtime/geom/simd_attr.h.
//
// What an attribute can NOT do here is raise the optimization level: clang ignores
// __attribute__((optimize(...))) outright (it warns and compiles the function as usual), and a
// debug build passes no -O at all, so every function would carry `optnone`. That is why the
// rasterizer is a module of its own with -O2 in its private flags; see raster.mk. Attributes
// carry ISA selection and inlining, which is what they are actually good for.

// A kernel body. `flatten` matters independently of -O2: it expands the shader and the sampler
// into the span loop, which is what makes the per-TextureKind specialization worth having.
#define SP_RASTER_KERNEL_FN __attribute__((flatten, hot))

// A helper that must disappear into its caller. Note the trap this implies once ISA kernels land:
// an always_inline callee inherits nothing, so a helper called from a target("avx2") function has
// to carry a compatible target of its own or clang errors out.
#define SP_RASTER_KERNEL_INLINE __attribute__((always_inline)) inline

// Per-function ISA selection inside one translation unit - the mechanism that lets AVX2 kernels
// live next to the scalar ones without a second compilation of the same source.
#define SP_RASTER_TARGET(x) __attribute__((target(x)))

// Which ISA-specific kernels exist in this build at all. Note the asymmetry, which is real and
// not an oversight: on x86 the set is chosen at run time from CPUID, while on AArch64 NEON is
// part of the base architecture and there is nothing to ask - so it is compiled in and used.
#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86_64 || __SPRT_ARCH_ID == __SPRT_ARCH_ID_X86
#define SP_RASTER_X86 1
#else
#define SP_RASTER_X86 0
#endif

#if __SPRT_ARCH_ID == __SPRT_ARCH_ID_AARCH64
#define SP_RASTER_NEON 1
#else
#define SP_RASTER_NEON 0
#endif

#endif /* STAPPLER_RASTER_SPRASTERATTR_H_ */
