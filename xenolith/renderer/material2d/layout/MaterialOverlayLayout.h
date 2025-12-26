/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#ifndef XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALOVERLAYLAYOUT_H_
#define XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALOVERLAYLAYOUT_H_

#include "MaterialSurface.h"
#include "XL2dSceneLayout.h"
#include "XL2dScrollView.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

class SP_PUBLIC OverlayLayout : public SceneLayout2d {
public:
	enum class Binding {
		Relative,
		OriginLeft,
		OriginRight,
		Anchor,
	};

	virtual ~OverlayLayout();

	virtual bool init(const Vec2 &globalOrigin, Binding b, Surface *root, Size2 targetSize);

	virtual void handleContentSizeDirty() override;

	virtual void handlePushTransitionEnded(SceneContent2d *l, bool replace) override;
	virtual void handlePopTransitionBegan(SceneContent2d *l, bool replace) override;

	virtual Rc<Transition> makeExitTransition(SceneContent2d *) const override;

	virtual void setReadyCallback(Function<void(bool)> &&);
	virtual void setCloseCallback(Function<void()> &&);

protected:
	void emplaceNode(const Vec2 &o, Binding b);

	Surface *_surface = nullptr;
	Vec2 _globalOrigin;
	Size2 _fullSize;
	Size2 _initSize;
	Binding _binding = Binding::Anchor;
	Function<void(bool)> _readyCallback;
	Function<void()> _closeCallback;
};

} // namespace stappler::xenolith::material2d

#endif /* XENOLITH_RENDERER_MATERIAL2D_LAYOUT_MATERIALOVERLAYLAYOUT_H_ */
