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

#ifndef XENOLITH_RENDERER_UI_XLUIAUXWINDOW_H_
#define XENOLITH_RENDERER_UI_XLUIAUXWINDOW_H_

#include "XLUiConfig.h" // IWYU pragma: keep

#include <sprt/runtime/window/window_info.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;

namespace basic2d {
class SceneLayout2d;
class SceneContent2d;
} // namespace basic2d

namespace ui {

// High-level entry for Popup / Tooltip windows.
//
// With WindowCapabilities::Subwindows opens a native auxiliary window through
// Context::createWindow; a Popup may parent another Popup (submenu chain), and dismissing a
// parent destroys its children (ContextController::destroyChildWindows). Without the capability
// falls back to an in-scene overlay on the parent Root's SceneContent2d.
//
// App-thread only.
class SP_PUBLIC AuxWindow {
public:
	using ContentBuilder = Function<Rc<basic2d::SceneLayout2d>(StringView id)>;

	struct OpenRequest {
		sprt::window::WindowType type = sprt::window::WindowType::Popup;
		sprt::window::WindowPlacement placement;
		Extent2 size = Extent2(200, 100);
		StringView title;
		StringView idPrefix; // used to generate a unique WindowInfo::id
		ContentBuilder builder;
	};

	// Returns the WindowInfo::id on success (native or overlay), empty on failure.
	static String open(NotNull<AppWindow> parent, OpenRequest &&);

	static String openPopup(NotNull<AppWindow> parent, const sprt::window::WindowPlacement &,
			Extent2 size, ContentBuilder &&, StringView title = StringView());

	static String showTooltip(NotNull<AppWindow> parent, const sprt::window::WindowPlacement &,
			Extent2 size, ContentBuilder &&, StringView title = StringView());

	// Scene-factory side: consume the builder registered for `id` (moved out).
	static ContentBuilder takeContentBuilder(StringView id);

	static bool platformSupportsSubwindows(NotNull<AppWindow> parent);

	// Overlay-backed aux surfaces (tooltips, and Popup without Subwindows) — dismiss by the id
	// returned from open()/showTooltip().
	static bool dismissOverlay(StringView id);
	static bool hasOverlay(StringView id);
};

} // namespace ui
} // namespace stappler::xenolith

#endif // XENOLITH_RENDERER_UI_XLUIAUXWINDOW_H_
