/**
Copyright (c) 2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef STAPPLER_GEOM_SPSIMD_H_
#define STAPPLER_GEOM_SPSIMD_H_

#include "SPCommon.h"

#if XWIN
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif

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

#if SP_GEOM_DEFAULT_SIMD == SP_GEOM_DEFAULT_SIMD_NEON
#include "simde/arm/neon.h"
#include "simde/x86/sse.h"
#else
#include "simde/x86/sse.h"
#endif

// Defined by build system
// If SP_DEDICATED_SIMD is defined, replacements for SIMD on other platforms is not available
// If SP_DEDICATED_SIMD is not defined, you can use simd::neon on sse or simd::sse on NEON
#ifdef SP_DEDICATED_SIMD
#if SP_GEOM_DEFAULT_SIMD == SP_GEOM_DEFAULT_SIMD_NEON
#include "SPSIMD_Neon.h"
#elif SP_GEOM_DEFAULT_SIMD == SP_GEOM_DEFAULT_SIMD_NEON64
#include "SPSIMD_Neon64.h"
#else
#include "SPSIMD_Sse.h"
#endif
#else
#include "SPSIMD_Sse.h"
#include "SPSIMD_Neon.h"
#include "SPSIMD_Neon64.h"
#endif


namespace STAPPLER_VERSIONIZED stappler::simd {

using f32x4 = SP_GEOM_DEFAULT_SIMD_NAMESPACE::f32x4;

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(float v1, float v2, float v3, float v4) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::load(v1, v2, v3, v4);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(const float v[4]) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::load(v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(float v) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::load(v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void store(float target[4], const f32x4 &v) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::store(target, v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 mul(const f32x4 &v1, const f32x4 &v2) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::mul(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 div(const f32x4 &v1, const f32x4 &v2) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::div(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 add(const f32x4 &v1, const f32x4 &v2) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::add(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 sub(const f32x4 &v1, const f32x4 &v2) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::sub(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 rsqrt(const f32x4 &v) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::rsqrt(v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load1(float v) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::load1(v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void store1(float *target, const f32x4 &v) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::store1(target, v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 mul1(const f32x4 &v1, const f32x4 &v2) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::mul1(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 add1(const f32x4 &v1, const f32x4 &v2) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::add1(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 sub1(const f32x4 &v1, const f32x4 &v2) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::sub1(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 rsqrt1(const f32x4 &v) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::rsqrt1(v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void add(const float a[4], const float b[4], float dst[4]) {
	store(dst, add(load(a), load(b)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void add(const float a[4], const float &b, float dst[4]) {
	store(dst, add(load(a), load(b)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void sub(const float a[4], const float b[4], float dst[4]) {
	store(dst, sub(load(a), load(b)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void sub(const float a[4], const float &b, float dst[4]) {
	store(dst, sub(load(a), load(b)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiply(const float a[4], const float b[4], float dst[4]) {
	store(dst, mul(load(a), load(b)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiply(const float a[4], const float &b, float dst[4]) {
	store(dst, mul(load(a), load(b)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void divide(const float a[4], const float b[4], float dst[4]) {
	store(dst, div(load(a), load(b)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void divide(const float a[4], const float &b, float dst[4]) {
	store(dst, div(load(a), load(b)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::addMat4Scalar(m, scalar, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::addMat4(m1, m2, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::subtractMat4(m1, m2, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::multiplyMat4Scalar(m, scalar, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::multiplyMat4(m1, m2, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void negateMat4(const float m[16], float dst[16]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::negateMat4(m, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transposeMat4(const float m[16], float dst[16]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::transposeMat4(m, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::transformVec4Components(m, x, y, z, w, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::transformVec4(m, v, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void crossVec3(const float v1[3], const float v2[3], float dst[3]) {
	SP_GEOM_DEFAULT_SIMD_NAMESPACE::crossVec3(v1, v2, dst);
}

// input for test A->B vs C->D (ax, ay, bx, by), (cx, cy, dx, dy)
SP_ATTR_OPTIMIZE_INLINE_FN inline bool isVec2BboxIntersects(const f32x4 & v1, const f32x4 & v2, f32x4 &isect) {
	return SP_GEOM_DEFAULT_SIMD_NAMESPACE::isVec2BboxIntersects(v1, v2, isect);
}

}

#if XWIN
#pragma clang diagnostic pop
#endif

#endif /* STAPPLER_GEOM_SPSIMD_H_ */
