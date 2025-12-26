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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DOVERLAYLAYOUT_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DOVERLAYLAYOUT_H_

#include "XL2dSceneLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SP_PUBLIC OverlayLayout : public SceneLayout2d {
public:
	static constexpr float Incr = 56.0f;

	enum class Binding {
		Relative,
		OriginLeft,
		OriginRight,
		Anchor,
	};

	virtual ~OverlayLayout() = default;

	virtual bool init(Vec2 globalOrigin, Binding b, Size2 targetSize);

	virtual void handleContentSizeDirty() override;

	virtual void handlePushTransitionEnded(SceneContent2d *l, bool replace) override;
	virtual void handlePopTransitionBegan(SceneContent2d *l, bool replace) override;

	virtual Rc<Transition> makeExitTransition(SceneContent2d *) const override;

	virtual void setReadyCallback(Function<void(bool)> &&);
	virtual void setCloseCallback(Function<void()> &&);

	virtual void setTargetSize(Size2);

protected:
	void emplaceNode(Vec2 o, Binding b);
	virtual Size2 emplaceContent(Node *, Vec2 o, Binding b, Size2 contentSize, Size2 targetSize);

	virtual Rc<Node> makeContent();
	virtual Rc<Action> makeEasing(Action *);

	virtual Size2 trimSize(Size2) const;

	virtual bool handleTap(Vec2);

	Node *_content = nullptr;
	Vec2 _globalOrigin;
	Size2 _collapsedSize;
	Size2 _fullSize;
	Size2 _displaySize;
	Binding _binding = Binding::Anchor;
	Function<void(bool)> _readyCallback;
	Function<void()> _closeCallback;
};

} // namespace stappler::xenolith::basic2d

#endif // XENOLITH_RENDERER_BASIC2D_XL2DOVERLAYLAYOUT_H_
