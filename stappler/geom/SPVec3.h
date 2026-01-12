/**
 Copyright 2013 BlackBerry Inc.
 Copyright (c) 2014-2015 Chukong Technologies
 Copyright (c) 2017-2022 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef STAPPLER_GEOM_SPVEC3_H_
#define STAPPLER_GEOM_SPVEC3_H_

#include "SPSIMD.h"
#include "SPVec2.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

class Mat4;
class Quaternion;
struct Size3;
struct Extent3;

class SP_PUBLIC Vec3 {
public:
	static constexpr size_t DIMENSIONS = 3;

	static const Vec3 ZERO;
	static const Vec3 ONE;
	static const Vec3 INVALID;
	static const Vec3 UNIT_X;
	static const Vec3 UNIT_Y;
	static const Vec3 UNIT_Z;

	static constexpr void add(const Vec3 &v1, const Vec3 &v2, Vec3 *dst) {
		dst->x = v1.x + v2.x;
		dst->y = v1.y + v2.y;
		dst->z = v1.z + v2.z;
	}

	static constexpr void subtract(const Vec3 &v1, const Vec3 &v2, Vec3 *dst) {
		dst->x = v1.x - v2.x;
		dst->y = v1.y - v2.y;
		dst->z = v1.z - v2.z;
	}

	static constexpr void scale(const Vec3 &v1, const Vec3 &v2, Vec3 *dst) {
		dst->x = v1.x * v2.x;
		dst->y = v1.y * v2.y;
		dst->z = v1.z * v2.z;
	}

	static constexpr void unscale(const Vec3 &v1, const Vec3 &v2, Vec3 *dst) {
		dst->x = v1.x / v2.x;
		dst->y = v1.y / v2.y;
		dst->z = v1.z / v2.z;
	}

	static void cross(const Vec3 &v1, const Vec3 &v2, Vec3 *dst) {
		simd::crossVec3(&v1.x, &v2.x, &dst->x);
	}

	static constexpr float dot(const Vec3 &v1, const Vec3 &v2) {
		return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
	}

	static float angle(const Vec3 &v1, const Vec3 &v2);

	static void clamp(const Vec3 &v, const Vec3 &min, const Vec3 &max, Vec3 *dst);

	template <typename Functor>
	static constexpr std::bitset<3> bitop(const Vec3 &v, const Functor &f) {
		std::bitset<3> ret;
		ret.set(0, f(v.x));
		ret.set(1, f(v.y));
		ret.set(2, f(v.z));
		return ret;
	}

	template <typename Functor>
	static constexpr std::bitset<3> bitop(const Vec3 &v1, const Vec3 &v2, const Functor &f) {
		std::bitset<3> ret;
		ret.set(0, f(v1.x, v2.x));
		ret.set(1, f(v1.y, v2.y));
		ret.set(2, f(v1.z, v2.z));
		return ret;
	}

	static constexpr Vec3 fill(float v) { return Vec3(v, v, v); }

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	constexpr Vec3() = default;

	constexpr Vec3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) { }
	constexpr Vec3(const Vec2 &pt, float zz) : x(pt.x), y(pt.y), z(zz) { }

	constexpr Vec3(const Vec3 &p1, const Vec3 &p2)
	: x(p2.x - p1.x), y(p2.y - p1.y), z(p2.z - p1.z) { }

	template <typename Functor>
	constexpr Vec3(const Vec3 &v, const Functor &f) : x(f(v.x)), y(f(v.y)), z(f(v.z)) { }

	template <typename Functor>
	constexpr Vec3(const Vec3 &v1, const Vec3 &v2, const Functor &f)
	: x(f(v1.x, v2.x)), y(f(v1.y, v2.y)), z(f(v1.z, v2.z)) { }

	constexpr Vec3(const Vec3 &copy) = default;

	Vec3(const SpanView<float> &buf)
	: x(buf.size() > 0 ? buf[0] : nan())
	, y(buf.size() > 1 ? buf[1] : nan())
	, z(buf.size() > 2 ? buf[2] : nan()) { }

	explicit Vec3(const Size3 &);
	explicit Vec3(const Extent3 &);

	constexpr bool isValid() const { return !std::isnan(x) && !std::isnan(y) && !std::isnan(z); }

	constexpr Vec2 xy() const { return Vec2(x, y); }

	constexpr void add(const float &v) {
		x += v;
		y += v;
		z += v;
	}

	constexpr void add(const Vec3 &v) {
		x += v.x;
		y += v.y;
		z += v.z;
	}

	constexpr void subtract(const float &v) {
		x -= v;
		y -= v;
		z -= v;
	}

	constexpr void subtract(const Vec3 &v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
	}

	constexpr void scale(const float &v) {
		x *= v;
		y *= v;
		z *= v;
	}

	constexpr void scale(const Vec3 &v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
	}

	constexpr void unscale(const float &v) {
		x /= v;
		y /= v;
		z /= v;
	}

	constexpr void unscale(const Vec3 &v) {
		x /= v.x;
		y /= v.y;
		z /= v.z;
	}

	void clamp(const Vec3 &min, const Vec3 &max);

	constexpr float distanceSquared(const Vec3 &v) const {
		const float dx = v.x - x;
		const float dy = v.y - y;
		const float dz = v.z - z;

		return (dx * dx + dy * dy + dz * dz);
	}

	constexpr float lengthSquared() const { return (x * x + y * y + z * z); }

	constexpr float distance(const Vec3 &other) const { return std::sqrt(distanceSquared(other)); }

	constexpr float length() const { return std::sqrt(lengthSquared()); }

	constexpr bool isWithinDistance(const Vec3 &v, float val) const {
		return distanceSquared(v) < val * val;
	}

	constexpr float dot(const Vec3 &v) const { return (x * v.x + y * v.y + z * v.z); }

	void cross(const Vec3 &v) { cross(*this, v, this); }

	constexpr void negate() {
		x = -x;
		y = -y;
		z = -z;
	}

	constexpr Vec3 &normalize();

	Vec3 getNormalized() const;

	constexpr bool fuzzyEquals(const Vec3 &b, float var = sprt::Epsilon<float>) const {
		return (x - var <= b.x && b.x <= x + var) && (y - var <= b.y && b.y <= y + var)
				&& (z - var <= b.z && b.z <= z + var);
	}

	constexpr Vec3 &operator+=(const float &v) {
		add(v);
		return *this;
	}

	constexpr Vec3 &operator+=(const Vec3 &v) {
		add(v);
		return *this;
	}

	constexpr Vec3 &operator-=(const float &v) {
		subtract(v);
		return *this;
	}

	constexpr Vec3 &operator-=(const Vec3 &v) {
		subtract(v);
		return *this;
	}

	constexpr Vec3 &operator*=(const float &v) {
		scale(v);
		return *this;
	}

	constexpr Vec3 &operator*=(const Vec3 &s) {
		scale(s);
		return *this;
	}

	constexpr Vec3 &operator/=(const float &v) {
		unscale(v);
		return *this;
	}

	constexpr Vec3 &operator/=(const Vec3 &s) {
		unscale(s);
		return *this;
	}

	constexpr auto operator<=>(const Vec3 &) const = default;
};

constexpr inline Vec3 &Vec3::normalize() {
	float n = x * x + y * y + z * z;
	// Already normalized.
	if (n == 1.0f) {
		return *this;
	}

	n = std::sqrt(n);
	// Too close to zero.
	if (n < math::MATH_TOLERANCE) {
		return *this;
	}

	n = 1.0f / n;
	x *= n;
	y *= n;
	z *= n;

	return *this;
}

constexpr const Vec3 Vec3::ZERO(0.0f, 0.0f, 0.0f);
constexpr const Vec3 Vec3::ONE(1.0f, 1.0f, 1.0f);
constexpr const Vec3 Vec3::INVALID(nan(), nan(), nan());
constexpr const Vec3 Vec3::UNIT_X(1.0f, 0.0f, 0.0f);
constexpr const Vec3 Vec3::UNIT_Y(0.0f, 1.0f, 0.0f);
constexpr const Vec3 Vec3::UNIT_Z(0.0f, 0.0f, 1.0f);

constexpr inline const Vec3 operator+(const Vec3 &l, const Vec3 &r) {
	Vec3 result;
	Vec3::add(l, r, &result);
	return result;
}

constexpr inline const Vec3 operator+(const Vec3 &l, const float &r) {
	Vec3 result(l);
	result.add(r);
	return result;
}

constexpr inline const Vec3 operator+(const float &l, const Vec3 &r) {
	Vec3 result(r);
	result.add(l);
	return result;
}

constexpr inline const Vec3 operator-(const Vec3 &l, const Vec3 &r) {
	Vec3 result;
	Vec3::subtract(l, r, &result);
	return result;
}

constexpr inline const Vec3 operator-(const Vec3 &l, const float &r) {
	Vec3 result(l);
	result.subtract(r);
	return result;
}

constexpr inline const Vec3 operator*(const Vec3 &l, const Vec3 &r) {
	Vec3 result;
	Vec3::scale(l, r, &result);
	return result;
}

constexpr inline const Vec3 operator*(const Vec3 &l, const float &r) {
	Vec3 result(l);
	result.scale(r);
	return result;
}

constexpr inline const Vec3 operator*(const float &l, const Vec3 &r) {
	Vec3 result(r);
	result.scale(l);
	return result;
}

constexpr inline const Vec3 operator/(const Vec3 &l, const Vec3 &r) {
	Vec3 result;
	Vec3::unscale(l, r, &result);
	return result;
}

constexpr inline const Vec3 operator/(const Vec3 &l, const float &r) {
	Vec3 result(l);
	result.unscale(r);
	return result;
}

constexpr Vec3 operator-(const Vec3 &v) {
	Vec3 result(v);
	result.negate();
	return result;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const Vec3 &vec) {
	stream << "(x: " << vec.x << "; y: " << vec.y << "; z: " << vec.z << ")";
	return stream;
}

inline std::basic_ostream<char> &operator<<(std::basic_ostream<char> &os, const Vec3 &vec) {
	os << "(x: " << vec.x << "; y: " << vec.y << "; z: " << vec.z << ")";
	return os;
}

} // namespace stappler::geom

#endif /* STAPPLER_GEOM_SPVEC3_H_ */
