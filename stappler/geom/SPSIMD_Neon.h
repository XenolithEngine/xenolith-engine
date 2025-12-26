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

// Excluded from documentation/codegen tool
///@ SP_EXCLUDE

#ifndef STAPPLER_GEOM_SPSIMD_NEON_H_
#define STAPPLER_GEOM_SPSIMD_NEON_H_

#include "SPSIMD.h"
#include "SPSIMD_Sse.h"
#include "simde/arm/neon.h"

namespace STAPPLER_VERSIONIZED stappler::simd::neon {

using f32x4 = simde__m128;

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(float v1, float v2, float v3, float v4) {
	return simde_mm_set_ps(v4, v3, v2, v1);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(const float v[4]) {
	return simde_mm_load_ps(v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load(float v) {
	return simde_mm_load1_ps(&v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void store(float target[4], const f32x4 &v) {
	simde_mm_store_ps(target, v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 mul(const f32x4 &v1, const f32x4 &v2) {
	return simde_mm_mul_ps(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 div(const f32x4 &v1, const f32x4 &v2) {
	return simde_mm_div_ps(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 add(const f32x4 &v1, const f32x4 &v2) {
	return simde_mm_add_ps(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 sub(const f32x4 &v1, const f32x4 &v2) {
	return simde_mm_sub_ps(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 rsqrt(const f32x4 &v) {
	return simde_mm_rsqrt_ps(v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 load1(float v) {
	return simde_mm_load_ss(&v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void store1(float *target, const f32x4 &v) {
	simde_mm_store_ss(target, v);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 mul1(const f32x4 &v1, const f32x4 &v2) {
	return simde_mm_mul_ss(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 add1(const f32x4 &v1, const f32x4 &v2) {
	return simde_mm_add_ss(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 sub1(const f32x4 &v1, const f32x4 &v2) {
	return simde_mm_sub_ss(v1, v2);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline f32x4 rsqrt1(const f32x4 &v) {
	return simde_mm_rsqrt_ss(v);
}

#if SP_GEOM_DEFAULT_SIMD == SP_GEOM_DEFAULT_SIMD_NEON

inline void addMat4Scalar_impl(const float* m, float scalar, float* dst) {
	asm volatile (
			"vld1.32 {q0, q1}, [%1]!    \n\t" // M[m0-m7]
			"vld1.32 {q2, q3}, [%1]     \n\t"// M[m8-m15]
			"vld1.32 {d8[0]},  [%2]     \n\t"// s
			"vmov.f32 s17, s16          \n\t"// s
			"vmov.f32 s18, s16          \n\t"// s
			"vmov.f32 s19, s16          \n\t"// s

			"vadd.f32 q8, q0, q4        \n\t"// DST->M[m0-m3] = M[m0-m3] + s
			"vadd.f32 q9, q1, q4        \n\t"// DST->M[m4-m7] = M[m4-m7] + s
			"vadd.f32 q10, q2, q4       \n\t"// DST->M[m8-m11] = M[m8-m11] + s
			"vadd.f32 q11, q3, q4       \n\t"// DST->M[m12-m15] = M[m12-m15] + s

			"vst1.32 {q8, q9}, [%0]!    \n\t"// DST->M[m0-m7]
			"vst1.32 {q10, q11}, [%0]   \n\t"// DST->M[m8-m15]
			:
			: "r"(dst), "r"(m), "r"(&scalar)
			: "q0", "q1", "q2", "q3", "q4", "q8", "q9", "q10", "q11", "memory"
	);
}

inline void addMat4_impl(const float* m1, const float* m2, float* dst) {
	asm volatile (
			"vld1.32     {q0, q1},     [%1]! \n\t" // M1[m0-m7]
			"vld1.32     {q2, q3},     [%1]  \n\t"// M1[m8-m15]
			"vld1.32     {q8, q9},     [%2]! \n\t"// M2[m0-m7]
			"vld1.32     {q10, q11}, [%2]    \n\t"// M2[m8-m15]

			"vadd.f32   q12, q0, q8          \n\t"// DST->M[m0-m3] = M1[m0-m3] + M2[m0-m3]
			"vadd.f32   q13, q1, q9          \n\t"// DST->M[m4-m7] = M1[m4-m7] + M2[m4-m7]
			"vadd.f32   q14, q2, q10         \n\t"// DST->M[m8-m11] = M1[m8-m11] + M2[m8-m11]
			"vadd.f32   q15, q3, q11         \n\t"// DST->M[m12-m15] = M1[m12-m15] + M2[m12-m15]

			"vst1.32    {q12, q13}, [%0]!    \n\t"// DST->M[m0-m7]
			"vst1.32    {q14, q15}, [%0]     \n\t"// DST->M[m8-m15]
			:
			: "r"(dst), "r"(m1), "r"(m2)
			: "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory"
	);
}

inline void subtractMat4_impl(const float* m1, const float* m2, float* dst) {
	asm volatile (
			"vld1.32     {q0, q1},     [%1]!  \n\t" // M1[m0-m7]
			"vld1.32     {q2, q3},     [%1]   \n\t"// M1[m8-m15]
			"vld1.32     {q8, q9},     [%2]!  \n\t"// M2[m0-m7]
			"vld1.32     {q10, q11}, [%2]     \n\t"// M2[m8-m15]

			"vsub.f32   q12, q0, q8         \n\t"// DST->M[m0-m3] = M1[m0-m3] - M2[m0-m3]
			"vsub.f32   q13, q1, q9         \n\t"// DST->M[m4-m7] = M1[m4-m7] - M2[m4-m7]
			"vsub.f32   q14, q2, q10        \n\t"// DST->M[m8-m11] = M1[m8-m11] - M2[m8-m11]
			"vsub.f32   q15, q3, q11        \n\t"// DST->M[m12-m15] = M1[m12-m15] - M2[m12-m15]

			"vst1.32    {q12, q13}, [%0]!   \n\t"// DST->M[m0-m7]
			"vst1.32    {q14, q15}, [%0]    \n\t"// DST->M[m8-m15]
			:
			: "r"(dst), "r"(m1), "r"(m2)
			: "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory"
	);
}

inline void multiplyMat4Scalar_impl(const float* m, float scalar, float* dst) {
	asm volatile (
			"vld1.32     {d0[0]},         [%2]        \n\t" // M[m0-m7]
			"vld1.32    {q4-q5},          [%1]!       \n\t"// M[m8-m15]
			"vld1.32    {q6-q7},          [%1]        \n\t"// s

			"vmul.f32     q8, q4, d0[0]               \n\t"// DST->M[m0-m3] = M[m0-m3] * s
			"vmul.f32     q9, q5, d0[0]               \n\t"// DST->M[m4-m7] = M[m4-m7] * s
			"vmul.f32     q10, q6, d0[0]              \n\t"// DST->M[m8-m11] = M[m8-m11] * s
			"vmul.f32     q11, q7, d0[0]              \n\t"// DST->M[m12-m15] = M[m12-m15] * s

			"vst1.32     {q8-q9},           [%0]!     \n\t"// DST->M[m0-m7]
			"vst1.32     {q10-q11},         [%0]      \n\t"// DST->M[m8-m15]
			:
			: "r"(dst), "r"(m), "r"(&scalar)
			: "q0", "q4", "q5", "q6", "q7", "q8", "q9", "q10", "q11", "memory"
	);
}

inline void multiplyMat4_impl(const float* m1, const float* m2, float* dst) {
	asm volatile (
			"vld1.32     {d16 - d19}, [%1]! \n\t"       // M1[m0-m7]
			"vld1.32     {d20 - d23}, [%1]  \n\t"// M1[m8-m15]
			"vld1.32     {d0 - d3}, [%2]!   \n\t"// M2[m0-m7]
			"vld1.32     {d4 - d7}, [%2]    \n\t"// M2[m8-m15]

			"vmul.f32    q12, q8, d0[0]     \n\t"// DST->M[m0-m3] = M1[m0-m3] * M2[m0]
			"vmul.f32    q13, q8, d2[0]     \n\t"// DST->M[m4-m7] = M1[m4-m7] * M2[m4]
			"vmul.f32    q14, q8, d4[0]     \n\t"// DST->M[m8-m11] = M1[m8-m11] * M2[m8]
			"vmul.f32    q15, q8, d6[0]     \n\t"// DST->M[m12-m15] = M1[m12-m15] * M2[m12]

			"vmla.f32    q12, q9, d0[1]     \n\t"// DST->M[m0-m3] += M1[m0-m3] * M2[m1]
			"vmla.f32    q13, q9, d2[1]     \n\t"// DST->M[m4-m7] += M1[m4-m7] * M2[m5]
			"vmla.f32    q14, q9, d4[1]     \n\t"// DST->M[m8-m11] += M1[m8-m11] * M2[m9]
			"vmla.f32    q15, q9, d6[1]     \n\t"// DST->M[m12-m15] += M1[m12-m15] * M2[m13]

			"vmla.f32    q12, q10, d1[0]    \n\t"// DST->M[m0-m3] += M1[m0-m3] * M2[m2]
			"vmla.f32    q13, q10, d3[0]    \n\t"// DST->M[m4-m7] += M1[m4-m7] * M2[m6]
			"vmla.f32    q14, q10, d5[0]    \n\t"// DST->M[m8-m11] += M1[m8-m11] * M2[m10]
			"vmla.f32    q15, q10, d7[0]    \n\t"// DST->M[m12-m15] += M1[m12-m15] * M2[m14]

			"vmla.f32    q12, q11, d1[1]    \n\t"// DST->M[m0-m3] += M1[m0-m3] * M2[m3]
			"vmla.f32    q13, q11, d3[1]    \n\t"// DST->M[m4-m7] += M1[m4-m7] * M2[m7]
			"vmla.f32    q14, q11, d5[1]    \n\t"// DST->M[m8-m11] += M1[m8-m11] * M2[m11]
			"vmla.f32    q15, q11, d7[1]    \n\t"// DST->M[m12-m15] += M1[m12-m15] * M2[m15]

			"vst1.32    {d24 - d27}, [%0]!  \n\t"// DST->M[m0-m7]
			"vst1.32    {d28 - d31}, [%0]   \n\t"// DST->M[m8-m15]

			:// output
			: "r"(dst), "r"(m1), "r"(m2)// input - note *value* of pointer doesn't change.
			: "memory", "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15"
	);
}

inline void negateMat4_impl(const float* m, float* dst) {
	asm volatile (
			"vld1.32     {q0-q1},  [%1]!     \n\t" // load m0-m7
			"vld1.32     {q2-q3},  [%1]      \n\t"// load m8-m15

			"vneg.f32     q4, q0             \n\t"// negate m0-m3
			"vneg.f32     q5, q1             \n\t"// negate m4-m7
			"vneg.f32     q6, q2             \n\t"// negate m8-m15
			"vneg.f32     q7, q3             \n\t"// negate m8-m15

			"vst1.32     {q4-q5},  [%0]!     \n\t"// store m0-m7
			"vst1.32     {q6-q7},  [%0]      \n\t"// store m8-m15
			:
			: "r"(dst), "r"(m)
			: "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "memory"
	);
}

inline void transposeMat4_impl(const float* m, float* dst) {
	asm volatile (
			"vld4.32 {d0[0], d2[0], d4[0], d6[0]}, [%1]!    \n\t" // DST->M[m0, m4, m8, m12] = M[m0-m3]
			"vld4.32 {d0[1], d2[1], d4[1], d6[1]}, [%1]!    \n\t"// DST->M[m1, m5, m9, m12] = M[m4-m7]
			"vld4.32 {d1[0], d3[0], d5[0], d7[0]}, [%1]!    \n\t"// DST->M[m2, m6, m10, m12] = M[m8-m11]
			"vld4.32 {d1[1], d3[1], d5[1], d7[1]}, [%1]     \n\t"// DST->M[m3, m7, m11, m12] = M[m12-m15]

			"vst1.32 {q0-q1}, [%0]!                         \n\t"// DST->M[m0-m7]
			"vst1.32 {q2-q3}, [%0]                          \n\t"// DST->M[m8-m15]
			:
			: "r"(dst), "r"(m)
			: "q0", "q1", "q2", "q3", "memory"
	);
}

inline void transformVec4Components_impl(const float* m, float x, float y, float z, float w, float* dst) {
	asm volatile (
			"vld1.32    {d0[0]},        [%1]    \n\t"    // V[x]
			"vld1.32    {d0[1]},        [%2]    \n\t"// V[y]
			"vld1.32    {d1[0]},        [%3]    \n\t"// V[z]
			"vld1.32    {d1[1]},        [%4]    \n\t"// V[w]
			"vld1.32    {d18 - d21},    [%5]!   \n\t"// M[m0-m7]
			"vld1.32    {d22 - d25},    [%5]    \n\t"// M[m8-m15]

			"vmul.f32 q13,  q9, d0[0]           \n\t"// DST->V = M[m0-m3] * V[x]
			"vmla.f32 q13, q10, d0[1]           \n\t"// DST->V += M[m4-m7] * V[y]
			"vmla.f32 q13, q11, d1[0]           \n\t"// DST->V += M[m8-m11] * V[z]
			"vmla.f32 q13, q12, d1[1]           \n\t"// DST->V += M[m12-m15] * V[w]

			"vst1.32 {d26}, [%0]!               \n\t"// DST->V[x, y]
			"vst1.32 {d27[0]}, [%0]             \n\t"// DST->V[z]
			:
			: "r"(dst), "r"(&x), "r"(&y), "r"(&z), "r"(&w), "r"(m)
			: "q0", "q9", "q10","q11", "q12", "q13", "memory"
	);
}

inline void transformVec4_impl(const float* m, const float* v, float* dst) {
	asm volatile (
			"vld1.32    {d0, d1}, [%1]     \n\t"   // V[x, y, z, w]
			"vld1.32    {d18 - d21}, [%2]! \n\t"// M[m0-m7]
			"vld1.32    {d22 - d25}, [%2]  \n\t"// M[m8-m15]

			"vmul.f32   q13, q9, d0[0]     \n\t"// DST->V = M[m0-m3] * V[x]
			"vmla.f32   q13, q10, d0[1]    \n\t"// DST->V = M[m4-m7] * V[y]
			"vmla.f32   q13, q11, d1[0]    \n\t"// DST->V = M[m8-m11] * V[z]
			"vmla.f32   q13, q12, d1[1]    \n\t"// DST->V = M[m12-m15] * V[w]

			"vst1.32    {d26, d27}, [%0]   \n\t"// DST->V
			:
			: "r"(dst), "r"(v), "r"(m)
			: "q0", "q9", "q10","q11", "q12", "q13", "memory"
	);
}

inline void crossVec3_impl(const float* v1, const float* v2, float* dst) {
	asm volatile(
			"vld1.32 {d1[1]},  [%1]         \n\t" //
			"vld1.32 {d0},     [%2]         \n\t"//
			"vmov.f32 s2, s1                \n\t"// q0 = (v1y, v1z, v1z, v1x)

			"vld1.32 {d2[1]},  [%3]         \n\t"//
			"vld1.32 {d3},     [%4]         \n\t"//
			"vmov.f32 s4, s7                  \n\t"// q1 = (v2z, v2x, v2y, v2z)

			"vmul.f32 d4, d0, d2            \n\t"// x = v1y * v2z, y = v1z * v2x
			"vmls.f32 d4, d1, d3            \n\t"// x -= v1z * v2y, y-= v1x - v2z

			"vmul.f32 d5, d3, d1[1]         \n\t"// z = v1x * v2y
			"vmls.f32 d5, d0, d2[1]         \n\t"// z-= v1y * vx

			"vst1.32 {d4},       [%0]!      \n\t"// V[x, y]
			"vst1.32 {d5[0]}, [%0]          \n\t"// V[z]
			:
			: "r"(dst), "r"(v1), "r"((v1+1)), "r"(v2), "r"((v2+1))
			: "q0", "q1", "q2", "memory"
	);
}

inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	addMat4Scalar_impl(m, scalar, dst);
}

inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	addMat4_impl(m1, m2, dst);
}

inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	subtractMat4_impl(m1, m2, dst);
}

inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	multiplyMat4Scalar_impl(m, scalar, dst);
}

inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	multiplyMat4_impl(m1, m2, dst);
}

inline void negateMat4(const float m[16], float dst[16]) {
	negateMat4_impl(m, dst);
}

inline void transposeMat4(const float m[16], float dst[16]) {
	transposeMat4_impl(m, dst);
}

inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	transformVec4Components_impl(m, x, y, z, w, dst);
}

inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	transformVec4_impl(m, v, dst);
}

inline void crossVec3(const float v1[3], const float v2[3], float dst[3]) {
	crossVec3_impl(v1, v2, dst);
}

#else

inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	sse::addMat4Scalar(m, scalar, dst);
}

inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	sse::addMat4(m1, m2, dst);
}

inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	sse::subtractMat4(m1, m2, dst);
}

inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	sse::multiplyMat4Scalar(m, scalar, dst);
}

inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	sse::multiplyMat4(m1, m2, dst);
}

inline void negateMat4(const float m[16], float dst[16]) {
	sse::negateMat4(m, dst);
}

inline void transposeMat4(const float m[16], float dst[16]) {
	sse::transposeMat4(m, dst);
}

inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	sse::transformVec4Components(m, x, y, z, w, dst);
}

inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	sse::transformVec4(m, v, dst);
}

inline void crossVec3(const float v1[3], const float v2[3], float dst[3]) {
	sse::crossVec3(v1, v2, dst);
}

#endif

// input for test A->B vs C->D (ax, ay, bx, by), (cx, cy, dx, dy)
SP_ATTR_OPTIMIZE_INLINE_FN inline bool isVec2BboxIntersects(const f32x4 & v1, const f32x4 & v2, f32x4 &isect) {
	struct alignas(16) data_t {
	    float data[4];
	} ret;

	simde__m128 v1vec = simde_mm_movelh_ps(v1, v2); // (ax, ay, cx, cy)
	simde__m128 v2vec = simde_mm_movehl_ps(v2, v1); // (bx, by, dx, dy)

	simde__m128 minVec = simde_mm_min_ps(v1vec, v2vec);
	simde__m128 maxVec = simde_mm_max_ps(v1vec, v2vec);

	isect = simde_mm_sub_ps(v2vec, v1vec);

	simde_mm_store_ps(ret.data, simde_mm_sub_ps(
			simde_mm_sub_ps(maxVec, minVec),
			simde_mm_sub_ps(
					simde_mm_movehl_ps(maxVec, minVec),
					minVec) ));

	if (ret.data[0] >= 0.0f && ret.data[1] >= 0.0f && (ret.data[0] != 0.0f || ret.data[1] != 0.0f)) {
		return true;
	}
	return false;
}

}

#endif /* STAPPLER_GEOM_SPSIMD_NEON_H_ */
