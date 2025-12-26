/**
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

#ifndef XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALICONSPRITE_H_
#define XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALICONSPRITE_H_

#include "XL2dIcons.h"
#include "XL2dVectorSprite.h"
#include "MaterialSurfaceInterior.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC IconSprite : public VectorSprite {
public:
	virtual ~IconSprite() { }

	virtual bool init(IconName);

	virtual IconName getIconName() const { return _iconName; }
	virtual void setIconName(IconName);

	virtual void setProgress(float);
	virtual float getProgress() const;

	virtual void setBlendColor(ColorRole, float value);
	virtual void setBlendColor(const Color4F &, float value);
	virtual ColorRole getBlendColorRule() const { return _blendColorRule; }
	virtual const Color4F &getBlendColor() const { return _blendColor; }
	virtual float getBlendColorValue() const { return _blendValue; }

	virtual void setPreserveOpacity(bool);
	virtual bool isPreserveOpacity() const;

	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	virtual void animate();
	virtual void animate(float targetProgress, float duration);

protected:
	using VectorSprite::init;

	virtual void updateIcon();

	bool _preserveOpacity = false;
	float _blendValue = 0.0f;
	Color4F _blendColor = Color4F::WHITE;
	ColorRole _blendColorRule = ColorRole::Undefined;

	IconName _iconName = IconName::None;
	float _progress = 0.0f;
};

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALICONSPRITE_H_ */
