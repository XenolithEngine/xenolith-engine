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

#ifndef STAPPLER_GEOM_SPVEC4_H
#define STAPPLER_GEOM_SPVEC4_H

#include "SPSIMD.h"
#include "SPVec2.h"
#include "SPVec3.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

class Mat4;

class SP_PUBLIC alignas(16) Vec4 {
public:
	static constexpr size_t DIMENSIONS = 4;

	static const Vec4 ZERO;
	static const Vec4 ONE;
	static const Vec4 IDENTITY;
	static const Vec4 INVALID;
	static const Vec4 UNIT_X;
	static const Vec4 UNIT_Y;
	static const Vec4 UNIT_Z;
	static const Vec4 UNIT_W;

	static void add(const Vec4 &v1, const Vec4 &v2, Vec4 *dst) { simd::add(&v1.x, &v2.x, &dst->x); }

	static void subtract(const Vec4 &v1, const Vec4 &v2, Vec4 *dst) {
		simd::sub(&v1.x, &v2.x, &dst->x);
	}

	static void scale(const Vec4 &v1, const Vec4 &v2, Vec4 *dst) {
		simd::multiply(&v1.x, &v2.x, &dst->x);
	}

	static void unscale(const Vec4 &v1, const Vec4 &v2, Vec4 *dst) {
		simd::divide(&v1.x, &v2.x, &dst->x);
	}

	static float dot(const Vec4 &v1, const Vec4 &v2) {
		return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w);
	}

	static float angle(const Vec4 &v1, const Vec4 &v2);

	static void clamp(const Vec4 &v, const Vec4 &min, const Vec4 &max, Vec4 *dst);

	template <typename Functor>
	static constexpr std::bitset<4> bitop(const Vec4 &v, const Functor &f) {
		std::bitset<4> ret;
		ret.set(0, f(v.x));
		ret.set(1, f(v.y));
		ret.set(2, f(v.z));
		ret.set(3, f(v.w));
		return ret;
	}

	template <typename Functor>
	static constexpr std::bitset<4> bitop(const Vec4 &v1, const Vec4 &v2, const Functor &f) {
		std::bitset<4> ret;
		ret.set(0, f(v1.x, v2.x));
		ret.set(1, f(v1.y, v2.y));
		ret.set(2, f(v1.z, v2.z));
		ret.set(3, f(v1.w, v2.w));
		return ret;
	}

	static constexpr Vec4 fill(float v) { return Vec4(v, v, v, v); }

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 0.0f;

	constexpr Vec4() = default;
	constexpr Vec4(float xx, float yy, float zz, float ww) : x(xx), y(yy), z(zz), w(ww) { }
	constexpr Vec4(const Vec2 &origin, float zz, float ww)
	: x(origin.x), y(origin.y), z(zz), w(ww) { }
	constexpr Vec4(const Vec3 &origin, float ww) : x(origin.x), y(origin.y), z(origin.z), w(ww) { }
	constexpr Vec4(const Vec2 &origin, const Vec2 &extra)
	: x(origin.x), y(origin.y), z(extra.x), w(extra.y) { }

	constexpr Vec4(const Vec4 &p1, const Vec4 &p2)
	: x(p2.x - p1.x), y(p2.y - p1.y), z(p2.z - p1.z), w(p2.w - p1.w) { }
	constexpr Vec4(const Vec4 &copy) = default;

	Vec4(const SpanView<float> &buf)
	: x(buf.size() > 0 ? buf[0] : nan())
	, y(buf.size() > 1 ? buf[1] : nan())
	, z(buf.size() > 2 ? buf[2] : nan())
	, w(buf.size() > 3 ? buf[3] : nan()) { }

	template <typename Functor>
	constexpr Vec4(const Vec4 &v, const Functor &f) : x(f(v.x)), y(f(v.y)), z(f(v.z)), w(f(v.w)) { }

	template <typename Functor>
	constexpr Vec4(const Vec4 &v1, const Vec4 &v2, const Functor &f)
	: x(f(v1.x, v2.x)), y(f(v1.y, v2.y)), z(f(v1.z, v2.z)), w(f(v1.w, v2.w)) { }

	constexpr bool isValid() const {
		return !std::isnan(x) && !std::isnan(y) && !std::isnan(z) && !std::isnan(w);
	}

	constexpr Vec2 xy() const { return Vec2(x, y); }

	constexpr Vec3 xyz() const { return Vec3(x, y, z); }

	void add(const float &v) { simd::add(&this->x, v, &this->x); }

	void add(const Vec4 &v) { simd::add(&this->x, &v.x, &this->x); }

	void subtract(const float &v) { simd::sub(&this->x, v, &this->x); }

	void subtract(const Vec4 &v) { simd::sub(&this->x, &v.x, &this->x); }

	void scale(const float &v) { simd::multiply(&this->x, v, &this->x); }

	void scale(const Vec4 &v) { simd::multiply(&this->x, &v.x, &this->x); }

	void unscale(const float &v) { simd::divide(&this->x, v, &this->x); }

	void unscale(const Vec4 &v) { simd::divide(&this->x, &v.x, &this->x); }

	void clamp(const Vec4 &min, const Vec4 &max);

	constexpr float distanceSquared(const Vec4 &v) const {
		const float dx = v.x - x;
		const float dy = v.y - y;
		const float dz = v.z - z;
		const float dw = v.w - w;

		return (dx * dx + dy * dy + dz * dz + dw * dw);
	}

	constexpr float lengthSquared() const { return (x * x + y * y + z * z + w * w); }

	constexpr float distance(const Vec4 &v) const { return std::sqrt(distanceSquared(v)); }

	constexpr float length() const { return std::sqrt(lengthSquared()); }

	constexpr bool isWithinDistance(const Vec4 &v, float val) const {
		return distanceSquared(v) < val * val;
	}

	constexpr float dot(const Vec4 &v) const { return (x * v.x + y * v.y + z * v.z + w * v.w); }

	constexpr void negate() {
		x = -x;
		y = -y;
		z = -z;
		w = -w;
	}

	constexpr Vec4 &normalize();

	Vec4 getNormalized() const;

	constexpr bool fuzzyEquals(const Vec4 &b, float var = sprt::Epsilon<float>) const {
		return (x - var <= b.x && b.x <= x + var) && (y - var <= b.y && b.y <= y + var)
				&& (z - var <= b.z && b.z <= z + var) && (w - var <= b.w && b.w <= w + var);
	}

	Vec4 &operator+=(const float &v) {
		add(v);
		return *this;
	}

	Vec4 &operator+=(const Vec4 &v) {
		add(v);
		return *this;
	}

	Vec4 &operator-=(const float &v) {
		subtract(v);
		return *this;
	}

	Vec4 &operator-=(const Vec4 &v) {
		subtract(v);
		return *this;
	}

	Vec4 &operator*=(const float &v) {
		scale(v);
		return *this;
	}

	Vec4 &operator*=(const Vec4 &s) {
		scale(s);
		return *this;
	}

	Vec4 &operator/=(const float &v) {
		unscale(v);
		return *this;
	}

	Vec4 &operator/=(const Vec4 &s) {
		unscale(s);
		return *this;
	}

	constexpr auto operator<=>(const Vec4 &) const = default;
};

constexpr inline Vec4 &Vec4::normalize() {
	float n = x * x + y * y + z * z + w * w;
	// Already normalized.
	if (n == 1.0f) {
		return *this;
	}

	n = sqrt(n);
	// Too close to zero.
	if (n < math::MATH_TOLERANCE) {
		return *this;
	}

	n = 1.0f / n;
	x *= n;
	y *= n;
	z *= n;
	w *= n;

	return *this;
}

constexpr const Vec4 Vec4::ZERO = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
constexpr const Vec4 Vec4::ONE = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
constexpr const Vec4 Vec4::IDENTITY = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
constexpr const Vec4 Vec4::INVALID = Vec4(nan(), nan(), nan(), nan());
constexpr const Vec4 Vec4::UNIT_X = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
constexpr const Vec4 Vec4::UNIT_Y = Vec4(0.0f, 1.0f, 0.0f, 0.0f);
constexpr const Vec4 Vec4::UNIT_Z = Vec4(0.0f, 0.0f, 1.0f, 0.0f);
constexpr const Vec4 Vec4::UNIT_W = Vec4(0.0f, 0.0f, 0.0f, 1.0f);

inline const Vec4 operator+(const Vec4 &l, const Vec4 &r) {
	Vec4 result;
	Vec4::add(l, r, &result);
	return result;
}

inline const Vec4 operator+(const Vec4 &l, const float &r) {
	Vec4 result(l);
	result.add(r);
	return result;
}

inline const Vec4 operator+(const float &l, const Vec4 &r) {
	Vec4 result(r);
	result.add(l);
	return result;
}

inline const Vec4 operator-(const Vec4 &l, const Vec4 &r) {
	Vec4 result;
	Vec4::subtract(l, r, &result);
	return result;
}

inline const Vec4 operator-(const Vec4 &l, const float &r) {
	Vec4 result(l);
	result.subtract(r);
	return result;
}

inline const Vec4 operator*(const Vec4 &l, const Vec4 &r) {
	Vec4 result;
	Vec4::scale(l, r, &result);
	return result;
}

inline const Vec4 operator*(const Vec4 &l, const float &r) {
	Vec4 result(l);
	result.scale(r);
	return result;
}

inline const Vec4 operator*(const float &l, const Vec4 &r) {
	Vec4 result(r);
	result.scale(l);
	return result;
}

inline const Vec4 operator/(const Vec4 &l, const Vec4 &r) {
	Vec4 result;
	Vec4::unscale(l, r, &result);
	return result;
}

inline const Vec4 operator/(const Vec4 &l, const float &r) {
	Vec4 result(l);
	result.unscale(r);
	return result;
}

constexpr Vec4 operator-(const Vec4 &v) {
	Vec4 result(v);
	result.negate();
	return result;
}

inline const CallbackStream &operator<<(const CallbackStream &stream, const Vec4 &vec) {
	stream << "(x: " << vec.x << "; y: " << vec.y << "; z: " << vec.z << "; w: " << vec.w << ")";
	return stream;
}

inline std::basic_ostream<char> &operator<<(std::basic_ostream<char> &os, const Vec4 &vec) {
	os << "(x: " << vec.x << "; y: " << vec.y << "; z: " << vec.z << "; w: " << vec.w << ")";
	return os;
}

} // namespace stappler::geom

#endif /* STAPPLER_GEOM_SPVEC4_H */
