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

#ifndef XENOLITH_RENDERER_UI_XLUIPOPUPSURFACE_H_
#define XENOLITH_RENDERER_UI_XLUIPOPUPSURFACE_H_

#include "XLUiPanel.h"
#include "XLUiSubWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** One panel on a surface of its own: a menu, a search palette, a colour picker. Handles what every
popup needs on top of ui::SubWindow:

 - native path: the surface is a separate scene the application's ui::StyleSystem does not reach,
   so the sheet in force where the popup was opened is shared by default (`stylesheet` /
   `stylesheetSource` override it);
 - a fallback colour for when no sheet arrives (an undeclared ui::Panel is opaque white);
 - native path: the panel fills the extent the window system settled on, not the requested one;
 - overlay path: SceneContent2d::pushOverlay stretches the layout over the parent with a
   bottom-left origin, so the panel is placed at the resolved rect, converted from Y-down to Y-up;
 - overlay path: no window system dismissal, so an outside press closes the surface.

SubWindow::openOverlay puts the layout on RenderingLevel::Overlay, so the panel may be translucent.
The caller supplies the extent, the panel node and its content. The returned Rc is the handle: the
surface stays open while it is held. */
struct SP_PUBLIC PopupSurfaceConfig {
	/* A stylesheet of the surface's own, replacing the inherited one; native path only. Leave both
	empty to inherit the opener's sheet; if none is found the panel uses `fallbackColor`. On the
	overlay path the layout is already inside the outer sheet's scope. */
	String stylesheet;
	FileCategory stylesheetCategory = FileCategory::Bundled;

	// The same thing as a literal. Applied after `stylesheet`, so the two compose.
	String stylesheetSource;

	/* The node the popup belongs to (the ui::Select, the row that opened a submenu). Read once,
	synchronously, inside openPopupSurface to find the sheet in force, and never stored, so a raw
	pointer is safe. Unset, the search starts at the parent window's top layout, which also chains
	a submenu onto its parent popup's sheet. */
	Node *styleSource = nullptr;

	String title;
	String idPrefix;

	// The extent the window is created for; fixed before any node exists, since the window request
	// carries it.
	Extent2 size;

	// The panel's name and CSS type. The name is also its CSS id and how the inspector finds it.
	String panelName;
	String panelType;
	String panelClass;

	// The layout's name, for the same reason. Defaults to "popup-layout".
	String layoutName;

	// Painted when no sheet reaches the surface.
	Color4B fallbackColor = Color4B(0x20, 0x20, 0x26, 0xFF);

	sprt::window::WindowCreationFlags flags = sprt::window::WindowCreationFlags::None;
	bool preferNative = true;

	// Fired exactly once, however the surface went away.
	Function<void()> onClose;

	/* Makes the panel node for a caller with its own panel class; unset, a plain ui::Panel. Runs in
	the content builder, on the native path when the popup's scene is created, so it must own
	everything it reads. */
	Function<Rc<Panel>(NotNull<SubWindow>, Extent2)> makePanel;

	// Fills the panel, which by then is named, typed, placed, sized and painted.
	Function<void(NotNull<SubWindow>, NotNull<Panel>)> content;

	/* A press outside the panel, overlay path only. Unset dismisses this surface; a menu overrides
	it to take down the whole submenu chain. */
	Function<void(NotNull<SubWindow>, NotNull<Panel>)> onOutsideTap;
};

SP_PUBLIC Rc<SubWindow> openPopupSurface(NotNull<AppWindow>, const sprt::window::WindowPlacement &,
		PopupSurfaceConfig &&);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_XLUIPOPUPSURFACE_H_
