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
#include "XLUiButton.h"
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

---- ONE CONSTANT, AND WHY IT IS A RATIO -----------------------------------------------------------

`ZoomStepRatio` is the only place the size of one step lives, and it is stated as the RATIO a step is
worth rather than as a distance. Equal steps must be equal ratios, which is why the curve is
exponential; and a ratio is the form of the number anybody actually chooses - a tenth is 1.1, and
nobody has to know what a tenth is in wheel pixels. It stood at 90 pixels over a divisor of 500,
which is `exp(0.18) = 1.197` per notch: a fifth at a time, which overshoots what a person aims at.
The wheel and the floating control's two buttons both take one step, so the two cannot disagree.

PER NOTCH, and the wheel has to divide before it can use it. A Scroll event carries an AMOUNT and not
a count of clicks - there is nothing to count on a trackpad - and one detent of a wheel is worth
`sprt::window::InputScrollNotch` of that amount. The first version of this handed the raw amount to
the exponent, so a detent raised the step to the TENTH power and one click of the wheel was 1.1^10:
two and a half times the scale, which is what "the wheel zooms too much" turned out to be. The check
that can see that has to inject a real event; one that asks in notches is asserting the curve and
never the units.

---- THE POINTER MOVES THE WORLD BY WHAT IT MOVED, AND NOT BY A MULTIPLE OF IT ----------------------

Gesture deltas arrive in SCENE units - physical pixels, because the Scene scales its whole subtree
by the display density - while the world's position is in THIS node's own space. A pan that added
the raw delta moved the world by `density` times what the pointer moved, and the picture slid out
from under the cursor: at density 2 a drag of 100pt moved the world 200. So the delta is divided by
this node's accumulated world scale, which folds in the density and any scaled ancestor at once.
`ui::ScrollSystem` divides by the same thing for the same reason. */
class SP_PUBLIC CanvasView : public Node {
public:
	// One wheel notch, and one press of the control's buttons, as a ratio. See the header note.
	static constexpr float ZoomStepRatio = 1.1f;

	// The floating control's box, and how far it is kept from the corner it hangs in.
	static constexpr Size2 ZoomControlSize = Size2(118.0f, 26.0f);
	static constexpr float ZoomControlMargin = 8.0f;

	virtual ~CanvasView() = default;

	virtual bool init() override;
	virtual bool init(const sprt::geom::ZoomLimits &);

	virtual void handleContentSizeDirty() override;
	virtual void handleGlobalTransformDirty(const Mat4 &) override;

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

	// Zoom about the middle of the surface. What a button press means: there is no cursor to keep a
	// point under, so the centre is the only anchor that does not move the view sideways.
	void zoomBy(float factor);

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

	/* THE FLOATING ZOOM CONTROL: "-", the zoom as a percentage, "+". On by default.

	It is chrome about the VIEWPORT, which is the one thing this widget does own - it names nothing
	that is drawn, and every canvas that has a wheel has the same need for a readout and a step that
	does not require one. A step is `ZoomStepRatio`, the wheel's own, so the two roads to a zoom
	cannot disagree.

	It hangs in the BOTTOM-LEFT corner by default, because the corner a canvas already uses for its
	own chrome is the bottom-right one (a minimap, an overview). `setZoomControlPlacement` moves it,
	and a caller with something of its own there turns it off. */
	void setZoomControlEnabled(bool);
	bool isZoomControlEnabled() const { return _zoomControl != nullptr; }

	// Which corner it hangs in, as an anchor of THIS node's box: (0,0) is the bottom-left and the
	// default, (1,1) the top-right. `margin` is in points, on both axes.
	void setZoomControlPlacement(const Vec2 &corner, float margin = ZoomControlMargin);

	// The control itself, for a caller that wants to restyle it. Null while it is off. Its type is
	// `canvas-zoom`, its buttons are ordinary `button`s and its readout an ordinary `label`, so a
	// stylesheet reaches all three without this.
	Node *getZoomControl() const { return _zoomControl; }

protected:
	// Rebuild the readout when the zoom has changed, and put the control back in its corner. Both
	// are called from the three places the world's transform is written - see the note on
	// setZoomControlEnabled for why nothing polls.
	void updateZoomControl();
	void layoutZoomControl();

	Node *_world = nullptr;
	DynamicStateSystem *_scissor = nullptr;
	sprt::geom::ZoomLimits _limits = sprt::geom::InteractiveZoom;
	bool _clipped = true;

	// This node's scale in SCENE units, folded from its ancestors and its own - what a gesture delta
	// is divided by. One, and never zero, until the first transform pass. Taken in the GLOBAL
	// transform phase rather than the local one: the density lives on the Scene, several ancestors
	// up, and a phase that only fires when THIS node's own transform moved would never see it
	// change.
	Vec2 _surfaceScale = Vec2(1.0f, 1.0f);

	// The floating control and its three parts. Null together.
	Panel *_zoomControl = nullptr;
	Button *_zoomOut = nullptr;
	Button *_zoomIn = nullptr;
	basic2d::Label *_zoomLabel = nullptr;

	Vec2 _zoomCorner = Vec2(0.0f, 0.0f);
	float _zoomMargin = ZoomControlMargin;

	// The percentage the readout is showing, so a pan does not rewrite a label that has not changed.
	int32_t _zoomShown = -1;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_VIEW_XLUICANVASVIEW_H_ */
