/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DLAYER_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DLAYER_H_

#include "XL2dSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

struct SP_PUBLIC SimpleGradient {
	using Color = Color4B;
	using ColorRef = const Color &;

	static const Vec2 Vertical;
	static const Vec2 Horizontal;

	static SimpleGradient progress(const SimpleGradient &a, const SimpleGradient &b, float p);

	SimpleGradient();
	SimpleGradient(ColorRef);
	SimpleGradient(ColorRef start, ColorRef end, const Vec2 & = Vertical);
	SimpleGradient(ColorRef bl, ColorRef br, ColorRef tl, ColorRef tr);

	bool hasAlpha() const;
	bool isMono() const;

	bool operator==(const SimpleGradient &) const;
	bool operator!=(const SimpleGradient &) const;

	Color4B colors[4]; // bl - br - tl - tr
};

/**
 *  Layer is a simple layout sprite, colored with solid color or simple linear gradient
 */
class SP_PUBLIC Layer : public Sprite {
public:
	virtual ~Layer() = default;

	virtual bool init() override;
	virtual bool init(const Color4F &);
	virtual bool init(const SimpleGradient &);

	virtual void handleContentSizeDirty() override;

	virtual void setGradient(const SimpleGradient &);
	virtual const SimpleGradient &getGradient() const;

protected:
	using Sprite::init;

	virtual void updateVertexes(FrameInfo &frame) override;
	virtual void updateVertexesColor() override;

	virtual RenderingLevel getRealRenderingLevel() const override;

	SimpleGradient _gradient;
};

} // namespace stappler::xenolith::basic2d

namespace STAPPLER_VERSIONIZED stappler {

template <>
inline xenolith::basic2d::SimpleGradient progress<xenolith::basic2d::SimpleGradient>(
		const xenolith::basic2d::SimpleGradient &a, const xenolith::basic2d::SimpleGradient &b,
		float p) {
	return xenolith::basic2d::SimpleGradient::progress(a, b, p);
}

} // namespace STAPPLER_VERSIONIZED stappler

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DLAYER_H_ */
