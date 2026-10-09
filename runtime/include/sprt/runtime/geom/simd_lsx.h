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

// Excluded from documentation/codegen tool
///@ SP_EXCLUDE

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_LSX_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_LSX_H_

#include <simde/x86/sse.h>
#include <sprt/runtime/geom/simd_attr.h>

// LoongArch LSX: products and sums stay separate instructions (no vfmadd), so the results are
// bit-identical to the scalar backend; vfrsqrt.s is a correctly rounded 1/sqrt, not an estimate.
#if __loongarch_sx

#include <lsxintrin.h>

namespace sprt::geom::simd::lsx {

// simde__m128 is v4f32 under LSX, the type the tesselator's simde_mm_* calls expect
using f32x4 = simde__m128;

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 ld(const float *v) { return (f32x4)__lsx_vld(v, 0); }

SP_ATTR_OPTIMIZE_INLINE_FN inline void st(float *target, const f32x4 &v) {
	__lsx_vst((__m128i)v, target, 0);
}

template <int Lane>
SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 splat(const f32x4 &v) {
	return (f32x4)__lsx_vreplvei_w((__m128i)v, Lane);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(float v1, float v2, float v3, float v4) {
	return f32x4{v1, v2, v3, v4};
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(const float v[4]) { return ld(v); }

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(float v) { return f32x4{v, v, v, v}; }

SP_ATTR_OPTIMIZE_INLINE_FN inline void store(float target[4], const f32x4 &v) { st(target, v); }

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 mul(const f32x4 &v1, const f32x4 &v2) {
	return (f32x4)__lsx_vfmul_s((__m128)v1, (__m128)v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 div(const f32x4 &v1, const f32x4 &v2) {
	return (f32x4)__lsx_vfdiv_s((__m128)v1, (__m128)v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 add(const f32x4 &v1, const f32x4 &v2) {
	return (f32x4)__lsx_vfadd_s((__m128)v1, (__m128)v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 sub(const f32x4 &v1, const f32x4 &v2) {
	return (f32x4)__lsx_vfsub_s((__m128)v1, (__m128)v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 rsqrt(const f32x4 &v) {
	return (f32x4)__lsx_vfrsqrt_s((__m128)v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load1(float v) { return f32x4{v, 0.0f, 0.0f, 0.0f}; }

SP_ATTR_OPTIMIZE_INLINE_FN inline void store1(float *target, const f32x4 &v) { *target = v[0]; }

// The *1 forms replace lane 0 of v1 and keep its other lanes, like the SSE _ss operations
SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 lane0(const f32x4 &v1, const f32x4 &r) {
	return (f32x4)__lsx_vextrins_w((__m128i)v1, (__m128i)r, 0x00);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 mul1(const f32x4 &v1, const f32x4 &v2) {
	return lane0(v1, mul(v1, v2));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 add1(const f32x4 &v1, const f32x4 &v2) {
	return lane0(v1, add(v1, v2));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 sub1(const f32x4 &v1, const f32x4 &v2) {
	return lane0(v1, sub(v1, v2));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 rsqrt1(const f32x4 &v) { return lane0(v, rsqrt(v)); }

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4Scalar(const float m[16], float scalar,
		float dst[16]) {
	const f32x4 s = load(scalar);
	const f32x4 c0 = ld(&m[0]), c1 = ld(&m[4]), c2 = ld(&m[8]), c3 = ld(&m[12]);
	st(&dst[0], add(c0, s));
	st(&dst[4], add(c1, s));
	st(&dst[8], add(c2, s));
	st(&dst[12], add(c3, s));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4(const float m1[16], const float m2[16],
		float dst[16]) {
	const f32x4 a0 = ld(&m1[0]), a1 = ld(&m1[4]), a2 = ld(&m1[8]), a3 = ld(&m1[12]);
	const f32x4 b0 = ld(&m2[0]), b1 = ld(&m2[4]), b2 = ld(&m2[8]), b3 = ld(&m2[12]);
	st(&dst[0], add(a0, b0));
	st(&dst[4], add(a1, b1));
	st(&dst[8], add(a2, b2));
	st(&dst[12], add(a3, b3));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void subtractMat4(const float m1[16], const float m2[16],
		float dst[16]) {
	const f32x4 a0 = ld(&m1[0]), a1 = ld(&m1[4]), a2 = ld(&m1[8]), a3 = ld(&m1[12]);
	const f32x4 b0 = ld(&m2[0]), b1 = ld(&m2[4]), b2 = ld(&m2[8]), b3 = ld(&m2[12]);
	st(&dst[0], sub(a0, b0));
	st(&dst[4], sub(a1, b1));
	st(&dst[8], sub(a2, b2));
	st(&dst[12], sub(a3, b3));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4Scalar(const float m[16], float scalar,
		float dst[16]) {
	const f32x4 s = load(scalar);
	const f32x4 c0 = ld(&m[0]), c1 = ld(&m[4]), c2 = ld(&m[8]), c3 = ld(&m[12]);
	st(&dst[0], mul(c0, s));
	st(&dst[4], mul(c1, s));
	st(&dst[8], mul(c2, s));
	st(&dst[12], mul(c3, s));
}

// One column of m1 * m2: the columns of m1 weighted by the lanes of e
SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 combine(const f32x4 &c0, const f32x4 &c1, const f32x4 &c2,
		const f32x4 &c3, const f32x4 &e) {
	const f32x4 v0 = mul(c0, splat<0>(e));
	const f32x4 v1 = mul(c1, splat<1>(e));
	const f32x4 v2 = mul(c2, splat<2>(e));
	const f32x4 v3 = mul(c3, splat<3>(e));
	return add(add(v0, v1), add(v2, v3));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4(const float m1[16], const float m2[16],
		float dst[16]) {
	// Every input is loaded before the first store: dst may alias either argument
	const f32x4 a0 = ld(&m1[0]), a1 = ld(&m1[4]), a2 = ld(&m1[8]), a3 = ld(&m1[12]);
	const f32x4 b0 = ld(&m2[0]), b1 = ld(&m2[4]), b2 = ld(&m2[8]), b3 = ld(&m2[12]);
	const f32x4 r0 = combine(a0, a1, a2, a3, b0);
	const f32x4 r1 = combine(a0, a1, a2, a3, b1);
	const f32x4 r2 = combine(a0, a1, a2, a3, b2);
	const f32x4 r3 = combine(a0, a1, a2, a3, b3);
	st(&dst[0], r0);
	st(&dst[4], r1);
	st(&dst[8], r2);
	st(&dst[12], r3);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void negateMat4(const float m[16], float dst[16]) {
	const f32x4 z = load(0.0f);
	const f32x4 c0 = ld(&m[0]), c1 = ld(&m[4]), c2 = ld(&m[8]), c3 = ld(&m[12]);
	st(&dst[0], sub(z, c0));
	st(&dst[4], sub(z, c1));
	st(&dst[8], sub(z, c2));
	st(&dst[12], sub(z, c3));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transposeMat4(const float m[16], float dst[16]) {
	const __m128i c0 = __lsx_vld(&m[0], 0), c1 = __lsx_vld(&m[4], 0);
	const __m128i c2 = __lsx_vld(&m[8], 0), c3 = __lsx_vld(&m[12], 0);

	// (c0[0], c1[0], c0[1], c1[1]) and so on: interleave words, then double words
	const __m128i t0 = __lsx_vilvl_w(c1, c0);
	const __m128i t1 = __lsx_vilvl_w(c3, c2);
	const __m128i t2 = __lsx_vilvh_w(c1, c0);
	const __m128i t3 = __lsx_vilvh_w(c3, c2);

	__lsx_vst(__lsx_vilvl_d(t1, t0), &dst[0], 0);
	__lsx_vst(__lsx_vilvh_d(t1, t0), &dst[4], 0);
	__lsx_vst(__lsx_vilvl_d(t3, t2), &dst[8], 0);
	__lsx_vst(__lsx_vilvh_d(t3, t2), &dst[12], 0);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4Components(const float m[16], float x, float y,
		float z, float w, float dst[4]) {
	const f32x4 c0 = ld(&m[0]), c1 = ld(&m[4]), c2 = ld(&m[8]), c3 = ld(&m[12]);
	st(dst, combine(c0, c1, c2, c3, f32x4{x, y, z, w}));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4(const float m[16], const float v[4],
		float dst[4]) {
	const f32x4 c0 = ld(&m[0]), c1 = ld(&m[4]), c2 = ld(&m[8]), c3 = ld(&m[12]);
	st(dst, combine(c0, c1, c2, c3, ld(v)));
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void crossVec3(const float v1[3], const float v2[3],
		float dst[3]) {
	// Vec3 is three floats: build the operands lane by lane instead of reading a fourth
	const f32x4 a = f32x4{v1[1], v1[2], v1[0], 0.0f};
	const f32x4 b = f32x4{v2[2], v2[0], v2[1], 0.0f};
	const f32x4 c = f32x4{v1[2], v1[0], v1[1], 0.0f};
	const f32x4 d = f32x4{v2[1], v2[2], v2[0], 0.0f};
	const f32x4 r = sub(mul(a, b), mul(c, d));

	dst[0] = r[0];
	dst[1] = r[1];
	dst[2] = r[2];
}

// input for test A->B vs C->D (ax, ay, bx, by), (cx, cy, dx, dy)
SP_ATTR_OPTIMIZE_INLINE_FN inline bool isVec2BboxIntersects(const f32x4 &v1, const f32x4 &v2,
		f32x4 &isect) {
	const f32x4 lo = (f32x4)__lsx_vilvl_d((__m128i)v2, (__m128i)v1); // (ax, ay, cx, cy)
	const f32x4 hi = (f32x4)__lsx_vilvh_d((__m128i)v2, (__m128i)v1); // (bx, by, dx, dy)

	// vfmin/vfmax differ from minps/maxps only for NaN and signed zeros
	const f32x4 minVec = (f32x4)__lsx_vfmin_s((__m128)lo, (__m128)hi);
	const f32x4 maxVec = (f32x4)__lsx_vfmax_s((__m128)lo, (__m128)hi);

	isect = sub(hi, lo);

	// movehl(maxVec, minVec) of the SSE backend: (min[2], min[3], max[2], max[3])
	const f32x4 shifted = (f32x4)__lsx_vilvh_d((__m128i)maxVec, (__m128i)minVec);
	const f32x4 ret = sub(sub(maxVec, minVec), sub(shifted, minVec));

	if (ret[0] >= 0.0f && ret[1] >= 0.0f && (ret[0] != 0.0f || ret[1] != 0.0f)) {
		return true;
	}
	return false;
}

} // namespace sprt::geom::simd::lsx

#endif // __loongarch_sx

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_LSX_H_ */
