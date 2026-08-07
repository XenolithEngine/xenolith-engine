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

#ifndef TESTS_WINDOW_SRC_SECONDARYWINDOW_H_
#define TESTS_WINDOW_SRC_SECONDARYWINDOW_H_

#include "XL2dScene.h"
#include "XL2dSceneLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {
class AppWindow;
}

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A second top-level (Root) window in the same process, with its own Director, its own scene and
// its own presentation engine - the thing a test needs to observe anything that is shared between
// windows but not between processes (the font atlas and its controller, the resource cache, the
// material compiler).
//
// It works headless: a Root window needs no parent and no WindowCapabilities::Subwindows (see
// ContextController::createWindow), and HeadlessContextController::loadWindow builds one
// HeadlessWindow per call with no single-window restriction.
//
// App-thread only.
class SecondaryWindow {
public:
	// Builds the content of the secondary window's scene. Called from the scene factory, on the app
	// thread, while the new window's Director is being constructed.
	using ContentBuilder = Function<Rc<basic2d::SceneLayout2d>(StringView id)>;

	// Ask the context for a Root window named `id` and register `builder` as its content. Returns
	// false if the request could not even be posted; the window itself appears asynchronously, so
	// wait for isOpen(id) (or for the builder to run) rather than assuming it exists on return.
	static bool open(NotNull<AppWindow> anyWindow, StringView id, Extent2 size,
			ContentBuilder &&builder);

	// Scene-factory side: consume the builder registered for `id` (moved out). Null when `id` is
	// not one of ours - that is how the factory tells the primary window from a secondary one.
	static ContentBuilder takeContentBuilder(StringView id);

	// True once the secondary window's scene has been built and entered.
	static bool isOpen(StringView id);

	// The scene of the window named `id`, or null. Lets a test reach into the other window's graph.
	static basic2d::Scene2d *getScene(StringView id);

	// Close the window named `id`. Safe to call for an id that was never opened.
	static void close(StringView id);

	// Called by the secondary scene itself - not part of the test-facing API.
	static void handleSceneEntered(StringView id, basic2d::Scene2d *);
	static void handleSceneExited(StringView id);
};

// Scene of a secondary Root window: a SceneContent2d holding whatever the registered builder
// produced. Deliberately minimal - no caption, no close guard, no inspector commands, so that what
// a test observes in the second window is only what the test itself put there.
class SecondaryScene : public basic2d::Scene2d {
public:
	virtual ~SecondaryScene() = default;

	// Not an override: Scene2d::init has no id/builder, so this is an extra overload that forwards
	// to it and then pushes the built content.
	virtual bool init(NotNull<AppThread>, NotNull<core::RenderServerChannel>,
			const core::FrameConstraints &, StringView id, SecondaryWindow::ContentBuilder &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;

	StringView getWindowId() const { return _windowId; }

protected:
	using Scene2d::init;

	String _windowId;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_SECONDARYWINDOW_H_
