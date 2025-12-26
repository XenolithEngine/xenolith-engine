/**
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

#ifndef CORE_TESS_SPTESSSIMD_HPP_
#define CORE_TESS_SPTESSSIMD_HPP_

#include "SPSIMD.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

SP_ATTR_OPTIMIZE_INLINE_FN static inline simd::f32x4 getNormalizedVec(const float v0[2], const float v1[2], const float v2[2]) {
	simd::f32x4 targetVec = simd::sub(
		simd::load(v0[0], v0[1], v2[0], v2[1]),
		simd::load(v1[0], v1[1], v1[0], v1[1])); // x0, y0, x1, y1

	simd::f32x4 squaredVec = simd::mul(targetVec, targetVec); // x0 * x0, y0 * y0, x1 * x1, y1 * y1

	return simd::mul(targetVec,
		simd::rsqrt( simd::add(squaredVec,
			simde_mm_shuffle_ps(squaredVec, squaredVec, SIMDE_MM_SHUFFLE(2, 3, 0, 1))) ) // sse_rsqrt: n0, n0, n1, n1
		); // nx0, ny0, nx1, ny1
}

SP_ATTR_OPTIMIZE_INLINE_FN static inline simd::f32x4 getBisectVec(const simd::f32x4 &normVec) {
	simd::f32x4 tmpNHalf = simd::load(-0.5f);
	simd::f32x4 normShuffleVec = simde_mm_shuffle_ps(normVec, normVec, SIMDE_MM_SHUFFLE(0, 1, 3, 2)); // nx1, ny1, ny0, nx0

	simd::f32x4 dotVec = simd::mul(normVec, normShuffleVec); // nx0 * nx1, ny0 * ny1, nx1 * ny0, ny1 * nx0

	simd::f32x4 crossVec = simd::sub(
		simde_mm_movehl_ps(dotVec, dotVec),
		simde_mm_shuffle_ps(dotVec, dotVec, SIMDE_MM_SHUFFLE(2, 3, 2, 3)));

	if (simde_mm_comieq_ss(crossVec, simde_mm_setzero_ps())) [[unlikely]] {

		// normal is -ny1, nx1, length is 1

		// return
		// 0.0f
		// 1.0f
		// -ny1
		// nx1

		return simd::mul(
			simd::load(0.0f, 1.0f, -1.0f, 1.0f),
			simde_mm_shuffle_ps(simd::load(1.0f), normVec, SIMDE_MM_SHUFFLE(2, 3, 1, 0))
		);
	} else {
		// -0.5      + -0.5
		// nx0 * nx1 + ny0 * ny1
		// nx0       + nx1
		// ny0       + ny1
		simd::f32x4 normTarget = simd::add(
			simde_mm_movelh_ps(simde_mm_unpacklo_ps(tmpNHalf, dotVec), normVec),   // -0.5, nx0 * nx1, nx0, ny0,
			simde_mm_movelh_ps(simde_mm_move_ss(dotVec, tmpNHalf), normShuffleVec) // -0.5, ny0 * ny1, nx1, ny1,
		); // -1.0 , dot , tx (nx0 + nx1) , ty (ny0 + ny1)

		// -0.5  * -1.0
		// -0.5 * dotValue
		// tx   * tx
		// ty   * ty

		simd::f32x4 squaredVec = simd::mul(simde_mm_movehl_ps(normTarget, tmpNHalf), normTarget); // 0.5, -0.5 * dot, tx * tx, ty * ty

		// combined normalizing, length calculation, ccw test
		// ccw = nx1 * ny0 - ny1 * nx0
		// len = 1.0 * rsqrt( 0.5   + (-0.5 * dot)
		// ntx = tx *  rsqrt( tx*tx +  ty*ty )
		// nty = ty *  rsqrt( tx*tx +  ty*ty )

		return simde_mm_move_ss(
			simd::mul(simde_mm_movehl_ps(normTarget, simd::load(1.0f)),
				simd::rsqrt(
					simd::add(
						squaredVec,
						simde_mm_shuffle_ps(squaredVec, squaredVec, SIMDE_MM_SHUFFLE(2, 3, 0, 1))
					)
				)
			),
			crossVec
		);
	}
}

// combined normalizing, length calculation, ccw test for 3 vertexes
// ccw = nx1 * ny0 - ny1 * nx0
// len = 1.0 * rsqrt( 0.5   + (-0.5 * dot)
// ntx = tx *  rsqrt( tx*tx +  ty*ty )
// nty = ty *  rsqrt( tx*tx +  ty*ty )

SP_ATTR_OPTIMIZE_FN static void getVertexNormal(const float v0[2], const float v1[], const float v2[], float result[4]) {
	simd::f32x4 normVec = getNormalizedVec(v0, v1, v2);
	simd::f32x4 bisectVec = getBisectVec(normVec);
	simd::store(result, bisectVec);
}

}

#endif /* CORE_TESS_SPTESSSIMD_HPP_ */
