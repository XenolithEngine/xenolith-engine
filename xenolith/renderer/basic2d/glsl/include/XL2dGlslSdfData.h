/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLSDFDATA_H_
#define XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLSDFDATA_H_

#include "SPGlsl.h"

#ifndef SP_GLSL
namespace STAPPLER_VERSIONIZED stappler::glsl {
#endif

struct Sdf2DObjectData {
	uint object;
	uint transform;
	float value;
	float opacity;

	vec2 bbMin;
	vec2 bbMax;

	vec2 a;
	vec2 b;

	vec2 c;
	vec2 d;

	// 0 - triangle
	// 1 - Circle
	// 2 - Rect
	// 3 - Rounded rect
	// 4 - Polygon
	uint type;
	uint padding0;
	uint origin;
	uint count;
};

struct Circle2DIndex {
	uint origin;
	uint transform;
	float value;
	float opacity;
};

struct Triangle2DIndex {
	uint a;
	uint b;
	uint c;
	uint transform;
	float value;
	float opacity;
};

struct Rect2DIndex {
	uint origin;
	uint transform;
	float value;
	float opacity;
};

struct RoundedRect2DIndex {
	uint origin;
	uint transform;
	float value;
	float opacity;
};

struct Polygon2DIndex {
	uint origin;
	uint count;
	uint transform;
	uint padding;
	float value;
	float opacity;
};

SP_GLSL_INLINE float triangle2d(SP_GLSL_IN(vec2) p, SP_GLSL_IN(vec2) a, SP_GLSL_IN(vec2) b,
		SP_GLSL_IN(vec2) c) {
	vec2 e0 = b - a;
	vec2 e1 = c - b;
	vec2 e2 = a - c;

	vec2 v0 = p - a;
	vec2 v1 = p - b;
	vec2 v2 = p - c;

	vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), float(0.0), float(1.0));
	vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), float(0.0), float(1.0));
	vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), float(0.0), float(1.0));

	float s = e0.x * e2.y - e0.y * e2.x;
	vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
						 vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
			vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));

	return -sqrt(d.x) * sign(d.y);
}

SP_GLSL_INLINE float circle2d(SP_GLSL_IN(vec2) p, SP_GLSL_IN(vec2) origin,
		SP_GLSL_IN(float) radius) {
	return length(p - origin) - radius;
}

SP_GLSL_INLINE float circle3d(SP_GLSL_IN(vec3) p, SP_GLSL_IN(vec2) origin, SP_GLSL_IN(float) radius,
		SP_GLSL_IN(float) value, SP_GLSL_IN(vec4) scale) {
	vec2 originVector = vec2(p.x, p.y) - origin;
	float l = length(originVector);
	float d = l - radius;
	float height = abs(value - p.z);

	if (d <= float(0.0)) {
		return height;
	} else {
		vec2 normal = originVector / l;
		vec2 targetVector = normal * d * vec2(scale.x, scale.y);
		float ds = length(targetVector);
		return sqrt(ds * ds + height * height);
	}
}

SP_GLSL_INLINE float dot2(SP_GLSL_IN(vec3) v) { return dot(v, v); }

SP_GLSL_INLINE float triangle3d(SP_GLSL_IN(vec3) p, SP_GLSL_IN(vec2) a, SP_GLSL_IN(vec2) b,
		SP_GLSL_IN(vec2) c, SP_GLSL_IN(float) value) {
	vec3 v1 = vec3(a, value);
	vec3 v2 = vec3(b, value);
	vec3 v3 = vec3(c, value);

	vec3 v21 = v2 - v1;
	vec3 p1 = p - v1;
	vec3 v32 = v3 - v2;
	vec3 p2 = p - v2;
	vec3 v13 = v1 - v3;
	vec3 p3 = p - v3;
	vec3 nor = cross(v21, v13);

	return sqrt((sign(dot(cross(v21, nor), p1)) + sign(dot(cross(v32, nor), p2))
								+ sign(dot(cross(v13, nor), p3))
						< 2.0)
					? min(min(dot2(v21 * clamp(dot(v21, p1) / dot2(v21), float(0.0), float(1.0))
									  - p1),
								  dot2(v32 * clamp(dot(v32, p2) / dot2(v32), float(0.0), float(1.0))
										  - p2)),
							  dot2(v13 * clamp(dot(v13, p3) / dot2(v13), float(0.0), float(1.0))
									  - p3))
					: dot(nor, p1) * dot(nor, p1) / dot2(nor));
}

SP_GLSL_INLINE float rect2d(SP_GLSL_IN(vec2) p, SP_GLSL_IN(vec2) origin, SP_GLSL_IN(vec2) size) {
	vec2 d = abs(p - origin) - size;
	return length(max(d, vec2(0.0f, 0.0f))) + min(max(d.x, d.y), float(0));
}

SP_GLSL_INLINE float rect3d(SP_GLSL_IN(vec3) p, SP_GLSL_IN(vec2) origin, SP_GLSL_IN(vec2) size,
		SP_GLSL_IN(float) value, SP_GLSL_IN(vec4) scale) {
	vec2 originVector = abs(vec2(p.x, p.y) - origin) - size;
	float height = abs(value - p.z);

	if (all(lessThanEqual(originVector, vec2(0.0f, 0.0f)))) {
		return height;
	} else {
		float ds = length(max(originVector, vec2(0.0f, 0.0f)) * vec2(scale.x, scale.y))
				+ min(max(originVector.x * scale.x, originVector.y * scale.y), float(0));
		return sqrt(ds * ds + height * height);
	}
}

SP_GLSL_INLINE float roundedRect2d(SP_GLSL_IN(vec2) p, SP_GLSL_IN(vec2) origin,
		SP_GLSL_IN(vec2) size, SP_GLSL_IN(vec4) corners) {
	p = p - origin;
#if SP_GLSL
	corners.xy = (p.x > 0.0) ? corners.xy : corners.zw;
	corners.x = (p.y > 0.0) ? corners.x : corners.y;
#endif
	vec2 q = abs(p) - size + corners.x;
	return min(max(q.x, q.y), float(0.0)) + length(max(q, vec2(0.0f, 0.0f))) - corners.x;
}

SP_GLSL_INLINE float roundedRect3d(SP_GLSL_IN(vec3) p, SP_GLSL_IN(vec2) origin,
		SP_GLSL_IN(vec2) size, SP_GLSL_IN(vec4) corners, SP_GLSL_IN(float) value,
		SP_GLSL_IN(vec4) scale) {
	vec2 pt = vec2(p.x, p.y) - origin;
#if SP_GLSL
	corners.xy = (pt.x > 0.0) ? corners.xy : corners.zw;
	corners.x = (pt.y > 0.0) ? corners.x : corners.y;
#endif
	vec2 originVector = abs(pt) - size + corners.x;
	float height = abs(value - p.z);

	if (all(lessThanEqual(originVector, vec2(0.0f, 0.0f)))) {
		return height;
	} else {
		float ds = length(max(originVector, vec2(0.0f, 0.0f)) * vec2(scale.x, scale.y))
				+ min(max(originVector.x * scale.x, originVector.y * scale.y), float(0))
				- corners.x * (scale.x + scale.y) * 0.5;
		return sqrt(ds * ds + height * height);
	}
}
#ifndef SP_GLSL
}
#endif

#endif /* XENOLITH_RENDERER_BASIC2D_GLSL_INCLUDE_XL2DGLSLSDFDATA_H_ */
