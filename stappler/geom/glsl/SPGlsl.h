/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#ifndef CORE_GEOM_GLSL_SPGLSL_H_
#define CORE_GEOM_GLSL_SPGLSL_H_

#if SP_GLSL

#define color4 vec4
#define SP_GLSL_IN(type) in type
#define SP_GLSL_INOUT(type) inout type
#define SP_GLSL_OUT(type) out type
#define SP_GLSL_INLINE

const float M_SQRT1_2 = 0.70710678118654752440;
const float M_PI = 3.14159265358979323846;

#else

#include "SPVec2.h"
#include "SPVec3.h"
#include "SPVec4.h"
#include "SPGeometry.h"
#include "SPMat4.h"
#include "SPColor.h"

#define SP_GLSL_IN(type) type
#define SP_GLSL_INOUT(type) type &
#define SP_GLSL_OUT(type) type &
#define SP_GLSL_INLINE inline

namespace STAPPLER_VERSIONIZED stappler::glsl {

using vec2 = geom::Vec2;
using vec3 = geom::Vec3;
using vec4 = geom::Vec4;
using mat4 = geom::Mat4;
using uint = uint32_t;
using color4 = geom::Color4F;

using uvec2 = geom::UVec2;
using uvec3 = geom::UVec3;
using uvec4 = geom::UVec4;

template <typename T>
inline T abs(const T &v) {
	return geom::_abs(v);
}

template <typename T>
inline bool all(const T &v) {
	return v.all();
}

template <typename T>
inline bool any(const T &v) {
	return v.any();
}

using math::clamp;

template <typename T>
inline T ceil(const T &v) {
	return geom::_ceil(v);
}

template <typename T>
inline T cross(const T &v1, const T &v2) {
	T ret;
	T::cross(v1, v2, &ret);
	return ret;
}

template <typename T>
inline float distance(const T &v1, const T &v2) {
	return v1.distance(v2);
}

template <typename T>
inline float dot(const T &v1, const T &v2) {
	return T::dot(v1, v2);
}

template <typename T>
inline auto equal(const T &v1, const T &v2) {
	return geom::_equal(v1, v2);
}

template <typename T>
inline auto floor(const T &v1) {
	return geom::_floor(v1);
}

template <typename T>
inline auto fract(const T &v1) {
	return geom::_fract(v1);
}

template <typename T>
inline auto greaterThan(const T &v1, const T &v2) {
	return geom::_greaterThan(v1, v2);
}

template <typename T>
inline auto greaterThanEqual(const T &v1, const T &v2) {
	return geom::_greaterThanEqual(v1, v2);
}

template <typename T>
inline auto inversesqrt(const T &v) {
	return geom::_inversesqrt(v);
}

template <typename T>
inline auto isinf(const T &v) {
	return geom::_isinf(v);
}

template <typename T>
inline auto isnan(const T &v) {
	return geom::_isnan(v);
}

template <typename T>
inline float length(const T &value) {
	return value.length();
}

inline float length(float v) { return v; }

template <typename T>
inline auto lessThan(const T &v1, const T &v2) {
	return geom::_lessThan(v1, v2);
}

template <typename T>
inline auto lessThanEqual(const T &v1, const T &v2) {
	return geom::_lessThanEqual(v1, v2);
}

template <typename T>
inline T max(const T &v1, const T &v2) {
	return geom::_max(v1, v2);
}

template <typename T>
inline T min(const T &v1, const T &v2) {
	return geom::_min(v1, v2);
}

template <typename T>
inline T normalize(const T &value) {
	return value.getNormalized();
}

template <typename T>
inline T sign(const T &value) {
	return geom::_sign(value);
}

template <typename T>
inline T step(const T &v1, const T &v2) {
	return geom::_step(v1, v2);
}

template <typename T>
inline T step(const float &v1, const T &v2) {
	return geom::_step(geom::fill<T>(v1), v2);
}

template <typename T>
inline T trunc(const T &v) {
	return geom::_trunc(v);
}

template <typename T>
inline bool none(const T &v) {
	return v.none();
}

template <typename T>
inline auto notEqual(const T &v1, const T &v2) {
	return geom::_notEqual(v1, v2);
}

template <typename T>
inline auto round(const T &v1) {
	return geom::_round(v1);
}

template <typename T, typename V>
inline T mix(const T &x, const T &y, const V &a) {
	return geom::_mix(x, y, a);
}

template <typename T>
inline T mix(const T &x, const T &y, const bool &a) {
	return geom::_mix(x, y, a);
}

template <typename T>
inline T smoothstep(const float &edge0, const float &edge1, const T &x) {
	return geom::_smoothstep(edge0, edge1, x);
}

#endif

// clang-format off
#define PCG_DEFAULT_MULTIPLIER_32  747796405U
// clang-format on

struct pcg16_state_t {
	uint state;
	uint inc;
};

SP_GLSL_INLINE uint pcg_rotr_16(uint value, uint rot) {
	return (value >> rot) | (value << ((-rot) & 15));
}

SP_GLSL_INLINE void pcg16_srandom_r(SP_GLSL_INOUT(pcg16_state_t) rng, uint initstate,
		uint initseq) {
	rng.state = 0U;
	rng.inc = (initseq << 1u) | 1u;
	rng.state = rng.state * PCG_DEFAULT_MULTIPLIER_32 + rng.inc;
	rng.state += initstate;
	rng.state = rng.state * PCG_DEFAULT_MULTIPLIER_32 + rng.inc;
}

SP_GLSL_INLINE uint pcg16_random_r(SP_GLSL_INOUT(pcg16_state_t) rng) {
	uint value = ((rng.state >> 10u) ^ rng.state) >> 12u;
	uint rot = rng.state >> 28u;

	rng.state = rng.state * PCG_DEFAULT_MULTIPLIER_32 + rng.inc;

	return (value >> rot) | (value << ((-rot) & 15));
}

SP_GLSL_INLINE uint pcg16_random_full_r(SP_GLSL_INOUT(pcg16_state_t) rng) {
	return (pcg16_random_r(rng) & 0xFFFF) << 16 | (pcg16_random_r(rng) & 0xFFFF);
}

SP_GLSL_INLINE float pcg16_random_float_r(SP_GLSL_INOUT(pcg16_state_t) rng) {
	return ldexp(float(pcg16_random_r(rng)), -16);
}

SP_GLSL_INLINE float pcg16_random_full_float_r(SP_GLSL_INOUT(pcg16_state_t) rng) {
	return ldexp(float(pcg16_random_full_r(rng)), -32);
}

SP_GLSL_INLINE uint pcg16_boundedrand_r(SP_GLSL_INOUT(pcg16_state_t) rng, uint bound) {
	uint threshold = (uint(~bound - 1)) % bound;
	for (;;) {
		uint r = pcg16_random_r(rng);
		if (r >= threshold) {
			return r % bound;
		}
	}
}

SP_GLSL_INLINE void pcg16_advance_r(SP_GLSL_INOUT(pcg16_state_t) rng, uint delta) {
	uint cur_mult = PCG_DEFAULT_MULTIPLIER_32;
	uint cur_plus = rng.inc;
	uint acc_mult = 1u;
	uint acc_plus = 0u;
	while (delta > 0) {
		if ((delta & 1) != 0) {
			acc_mult *= cur_mult;
			acc_plus = acc_plus * cur_mult + cur_plus;
		}
		cur_plus = (cur_mult + 1) * cur_plus;
		cur_mult *= cur_mult;
		delta /= 2;
	}
	rng.state = acc_mult * rng.state + acc_plus;
}

#if SP_GLSL
#else

} // namespace stappler::glsl

#endif

#endif /* CORE_GEOM_GLSL_SPGLSL_H_ */
