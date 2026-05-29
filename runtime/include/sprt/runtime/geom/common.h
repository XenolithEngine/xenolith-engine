/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_COMMON_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_COMMON_H_

#include <sprt/cxx/detail/ctypes.h>
#include <sprt/cxx/algorithm>
#include <sprt/cxx/bitset>
#include <sprt/cxx/cmath>

namespace sprt::geom {

namespace {

template <typename T>
struct _StructFieldCount {
	static constexpr size_t Count = T::DIMENSIONS;
};

template <>
struct _StructFieldCount<float> {
	static constexpr size_t Count = 1;
};

template <typename T>
struct _ApplyTrait {
	using Bitset = sprt::bitset<_StructFieldCount<T>::Count>;

	template <typename Functor>
	static constexpr T apply(const T &t, const Functor &f) {
		return T(t, f);
	}

	template <typename Functor>
	static constexpr T apply(const T &t1, const T &t2, const Functor &f) {
		return T(t1, t2, f);
	}

	template <typename Functor>
	static constexpr Bitset bitop(const T &t, const Functor &f) {
		return T::bitop(t, f);
	}

	template <typename Functor>
	static constexpr Bitset bitop(const T &t1, const T &t2, const Functor &f) {
		return T::bitop(t1, t2, f);
	}
};

template <>
struct _ApplyTrait<float> {
	using Bitset = sprt::bitset<_StructFieldCount<float>::Count>;

	template <typename Functor>
	static constexpr float apply(const float &t, const Functor &f) {
		return f(t);
	}

	template <typename Functor>
	static constexpr float apply(const float &t1, const float &t2, const Functor &f) {
		return f(t1, t2);
	}

	template <typename Functor>
	static constexpr Bitset bitop(const float &t, const Functor &f) {
		Bitset ret;
		ret.set(0, f(t));
		return ret;
	}

	template <typename Functor>
	static constexpr Bitset bitop(const float &t1, const float &t2, const Functor &f) {
		Bitset ret;
		ret.set(0, f(t1, t2));
		return ret;
	}
};

} // namespace

template <typename T>
inline constexpr T fill(float v) {
	return T::fill(v);
}

inline constexpr float fill(float v) { return v; }

template <typename T, typename Functor>
inline constexpr T apply(const T &t, const Functor &f) {
	return _ApplyTrait<T>::apply(t, f);
}

template <typename T, typename Functor>
inline constexpr T apply(const T &t1, const T &t2, const Functor &f) {
	return _ApplyTrait<T>::apply(t1, t2, f);
}

template <typename T, typename Functor>
inline constexpr auto bitop(const T &t, const Functor &f)
		-> sprt::bitset<_StructFieldCount<T>::Count> {
	return _ApplyTrait<T>::bitop(t, f);
}

template <typename T, typename Functor>
inline constexpr auto bitop(const T &t1, const T &t2, const Functor &f)
		-> sprt::bitset<_StructFieldCount<T>::Count> {
	return _ApplyTrait<T>::bitop(t1, t2, f);
}

template <typename T>
inline constexpr T _abs(const T &t) {
	return apply(t, [](float v) { return sprt::abs(v); });
}

template <typename T>
inline constexpr T _ceil(const T &t) {
	return apply(t, [](float v) { return sprt::ceil(v); });
}

template <typename T>
inline constexpr T _floor(const T &t) {
	return apply(t, [](float v) { return sprt::floor(v); });
}

template <typename T>
inline constexpr T _trunc(const T &t) {
	return apply(t, [](float v) { return sprt::trunc(v); });
}

template <typename T>
inline constexpr T _fract(const T &t) {
	return apply(t, [](float v) {
		float tmp;
		return sprt::modf(v, &tmp);
	});
}

template <typename T>
inline constexpr T _round(const T &t) {
	return apply(t, [](float v) { return sprt::round(v); });
}

template <typename T, typename V>
inline constexpr T _mix(const T &x, const T &y, const V &a) {
	return math::lerp(x, y, a);
}

template <typename T>
inline constexpr T _mix(const T &x, const T &y, bool a) {
	return a ? x : y;
}

template <typename T>
inline constexpr T _smoothstep(const float &edge0, const float &edge1, const T &x) {
	return apply(x, [&](float v) { return math::smoothstep(edge0, edge1, v); });
}

template <typename T>
inline constexpr T _sign(const T &t) {
	return apply(t, [](float v) { return sprt::copysign(1.0f, v); });
}

template <typename T>
inline constexpr T _inversesqrt(const T &t) {
	return apply(t, [](float v) { return 1.0f / sprt::sqrt(v); });
}

template <typename T>
inline constexpr T _max(const T &t1, const T &t2) {
	return apply(t1, t2, [](float v1, float v2) { return sprt::max(v1, v2); });
}

template <typename T>
inline constexpr T _min(const T &t1, const T &t2) {
	return apply(t1, t2, [](float v1, float v2) { return sprt::min(v1, v2); });
}

template <typename T>
inline constexpr T _mod(const T &t1, const T &t2) {
	return apply(t1, t2, [](float x, float y) { return x - y * sprt::floor(x / y); });
}

template <typename T>
inline constexpr T _step(const T &t1, const T &t2) {
	return apply(t1, t2, [](float edge, float x) { return (x < edge) ? 0.0f : 1.0f; });
}

template <typename T>
inline constexpr auto _equal(const T &l, const T &r) {
	return bitop(l, r, [](float lv, float rv) { return lv == rv; });
}

template <typename T>
inline constexpr auto _greaterThan(const T &l, const T &r) {
	return bitop(l, r, [](float lv, float rv) { return lv > rv; });
}

template <typename T>
inline constexpr auto _greaterThanEqual(const T &l, const T &r) {
	return bitop(l, r, [](float lv, float rv) { return lv >= rv; });
}

template <typename T>
inline constexpr auto _lessThan(const T &l, const T &r) {
	return bitop(l, r, [](float lv, float rv) { return lv < rv; });
}

template <typename T>
inline constexpr auto _lessThanEqual(const T &l, const T &r) {
	return bitop(l, r, [](float lv, float rv) { return lv <= rv; });
}

template <typename T>
inline constexpr auto _notEqual(const T &l, const T &r) {
	return bitop(l, r, [](float lv, float rv) { return lv != rv; });
}

template <typename T>
inline constexpr auto _isinf(const T &t) {
	return bitop(t, [](float v) { return sprt::isinf(v); });
}

template <typename T>
inline constexpr auto _isnan(const T &t) {
	return bitop(t, [](float v) { return sprt::isnan(v); });
}

} // namespace sprt::geom

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_COMMON_H_ */
