/**
Copyright 2013 BlackBerry Inc.
Copyright (c) 2014-2015 Chukong Technologies
Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Original file from GamePlay3D: http://gameplay3d.org

This file was modified to fit the cocos2d-x project
This file was modified for stappler project
*/

#ifndef STAPPLER_GEOM_SPQUATERNION_H_
#define STAPPLER_GEOM_SPQUATERNION_H_

#include "SPVec3.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

class Mat4;

/**
 * Defines a 4-element quaternion that represents the orientation of an object in space.
 *
 * Quaternions are typically used as a replacement for euler angles and rotation matrices
 * as a way to achieve smooth interpolation and avoid gimbal lock.
 *
 * Note that this quaternion class does not automatically keep the quaternion normalized.
 * Therefore, care must be taken to normalize the quaternion when necessary, by calling
 * the normalize method.
 * This class provides three methods for doing quaternion interpolation: lerp, slerp, and squad.
 *
 * lerp (linear interpolation): the interpolation curve gives a straight line in quaternion
 * space. It is simple and fast to compute. The only problem is that it does not provide
 * constant angular velocity. Note that a constant velocity is not necessarily a requirement
 * for a curve;
 * slerp (spherical linear interpolation): the interpolation curve forms a great arc on the
 * quaternion unit sphere. Slerp provides constant angular velocity;
 * squad (spherical spline interpolation): interpolating between a series of rotations using
 * slerp leads to the following problems:
 * - the curve is not smooth at the control points;
 * - the angular velocity is not constant;
 * - the angular velocity is not continuous at the control points.
 *
 * Since squad is continuously differentiable, it remedies the first and third problems
 * mentioned above.
 * The slerp method provided here is intended for interpolation of principal rotations.
 * It treats +q and -q as the same principal rotation and is at liberty to use the negative
 * of either input. The resulting path is always the shorter arc.
 *
 * The lerp method provided here interpolates strictly in quaternion space. Note that the
 * resulting path may pass through the origin if interpolating between a quaternion and its
 * exact negative.
 *
 * As an example, consider the following quaternions:
 *
 * q1 = (0.6, 0.8, 0.0, 0.0),
 * q2 = (0.0, 0.6, 0.8, 0.0),
 * q3 = (0.6, 0.0, 0.8, 0.0), and
 * q4 = (-0.8, 0.0, -0.6, 0.0).
 * For the point p = (1.0, 1.0, 1.0), the following figures show the trajectories of p
 * using lerp, slerp, and squad.
 */
class SP_PUBLIC Quaternion {
public:
	static void multiply(const Quaternion &q1, const Quaternion &q2, Quaternion *dst);
	static void lerp(const Quaternion &q1, const Quaternion &q2, float t, Quaternion *dst);

	/**
	 * Interpolates between two quaternions using spherical linear interpolation.
	 *
	 * Spherical linear interpolation provides smooth transitions between different
	 * orientations and is often useful for animating models or cameras in 3D.
	 *
	 * Note: For accurate interpolation, the input quaternions must be at (or close to) unit length.
	 * This method does not automatically normalize the input quaternions, so it is up to the
	 * caller to ensure they call normalize beforehand, if necessary.
	 *
	 * @param q1 The first quaternion.
	 * @param q2 The second quaternion.
	 * @param t The interpolation coefficient.
	 * @param dst A quaternion to store the result in.
	 */
	static void slerp(const Quaternion &q1, const Quaternion &q2, float t, Quaternion *dst);

	/**
	 * Interpolates over a series of quaternions using spherical spline interpolation.
	 *
	 * Spherical spline interpolation provides smooth transitions between different
	 * orientations and is often useful for animating models or cameras in 3D.
	 *
	 * Note: For accurate interpolation, the input quaternions must be unit.
	 * This method does not automatically normalize the input quaternions,
	 * so it is up to the caller to ensure they call normalize beforehand, if necessary.
	 *
	 * @param q1 The first quaternion.
	 * @param q2 The second quaternion.
	 * @param s1 The first control point.
	 * @param s2 The second control point.
	 * @param t The interpolation coefficient.
	 * @param dst A quaternion to store the result in.
	 */
	static void squad(const Quaternion &q1, const Quaternion &q2, const Quaternion &s1,
			const Quaternion &s2, float t, Quaternion *dst);

	float x;
	float y;
	float z;
	float w;

	constexpr Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) { }
	constexpr Quaternion(float xx, float yy, float zz, float ww) : x(xx), y(yy), z(zz), w(ww) { }

	/** Constructs a quaternion equal to the rotational part of the specified matrix */
	Quaternion(const Mat4 &m);

	constexpr Quaternion(const Vec3 &eulerAngles) {
		float halfRadx = eulerAngles.x / 2.f, halfRady = eulerAngles.y,
			  halfRadz = -eulerAngles.z / 2.f;
		float coshalfRadx = std::cos(halfRadx), sinhalfRadx = std::sin(halfRadx),
			  coshalfRady = std::cos(halfRady), sinhalfRady = std::sin(halfRady),
			  coshalfRadz = std::cos(halfRadz), sinhalfRadz = std::sin(halfRadz);

		x = sinhalfRadx * coshalfRady * coshalfRadz - coshalfRadx * sinhalfRady * sinhalfRadz;
		y = coshalfRadx * sinhalfRady * coshalfRadz + sinhalfRadx * coshalfRady * sinhalfRadz;
		z = coshalfRadx * coshalfRady * sinhalfRadz - sinhalfRadx * sinhalfRady * coshalfRadz;
		w = coshalfRadx * coshalfRady * coshalfRadz + sinhalfRadx * sinhalfRady * sinhalfRadz;
	}

	/** Constructs a quaternion equal to the rotation from the specified axis and angle. */
	constexpr Quaternion(const Vec3 &axis, float angle) {
		float halfAngle = angle * 0.5f;
		float sinHalfAngle = std::sin(halfAngle);

		Vec3 normal(axis);
		normal.normalize();
		x = normal.x * sinHalfAngle;
		y = normal.y * sinHalfAngle;
		z = normal.z * sinHalfAngle;
		w = std::cos(halfAngle);
	}

	constexpr Quaternion(const Quaternion &copy) = default;

	constexpr bool operator==(const Quaternion &q) const {
		return q.x == x && q.y == y && q.z == z && q.w == w;
	}
	constexpr bool operator!=(const Quaternion &q) const {
		return q.x != x || q.y != y || q.z != z || q.w != w;
	}

	constexpr Vec3 toEulerAngles() const {
		Vec3 ret;
		ret.x = atan2f(2.f * (w * x + y * z), 1.f - 2.f * (x * x + y * y));
		ret.y = asinf(2.f * (w * y - z * x));
		ret.z = -atanf(2.f * (w * z + x * y) / (1.f - 2.f * (y * y + z * z)));
		return ret;
	}

	constexpr bool isIdentity() const { return x == 0.0f && y == 0.0f && z == 0.0f && w == 1.0f; }
	constexpr bool isZero() const { return x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f; }
	constexpr void conjugate() {
		x = -x;
		y = -y;
		z = -z;
	}

	constexpr Quaternion getConjugated() const {
		Quaternion q(*this);
		q.conjugate();
		return q;
	}

	bool inverse();
	void multiply(const Quaternion &q);
	void normalize();

	Quaternion getInversed() const;
	Quaternion getNormalized() const;

	float toAxisAngle(Vec3 *e) const;

	const Quaternion operator*(const Quaternion &q) const {
		Quaternion result(*this);
		result.multiply(q);
		return result;
	}

	Vec3 operator*(const Vec3 &v) const {
		Vec3 uv, uuv;
		Vec3 qvec(x, y, z);
		Vec3::cross(qvec, v, &uv);
		Vec3::cross(qvec, uv, &uuv);

		uv *= (2.0f * w);
		uuv *= 2.0f;

		return v + uv + uuv;
	}

	Quaternion &operator*=(const Quaternion &q) {
		multiply(q);
		return *this;
	}

	static const Quaternion IDENTITY;
	static const Quaternion ZERO;
};

constexpr const Quaternion Quaternion::IDENTITY(0.0f, 0.0f, 0.0f, 1.0f);
constexpr const Quaternion Quaternion::ZERO(0.0f, 0.0f, 0.0f, 0.0f);

} // namespace stappler::geom

#endif /* STAPPLER_GEOM_SPQUATERNION_H_ */
