/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_COLORHCT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_COLORHCT_H_

#include <sprt/runtime/geom/color_cam16.h>

namespace sprt::geom {

struct alignas(16) SPRT_API ColorHCT {
	struct alignas(16) Values {
		float hue;
		float chroma;
		float tone;
		float alpha;

		bool operator==(const Values &right) const = default;
		bool operator!=(const Values &right) const = default;
	};

	static ColorHCT progress(const ColorHCT &a, const ColorHCT &b, float p);

	// returns closest possible HCT, that can be represented in sRGB by given HCT
	static ColorHCT solveColorHCT(Cam16Float h, Cam16Float c, Cam16Float t, float a);
	static Color4F solveColor4F(Cam16Float h, Cam16Float c, Cam16Float t, float a);

	constexpr ColorHCT() : data({0.0f, 50.0f, 0.0f, 1.0f}), color(Color4F::BLACK) { }

	ColorHCT(float h, float c, float t, float a)
	: data({Cam16::sanitizeDegrees(h), c, t, a}), color() {
		color = solveColor4F(data.hue, data.chroma, data.tone, data.alpha);
	}

	ColorHCT(const Values &d)
	: data(d)
	, color(solveColor4F(Cam16::sanitizeDegrees(data.hue), data.chroma, data.tone, data.alpha)) { }

	explicit ColorHCT(const Color4F &c) {
		auto cam = Cam16::create(c);
		data.hue = cam.hue;
		data.chroma = cam.chroma;
		data.tone = Cam16::LstarFromColor4F(c);
		data.alpha = c.a;
		color = c;
	}

	ColorHCT(const Color4F &c, float a) : ColorHCT(c) { data.alpha = a; }

	Color4F asColor4F() const { return color; }

	ColorHCT &operator=(const Color4F &c) {
		*this = ColorHCT(c);
		return *this;
	}
	ColorHCT &operator=(const ColorHCT &c) = default;

	inline operator Color4F() const { return asColor4F(); }

	bool operator==(const ColorHCT &right) const = default;
	bool operator!=(const ColorHCT &right) const = default;

	Values data;
	Color4F color;
};

} // namespace sprt::geom

namespace sprt {

template <>
struct io_traits<geom::ColorHCT> {
	template <io_character CharType>
	static void encode(const callback<void(StringViewBase<CharType>)> &stream,
			const geom::ColorHCT &obj) {
		stream << "ColorHCT(h:" << obj.data.hue << " c:" << obj.data.chroma
			   << " t:" << obj.data.tone << " a:" << obj.data.alpha << ");";
	}
};

} // namespace sprt

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_COLORHCT_H_ */
