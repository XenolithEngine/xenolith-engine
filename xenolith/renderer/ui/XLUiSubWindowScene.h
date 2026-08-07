/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#ifndef XENOLITH_RENDERER_UI_XLUISUBWINDOWSCENE_H_
#define XENOLITH_RENDERER_UI_XLUISUBWINDOWSCENE_H_

#include "XLUiSubWindow.h"
#include "XL2dScene.h"
#include "XL2dSceneContent.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// Scene of a natively materialized SubWindow: a SceneContent2d carrying whatever the handle's
// ContentBuilder produced.
//
// The builder is reached through the SubWindow handle the window was created with, so this scene
// needs no id lookup — which is the whole point, and also why the same builder can serve the
// overlay path unchanged.
class SP_PUBLIC SubWindowScene : public basic2d::Scene2d {
public:
	virtual ~SubWindowScene() = default;

	// Not an override: Scene2d::init knows nothing about SubWindow, so this is an extra overload.
	virtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &, NotNull<SubWindow>, SubWindow::ContentBuilder &&);

	virtual void handlePresented(Director *) override;

	SubWindow *getSubWindow() const { return _subWindow; }

protected:
	using Scene2d::init;

	// Held strongly: the handle owns the builder and the close callback, and an opener that let go
	// of its Rc must not take the surface's own content down with it.
	Rc<SubWindow> _subWindow;
	SubWindow::ContentBuilder _builder;
	Rc<basic2d::SceneContent2d> _content;
	bool _contentPushed = false;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_XLUISUBWINDOWSCENE_H_
