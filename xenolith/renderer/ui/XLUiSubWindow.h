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
// It materializes as a native subwindow where the platform advertises
// WindowCapabilities::Subwindows, and as an in-scene overlay on the parent's SceneContent2d where
// it does not (Android, wasm, direct output). Both paths honour the same placement, the same
// dismiss rules and the same close callback, so the caller never branches on the platform.
//
// Headless is on the native side of that line: the pseudo-controller emulates the window manager
// and gives each auxiliary window a pseudo-swapchain of its own, so a menu or a dialog is a real,
// separately renderable window with no display in play.
//
// The returned object IS the handle — keep the Rc. There is no lookup by id anywhere: what the
// surface should contain travels with the window request itself, as WindowSceneInfo.
//
// App-thread only.
class SP_PUBLIC SubWindow : public Ref {
public:
	using WindowType = sprt::window::WindowType;
	using WindowPlacement = sprt::window::WindowPlacement;
	using WindowCreationFlags = sprt::window::WindowCreationFlags;

	// Builds the surface's content. Used by BOTH materializations, which is what keeps a caller
	// portable; it is handed the handle so the content can dismiss itself.
	using ContentBuilder = Function<Rc<basic2d::SceneLayout2d>(NotNull<SubWindow>)>;

	// Escape hatch for a surface that needs its own Scene subclass. Only the native path can honour
	// it — with `content` unset and no subwindow support, open() fails rather than silently
	// producing something different.
	//
	// It is handed the handle, so a scene can be told what it is (which menu level, which document)
	// by the closure that asked for the window, with nothing looked up by id afterwards.
	using SceneBuilder = Function<Rc<Scene>(NotNull<SubWindow>, NotNull<AppThread>,
			NotNull<core::RenderServerChannel>, const core::FrameConstraints &)>;

	// Fired exactly once, on the app thread, however the surface went away.
	using CloseCallback = Function<void(NotNull<SubWindow>)>;

	struct Config {
		WindowType type = WindowType::Popup;
		WindowCreationFlags flags = WindowCreationFlags::None;

		// Popup/Tooltip: where it opens relative to the parent. Dialog/Utility are placed by the WM.
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
		// a native tip costs a swapchain for a few hundred milliseconds of hint, and it takes hover
		// away from the node it describes.
		bool preferNative = true;
	};

	virtual ~SubWindow();

	static Rc<SubWindow> open(NotNull<AppWindow> parent, Config &&);

	static Rc<SubWindow> openPopup(NotNull<AppWindow>, const WindowPlacement &, Extent2,
			ContentBuilder &&, StringView title = StringView());
	static Rc<SubWindow> openDialog(NotNull<AppWindow>, Extent2, ContentBuilder &&,
			bool modal = false, StringView title = StringView());
	static Rc<SubWindow> openUtility(NotNull<AppWindow>, Extent2, ContentBuilder &&,
			StringView title = StringView());
	static Rc<SubWindow> showTooltip(NotNull<AppWindow>, const WindowPlacement &, Extent2,
			ContentBuilder &&, StringView title = StringView());

	// True when this surface became a real OS window rather than an overlay.
	bool isNative() const { return _sceneInfo != nullptr; }

	// True while the surface is on screen.
	bool isOpen() const;

	/* Overlay path only: where the placement put the surface, in the parent SceneContent's own
	Y-DOWN coordinates (x, y = the surface's top-left; w, h = the size it was opened with).

	Meaningful inside the content builder, which is what needs it: everything but a Tooltip is pushed
	as a full-parent overlay, so the builder - not the push - is what decides where the visible box of
	a menu or a palette actually sits. Empty on the native path, where the window system owns the
	position. */
	IRect getOverlayRect() const { return _overlayRect; }

	// The surface's own window (native path) or the layout it was pushed as (overlay path).
	AppWindow *getWindow() const;
	basic2d::SceneLayout2d *getLayout() const { return _layout; }

	/* The panel the surface was built AROUND: what `PopupSurfaceConfig::makePanel` returned, or the
	plain Panel that stands in for it. Null for a surface built from a bare ContentBuilder, which
	owes nobody a panel.

	The layout above is a wrapper - it paints nothing, and exists to carry the style systems a
	native path needs and to be the thing pushOverlay stretches - so the node a caller asked to
	have built is a child of it and not it. Without this a caller reaching for its own content had
	to walk the layout's children and cast, which is a guess about a structure this class owns and
	changes: it was wrong the first time anyone tried, and silently, because a failed cast and an
	unopened surface both read as null. */
	Panel *getPanel() const { return _panel; }

	// The parent this surface hangs off. Null once the parent is gone.
	AppWindow *getParent() const { return _parent; }

	// Final, uniqued WindowInfo::id on the native path; the generated id on the overlay path.
	StringView getId() const;

	WindowType getType() const { return _type; }

	// Take the surface down. Idempotent; the close callback fires exactly once either way.
	void dismiss();

	// Whether `parent` can host a native subwindow at all.
	static bool platformSupportsSubwindows(NotNull<AppWindow> parent);

protected:
	friend class SubWindowSession;
	// Sets _panel while the content is being built. Not part of the surface's public life: the
	// panel is settled once, by whoever builds it, and is read-only from then on.
	friend Rc<SubWindow> openPopupSurface(NotNull<AppWindow>, const sprt::window::WindowPlacement &,
			PopupSurfaceConfig &&);

	bool openNative(NotNull<AppWindow> parent, Config &&);
	bool openOverlay(NotNull<AppWindow> parent, Config &&);

	void handleClosed();

	// Native path: this is both the scene provider and the window handle.
	Rc<WindowSceneInfo> _sceneInfo;

	// Overlay path: the node pushed onto the parent's content.
	Rc<basic2d::SceneLayout2d> _layout;

	// Borrowed, not owned: _layout holds it, and it is cleared with _layout.
	Panel *_panel = nullptr;

	AppWindow *_parent = nullptr;
	CloseCallback _onClose;
	String _id;
	WindowType _type = WindowType::Popup;

	// Overlay path, modal Dialog only: the node that covers the parent's content and eats its
	// input. There is no second window to block, so this is what "modal" means there.
	Rc<basic2d::Layer> _backdrop;

	// Overlay path only: the placement the surface resolved to, published for the content builder.
	IRect _overlayRect;

	// Overlay path only: a tip is parented directly (it keeps its measured size), everything else
	// goes through pushOverlay.
	bool _overlayIsTip = false;
	bool _closeFired = false;
};

/** The anchor rect a WindowPlacement wants for `anchor`: the box that node actually occupies, in
the coordinates the placement is resolved in.

This is the arithmetic every hand-written popup in this tree has got wrong at least once, so it has
one answer and everything that opens off a node calls it - menus, dropdowns, hints:

- the box is built from the node's four CORNERS, not from its origin and size, because the node may
  be rotated or scaled and what the popup hangs off is the box that is actually on screen;
- the corners are then converted into the scene CONTENT's space. Both conversions are load-bearing:
  convertToWorldSpace alone answers in SCENE space, which is physical pixels - Scene scales its
  whole subtree by the density - while WindowPlacement is in the window's logical points. On a
  HiDPI display the two differ by a factor of two, and mixing them puts the popup somewhere off the
  window entirely;
- and it is flipped into WindowPlacement's Y-DOWN space at the end, from the content's top-left.

Answers an empty rect for a node that is in no scene, which is the same thing every backend reads
as "the origin", and is the only sane answer when there is no space to be placed in. */
SP_PUBLIC IRect placementAnchorRect(NotNull<Node> anchor);

/** The same, for a POINT rather than a node - what a context menu and a pointer-anchored hint open
off.

`worldLocation` is in WORLD space, which is what an input event carries and what
convertToWorldSpace answers; `inScene` is any node of the scene the point belongs to, and is only
there to find the content. A caller holding a point in some node's OWN coordinates converts it
first - naming the space in the signature is what keeps the density scale from being applied twice
or not at all.

The rect comes out EMPTY, which every backend reads as "this exact point". */
SP_PUBLIC IRect placementAnchorPoint(NotNull<Node> inScene, const Vec2 &worldLocation);

} // namespace ui
} // namespace stappler::xenolith

#endif // XENOLITH_RENDERER_UI_XLUISUBWINDOW_H_
