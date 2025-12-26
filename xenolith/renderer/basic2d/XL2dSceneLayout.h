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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DSCENELAYOUT_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DSCENELAYOUT_H_

#include "XLNode.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

class SceneContent2d;

enum class DecorationStatus {
	DontCare,
	Visible,
	Hidden
};

class SP_PUBLIC SceneLayout2d : public Node {
public:
	using BackButtonCallback = Function<bool()>;
	using Transition = ActionInterval;
	using DecorationMask = core::ViewConstraints;

	virtual ~SceneLayout2d();

	// Which decorations we should honor in layout placement
	virtual void setDecorationMask(DecorationMask mask);
	virtual DecorationMask getDecorationMask() const { return _decorationMask; }

	virtual void setDecorationPadding(Padding);
	virtual Padding getDecorationPadding() const { return _decorationPadding; }

	virtual void setTargetContentSize(const Size2 &);
	virtual Size2 getTargetContentSize() const { return _targetContentSize; }

	virtual bool handleBackButton();

	virtual void setBackButtonCallback(const BackButtonCallback &);
	virtual const BackButtonCallback &getBackButtonCallback() const;

	virtual void handlePush(SceneContent2d *l, bool replace);
	virtual void handlePushTransitionEnded(SceneContent2d *l, bool replace);

	virtual void handlePopTransitionBegan(SceneContent2d *l, bool replace);
	virtual void handlePop(SceneContent2d *l, bool replace);

	virtual void handleBackground(SceneContent2d *l, SceneLayout2d *overlay);
	virtual void handleBackgroundTransitionEnded(SceneContent2d *l, SceneLayout2d *overlay);

	virtual void handleForegroundTransitionBegan(SceneContent2d *l, SceneLayout2d *overlay);
	virtual void handleForeground(SceneContent2d *l, SceneLayout2d *overlay);

	virtual Rc<Transition> makeEnterTransition(SceneContent2d *) const;
	virtual Rc<Transition> makeExitTransition(SceneContent2d *) const;

	virtual bool hasBackButtonAction() const;

	virtual void setLayoutName(StringView);
	virtual StringView getLayoutName() const;

	virtual bool pop();

	virtual DecorationStatus getDecorationStatus() const { return DecorationStatus::DontCare; }
	virtual SceneContent2d *getSceneContent() const { return _sceneContent; }

protected:
	// Which decorations we should honor in layout placement
	// All - by default
	DecorationMask _decorationMask = DecorationMask::All;
	Padding _decorationPadding;
	bool _inTransition = false;
	BackButtonCallback _backButtonCallback;
	SceneContent2d *_sceneContent = nullptr;
	String _name;
	Size2 _targetContentSize;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DSCENELAYOUT_H_ */
