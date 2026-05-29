/**
 Copyright (c) 2016-2017 Roman Katuntsev <sbkarr@stappler.org>
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_PADDING_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_PADDING_H_

#include <sprt/runtime/geom/geom.h>
#include <sprt/runtime/geom/vec2.h>

namespace sprt::geom {

struct SPRT_API Padding {
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	float left = 0.0f;

	constexpr float horizontal() const { return right + left; }
	constexpr float vertical() const { return top + bottom; }

	constexpr Vec2 getBottomLeft(const Size2 &size) const { return Vec2(left, bottom); }
	constexpr Vec2 getTopLeft(const Size2 &size) const { return Vec2(left, size.height - top); }

	constexpr Vec2 getBottomRight(const Size2 &size) const {
		return Vec2(size.width - right, bottom);
	}
	constexpr Vec2 getTopRight(const Size2 &size) const {
		return Vec2(size.width - right, size.height - top);
	}

	constexpr Padding &setTop(float value) {
		top = value;
		return *this;
	}
	constexpr Padding &setBottom(float value) {
		bottom = value;
		return *this;
	}
	constexpr Padding &setLeft(float value) {
		left = value;
		return *this;
	}
	constexpr Padding &setRight(float value) {
		right = value;
		return *this;
	}

	constexpr Padding &set(float vtop, float vright, float vbottom, float vleft) {
		top = vtop;
		right = vright;
		bottom = vbottom;
		left = vleft;
		return *this;
	}

	constexpr Padding &set(float vtop, float rightAndLeft, float vbottom) {
		top = vtop;
		right = rightAndLeft;
		bottom = vbottom;
		left = rightAndLeft;
		return *this;
	}

	constexpr Padding &set(float topAndBottom, float rightAndLeft) {
		top = topAndBottom;
		right = rightAndLeft;
		bottom = topAndBottom;
		left = rightAndLeft;
		return *this;
	}

	constexpr Padding &set(float all) {
		top = all;
		right = all;
		bottom = all;
		left = all;
		return *this;
	}

	constexpr Padding(float vtop, float vright, float vbottom, float vleft) {
		top = vtop;
		right = vright;
		bottom = vbottom;
		left = vleft;
	}

	constexpr Padding(float vtop, float rightAndLeft, float vbottom) {
		top = vtop;
		right = rightAndLeft;
		bottom = vbottom;
		left = rightAndLeft;
	}

	constexpr Padding(float topAndBottom, float rightAndLeft) {
		top = topAndBottom;
		right = rightAndLeft;
		bottom = topAndBottom;
		left = rightAndLeft;
	}

	constexpr Padding(float all) {
		top = all;
		right = all;
		bottom = all;
		left = all;
	}

	constexpr Padding() = default;
	constexpr Padding(const Padding &) = default;

	constexpr bool operator==(const Padding &p) const {
		return sprt::fabs(top - p.top) < Epsilon<float>
				&& sprt::fabs(bottom - p.bottom) < Epsilon<float>
				&& sprt::fabs(left - p.left) < Epsilon<float>
				&& sprt::fabs(right - p.right) < Epsilon<float>;
	}

	constexpr bool operator!=(const Padding &p) const { return !(*this == p); }

	constexpr Padding &operator*=(float v) {
		top *= v;
		right *= v;
		bottom *= v;
		left *= v;
		return *this;
	}
	constexpr Padding &operator/=(float v) {
		top /= v;
		right /= v;
		bottom /= v;
		left /= v;
		return *this;
	}

	constexpr Padding &operator+=(const Padding &p) {
		top += p.top;
		right += p.right;
		bottom += p.bottom;
		left += p.left;
		return *this;
	}

	constexpr Padding operator*(float v) const {
		Padding ret(*this);
		ret *= v;
		return ret;
	}

	constexpr Padding operator/(float v) const {
		Padding ret(*this);
		ret /= v;
		return ret;
	}
};

using Margin = Padding;

} // namespace sprt::geom

namespace sprt {

template <>
struct io_traits<geom::Padding> {
	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &stream,
			const geom::Padding &p) {
		stream << "(top: " << p.top << "; right: " << p.right << "; bottom: " << p.bottom
			   << "; left: " << p.left << ")";
	}
};

} // namespace sprt

#endif /* STAPPLER_GEOM_SPPADDING_H_ */
