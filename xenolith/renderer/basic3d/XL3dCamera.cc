/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>
 SPDX-License-Identifier: MIT
**/

#include "XL3dCamera.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic3d {

void Camera::setAngles(float yaw, float elevation) {
	_yaw = yaw;
	_elevation = elevation;
}

void Camera::setOrthographic(float pointsPerUnit) {
	_projection = Projection::Orthographic;
	_pointsPerUnit = pointsPerUnit;
}

void Camera::setPerspective(float fieldOfViewDegrees) {
	_projection = Projection::Perspective;
	_fieldOfView = fieldOfViewDegrees;
}

void Camera::setDepthRange(float zNear, float zFar) {
	_near = zNear;
	_far = zFar;
}

void Camera::setViewport(const Rect &pixels, float density) {
	_viewport = pixels;
	_density = density;
}

Vec3 Camera::getEye() const { return _target - getForward() * _distance; }

Vec3 Camera::getForward() const {
	auto c = cosf(_elevation);
	return Vec3(-c * cosf(_yaw), -c * sinf(_yaw), -sinf(_elevation));
}

Mat4 Camera::getViewMatrix() const {
	// Looking straight down, z can not be up; the screen's up is then away from the yaw.
	Vec3 up = Vec3::UNIT_Z;
	if (fabsf(sinf(_elevation)) > 0.9999f) {
		up = Vec3(-cosf(_yaw), -sinf(_yaw), 0.0f);
	}

	Mat4 view;
	Mat4::createLookAt(getEye(), _target, up, &view);
	return view;
}

Mat4 Camera::getProjectionMatrix() const {
	Mat4 proj;
	auto width = sprt::max(_viewport.size.width, 1.0f);
	auto height = sprt::max(_viewport.size.height, 1.0f);
	if (_projection == Projection::Orthographic) {
		auto pixelsPerUnit = _pointsPerUnit * _density;
		Mat4::createOrthographicZO(width / pixelsPerUnit, height / pixelsPerUnit, _near, _far,
				&proj);
	} else {
		Mat4::createPerspectiveZO(_fieldOfView, width / height, sprt::max(_near, 0.01f), _far,
				&proj);
	}
	proj.m[5] = -proj.m[5]; // Vulkan clip y points down
	return proj;
}

Mat4 Camera::getViewProjection() const { return getProjectionMatrix() * getViewMatrix(); }

Vec3 Camera::project(const Vec3 &world) const {
	Vec4 clip;
	getViewProjection().transformVector(world.x, world.y, world.z, 1.0f, &clip);
	auto x = clip.x / clip.w;
	auto y = clip.y / clip.w;

	// clip y is flipped, so the screen's y up is its negation
	return Vec3(_viewport.origin.x + (x + 1.0f) * 0.5f * _viewport.size.width,
			_viewport.origin.y + (1.0f - y) * 0.5f * _viewport.size.height, clip.z / clip.w);
}

Vec3 Camera::unproject(const Vec2 &pixel, float depth) const {
	auto x = (pixel.x - _viewport.origin.x) / _viewport.size.width * 2.0f - 1.0f;
	auto y = 1.0f - (pixel.y - _viewport.origin.y) / _viewport.size.height * 2.0f;

	auto inverse = getViewProjection();
	inverse.inverse();

	Vec4 world;
	inverse.transformVector(x, y, depth, 1.0f, &world);
	return Vec3(world.x / world.w, world.y / world.w, world.z / world.w);
}

void Camera::screenRay(const Vec2 &pixel, Vec3 &origin, Vec3 &direction) const {
	origin = unproject(pixel, 0.0f);
	direction = unproject(pixel, 1.0f) - origin;
	direction.normalize();
}

bool Camera::screenToGround(const Vec2 &pixel, float z, Vec3 &out) const {
	Vec3 origin, direction;
	screenRay(pixel, origin, direction);
	if (fabsf(direction.z) < 1.0e-6f) {
		return false;
	}
	auto t = (z - origin.z) / direction.z;
	out = origin + direction * t;
	return true;
}

Rect Camera::projectAabb(const Vec3 &min, const Vec3 &max) const {
	float x0 = maxOf<float>(), y0 = maxOf<float>();
	float x1 = -x0, y1 = -y0;
	for (uint32_t i = 0; i < 8; ++i) {
		auto p = project(Vec3((i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y,
				(i & 4) ? max.z : min.z));
		x0 = sprt::min(x0, p.x);
		y0 = sprt::min(y0, p.y);
		x1 = sprt::max(x1, p.x);
		y1 = sprt::max(y1, p.y);
	}
	return Rect(x0, y0, x1 - x0, y1 - y0);
}

} // namespace stappler::xenolith::basic3d
