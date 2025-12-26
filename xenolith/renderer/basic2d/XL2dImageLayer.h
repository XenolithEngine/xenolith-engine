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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DIMAGELAYER_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DIMAGELAYER_H_

#include "XL2d.h"
#include "XL2dSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC ImageLayer : public Node {
public:
	static constexpr float GetMaxScaleFactor() { return 1.0f; }

	virtual ~ImageLayer();

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;
    virtual void handleTransformDirty(const Mat4 &) override;

	virtual void setTexture(Rc<Texture> &&);
	virtual const Rc<Texture> &getTexture() const;

	virtual void setActionCallback(Function<void()> &&);

	virtual Vec2 getTexturePosition() const;

	virtual void setScaleDisabled(bool);
	virtual void setImageSize(Size2);

	virtual bool handleTap(Vec2 point, int count);

	virtual bool handleSwipeBegin(Vec2 point);
	virtual bool handleSwipe(Vec2 delta);
	virtual bool handleSwipeEnd(Vec2 velocity);

	virtual bool handlePinch(Vec2 point, float scale, float velocity, bool isEnded);

	Node *getRoot() const { return _root; }

protected:
	Rect getCorrectRect(Size2 containerSize);
	Vec2 getCorrectPosition(Size2 containerSize, Vec2 point);

	Size2 getContainerSize() const;
	Size2 getContainerSizeForScale(float value) const;

	InputListener *_gestureListener = nullptr;

	Node *_root = nullptr;
	Sprite *_image = nullptr;

	Size2 _imageSize;
	Size2 _prevContentSize;
    Vec2 _globalScale; // values to scale input gestures

	float _minScale = 0.0f;
	float _maxScale = 0.0f;

	float _scaleSource;
	bool _imageSizePredefined = false;
	bool _scaleDisabled = false;
	bool _hasPinch = false;
	bool _textureDirty = false;

	Function<void()> _actionCallback;
};

}

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DIMAGELAYER_H_ */
