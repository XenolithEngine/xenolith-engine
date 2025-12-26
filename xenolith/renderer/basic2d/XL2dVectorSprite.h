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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DVECTORSPRITE_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DVECTORSPRITE_H_

#include "XL2dSprite.h"
#include "XL2dVectorCanvas.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC VectorSprite : public Sprite {
public:
	constexpr static float QualityWorst = 0.1f;
	constexpr static float QualityLow = 0.25f;
	constexpr static float QualityNormal = 0.75f;
	constexpr static float QualityHigh = 1.25f;
	constexpr static float QualityPerfect = 1.75f;

	virtual ~VectorSprite() { }

	VectorSprite();

	virtual bool init(Rc<VectorImage> &&);
	virtual bool init(Size2, StringView);
	virtual bool init(Size2, VectorPath &&);
	virtual bool init(Size2);
	virtual bool init(StringView) override;
	virtual bool init(BytesView);
	virtual bool init(const FileInfo &);

	virtual Rc<VectorPathRef> addPath(StringView id = StringView(), StringView cache = StringView(),
			Mat4 = Mat4::IDENTITY);
	virtual Rc<VectorPathRef> addPath(const VectorPath &path, StringView id = StringView(),
			StringView cache = StringView(), Mat4 = Mat4::IDENTITY);
	virtual Rc<VectorPathRef> addPath(VectorPath &&path, StringView id = StringView(),
			StringView cache = StringView(), Mat4 = Mat4::IDENTITY);

	virtual Rc<VectorPathRef> getPath(StringView);

	virtual void removePath(const Rc<VectorPathRef> &);
	virtual void removePath(StringView);

	virtual void clear();

	virtual void setImage(Rc<VectorImage> &&);
	virtual const Rc<VectorImage> &getImage() const;

	virtual void setQuality(float);
	virtual float getQuality() const { return _quality; }

	virtual void handleTransformDirty(const Mat4 &) override;

	virtual bool visitDraw(FrameInfo &, NodeVisitFlags parentFlags) override;

	virtual uint32_t getTrianglesCount() const;
	virtual uint32_t getVertexesCount() const;

	virtual void setDeferred(bool);
	virtual bool isDeferred() const { return _deferred; }

	// If true - do not draw image when it's drawOrder is empty
	// If false - image with empty draw order is drawn path-by-path in undefined order
	virtual void setRespectEmptyDrawOrder(bool);
	virtual bool isRespectEmptyDrawOrder() const { return _respectEmptyDrawOrder; }

	virtual void setWaitDeferred(bool value) { _waitDeferred = value; }
	virtual bool isWaitDeferred() const { return _waitDeferred; }

	virtual void setImageAutofit(Autofit);
	virtual Autofit getImageAutofit() const { return _imagePlacement.autofit; }

	virtual void setImageAutofitPosition(const Vec2 &);
	virtual const Vec2 &getImageAutofitPosition() const { return _imagePlacement.autofitPos; }

	virtual Vec2 convertToImageFromWorld(const Vec2 &worldLocation) const;
	virtual Vec2 convertToImageFromNode(const Vec2 &worldLocation) const;

	virtual Vec2 convertFromImageToNode(const Vec2 &imageLocation) const;
	virtual Vec2 convertFromImageToWorld(const Vec2 &imageLocation) const;

protected:
	using Sprite::init;

	virtual void pushCommands(FrameInfo &, NodeVisitFlags flags) override;

	virtual void initVertexes() override;
	virtual void updateVertexes(FrameInfo &frame) override;
	virtual void updateVertexesColor() override;

	virtual RenderingLevel getRealRenderingLevel() const override;

	virtual bool checkVertexDirty() const override;

	bool _deferred = true;
	bool _waitDeferred = true;
	bool _imageIsSolid = false;
	bool _respectEmptyDrawOrder = false;

	uint64_t _asyncJobId = 0;
	Rc<VectorImage> _image;
	float _quality = QualityNormal;
	float _savedSdfValue = nan();
	Rc<VectorCanvasResult> _result;
	Rc<VectorCanvasDeferredResult> _deferredResult;

	ImagePlacementInfo _imagePlacement;
	Size2 _imageTargetSize;
	Mat4 _imageTargetTransform;

	DynamicStateSystem *_imageScissorComponent = nullptr;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DVECTORSPRITE_H_ */
