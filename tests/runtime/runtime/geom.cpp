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

#include <sprt/runtime/stream.h>
#include <sprt/runtime/geom/geom.h>
#include <sprt/runtime/geom/mat4.h>
#include <sprt/runtime/geom/vec3.h>
#include <sprt/runtime/geom/vec4.h>

#include "../tests.h"

// The geometry that runs on the platform's SIMD path (simd_neon64.h on aarch64, simd_neon.h on
// armv7, SSE elsewhere). The NEON asm once advanced its pointer operands with post-increment
// addressing while declaring them input-only; the compiler, free to keep such a register for the
// next inlined call, then wrote that call's result 8 bytes past where it read it. Only an
// optimized build inlines the calls next to each other, so a debug build passes regardless:
// the cases below are for release builds on ARM targets (BF-77, where TransformRect lost three
// of its four corners and the damage of every frame collapsed to the middle of the screen).

namespace sprt {

using namespace sprt::geom;

static unsigned s_geomFailures = 0;

static void check(bool ok, const char *what) {
	if (!ok) {
		++s_geomFailures;
		sprt::cerr << sprt::test::failed("  FAIL: ") << what << "\n";
	}
}

static bool same(const Rect &a, const Rect &b) {
	return a.origin.x == b.origin.x && a.origin.y == b.origin.y && a.size.width == b.size.width
			&& a.size.height == b.size.height;
}

static bool same(const Mat4 &a, const Mat4 &b) {
	for (int i = 0; i < 16; ++i) {
		if (a.m[i] != b.m[i]) {
			return false;
		}
	}
	return true;
}

static bool near(float a, float b, float eps = 1.0e-5f) {
	const float d = a - b;
	return (d < 0.0f ? -d : d) <= eps;
}

// A view-space point through a projection: (x, y, z) after the divide by w, and w itself
static Vec4 project(const Mat4 &proj, float x, float y, float z) {
	Vec4 clip;
	proj.transformVector(x, y, z, 1.0f, &clip);
	return Vec4(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w, clip.w);
}

// The projections: GL keeps depth in [-1, 1], the ZO variants put it in [0, 1] for Vulkan, WebGPU
// and Metal. The view looks along -z with near and far as positive distances, as createLookAt
// builds it.
static void performProjectionTests() {
	const float zNear = 2.0f, zFar = 50.0f;

	Mat4 orthoGl, orthoZo;
	Mat4::createOrthographic(8.0f, 6.0f, zNear, zFar, &orthoGl);
	Mat4::createOrthographicZO(8.0f, 6.0f, zNear, zFar, &orthoZo);

	check(near(project(orthoZo, 0.0f, 0.0f, -zNear).z, 0.0f)
					&& near(project(orthoZo, 0.0f, 0.0f, -zFar).z, 1.0f),
			"Mat4::createOrthographicZO: near at depth 0, far at depth 1");
	auto corner = project(orthoZo, 4.0f, -3.0f, -10.0f);
	check(near(corner.x, 1.0f) && near(corner.y, -1.0f) && near(corner.w, 1.0f),
			"Mat4::createOrthographicZO: half extents at the clip edges");
	check(near(project(orthoGl, 0.0f, 0.0f, -zNear).z, -1.0f)
					&& near(project(orthoGl, 0.0f, 0.0f, -zFar).z, 1.0f),
			"Mat4::createOrthographic: near at depth -1, far at depth 1");

	Mat4 offCenter;
	Mat4::createOrthographicOffCenterZO(1.0f, 5.0f, -2.0f, 4.0f, zNear, zFar, &offCenter);
	auto lo = project(offCenter, 1.0f, -2.0f, -zNear);
	auto hi = project(offCenter, 5.0f, 4.0f, -zFar);
	check(near(lo.x, -1.0f) && near(lo.y, -1.0f) && near(lo.z, 0.0f) && near(hi.x, 1.0f)
					&& near(hi.y, 1.0f) && near(hi.z, 1.0f),
			"Mat4::createOrthographicOffCenterZO: asymmetric bounds at the clip edges");

	Mat4 perspGl, perspZo;
	Mat4::createPerspective(60.0f, 1.5f, zNear, zFar, &perspGl);
	Mat4::createPerspectiveZO(60.0f, 1.5f, zNear, zFar, &perspZo);

	auto pNear = project(perspZo, 0.0f, 0.0f, -zNear);
	auto pFar = project(perspZo, 0.0f, 0.0f, -zFar);
	check(near(pNear.z, 0.0f) && near(pFar.z, 1.0f) && near(pNear.w, zNear)
					&& near(pFar.w, zFar),
			"Mat4::createPerspectiveZO: near at depth 0, far at depth 1, w is the distance");
	check(near(project(perspGl, 0.0f, 0.0f, -zNear).z, -1.0f)
					&& near(project(perspGl, 0.0f, 0.0f, -zFar).z, 1.0f),
			"Mat4::createPerspective: near at depth -1, far at depth 1");
	check(near(perspZo.m[0], perspGl.m[0]) && near(perspZo.m[5], perspGl.m[5]),
			"Mat4::createPerspectiveZO: x and y as createPerspective");

	// The two conventions differ by the remap z' = (z + w) / 2 in clip space, at any point
	bool remapped = true;
	const float points[][3] = {{0.5f, -0.25f, -3.0f}, {-1.0f, 2.0f, -17.0f}, {3.0f, 1.0f, -44.0f}};
	for (auto &p : points) {
		Vec4 gl, zo;
		orthoGl.transformVector(p[0], p[1], p[2], 1.0f, &gl);
		orthoZo.transformVector(p[0], p[1], p[2], 1.0f, &zo);
		remapped = remapped && near(zo.z, 0.5f * gl.z + 0.5f * gl.w, 1.0e-4f);
		perspGl.transformVector(p[0], p[1], p[2], 1.0f, &gl);
		perspZo.transformVector(p[0], p[1], p[2], 1.0f, &zo);
		remapped = remapped && near(zo.z, 0.5f * gl.z + 0.5f * gl.w, 1.0e-4f)
				&& near(zo.w, gl.w);
	}
	check(remapped, "ZO projections: z is the GL z remapped by (z + w) / 2");
}

void performGeomTests() {
	// scale 2, then move by (5, 7): every corner of the rect lands somewhere else
	const Mat4 affine(2.0f, 0.0f, 0.0f, 2.0f, 5.0f, 7.0f);

	// four transformPoint calls inlined side by side, all four corners must count
	check(same(TransformRect(Rect(0.0f, 0.0f, 10.0f, 20.0f), affine), Rect(5.0f, 7.0f, 20.0f, 40.0f)),
			"TransformRect: scale and translate");

	// a flip moves the maximum to the minimum: each corner in turn is the one the box is taken from
	const Mat4 flip(-1.0f, 0.0f, 0.0f, -1.0f, 100.0f, 50.0f);
	check(same(TransformRect(Rect(10.0f, 5.0f, 30.0f, 15.0f), flip), Rect(60.0f, 30.0f, 30.0f, 15.0f)),
			"TransformRect: flip");

	Vec2 p0(0.0f, 0.0f), p1(10.0f, 0.0f), p2(0.0f, 20.0f), p3(10.0f, 20.0f);
	affine.transformPoint(&p0);
	affine.transformPoint(&p1);
	affine.transformPoint(&p2);
	affine.transformPoint(&p3);
	check(p0 == Vec2(5.0f, 7.0f) && p1 == Vec2(25.0f, 7.0f) && p2 == Vec2(5.0f, 47.0f)
					&& p3 == Vec2(25.0f, 47.0f),
			"Mat4::transformPoint: four in a row");

	Vec3 x(1.0f, 0.0f, 0.0f), y(0.0f, 1.0f, 0.0f), z1, z2;
	Vec3::cross(x, y, &z1);
	Vec3::cross(y, x, &z2);
	check(z1 == Vec3(0.0f, 0.0f, 1.0f) && z2 == Vec3(0.0f, 0.0f, -1.0f), "Vec3::cross: twice");

	// the matrix operations write 16 floats through a pointer they advance on the way
	Mat4 prod, sum, diff, neg;
	Mat4::multiply(affine, affine, &prod);
	Mat4::add(prod, affine, &sum);
	Mat4::subtract(sum, affine, &diff);
	neg = diff;
	neg.transpose();
	neg.transpose();
	neg.negate();
	neg.negate();
	check(same(prod, Mat4(4.0f, 0.0f, 0.0f, 4.0f, 15.0f, 21.0f)), "Mat4::multiply");
	check(same(diff, prod), "Mat4::add, Mat4::subtract");
	check(same(neg, prod), "Mat4::transpose, Mat4::negate");

	performProjectionTests();

	sprt::cout << "geom tests: " << (s_geomFailures == 0 ? "ALL PASS" : "FAILURES") << "\n";
}

} // namespace sprt
