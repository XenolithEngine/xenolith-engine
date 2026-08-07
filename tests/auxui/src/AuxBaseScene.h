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

#ifndef TESTS_AUXUI_SRC_AUXBASESCENE_H_
#define TESTS_AUXUI_SRC_AUXBASESCENE_H_

#include "XL2dScene.h"
#include "XL2dSceneContent.h"
#include "XLUiSubWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;

namespace app {

// Common base for the auxiliary-window scenes (Popup and Tooltip).
//
// Each of these scenes is produced by its own SubWindow's scene builder (SubWindow::Config::scene),
// so everything it needs - which surface it belongs to, and for a menu, which level it is - was
// captured at the moment the window was requested. There is no per-id lookup anywhere.
//
// Closing the window from inside the scene (Esc handler, menu-item activation) goes through
// `closeThisWindow`, which dismisses the surface and cascades to its children.
class AuxBaseScene : public basic2d::Scene2d {
public:
	virtual ~AuxBaseScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints, NotNull<ui::SubWindow>);

	StringView getId() const { return _id; }

	ui::SubWindow *getSubWindow() const { return _subWindow; }

	// Close this window via AppWindow::hide. Subclasses may clear transient child UI first.
	virtual void closeThisWindow();

	// Open / dismiss a tooltip on THIS window's own session (one tip slot per window).
	void showSceneTooltip(StringView text, Vec2 anchorSceneYUp);
	void dismissSceneTooltip();

protected:
	// Subclass hook: build the content layout to push onto the scene content.
	virtual Rc<basic2d::SceneLayout2d> buildContent() = 0;

	// Push the built layout. Called from handlePresented, so the content is constructed after the
	// director is fully wired and can reach back through the scene.
	// Also starts the frame pump — see the RenderContinuously note in the implementation.
	void pushContentLayout();

	// The window this scene is presented in. Set at init from the factory argument so the scene
	// does not need to reach back through the director (Director has no public RenderServerChannel
	// accessor today).
	AppWindow *_appWindow = nullptr;

	// The surface this scene belongs to; also how it dismisses itself.
	Rc<ui::SubWindow> _subWindow;

	String _id;
	Rc<basic2d::SceneContent2d> _content;
};

} // namespace app

} // namespace stappler::xenolith

#endif // TESTS_AUXUI_SRC_AUXBASESCENE_H_
