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

/** One panel on a surface of its own: what a menu, a search palette and a colour picker all are.

A ui::SubWindow is the WINDOW. What every popup then has to do with it is the same six things, and
each of them is a thing to get wrong exactly once:

 1. on the NATIVE path the surface is a scene of its own, and the ui::StyleSystem carrying the
    application's sheet lives in the parent window's scene and does not reach it - so unless the
    sheet is handed over, the popup comes up unstyled. It is handed over by DEFAULT here: the sheet
    in force where the popup was opened from is shared with the surface's own scene, so an
    application that never heard of any of this gets a styled dropdown. `stylesheet` /
    `stylesheetSource` still override it for a surface whose look is its own;
 2. the panel has to be RenderingLevel::Solid: opaque geometry is drawn first and writes depth
    while the surface pass only TESTS against it, so a panel left at the default level cannot cover
    the labels of whatever is under it on the overlay path;
 3. it needs a colour of its own for the case where no sheet ever arrives, because a ui::Panel with
    nothing declared is opaque WHITE;
 4. on the native path it fills whatever extent the window system actually settled on, which is not
    necessarily the one that was asked for;
 5. on the OVERLAY path SceneContent2d::pushOverlay stretches the layout over the whole parent and
    puts its origin at the bottom left - right for an overlay and wrong for a popup - so the panel
    goes at the rect the placement resolved, converted from that rect's Y-DOWN space into the
    scene's Y-UP one;
 6. a native Popup is dismissed by the window system when the user clicks away from it; an overlay
    has no such contract and has to take itself down.

Everything above is this function's. What is left to a caller is what actually differs: the extent
(a menu measures its source, a palette opens at its full height), the node that IS the surface, and
what goes in it.

The result IS the handle: keep the Rc for as long as the surface should stay open. */
struct SP_PUBLIC PopupSurfaceConfig {
	/* A stylesheet OF THE SURFACE'S OWN, replacing the inherited one - see (1). Used on the NATIVE
	path only.

	Reach for it when the popup's look is genuinely not the application's: a test stand, an
	auxiliary window that ships no .css of its own. Leaving both of these empty is the ordinary
	case, and means "the sheet that styles whatever opened me"; if that search also comes up empty
	the panel paints itself with `fallbackColor`, the way ui::TooltipSystem's stock hint does.

	On the overlay path neither is needed nor read: the layout is pushed under the parent's content
	and is already inside the outer sheet's scope. */
	String stylesheet;
	FileCategory stylesheetCategory = FileCategory::Bundled;

	// The same thing as a literal. Applied after `stylesheet`, so the two compose.
	String stylesheetSource;

	/* Which node the popup belongs to - the ui::Select, the ui::ColorField, the row that opened a
	submenu. Read ONCE, synchronously, inside openPopupSurface: the sheet in force for this node is
	found by walking up from it and shared with the surface's own scene. Never stored, so a raw
	pointer is safe here and an owning one would be a lie.

	Left unset, the search starts at the parent window's top layout instead, which is where an
	application that declares its sheet on the layout rather than on the content keeps it - and
	which is also what chains a submenu onto its parent popup's inherited sheet. */
	Node *styleSource = nullptr;

	String title;
	String idPrefix;

	// The extent the window is created for. Settled BEFORE any node exists, because it is what the
	// window request carries.
	Extent2 size;

	// What the panel is called and what it IS for a stylesheet. The name is also its CSS id and how
	// the inspector finds it.
	String panelName;
	String panelType;
	String panelClass;

	// The layout's name, for the same reason. Defaults to "popup-layout".
	String layoutName;

	// Painted when no sheet reaches the surface - see (3).
	Color4B fallbackColor = Color4B(0x20, 0x20, 0x26, 0xFF);

	sprt::window::WindowCreationFlags flags = sprt::window::WindowCreationFlags::None;
	bool preferNative = true;

	// Fired exactly once, however the surface went away.
	Function<void()> onClose;

	/* Makes the node that IS the surface, for a caller whose panel is a class of its own. Left
	unset, a plain ui::Panel is made. Called inside the content builder, so on the native path it
	runs when the popup's scene is created - which is why everything it reads must be owned by it
	rather than pointed at. */
	Function<Rc<Panel>(NotNull<SubWindow>, Extent2)> makePanel;

	// Fills the panel, which by then is named, typed, placed, sized and painted.
	Function<void(NotNull<SubWindow>, NotNull<Panel>)> content;

	/* A press outside the panel, on the OVERLAY path only - see (6). Unset means "dismiss this
	surface", which is right for a lone popup and not for a menu: a submenu's own surface is one
	link of a chain, and clicking away takes the whole chain down rather than that link.
	*/
	Function<void(NotNull<SubWindow>, NotNull<Panel>)> onOutsideTap;
};

SP_PUBLIC Rc<SubWindow> openPopupSurface(NotNull<AppWindow>, const sprt::window::WindowPlacement &,
		PopupSurfaceConfig &&);

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_XLUIPOPUPSURFACE_H_
