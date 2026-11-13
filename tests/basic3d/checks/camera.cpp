/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XLCommon.h"
#include "XL3dCamera.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

using test::check;
using test::near;

// The screen rect of a unit ground cell at the origin
static Rect cellRect(const Camera &camera) {
	return camera.projectAabb(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 0.0f));
}

void performCameraTests() {
	Camera camera;
	camera.setViewport(Rect(0.0f, 0.0f, 1'000.0f, 800.0f), 1.0f);

	// yaw 45, elevation 30: a ground cell is a rhombus twice as wide as high, sqrt(2) * 64 wide
	auto cell = cellRect(camera);
	check(near(cell.size.width / cell.size.height, 2.0f, 1.0e-3f),
			"camera: a ground cell is a 2:1 rhombus",
			toString(cell.size.width, "x", cell.size.height));
	check(near(cell.size.width, 90.5097f, 0.01f),
			"camera: its width is sqrt(2) times the points per unit", toString(cell.size.width));

	// the target is in the middle of the viewport, half way through the depth range
	auto centre = camera.project(Vec3::ZERO);
	check(near(centre.x, 500.0f, 1.0e-3f) && near(centre.y, 400.0f, 1.0e-3f)
					&& near(centre.z, 0.5f, 1.0e-5f),
			"camera: the target is at the viewport's centre, depth 0.5",
			toString(centre.x, ",", centre.y, ",", centre.z));

	// the scene's y is up: higher in the world is higher on the screen
	check(camera.project(Vec3(0.0f, 0.0f, 1.0f)).y > centre.y, "camera: up in the world is up");

	// nearer the eye is less deep
	auto toEye = camera.getEye() - camera.getTarget();
	toEye.normalize();
	check(camera.project(toEye * 10.0f).z < centre.z, "camera: nearer the eye is less deep");

	// pixels are points times the density
	auto dense = camera;
	dense.setViewport(Rect(0.0f, 0.0f, 2'000.0f, 1'600.0f), 2.0f);
	check(near(cellRect(dense).size.width, cell.size.width * 2.0f, 0.01f),
			"camera: at density 2 a cell is twice as many pixels");

	// a pixel back to the ground: screenToGround undoes project, at any height and zoom
	bool roundTrip = true;
	String detail;
	for (float points : {16.0f, 64.0f, 200.0f}) {
		auto zoomed = camera;
		zoomed.setOrthographic(points);
		for (float z : {0.0f, 3.5f}) {
			for (float x = -6.0f; x <= 6.0f; x += 1.5f) {
				for (float y = -4.0f; y <= 4.0f; y += 2.0f) {
					Vec3 p(x, y, z), out;
					auto s = zoomed.project(p);
					if (!zoomed.screenToGround(Vec2(s.x, s.y), z, out) || !near(out.x, x, 1.0e-3f)
							|| !near(out.y, y, 1.0e-3f) || !near(out.z, z, 1.0e-3f)) {
						roundTrip = false;
						detail = toString(points, " pt: ", x, ",", y, ",", z, " -> ", out.x, ",",
								out.y, ",", out.z);
					}
				}
			}
		}
	}
	check(roundTrip, "camera: screenToGround(project(p), p.z) is p", detail);

	// every corner of a box is inside its projected rect
	Vec3 bmin(-1.0f, -2.0f, 0.0f), bmax(3.0f, 1.0f, 2.0f);
	auto rect = camera.projectAabb(bmin, bmax);
	bool inside = true;
	for (uint32_t i = 0; i < 8; ++i) {
		auto p = camera.project(Vec3((i & 1) ? bmax.x : bmin.x, (i & 2) ? bmax.y : bmin.y,
				(i & 4) ? bmax.z : bmin.z));
		inside = inside && p.x >= rect.getMinX() - 1.0e-3f && p.x <= rect.getMaxX() + 1.0e-3f
				&& p.y >= rect.getMinY() - 1.0e-3f && p.y <= rect.getMaxY() + 1.0e-3f;
	}
	check(inside, "camera: projectAabb holds every corner of the box");

	// perspective: the near plane is depth 0, the far one depth 1, and the round trip holds
	auto persp = camera;
	persp.setPerspective(45.0f);
	persp.setDepthRange(1.0f, 300.0f);
	auto forward = persp.getForward();
	auto eye = persp.getEye();
	auto nearZ = persp.project(eye + forward * 1.0f).z;
	auto farZ = persp.project(eye + forward * 300.0f).z;
	check(near(nearZ, 0.0f, 1.0e-4f) && near(farZ, 1.0f, 1.0e-4f),
			"camera: perspective depth goes from 0 at near to 1 at far",
			toString(nearZ, " ", farZ));
	Vec3 pp(2.0f, -3.0f, 0.0f), back;
	auto ps = persp.project(pp);
	check(persp.screenToGround(Vec2(ps.x, ps.y), 0.0f, back) && near(back.x, pp.x, 1.0e-2f)
					&& near(back.y, pp.y, 1.0e-2f),
			"camera: perspective screenToGround undoes project",
			toString(back.x, ",", back.y, ",", back.z));

	// looking straight down still makes a matrix
	auto down = camera;
	down.setAngles(0.3f, 1.5707963f);
	auto d = down.project(Vec3(1.0f, 1.0f, 0.0f));
	check(d.x == d.x && d.y == d.y && d.z == d.z, "camera: looking straight down is defined");
}

} // namespace stappler::xenolith::basic3d
