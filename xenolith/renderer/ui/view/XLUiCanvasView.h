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

#ifndef XENOLITH_RENDERER_UI_VIEW_XLUICANVASVIEW_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUICANVASVIEW_H_

#include "XLUiConfig.h"
#include "XLNode.h"
#include "XLInputListener.h"
#include "XLDynamicStateSystem.h"

#include <sprt/runtime/geom/viewport.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/** A WORLD ON A SURFACE: pan, anchored zoom, framing, and clipping.

Four applications had written this by hand before it was written here - three editor canvases in one
studio and `basic2d::ImageLayer`, which had no caller at all - and the copies had already parted
company in two places that a person can see. One notch of the wheel zoomed one canvas by a tenth more
than another, because each carried its own "how many pixels is a notch" constant. And only one of the
four clipped, so a thing dragged past the edge of the other three painted over whatever sat beside
them. Neither divergence is a matter of taste; both are what happens to a fifth copy of anything.

---- WHAT IS HERE AND WHAT IS NOT -----------------------------------------------------------------

Here: a child node that IS the world, the two gestures that move it, the arithmetic that keeps a
point under the cursor while the zoom changes, framing, and the scissor.

Not here: anything that knows what is being drawn. Layout, hit testing, selection, drag modes,
overlays and themes stay with the caller. This widget cannot name a single thing it shows.

---- THE VIEWPORT LIVES IN THE TRANSFORM, NOT IN A FIELD ------------------------------------------

`getViewport()` reads the world node's position and scale rather than a remembered struct, and
`setViewport()` writes them. That is not a saving of four bytes: two of the four callers this
replaces kept a `Viewport` member and mirrored it into the node on every change, and a mirror is a
thing that can be out of step with what is actually on screen. With the transform as the only copy,
`screen = world * zoom + offset` holds by construction and there is nothing to keep in step.

The world's anchor point is (0,0) and its rotation is never touched. Both are invariants this class
relies on, and both are set once in init().

---- GESTURES GO ON SOMEBODY ELSE'S LISTENER ------------------------------------------------------

`attachGestures(InputListener *)` rather than a listener of this widget's own, and the reason comes
from the caller that has the most to lose. An editing canvas is MODAL: the same button means a rubber
band, a drag of an object or a drag of a connection depending on what is under the cursor, and that
decision needs every gesture on ONE listener to be made once. A widget that brought its own listener
on a nested node would make them two, and the two would race for the same press.

So the caller keeps its listener and asks this to add its pan and zoom to it. Pan is the middle and
right buttons, zoom is the wheel; neither collides with a left-button gesture, which is what makes
the arrangement safe as well as necessary.

---- ONE CONSTANT, AND WHY IT IS THIS ONE ---------------------------------------------------------

`WheelNotchPixels` is the only place a notch is turned into pixels. The engine reports scrolling in
notches; `sprt::geom::wheelZoomFactor` is a curve over pixels; 90 is 0.18 * 500, which reproduces
`exp(0.18 * notches)` exactly. The other value in circulation was 100, chosen for being round. */
class SP_PUBLIC CanvasView : public Node {
public:
	// The one place a wheel notch becomes pixels. See the header note.
	static constexpr float WheelNotchPixels = 90.0f;

	virtual ~CanvasView() = default;

	virtual bool init() override;
	virtual bool init(const sprt::geom::ZoomLimits &);

	virtual void handleContentSizeDirty() override;

	// Everything the caller draws goes under this. Its position IS the viewport's offset and its
	// scale IS the zoom, which is what makes the two impossible to disagree.
	Node *getWorld() const { return _world; }

	// Read from the transform, with the surface size taken from this node - so a viewport asked for
	// before the first layout honestly reports a zero surface rather than a stale one.
	sprt::geom::Viewport getViewport() const;

	// The zoom is clamped to the limits this was built with; the offset is taken as given. A caller
	// that wants a clamped offset has to say what it would be clamped to, and no two callers agree.
	void setViewport(const sprt::geom::Viewport &);

	// Zoom by `factor` keeping the world point under `anchor` where it is. `anchor` is in THIS
	// node's space - use convertToNodeSpace on a pointer location first.
	void zoomAt(const Vec2 &anchor, float factor);

	// Frame `worldBounds` in the surface. Framing has its own, wider zoom range: a world too big to
	// be framed inside the gesture range must still be framable.
	void fit(const sprt::geom::Bounds &worldBounds,
			const sprt::geom::FitConfig & = sprt::geom::FitConfig(),
			const sprt::geom::ZoomLimits & = sprt::geom::FramingZoom);

	// A location as an InputEvent carries it, in world coordinates.
	Vec2 worldLocation(const Vec2 &sceneLocation) const;

	// Pan on middle and right, zoom on the wheel, added to the caller's listener. Called once.
	void attachGestures(InputListener *);

	// On by default. A world is unbounded and a surface is not, so a caller that turns this off is
	// saying it wants what leaves the surface to be drawn on top of its neighbours.
	void setClipped(bool);
	bool isClipped() const { return _clipped; }

	const sprt::geom::ZoomLimits &getZoomLimits() const { return _limits; }

protected:
	Node *_world = nullptr;
	DynamicStateSystem *_scissor = nullptr;
	sprt::geom::ZoomLimits _limits = sprt::geom::InteractiveZoom;
	bool _clipped = true;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_VIEW_XLUICANVASVIEW_H_ */
