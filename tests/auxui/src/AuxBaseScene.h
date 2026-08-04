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

#include "SceneRegistry.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;

namespace app {

// Common base for the auxiliary-window scenes (Popup and Tooltip).
//
// The scene factory (`main.cpp`) hands the new window's `WindowInfo::id` here, and the base uses
// it to look up the per-open content builder that Root registered in `SceneRegistry` before the
// `Context::createWindow` call. Subclasses only customise how the returned layout is presented
// (e.g. flush against the cursor for a tooltip, fill the window for a popup).
//
// Closing the window from inside the scene (Esc handler, menu-item activation) goes through
// `closeThisWindow`, which routes to the context's close path. In Phase 0 that simply drops the
// window; later phases add parent/children cascade close on top of the same entry point.
class AuxBaseScene : public basic2d::Scene2d {
public:
	virtual ~AuxBaseScene() = default;

	virtual bool init(NotNull<AppThread> app, NotNull<core::RenderServerChannel> window,
			const core::FrameConstraints &constraints, StringView id);

	StringView getId() const { return _id; }

	// Close this window via AppWindow::hide. Subclasses may clear transient child UI first.
	virtual void closeThisWindow();

	// Open / dismiss a tooltip as a child of this window via the process-wide AuxSession.
	void showSceneTooltip(StringView text, Vec2 anchorSceneYUp);
	void dismissSceneTooltip();

protected:
	// Subclass hook: build the content layout to push onto the scene content. `builder` is
	// whatever Root registered for our id (or null — subclass falls back to a placeholder).
	virtual Rc<basic2d::SceneLayout2d> buildContent(SceneRegistry::Builder &&builder) = 0;

	// Push the built layout. Called from handlePresented so the content is constructed on the
	// app thread (where the registry must be touched) and after the director is fully wired.
	// Also starts the frame pump — see the RenderContinuously note in the implementation.
	void pushContentLayout();

	// The window this scene is presented in. Set at init from the factory argument so the scene
	// does not need to reach back through the director (Director has no public RenderServerChannel
	// accessor today).
	AppWindow *_appWindow = nullptr;

	String _id;
	Rc<basic2d::SceneContent2d> _content;
};

} // namespace app

} // namespace stappler::xenolith

#endif // TESTS_AUXUI_SRC_AUXBASESCENE_H_
