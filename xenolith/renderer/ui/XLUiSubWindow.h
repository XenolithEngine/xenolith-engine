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

#ifndef XENOLITH_RENDERER_UI_XLUISUBWINDOW_H_
#define XENOLITH_RENDERER_UI_XLUISUBWINDOW_H_

#include "XLUiConfig.h" // IWYU pragma: keep
#include "XLWindowSceneInfo.h"

#include <sprt/runtime/window/window_info.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;
class Director;

namespace basic2d {
class Layer;
class SceneLayout2d;
class SceneContent2d;
} // namespace basic2d

namespace ui {

class Panel;
struct PopupSurfaceConfig;

// One auxiliary surface belonging to a parent window: Dialog, Utility, Popup or Tooltip.
//
// Materializes as a native subwindow where the parent is a local window whose platform advertises
// WindowCapabilities::Subwindows (headless included: the pseudo-controller gives each one a
// pseudo-swapchain), and as an in-scene overlay on the parent's SceneContent2d otherwise (Android,
// wasm, direct output, and every window of a remote client). Both paths honour the same placement,
// dismiss rules and close callback.
//
// The returned object is the handle; keep the Rc. Content travels with the window request as
// WindowSceneInfo, with no lookup by id. App-thread only.
class SP_PUBLIC SubWindow : public Ref {
public:
	using WindowType = sprt::window::WindowType;
	using WindowPlacement = sprt::window::WindowPlacement;
	using WindowCreationFlags = sprt::window::WindowCreationFlags;

	// Builds the surface's content, on both paths; handed the handle so content can dismiss itself.
	using ContentBuilder = Function<Rc<basic2d::SceneLayout2d>(NotNull<SubWindow>)>;

	// For a surface that needs its own Scene subclass. Native path only: with `content` unset and
	// no subwindow support, open() fails. Handed the handle, so the scene needs no id lookup.
	using SceneBuilder = Function<Rc<Scene>(NotNull<SubWindow>, NotNull<AppThread>,
			NotNull<core::RenderServerChannel>, const core::FrameConstraints &)>;

	// Fired exactly once, on the app thread, however the surface went away.
	using CloseCallback = Function<void(NotNull<SubWindow>)>;

	struct Config {
		WindowType type = WindowType::Popup;
		WindowCreationFlags flags = WindowCreationFlags::None;

		// Popup/Tooltip: placement relative to the parent. Dialog/Utility are placed by the WM.
		WindowPlacement placement;

		Extent2 size = Extent2(320, 200);
		Extent2 minExtent = Extent2::ZERO;
		Extent2 maxExtent = Extent2::ZERO;

		StringView title;
		StringView idPrefix; // seeds the generated WindowInfo::id, for logs only

		// Exactly one of these.
		ContentBuilder content;
		SceneBuilder scene;

		// Adopt this already-compiled render queue instead of having the scene build one.
		Rc<core::Queue> queue;

		CloseCallback onClose;

		// False forces the in-scene path even where subwindows exist. Tooltips default to false:
		// a native tip costs a swapchain and takes hover away from the node it describes.
		bool preferNative = true;
	};

	virtual ~SubWindow();

	static Rc<SubWindow> open(NotNull<core::RenderServerChannel> parent, Config &&);

	static Rc<SubWindow> openPopup(NotNull<core::RenderServerChannel>, const WindowPlacement &,
			Extent2, ContentBuilder &&, StringView title = StringView());
	static Rc<SubWindow> openDialog(NotNull<core::RenderServerChannel>, Extent2, ContentBuilder &&,
			bool modal = false, StringView title = StringView());
	static Rc<SubWindow> openUtility(NotNull<core::RenderServerChannel>, Extent2, ContentBuilder &&,
			StringView title = StringView());
	static Rc<SubWindow> showTooltip(NotNull<core::RenderServerChannel>, const WindowPlacement &,
			Extent2, ContentBuilder &&, StringView title = StringView());

	// True when this surface became a real OS window rather than an overlay.
	bool isNative() const { return _sceneInfo != nullptr; }

	// True while the surface is on screen.
	bool isOpen() const;

	/* Overlay path only: where the placement put the surface, in the parent SceneContent's Y-down
	coordinates (x, y: top-left; w, h: opened size). For the content builder: everything but a
	Tooltip is pushed as a full-parent overlay, so the builder positions the visible box. Empty on
	the native path. */
	IRect getOverlayRect() const { return _overlayRect; }

	// The surface's own window (native path) or the layout it was pushed as (overlay path).
	AppWindow *getWindow() const;
	basic2d::SceneLayout2d *getLayout() const { return _layout; }

	/* The panel the surface was built around: what `PopupSurfaceConfig::makePanel` returned, or the
	plain Panel standing in for it; null for a bare ContentBuilder. The layout is a non-painting
	wrapper, so the panel is its child; use this instead of walking the layout's children. */
	Panel *getPanel() const { return _panel; }

	// The parent this surface hangs off. Null once the parent is gone.
	core::RenderServerChannel *getParent() const { return _parent; }

	// Final, uniqued WindowInfo::id on the native path; the generated id on the overlay path.
	StringView getId() const;

	WindowType getType() const { return _type; }

	// Take the surface down. Idempotent; the close callback fires exactly once either way.
	void dismiss();

	// Whether `parent` can host a native subwindow at all: a local window on a platform with
	// subwindows. A remote window reports its server's capabilities, but its client opens no windows.
	static bool platformSupportsSubwindows(NotNull<core::RenderServerChannel> parent);

protected:
	friend class SubWindowSession;
	// Sets _panel while the content is being built; read-only afterwards.
	friend Rc<SubWindow> openPopupSurface(NotNull<core::RenderServerChannel>,
			const sprt::window::WindowPlacement &, PopupSurfaceConfig &&);

	bool openNative(NotNull<AppWindow> parent, Config &&);
	bool openOverlay(NotNull<core::RenderServerChannel> parent, Config &&);

	void handleClosed();

	// Native path: this is both the scene provider and the window handle.
	Rc<WindowSceneInfo> _sceneInfo;

	// Overlay path: the node pushed onto the parent's content.
	Rc<basic2d::SceneLayout2d> _layout;

	// Borrowed, not owned: _layout holds it, and it is cleared with _layout.
	Panel *_panel = nullptr;

	core::RenderServerChannel *_parent = nullptr;
	CloseCallback _onClose;
	String _id;
	WindowType _type = WindowType::Popup;

	// Overlay path, modal Dialog only: the node that covers the parent's content and eats its
	// input, since there is no second window to block.
	Rc<basic2d::Layer> _backdrop;

	// Overlay path only: the placement the surface resolved to, published for the content builder.
	IRect _overlayRect;

	// Overlay path only: a tip is parented directly (it keeps its measured size), everything else
	// goes through pushOverlay.
	bool _overlayIsTip = false;
	bool _closeFired = false;
};

/** The anchor rect a WindowPlacement wants for `anchor`: the box the node occupies on screen, in
placement coordinates. Everything that opens off a node (menus, dropdowns, hints) uses this.

- built from the node's four corners, so rotation and scale are honoured;
- converted into the scene content's space: world space is physical pixels (Scene scales by the
  density), while WindowPlacement is in the window's logical points;
- scaled by `density / surfaceDensity` (the application's `WindowInfo::density`, usually 1), since
  content space is pixels over the full density and window points are pixels over the display's;
- flipped into WindowPlacement's Y-down space from the content's top-left.

An empty rect for a node in no scene, which backends read as the origin. */
SP_PUBLIC IRect placementAnchorRect(NotNull<Node> anchor);

/** The same for a point: what a context menu or pointer-anchored hint opens off. `worldLocation`
is in world space (as input events and convertToWorldSpace give); `inScene` is any node of that
scene, used to find the content. Convert node-local points first, or the density scale is wrong.
The rect is empty, which backends read as this exact point. */
SP_PUBLIC IRect placementAnchorPoint(NotNull<Node> inScene, const Vec2 &worldLocation);

// The window `node` is drawn into: an AppWindow in a local process, a RemoteWindow in a client.
SP_PUBLIC core::RenderServerChannel *getSubWindowParent(const Node *node);

// The Director that runs `window`'s scene, for either kind of window.
SP_PUBLIC Director *getWindowDirector(core::RenderServerChannel *window);

// Whether `window` is being closed; only a local window answers this.
SP_PUBLIC bool isWindowClosing(core::RenderServerChannel *window);

} // namespace ui
} // namespace stappler::xenolith

#endif // XENOLITH_RENDERER_UI_XLUISUBWINDOW_H_
