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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_SCALAR_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_SCALAR_H_

#include <simde/x86/sse.h>
#include <sprt/runtime/geom/simd_attr.h>

// The scalar backend is the reference the SIMD ones are checked against: one IEEE operation per
// lane, in the order the SSE backend uses, and never contracted into an FMA.
#if __clang__
#define SP_SIMD_SCALAR_NO_CONTRACT _Pragma("clang fp contract(off)")
#else
#define SP_SIMD_SCALAR_NO_CONTRACT
#endif

namespace sprt::geom::simd::scalar {

// Same type as the SIMD backends, so code that mixes the facade with simde_mm_* keeps compiling
using f32x4 = simde__m128;

struct alignas(16) Lanes {
	float v[4];
};

SP_ATTR_OPTIMIZE_INLINE_FN inline Lanes lanes(const f32x4 &v) {
	Lanes ret;
	simde_mm_storeu_ps(ret.v, v);
	return ret;
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 vec(const Lanes &v) { return simde_mm_loadu_ps(v.v); }

SP_ATTR_OPTIMIZE_INLINE_FN inline float rsqrtLane(float v) { return 1.0f / __builtin_sqrtf(v); }

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(float v1, float v2, float v3, float v4) {
	return vec(Lanes{{v1, v2, v3, v4}});
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(const float v[4]) {
	return vec(Lanes{{v[0], v[1], v[2], v[3]}});
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(float v) { return vec(Lanes{{v, v, v, v}}); }

SP_ATTR_OPTIMIZE_INLINE_FN inline void store(float target[4], const f32x4 &v) {
	auto l = lanes(v);
	for (int i = 0; i < 4; ++i) { target[i] = l.v[i]; }
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 mul(const f32x4 &v1, const f32x4 &v2) {
	SP_SIMD_SCALAR_NO_CONTRACT
	auto a = lanes(v1), b = lanes(v2);
	for (int i = 0; i < 4; ++i) { a.v[i] = a.v[i] * b.v[i]; }
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 div(const f32x4 &v1, const f32x4 &v2) {
	auto a = lanes(v1), b = lanes(v2);
	for (int i = 0; i < 4; ++i) { a.v[i] = a.v[i] / b.v[i]; }
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 add(const f32x4 &v1, const f32x4 &v2) {
	SP_SIMD_SCALAR_NO_CONTRACT
	auto a = lanes(v1), b = lanes(v2);
	for (int i = 0; i < 4; ++i) { a.v[i] = a.v[i] + b.v[i]; }
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 sub(const f32x4 &v1, const f32x4 &v2) {
	SP_SIMD_SCALAR_NO_CONTRACT
	auto a = lanes(v1), b = lanes(v2);
	for (int i = 0; i < 4; ++i) { a.v[i] = a.v[i] - b.v[i]; }
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 rsqrt(const f32x4 &v) {
	auto a = lanes(v);
	for (int i = 0; i < 4; ++i) { a.v[i] = rsqrtLane(a.v[i]); }
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load1(float v) { return vec(Lanes{{v, 0.0f, 0.0f, 0.0f}}); }

SP_ATTR_OPTIMIZE_INLINE_FN inline void store1(float *target, const f32x4 &v) {
	*target = lanes(v).v[0];
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 mul1(const f32x4 &v1, const f32x4 &v2) {
	SP_SIMD_SCALAR_NO_CONTRACT
	auto a = lanes(v1);
	a.v[0] = a.v[0] * lanes(v2).v[0];
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 add1(const f32x4 &v1, const f32x4 &v2) {
	SP_SIMD_SCALAR_NO_CONTRACT
	auto a = lanes(v1);
	a.v[0] = a.v[0] + lanes(v2).v[0];
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 sub1(const f32x4 &v1, const f32x4 &v2) {
	SP_SIMD_SCALAR_NO_CONTRACT
	auto a = lanes(v1);
	a.v[0] = a.v[0] - lanes(v2).v[0];
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 rsqrt1(const f32x4 &v) {
	auto a = lanes(v);
	a.v[0] = rsqrtLane(a.v[0]);
	return vec(a);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4Scalar(const float m[16], float scalar,
		float dst[16]) {
	SP_SIMD_SCALAR_NO_CONTRACT
	for (int i = 0; i < 16; ++i) { dst[i] = m[i] + scalar; }
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4(const float m1[16], const float m2[16],
		float dst[16]) {
	SP_SIMD_SCALAR_NO_CONTRACT
	for (int i = 0; i < 16; ++i) { dst[i] = m1[i] + m2[i]; }
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void subtractMat4(const float m1[16], const float m2[16],
		float dst[16]) {
	SP_SIMD_SCALAR_NO_CONTRACT
	for (int i = 0; i < 16; ++i) { dst[i] = m1[i] - m2[i]; }
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4Scalar(const float m[16], float scalar,
		float dst[16]) {
	SP_SIMD_SCALAR_NO_CONTRACT
	for (int i = 0; i < 16; ++i) { dst[i] = m[i] * scalar; }
}

// Column-major, dst may alias either argument
SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4(const float m1[16], const float m2[16],
		float dst[16]) {
	SP_SIMD_SCALAR_NO_CONTRACT
	float ret[16];
	for (int col = 0; col < 4; ++col) {
		const float *e = &m2[col * 4];
		for (int row = 0; row < 4; ++row) {
			const float v0 = m1[row] * e[0];
			const float v1 = m1[4 + row] * e[1];
			const float v2 = m1[8 + row] * e[2];
			const float v3 = m1[12 + row] * e[3];
			ret[col * 4 + row] = (v0 + v1) + (v2 + v3);
		}
	}
	for (int i = 0; i < 16; ++i) { dst[i] = ret[i]; }
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void negateMat4(const float m[16], float dst[16]) {
	SP_SIMD_SCALAR_NO_CONTRACT
	// 0 - x, not -x: zero stays +0, as it does with a vector subtraction
	for (int i = 0; i < 16; ++i) { dst[i] = 0.0f - m[i]; }
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transposeMat4(const float m[16], float dst[16]) {
	float ret[16];
	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) { ret[row * 4 + col] = m[col * 4 + row]; }
	}
	for (int i = 0; i < 16; ++i) { dst[i] = ret[i]; }
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4Components(const float m[16], float x, float y,
		float z, float w, float dst[4]) {
	SP_SIMD_SCALAR_NO_CONTRACT
	float ret[4];
	for (int i = 0; i < 4; ++i) {
		const float v0 = m[i] * x;
		const float v1 = m[4 + i] * y;
		const float v2 = m[8 + i] * z;
		const float v3 = m[12 + i] * w;
		ret[i] = (v0 + v1) + (v2 + v3);
	}
	for (int i = 0; i < 4; ++i) { dst[i] = ret[i]; }
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4(const float m[16], const float v[4],
		float dst[4]) {
	transformVec4Components(m, v[0], v[1], v[2], v[3], dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void crossVec3(const float v1[3], const float v2[3],
		float dst[3]) {
	SP_SIMD_SCALAR_NO_CONTRACT
	const float x = (v1[1] * v2[2]) - (v1[2] * v2[1]);
	const float y = (v1[2] * v2[0]) - (v1[0] * v2[2]);
	const float z = (v1[0] * v2[1]) - (v1[1] * v2[0]);

	dst[0] = x;
	dst[1] = y;
	dst[2] = z;
}

// input for test A->B vs C->D (ax, ay, bx, by), (cx, cy, dx, dy)
SP_ATTR_OPTIMIZE_INLINE_FN inline bool isVec2BboxIntersects(const f32x4 &v1, const f32x4 &v2,
		f32x4 &isect) {
	SP_SIMD_SCALAR_NO_CONTRACT
	auto a = lanes(v1), b = lanes(v2);
	const Lanes lo{{a.v[0], a.v[1], b.v[0], b.v[1]}}; // (ax, ay, cx, cy)
	const Lanes hi{{a.v[2], a.v[3], b.v[2], b.v[3]}}; // (bx, by, dx, dy)

	Lanes minV, maxV, diff;
	for (int i = 0; i < 4; ++i) {
		minV.v[i] = (lo.v[i] < hi.v[i]) ? lo.v[i] : hi.v[i];
		maxV.v[i] = (lo.v[i] > hi.v[i]) ? lo.v[i] : hi.v[i];
		diff.v[i] = hi.v[i] - lo.v[i];
	}
	isect = vec(diff);

	const float ret0 = (maxV.v[0] - minV.v[0]) - (minV.v[2] - minV.v[0]);
	const float ret1 = (maxV.v[1] - minV.v[1]) - (minV.v[3] - minV.v[1]);

	return ret0 >= 0.0f && ret1 >= 0.0f && (ret0 != 0.0f || ret1 != 0.0f);
}

} // namespace sprt::geom::simd::scalar

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_SIMD_SCALAR_H_ */
