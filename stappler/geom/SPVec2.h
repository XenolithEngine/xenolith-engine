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

#ifndef STAPPLER_GEOM_SPVEC2_H_
#define STAPPLER_GEOM_SPVEC2_H_

#include "SPGeom.h"
#include "SPStringView.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

class Mat4;
struct Size2;
struct Extent2;

class SP_PUBLIC Vec2 {
public:
	static constexpr size_t DIMENSIONS = 2;

	static const Vec2 ZERO;
	static const Vec2 ONE;
	static const Vec2 INVALID;
	static const Vec2 UNIT_X;
	static const Vec2 UNIT_Y;

	static constexpr void add(const Vec2 &v1, const Vec2 &v2, Vec2 *dst) {
		dst->x = v1.x + v2.x;
		dst->y = v1.y + v2.y;
	}

	static constexpr void subtract(const Vec2 &v1, const Vec2 &v2, Vec2 *dst) {
		dst->x = v1.x - v2.x;
		dst->y = v1.y - v2.y;
	}

	static constexpr void scale(const Vec2 &v1, const Vec2 &v2, Vec2 *dst) {
		dst->x = v1.x * v2.x;
		dst->y = v1.y * v2.y;
	}

	static constexpr void unscale(const Vec2 &v1, const Vec2 &v2, Vec2 *dst) {
		dst->x = v1.x / v2.x;
		dst->y = v1.y / v2.y;
	}

	static constexpr float cross(const Vec2 &v1, const Vec2 &v2) {
		return v1.x * v2.y - v1.y * v2.x;
	}

	static constexpr float dot(const Vec2 &v1, const Vec2 &v2) { return v1.x * v2.x + v1.y * v2.y; }

	static float angle(const Vec2 &v1, const Vec2 &v2);

	/** Clamps the specified vector within the specified range and returns it in dst. */
	static void clamp(const Vec2 &v, const Vec2 &min, const Vec2 &max, Vec2 *dst);

	static constexpr Vec2 forAngle(const float a) { return Vec2(cosf(a), sinf(a)); }

	/** A general line-line intersection test
	 @param A   the startpoint for the first line L1 = (A - B)
	 @param B   the endpoint for the first line L1 = (A - B)
	 @param C   the startpoint for the second line L2 = (C - D)
	 @param D   the endpoint for the second line L2 = (C - D)
	 @param S   the range for a hitpoint in L1 (p = A + S*(B - A))
	 @param T   the range for a hitpoint in L2 (p = C + T*(D - C))
	 @returns   whether these two lines interects. */
	static bool isLineIntersect(const Vec2 &A, const Vec2 &B, const Vec2 &C, const Vec2 &D,
			float *S = nullptr, float *T = nullptr);

	static bool isLineOverlap(const Vec2 &A, const Vec2 &B, const Vec2 &C, const Vec2 &D);

	static bool isLineParallel(const Vec2 &A, const Vec2 &B, const Vec2 &C, const Vec2 &D);

	static bool isSegmentOverlap(const Vec2 &A, const Vec2 &B, const Vec2 &C, const Vec2 &D,
			Vec2 *S = nullptr, Vec2 *E = nullptr);

	static bool isSegmentIntersect(const Vec2 &A, const Vec2 &B, const Vec2 &C, const Vec2 &D);
	static Vec2 getIntersectPoint(const Vec2 &A, const Vec2 &B, const Vec2 &C, const Vec2 &D);

	template <typename Callback>
	static bool getSegmentIntersectPoint(const Vec2 &A, const Vec2 &B, const Vec2 &C, const Vec2 &D,
			const Callback &);

	static bool isCounterClockwise(const Vec2 &u, const Vec2 &v, const Vec2 &w);

	template <typename Functor>
	static constexpr std::bitset<2> bitop(const Vec2 &v, const Functor &f) {
		std::bitset<2> ret;
		ret.set(0, f(v.x));
		ret.set(1, f(v.y));
		return ret;
	}

	template <typename Functor>
	static constexpr std::bitset<2> bitop(const Vec2 &v1, const Vec2 &v2, const Functor &f) {
		std::bitset<2> ret;
		ret.set(0, f(v1.x, v2.x));
		ret.set(1, f(v1.y, v2.y));
		return ret;
	}

	static constexpr Vec2 fill(float v) { return Vec2(v, v); }

	float x = 0.0f;
	float y = 0.0f;

	constexpr Vec2() = default;

	constexpr Vec2(float xx, float yy) : x(xx), y(yy) { }

	constexpr Vec2(const Vec2 &p1, const Vec2 &p2) : x(p2.x - p1.x), y(p2.y - p1.y) { }

	template <typename Functor>
	constexpr Vec2(const Vec2 &v, const Functor &f) : x(f(v.x)), y(f(v.y)) { }

	template <typename Functor>
	constexpr Vec2(const Vec2 &v1, const Vec2 &v2, const Functor &f)
	: x(f(v1.x, v2.x)), y(f(v1.y, v2.y)) { }

	constexpr Vec2(const Vec2 &copy) = default;

	// disable <int, int> overload resolutions
	explicit Vec2(const SpanView<float> &buf)
	: x(buf.size() > 0 ? buf[0] : nan()), y(buf.size() > 1 ? buf[1] : nan()) { }

	explicit Vec2(const Size2 &);
	explicit Vec2(const Extent2 &);

	constexpr bool isValid() const { return !std::isnan(x) && !std::isnan(y); }

	constexpr void add(const float &v) {
		x += v;
		y += v;
	}

	constexpr void add(const Vec2 &v) {
		x += v.x;
		y += v.y;
	}

	constexpr void subtract(const float &v) {
		x -= v;
		y -= v;
	}

	constexpr void subtract(const Vec2 &v) {
		x -= v.x;
		y -= v.y;
	}

	constexpr void scale(const float &v) {
		x *= v;
		y *= v;
	}

	constexpr void scale(const Vec2 &v) {
		x *= v.x;
		y *= v.y;
	}

	constexpr void unscale(const float &v) {
		x /= v;
		y /= v;
	}

	constexpr void unscale(const Vec2 &v) {
		x /= v.x;
		y /= v.y;
	}

	void clamp(const Vec2 &min, const Vec2 &max);

	constexpr float distanceSquared(const Vec2 &v) const {
		const float dx = v.x - x;
		const float dy = v.y - y;
		return (dx * dx + dy * dy);
	}

	constexpr float lengthSquared() const { return (x * x + y * y); }

	constexpr float distance(const Vec2 &other) const { return std::sqrt(distanceSquared(other)); }

	constexpr float length() const { return std::sqrt(lengthSquared()); }

	constexpr bool isWithinDistance(const Vec2 &v, float val) const {
		return distanceSquared(v) < val * val;
	}

	constexpr float dot(const Vec2 &v) const { return (x * v.x + y * v.y); }

	constexpr float cross(const Vec2 &v) const { return x * v.y - y * v.x; }

	constexpr void negate() {
		x = -x;
		y = -y;
	}

	constexpr Vec2 &normalize();

	Vec2 getNormalized() const;

	constexpr bool fuzzyEquals(const Vec2 &b, float var = NumericLimits<float>::epsilon()) const {

		return (x - var <= b.x && b.x <= x + var) && (y - var <= b.y && b.y <= y + var);
	}

	constexpr float getAngle() const { return std::atan2(y, x); }

	/** Calculates perpendicular of v, rotated 90 degrees counter-clockwise -- cross(v, perp(v)) >= 0 */
	constexpr Vec2 getPerp() const { return Vec2(-y, x); }

	constexpr Vec2 getMidpoint(const Vec2 &other) const {
		return Vec2((x + other.x) / 2.0f, (y + other.y) / 2.0f);
	}

	constexpr Vec2 getClampPoint(const Vec2 &min_inclusive, const Vec2 &max_inclusive) const {
		return Vec2(math::clamp(x, min_inclusive.x, max_inclusive.x),
				math::clamp(y, min_inclusive.y, max_inclusive.y));
	}

	/** Calculates perpendicular of v, rotated 90 degrees clockwise -- cross(v, rperp(v)) <= 0 */
	constexpr Vec2 getRPerp() const { return Vec2(y, -x); }

	/** Calculates the projection of this over other. */
	constexpr Vec2 project(const Vec2 &other) const;

	/** Complex multiplication of two points ("rotates" two points).
	 @return Vec2 vector with an angle of this.getAngle() + other.getAngle(),
	 and a length of this.getLength() * other.getLength(). */
	constexpr Vec2 rotate(const Vec2 &other) const {
		return Vec2(x * other.x - y * other.y, x * other.y + y * other.x);
	}

	/** Unrotates two points.
	 @return Vec2 vector with an angle of this.getAngle() - other.getAngle(),
	 and a length of this.getLength() * other.getLength(). */
	constexpr Vec2 unrotate(const Vec2 &other) const {
		return Vec2(x * other.x + y * other.y, y * other.x - x * other.y);
	}

	float getAngle(const Vec2 &other) const;
	Vec2 rotateByAngle(const Vec2 &pivot, float angle) const;

	void rotate(const Vec2 &point, float angle);

	constexpr Vec2 &operator+=(const float &v) {
		add(v);
		return *this;
	}

	constexpr Vec2 &operator+=(const Vec2 &v) {
		add(v);
		return *this;
	}

	constexpr Vec2 &operator-=(const float &v) {
		subtract(v);
		return *this;
	}

	constexpr Vec2 &operator-=(const Vec2 &v) {
		subtract(v);
		return *this;
	}

	constexpr Vec2 &operator*=(const float &v) {
		scale(v);
		return *this;
	}

	constexpr Vec2 &operator*=(const Vec2 &s) {
		scale(s);
		return *this;
	}

	constexpr Vec2 &operator/=(const float &v) {
		unscale(v);
		return *this;
	}

	constexpr Vec2 &operator/=(const Vec2 &s) {
		unscale(s);
		return *this;
	}

	constexpr auto operator<=>(const Vec2 &) const = default;
};

constexpr inline Vec2 &Vec2::normalize() {
	float n = x * x + y * y;
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

	return *this;
}

constexpr const Vec2 Vec2::ZERO(0.0f, 0.0f);
constexpr const Vec2 Vec2::ONE(1.0f, 1.0f);
constexpr const Vec2 Vec2::INVALID(nan(), nan());
constexpr const Vec2 Vec2::UNIT_X(1.0f, 0.0f);
constexpr const Vec2 Vec2::UNIT_Y(0.0f, 1.0f);

constexpr inline const Vec2 operator+(const Vec2 &l, const Vec2 &r) {
	Vec2 result;
	Vec2::add(l, r, &result);
	return result;
}

constexpr inline const Vec2 operator+(const Vec2 &l, const float &r) {
	Vec2 result(l);
	result.add(r);
	return result;
}

constexpr inline const Vec2 operator+(const float &l, const Vec2 &r) {
	Vec2 result(r);
	result.add(l);
	return result;
}

constexpr inline const Vec2 operator-(const Vec2 &l, const Vec2 &r) {
	Vec2 result;
	Vec2::subtract(l, r, &result);
	return result;
}

constexpr inline const Vec2 operator-(const Vec2 &l, const float &r) {
	Vec2 result(l);
	result.subtract(r);
	return result;
}

constexpr inline const Vec2 operator*(const Vec2 &l, const Vec2 &r) {
	Vec2 result;
	Vec2::scale(l, r, &result);
	return result;
}

constexpr inline const Vec2 operator*(const Vec2 &l, const float &r) {
	Vec2 result(l);
	result.scale(r);
	return result;
}

constexpr inline const Vec2 operator*(const float &l, const Vec2 &r) {
	Vec2 result(r);
	result.scale(l);
	return result;
}

constexpr inline const Vec2 operator/(const Vec2 &l, const Vec2 &r) {
	Vec2 result;
	Vec2::unscale(l, r, &result);
	return result;
}

constexpr inline const Vec2 operator/(const Vec2 &l, const float &r) {
	Vec2 result(l);
	result.unscale(r);
	return result;
}

constexpr Vec2 operator-(const Vec2 &v) {
	Vec2 result(v);
	result.negate();
	return result;
}

constexpr inline Vec2 Vec2::project(const Vec2 &other) const {
	return other * (dot(other) / other.dot(other));
}

namespace Anchor {

constexpr const Vec2 Middle(0.5f, 0.5f);
constexpr const Vec2 BottomLeft(0.0f, 0.0f);
constexpr const Vec2 TopLeft(0.0f, 1.0f);
constexpr const Vec2 BottomRight(1.0f, 0.0f);
constexpr const Vec2 TopRight(1.0f, 1.0f);
constexpr const Vec2 MiddleRight(1.0f, 0.5f);
constexpr const Vec2 MiddleLeft(0.0f, 0.5f);
constexpr const Vec2 MiddleTop(0.5f, 1.0f);
constexpr const Vec2 MiddleBottom(0.5f, 0.0f);

} // namespace Anchor

inline const CallbackStream &operator<<(const CallbackStream &stream, const Vec2 &vec) {
	stream << "(x: " << vec.x << "; y: " << vec.y << ")";
	return stream;
}

inline std::basic_ostream<char> &operator<<(std::basic_ostream<char> &os, const Vec2 &vec) {
	memory::makeCallback(os) << vec;
	return os;
}

template <typename Callback>
inline bool Vec2::getSegmentIntersectPoint(const Vec2 &A, const Vec2 &B, const Vec2 &C,
		const Vec2 &D, const Callback &cb) {
	static_assert(std::is_invocable_v<Callback, const Vec2 &, float, float>,
			"Invalid callback type");
	float S, T;

	const auto minXAB = std::min(A.x, B.x);
	const auto minYAB = std::min(A.y, B.y);

	const auto minXCD = std::min(C.x, D.x);
	const auto minYCD = std::min(C.y, D.y);

	const auto maxXAB = std::max(A.x, B.x);
	const auto maxYAB = std::max(A.y, B.y);

	const auto internalBoxWidth = (maxXAB - minXAB) - (minXCD - minXAB);
	const auto internalBoxheight = (maxYAB - minYAB) - (minYCD - minYAB);

	if (internalBoxWidth > 0.0f && internalBoxheight > 0.0f) {
		if (isLineIntersect(A, B, C, D, &S, &T) && (S > 0.0f && S < 1.0f && T > 0.0f && T < 1.0f)) {
			// Vec2 of intersection
			cb(Vec2(A.x + S * (B.x - A.x), A.y + S * (B.y - A.y)), S, T);
			return true;
		}
	}

	return false;
}

inline bool Vec2::isCounterClockwise(const Vec2 &u, const Vec2 &v, const Vec2 &w) {
	return (u.x * (v.y - w.y) + v.x * (w.y - u.y) + w.x * (u.y - v.y)) >= 0;
}

} // namespace stappler::geom

#endif /* STAPPLER_GEOM_SPVEC2_H_ */
