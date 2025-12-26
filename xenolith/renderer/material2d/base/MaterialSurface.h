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

#ifndef XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSURFACE_H_
#define XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSURFACE_H_

#include "MaterialSurfaceStyle.h"
#include "XL2dVectorSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SurfaceInterior;
class StyleContainer;

class SP_PUBLIC Surface : public VectorSprite {
public:
	static constexpr uint32_t TransitionActionTag = maxOf<uint32_t>() - 1;

	virtual ~Surface() { }

	virtual bool init(const SurfaceStyle &);

	virtual const SurfaceStyle &getStyleOrigin() const { return _styleOrigin; }
	virtual const SurfaceStyle &getStyleTarget() const { return _styleTarget; }

	virtual const SurfaceStyleData &getStyleCurrent() const { return _styleDataCurrent; }

	virtual void setStyle(const SurfaceStyle &);
	virtual void setStyle(const SurfaceStyle &, float duration);

	virtual void setColorRole(ColorRole value);
	virtual void setElevation(Elevation value);
	virtual void setShapeFamily(ShapeFamily value);
	virtual void setShapeStyle(ShapeStyle value);
	virtual void setNodeStyle(NodeStyle value);
	virtual void setActivityState(ActivityState value);

	virtual void setStyleDirtyCallback(Function<void(const SurfaceStyleData &)> &&);
	virtual const Function<void(const SurfaceStyleData &)> &getStyleDirtyCallback() const {
		return _styleDirtyCallback;
	}

	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	virtual Pair<float, float> getHeightLimits(bool flex) const;

	virtual void setHeightLimits(float min, float max);

protected:
	virtual void applyStyle(StyleContainer *, const SurfaceStyleData &);
	virtual void updateBackgroundImage(VectorImage *img, const SurfaceStyleData &style,
			float radius);

	virtual StyleContainer *getStyleContainerForFrame(FrameInfo &) const;
	virtual SurfaceInterior *getSurfaceInteriorForFrame(FrameInfo &) const;
	virtual RenderingLevel getRealRenderingLevel() const override;

	SurfaceInterior *_interior = nullptr;

	SurfaceStyle _styleOrigin;
	SurfaceStyle _styleTarget;

	SurfaceStyleData _styleDataOrigin;
	SurfaceStyleData _styleDataTarget;
	SurfaceStyleData _styleDataCurrent;

	Function<void(const SurfaceStyleData &)> _styleDirtyCallback;

	ShapeFamily _realShapeFamily = ShapeFamily::RoundedCorners;
	float _fillValue = 0.0f;
	float _outlineValue = 0.0f;
	float _styleProgress = 0.0f;
	float _realCornerRadius = nan();
	bool _styleDirty = true;
	bool _inTransition = false;

	float _minHeight = nan();
	float _maxHeight = nan();
};

class SP_PUBLIC BackgroundSurface : public Surface {
public:
	virtual ~BackgroundSurface() { }

	virtual bool init() override;
	virtual bool init(const SurfaceStyle &) override;

	virtual StyleContainer *getStyleContainer() const { return _styleContainer; }

protected:
	virtual StyleContainer *getStyleContainerForFrame(FrameInfo &) const override;

	StyleContainer *_styleContainer = nullptr;
};

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_BASE_MATERIALSURFACE_H_ */
