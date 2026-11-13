/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#ifndef XENOLITH_RENDERER_BASIC3D_XL3DCAMERA_H_
#define XENOLITH_RENDERER_BASIC3D_XL3DCAMERA_H_

#include "XL3d.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

/* A camera that looks at a target from a yaw, an elevation and a distance, over a world whose z
points up. Its matrices put depth in [0, 1] and flip clip y, as Vulkan expects. Screen positions
are pixels of the scene with y up - the space of Node::getWorldBoundingBox and of input locations -
inside the viewport rect.

The defaults are the isometric game camera: yaw 45 deg, elevation 30 deg (a ground cell is a 2:1
rhombus), orthographic at 64 points per unit. */
class SP_PUBLIC Camera {
public:
	enum class Projection {
		Orthographic,
		Perspective,
	};

	void setTarget(const Vec3 &target) { _target = target; }
	const Vec3 &getTarget() const { return _target; }

	// yaw turns around +z from +x, elevation is above the ground plane; both in radians
	void setAngles(float yaw, float elevation);
	float getYaw() const { return _yaw; }
	float getElevation() const { return _elevation; }

	// from the target to the eye
	void setDistance(float distance) { _distance = distance; }
	float getDistance() const { return _distance; }

	// points of the scene per world unit, at density 1; pixels are points times the density
	void setOrthographic(float pointsPerUnit);
	void setPerspective(float fieldOfViewDegrees);

	Projection getProjection() const { return _projection; }
	float getPointsPerUnit() const { return _pointsPerUnit; }
	float getFieldOfView() const { return _fieldOfView; }

	// distances from the eye along the view; perspective keeps the near plane above zero
	void setDepthRange(float zNear, float zFar);
	float getNear() const { return _near; }
	float getFar() const { return _far; }

	void setViewport(const Rect &pixels, float density);
	const Rect &getViewport() const { return _viewport; }
	float getDensity() const { return _density; }

	Vec3 getEye() const;

	// unit length, from the eye toward the target
	Vec3 getForward() const;

	Mat4 getViewMatrix() const;
	Mat4 getProjectionMatrix() const;
	Mat4 getViewProjection() const;

	// screen pixel (x, y) and depth in [0, 1] of a world point
	Vec3 project(const Vec3 &world) const;

	// the world point at a screen pixel and a depth in [0, 1]
	Vec3 unproject(const Vec2 &pixel, float depth) const;

	// the ray through a screen pixel: its origin on the near plane, its direction of unit length
	void screenRay(const Vec2 &pixel, Vec3 &origin, Vec3 &direction) const;

	// where the ray through a pixel meets the plane at height z; false when it runs along it
	bool screenToGround(const Vec2 &pixel, float z, Vec3 &out) const;

	// the screen rect that holds a world box
	Rect projectAabb(const Vec3 &min, const Vec3 &max) const;

protected:
	Vec3 _target = Vec3::ZERO;
	float _yaw = 0.78539816f;
	float _elevation = 0.52359878f;
	float _distance = 100.0f;

	Projection _projection = Projection::Orthographic;
	float _pointsPerUnit = 64.0f;
	float _fieldOfView = 45.0f;
	float _near = 0.0f;
	float _far = 200.0f;

	Rect _viewport = Rect(0.0f, 0.0f, 1.0f, 1.0f);
	float _density = 1.0f;
};

} // namespace stappler::xenolith::basic3d

#endif /* XENOLITH_RENDERER_BASIC3D_XL3DCAMERA_H_ */
