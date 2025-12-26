/**
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

#ifndef CORE_GEOM_SPVEC1_H_
#define CORE_GEOM_SPVEC1_H_

#include "SPGeom.h"

namespace STAPPLER_VERSIONIZED stappler::geom {

class SP_PUBLIC Vec1 {
public:
	static constexpr size_t DIMENSIONS = 1;

	static const Vec1 ZERO;
	static const Vec1 ONE;
	static const Vec1 INVALID;
	static const Vec1 UNIT_X;

	static void add(const Vec1 &v1, const Vec1 &v2, Vec1 *dst) { dst->x = v1.x + v2.x; }

	static void subtract(const Vec1 &v1, const Vec1 &v2, Vec1 *dst) { dst->x = v1.x - v2.x; }

	static void scale(const Vec1 &v1, const Vec1 &v2, Vec1 *dst) { dst->x = v1.x * v2.x; }

	static void unscale(const Vec1 &v1, const Vec1 &v2, Vec1 *dst) { dst->x = v1.x / v2.x; }

	static void clamp(const Vec1 &v, const Vec1 &min, const Vec1 &max, Vec1 *dst) {
		dst->x = math::clamp(v.x, min.x, max.x);
	}

	template <typename Functor>
	static constexpr std::bitset<1> bitop(const Vec1 &v, const Functor &f) {
		std::bitset<1> ret;
		ret.set(0, f(v.x));
		return ret;
	}

	template <typename Functor>
	static constexpr std::bitset<1> bitop(const Vec1 &v1, const Vec1 &v2, const Functor &f) {
		std::bitset<1> ret;
		ret.set(0, f(v1.x, v2.x));
		return ret;
	}

	static constexpr Vec1 fill(float v) { return Vec1(v); }

	float x = 0.0f;

	constexpr Vec1() = default;
	constexpr Vec1(float xx) : x(xx) { }

	constexpr Vec1(const Vec1 &p1, const Vec1 &p2) : x(p2.x - p1.x) { }
	constexpr Vec1(const Vec1 &copy) = default;

	explicit Vec1(const SpanView<float> &buf) : x(buf.size() > 0 ? buf[0] : nan()) { }

	template <typename Functor>
	constexpr Vec1(const Vec1 &v, const Functor &f) : x(f(v.x)) { }

	template <typename Functor>
	constexpr Vec1(const Vec1 &v1, const Vec1 &v2, const Functor &f) : x(f(v1.x, v2.x)) { }

	constexpr bool isValid() const { return !std::isnan(x); }

	void add(const float &v) { x += v; }

	void add(const Vec1 &v) { x += v.x; }

	void subtract(const float &v) { x -= v; }

	void subtract(const Vec1 &v) { x -= v.x; }

	void scale(const float &v) { x *= v; }

	void scale(const Vec1 &v) { x *= v.x; }

	void unscale(const float &v) { x /= v; }

	void unscale(const Vec1 &v) { x /= v.x; }

	void clamp(const Vec1 &min, const Vec1 &max);

	constexpr float distanceSquared(const Vec1 &v) const {
		const float dx = v.x - x;

		return (dx * dx);
	}

	constexpr float lengthSquared() const { return (x * x); }

	constexpr float distance(const Vec1 &v) const { return v.x - x; }

	constexpr float length() const { return x; }

	constexpr bool isWithinDistance(const Vec1 &v, float val) const { return distance(v) < val; }

	constexpr void negate() { x = -x; }

	constexpr Vec1 &normalize();

	constexpr Vec1 getNormalized() const;

	constexpr bool fuzzyEquals(const Vec1 &b, float var = NumericLimits<float>::epsilon()) const {
		return (x - var <= b.x && b.x <= x + var);
	}

	Vec1 &operator+=(const float &v) {
		add(v);
		return *this;
	}

	Vec1 &operator+=(const Vec1 &v) {
		add(v);
		return *this;
	}

	Vec1 &operator-=(const float &v) {
		subtract(v);
		return *this;
	}

	Vec1 &operator-=(const Vec1 &v) {
		subtract(v);
		return *this;
	}

	Vec1 &operator*=(const float &v) {
		scale(v);
		return *this;
	}

	Vec1 &operator*=(const Vec1 &s) {
		scale(s);
		return *this;
	}

	Vec1 &operator/=(const float &v) {
		unscale(v);
		return *this;
	}

	Vec1 &operator/=(const Vec1 &s) {
		unscale(s);
		return *this;
	}

	constexpr auto operator<=>(const Vec1 &) const = default;
};

constexpr inline Vec1 &Vec1::normalize() {
	x = 1.0f;
	return *this;
}

constexpr inline Vec1 Vec1::getNormalized() const {
	Vec1 v(*this);
	v.normalize();
	return v;
}

constexpr const Vec1 Vec1::ZERO = Vec1(0.0f);
constexpr const Vec1 Vec1::ONE = Vec1(1.0f);
constexpr const Vec1 Vec1::INVALID = Vec1(nan());
constexpr const Vec1 Vec1::UNIT_X = Vec1(1.0f);

inline const Vec1 operator+(const Vec1 &l, const Vec1 &r) {
	Vec1 result;
	Vec1::add(l, r, &result);
	return result;
}

inline const Vec1 operator+(const Vec1 &l, const float &r) {
	Vec1 result(l);
	result.add(r);
	return result;
}

inline const Vec1 operator+(const float &l, const Vec1 &r) {
	Vec1 result(r);
	result.add(l);
	return result;
}

inline const Vec1 operator-(const Vec1 &l, const Vec1 &r) {
	Vec1 result;
	Vec1::subtract(l, r, &result);
	return result;
}

inline const Vec1 operator-(const Vec1 &l, const float &r) {
	Vec1 result(l);
	result.subtract(r);
	return result;
}

inline const Vec1 operator*(const Vec1 &l, const Vec1 &r) {
	Vec1 result;
	Vec1::scale(l, r, &result);
	return result;
}

inline const Vec1 operator*(const Vec1 &l, const float &r) {
	Vec1 result(l);
	result.scale(r);
	return result;
}

inline const Vec1 operator*(const float &l, const Vec1 &r) {
	Vec1 result(r);
	result.scale(l);
	return result;
}

inline const Vec1 operator/(const Vec1 &l, const Vec1 &r) {
	Vec1 result;
	Vec1::unscale(l, r, &result);
	return result;
}

inline const Vec1 operator/(const Vec1 &l, const float &r) {
	Vec1 result(l);
	result.unscale(r);
	return result;
}

constexpr Vec1 operator-(const Vec1 &v) {
	Vec1 result(v);
	result.negate();
	return result;
}

inline std::basic_ostream<char> &operator<<(std::basic_ostream<char> &os, const Vec1 &vec) {
	os << "(x: " << vec.x << ")";
	return os;
}

} // namespace stappler::geom

#endif /* CORE_GEOM_SPVEC1_H_ */
