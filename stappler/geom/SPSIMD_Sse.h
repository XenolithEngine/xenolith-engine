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

#ifndef STAPPLER_GEOM_SPSIMD_SSE_H_
#define STAPPLER_GEOM_SPSIMD_SSE_H_

#include "SPSIMD.h"
#include "simde/x86/sse.h"

#if __SSE__
#define SP_SIMD_SSE_STORE_VEC4(vec, value)	*((simde__m128 *)&vec.x) = (value)
#define SP_SIMD_SSE_LOAD_VEC4(vec)			*((simde__m128 *)(&vec.x))
#else
#define SP_SIMD_SSE_STORE_VEC4(vec, value)	simde_mm_store_ps(&vec.x, value)
#define SP_SIMD_SSE_LOAD_VEC4(vec)			simde_mm_load_ps(&vec.x)
#endif

namespace STAPPLER_VERSIONIZED stappler::simd::sse {

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

[[maybe_unused]] SP_ATTR_OPTIMIZE_INLINE_FN inline void loadMat4_impl(const float m[16], simde__m128 dst[4]) {
	dst[0] = simde_mm_load_ps(&m[0]);
	dst[1] = simde_mm_load_ps(&m[4]);
	dst[2] = simde_mm_load_ps(&m[8]);
	dst[3] = simde_mm_load_ps(&m[12]);
}

[[maybe_unused]] SP_ATTR_OPTIMIZE_INLINE_FN inline void storeMat4_impl(const simde__m128 m[4], float dst[16]) {
	simde_mm_store_ps((simde_float32 *)&dst[0], m[0]);
	simde_mm_store_ps((simde_float32 *)&dst[4], m[1]);
	simde_mm_store_ps((simde_float32 *)&dst[8], m[2]);
	simde_mm_store_ps((simde_float32 *)&dst[12], m[3]);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4Scalar_impl(const simde__m128 *m, float scalar, simde__m128 *dst) {
	auto s = simde_mm_set1_ps(scalar);
	dst[0] = simde_mm_add_ps(m[0], s);
	dst[1] = simde_mm_add_ps(m[1], s);
	dst[2] = simde_mm_add_ps(m[2], s);
	dst[3] = simde_mm_add_ps(m[3], s);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4_impl(const simde__m128 *m1, const simde__m128 *m2, simde__m128 *dst) {
	dst[0] = simde_mm_add_ps(m1[0], m2[0]);
	dst[1] = simde_mm_add_ps(m1[1], m2[1]);
	dst[2] = simde_mm_add_ps(m1[2], m2[2]);
	dst[3] = simde_mm_add_ps(m1[3], m2[3]);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void subtractMat4_impl(const simde__m128 *m1, const simde__m128 *m2, simde__m128 *dst) {
	dst[0] = simde_mm_sub_ps(m1[0], m2[0]);
	dst[1] = simde_mm_sub_ps(m1[1], m2[1]);
	dst[2] = simde_mm_sub_ps(m1[2], m2[2]);
	dst[3] = simde_mm_sub_ps(m1[3], m2[3]);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4Scalar_impl(const simde__m128 *m, float scalar, simde__m128 *dst) {
	auto s = simde_mm_set1_ps(scalar);
	dst[0] = simde_mm_mul_ps(m[0], s);
	dst[1] = simde_mm_mul_ps(m[1], s);
	dst[2] = simde_mm_mul_ps(m[2], s);
	dst[3] = simde_mm_mul_ps(m[3], s);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4_impl(const simde__m128 m1[4], const simde__m128 m2[4], simde__m128 dst[4]) {
	simde__m128 dst0, dst1, dst2, dst3;
	{
		simde__m128 e0 = simde_mm_shuffle_ps(m2[0], m2[0], SIMDE_MM_SHUFFLE(0, 0, 0, 0));
		simde__m128 e1 = simde_mm_shuffle_ps(m2[0], m2[0], SIMDE_MM_SHUFFLE(1, 1, 1, 1));
		simde__m128 e2 = simde_mm_shuffle_ps(m2[0], m2[0], SIMDE_MM_SHUFFLE(2, 2, 2, 2));
		simde__m128 e3 = simde_mm_shuffle_ps(m2[0], m2[0], SIMDE_MM_SHUFFLE(3, 3, 3, 3));

		simde__m128 v0 = simde_mm_mul_ps(m1[0], e0);
		simde__m128 v1 = simde_mm_mul_ps(m1[1], e1);
		simde__m128 v2 = simde_mm_mul_ps(m1[2], e2);
		simde__m128 v3 = simde_mm_mul_ps(m1[3], e3);

		simde__m128 a0 = simde_mm_add_ps(v0, v1);
		simde__m128 a1 = simde_mm_add_ps(v2, v3);
		simde__m128 a2 = simde_mm_add_ps(a0, a1);

		dst0 = a2;
	}

	{
		simde__m128 e0 = simde_mm_shuffle_ps(m2[1], m2[1], SIMDE_MM_SHUFFLE(0, 0, 0, 0));
		simde__m128 e1 = simde_mm_shuffle_ps(m2[1], m2[1], SIMDE_MM_SHUFFLE(1, 1, 1, 1));
		simde__m128 e2 = simde_mm_shuffle_ps(m2[1], m2[1], SIMDE_MM_SHUFFLE(2, 2, 2, 2));
		simde__m128 e3 = simde_mm_shuffle_ps(m2[1], m2[1], SIMDE_MM_SHUFFLE(3, 3, 3, 3));

		simde__m128 v0 = simde_mm_mul_ps(m1[0], e0);
		simde__m128 v1 = simde_mm_mul_ps(m1[1], e1);
		simde__m128 v2 = simde_mm_mul_ps(m1[2], e2);
		simde__m128 v3 = simde_mm_mul_ps(m1[3], e3);

		simde__m128 a0 = simde_mm_add_ps(v0, v1);
		simde__m128 a1 = simde_mm_add_ps(v2, v3);
		simde__m128 a2 = simde_mm_add_ps(a0, a1);

		dst1 = a2;
	}

	{
		simde__m128 e0 = simde_mm_shuffle_ps(m2[2], m2[2], SIMDE_MM_SHUFFLE(0, 0, 0, 0));
		simde__m128 e1 = simde_mm_shuffle_ps(m2[2], m2[2], SIMDE_MM_SHUFFLE(1, 1, 1, 1));
		simde__m128 e2 = simde_mm_shuffle_ps(m2[2], m2[2], SIMDE_MM_SHUFFLE(2, 2, 2, 2));
		simde__m128 e3 = simde_mm_shuffle_ps(m2[2], m2[2], SIMDE_MM_SHUFFLE(3, 3, 3, 3));

		simde__m128 v0 = simde_mm_mul_ps(m1[0], e0);
		simde__m128 v1 = simde_mm_mul_ps(m1[1], e1);
		simde__m128 v2 = simde_mm_mul_ps(m1[2], e2);
		simde__m128 v3 = simde_mm_mul_ps(m1[3], e3);

		simde__m128 a0 = simde_mm_add_ps(v0, v1);
		simde__m128 a1 = simde_mm_add_ps(v2, v3);
		simde__m128 a2 = simde_mm_add_ps(a0, a1);

		dst2 = a2;
	}

	{
		simde__m128 e0 = simde_mm_shuffle_ps(m2[3], m2[3], SIMDE_MM_SHUFFLE(0, 0, 0, 0));
		simde__m128 e1 = simde_mm_shuffle_ps(m2[3], m2[3], SIMDE_MM_SHUFFLE(1, 1, 1, 1));
		simde__m128 e2 = simde_mm_shuffle_ps(m2[3], m2[3], SIMDE_MM_SHUFFLE(2, 2, 2, 2));
		simde__m128 e3 = simde_mm_shuffle_ps(m2[3], m2[3], SIMDE_MM_SHUFFLE(3, 3, 3, 3));

		simde__m128 v0 = simde_mm_mul_ps(m1[0], e0);
		simde__m128 v1 = simde_mm_mul_ps(m1[1], e1);
		simde__m128 v2 = simde_mm_mul_ps(m1[2], e2);
		simde__m128 v3 = simde_mm_mul_ps(m1[3], e3);

		simde__m128 a0 = simde_mm_add_ps(v0, v1);
		simde__m128 a1 = simde_mm_add_ps(v2, v3);
		simde__m128 a2 = simde_mm_add_ps(a0, a1);

		dst3 = a2;
	}
	dst[0] = dst0;
	dst[1] = dst1;
	dst[2] = dst2;
	dst[3] = dst3;
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void negateMat4_impl(const simde__m128 m[4], simde__m128 dst[4]) {
	simde__m128 z = simde_mm_setzero_ps();
	dst[0] = simde_mm_sub_ps(z, m[0]);
	dst[1] = simde_mm_sub_ps(z, m[1]);
	dst[2] = simde_mm_sub_ps(z, m[2]);
	dst[3] = simde_mm_sub_ps(z, m[3]);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transposeMat4_impl(const simde__m128 m[4], simde__m128 dst[4]) {
	simde__m128 tmp0 = simde_mm_shuffle_ps(m[0], m[1], 0x44);
	simde__m128 tmp2 = simde_mm_shuffle_ps(m[0], m[1], 0xEE);
	simde__m128 tmp1 = simde_mm_shuffle_ps(m[2], m[3], 0x44);
	simde__m128 tmp3 = simde_mm_shuffle_ps(m[2], m[3], 0xEE);

	dst[0] = simde_mm_shuffle_ps(tmp0, tmp1, 0x88);
	dst[1] = simde_mm_shuffle_ps(tmp0, tmp1, 0xDD);
	dst[2] = simde_mm_shuffle_ps(tmp2, tmp3, 0x88);
	dst[3] = simde_mm_shuffle_ps(tmp2, tmp3, 0xDD);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4Components_impl(const simde__m128 m[4], float x, float y, float z, float w, simde__m128& dst) {
	simde__m128 col1 = simde_mm_set1_ps(x);
	simde__m128 col2 = simde_mm_set1_ps(y);
	simde__m128 col3 = simde_mm_set1_ps(z);
	simde__m128 col4 = simde_mm_set1_ps(w);

	dst = simde_mm_add_ps(
			simde_mm_add_ps(simde_mm_mul_ps(m[0], col1), simde_mm_mul_ps(m[1], col2)),
			simde_mm_add_ps(simde_mm_mul_ps(m[2], col3), simde_mm_mul_ps(m[3], col4))
	);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4_impl(const simde__m128 m[4], const simde__m128 &v, simde__m128& dst) {
	simde__m128 col1 = simde_mm_shuffle_ps(v, v, SIMDE_MM_SHUFFLE(0, 0, 0, 0));
	simde__m128 col2 = simde_mm_shuffle_ps(v, v, SIMDE_MM_SHUFFLE(1, 1, 1, 1));
	simde__m128 col3 = simde_mm_shuffle_ps(v, v, SIMDE_MM_SHUFFLE(2, 2, 2, 2));
	simde__m128 col4 = simde_mm_shuffle_ps(v, v, SIMDE_MM_SHUFFLE(3, 3, 3, 3));

	dst = simde_mm_add_ps(
		simde_mm_add_ps(simde_mm_mul_ps(m[0], col1), simde_mm_mul_ps(m[1], col2)),
		simde_mm_add_ps(simde_mm_mul_ps(m[2], col3), simde_mm_mul_ps(m[3], col4))
	);
}

#if SP_GEOM_DEFAULT_SIMD == SP_GEOM_DEFAULT_SIMD_SSE

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	addMat4Scalar_impl((const simde__m128 *)m, scalar, (simde__m128 *)dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	addMat4_impl((const simde__m128 *)m1, (const simde__m128 *)m2, (simde__m128 *)dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	subtractMat4_impl((const simde__m128 *)m1, (const simde__m128 *)m2, (simde__m128 *)dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	multiplyMat4Scalar_impl((const simde__m128 *)m, scalar, (simde__m128 *)dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	multiplyMat4_impl((const simde__m128 *)m1, (const simde__m128 *)m2, (simde__m128 *)dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void negateMat4(const float m[16], float dst[16]) {
	negateMat4_impl((const simde__m128 *)m, (simde__m128 *)dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transposeMat4(const float m[16], float dst[16]) {
	transposeMat4_impl((const simde__m128 *)m, (simde__m128 *)dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	transformVec4Components_impl((const simde__m128 *)m, x, y, z, w, *(simde__m128 *)dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	transformVec4_impl((const simde__m128 *)m, *(const simde__m128 *)v, *(simde__m128 *)dst);
}

#else

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4Scalar(const float m[16], float scalar, float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM[4];

	loadMat4_impl(m, tmpM);
	addMat4Scalar_impl(tmpM, scalar, dstM);
	storeMat4_impl(dstM, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void addMat4(const float m1[16], const float m2[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM1[4];
	simde__m128 tmpM2[4];

	loadMat4_impl(m1, tmpM1);
	loadMat4_impl(m2, tmpM2);
	addMat4_impl(tmpM1, tmpM2, dstM);
	storeMat4_impl(dstM, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void subtractMat4(const float m1[16], const float m2[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM1[4];
	simde__m128 tmpM2[4];

	loadMat4_impl(m1, tmpM1);
	loadMat4_impl(m2, tmpM2);
	subtractMat4_impl(tmpM1, tmpM2, dstM);
	storeMat4_impl(dstM, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4Scalar(const float m[16], float scalar, float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM[4];

	loadMat4_impl(m, tmpM);
	multiplyMat4Scalar_impl(tmpM, scalar, dstM);
	storeMat4_impl(dstM, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void multiplyMat4(const float m1[16], const float m2[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM1[4];
	simde__m128 tmpM2[4];

	loadMat4_impl(m1, tmpM1);
	loadMat4_impl(m2, tmpM2);
	multiplyMat4_impl(tmpM1, tmpM2, dstM);
	storeMat4_impl(dstM, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void negateMat4(const float m[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM[4];

	loadMat4_impl(m, tmpM);
	negateMat4_impl(tmpM, dstM);
	storeMat4_impl(dstM, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transposeMat4(const float m[16], float dst[16]) {
	simde__m128 dstM[4];
	simde__m128 tmpM[4];

	loadMat4_impl(m, tmpM);
	transposeMat4_impl(tmpM, dstM);
	storeMat4_impl(dstM, dst);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4Components(const float m[16], float x, float y, float z, float w, float dst[4]) {
	simde__m128 tmpM[4];
	simde__m128 dstV;
	loadMat4_impl(m, tmpM);

	transformVec4Components_impl(tmpM, x, y, z, w, dstV);
	simde_mm_store_ps((simde_float32 *)dst, dstV);
}

SP_ATTR_OPTIMIZE_INLINE_FN inline void transformVec4(const float m[16], const float v[4], float dst[4]) {
	simde__m128 tmpM[4];
	simde__m128 dstV;
	loadMat4_impl(m, tmpM);

	transformVec4_impl(tmpM, simde_mm_load_ps(v), dstV);
	simde_mm_store_ps((simde_float32 *)dst, dstV);
}

#endif

SP_ATTR_OPTIMIZE_INLINE_FN inline void crossVec3(const float v1[3], const float v2[3], float dst[3]) {
	const float x = (v1[1] * v2[2]) - (v1[2] * v2[1]);
	const float y = (v1[2] * v2[0]) - (v1[0] * v2[2]);
	const float z = (v1[0] * v2[1]) - (v1[1] * v2[0]);

	dst[0] = x;
	dst[1] = y;
	dst[2] = z;
}

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

#endif /* STAPPLER_GEOM_SPSIMD_SSE_H_ */
