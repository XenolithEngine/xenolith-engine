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

/** A world on a surface: pan, anchored zoom, framing and clipping. Knows nothing about what is
drawn; layout, hit testing, selection and overlays stay with the caller.

The viewport is the world node's transform (anchor (0,0), no rotation, set in init()):
`screen = world * zoom + offset`, with no separate copy to keep in sync.

Gestures are added to the caller's listener (attachGestures) so a modal canvas keeps all its
gestures on one listener: pan on middle and right buttons, zoom on the wheel.

`ZoomStepRatio` is the zoom per wheel notch and per control button press. A Scroll event carries
an amount, not a count; one notch is `sprt::window::InputScrollNotch` of it.

Gesture deltas are in scene units, so pan divides them by this node's accumulated world scale
(display density and scaled ancestors), like `ui::ScrollSystem`. */
class SP_PUBLIC CanvasView : public Node {
public:
	// One wheel notch, and one press of the control's buttons, as a ratio.
	static constexpr float ZoomStepRatio = 1.1f;

	// The floating control's box and its distance from the corner it hangs in.
	static constexpr Size2 ZoomControlSize = Size2(212.0f, 26.0f);
	static constexpr float ZoomControlMargin = 8.0f;

	virtual ~CanvasView() = default;

	virtual bool init() override;
	virtual bool init(const sprt::geom::ZoomLimits &);

	virtual void handleContentSizeDirty() override;
	virtual void handleGlobalTransformDirty(const Mat4 &) override;

	// Everything the caller draws goes under this. Its position is the viewport's offset and its
	// scale is the zoom.
	Node *getWorld() const { return _world; }

	// Read from the transform; the surface size is this node's (zero before the first layout).
	sprt::geom::Viewport getViewport() const;

	// The zoom is clamped to the limits this was built with; the offset is taken as given.
	void setViewport(const sprt::geom::Viewport &);

	// Zoom by `factor` keeping the world point under `anchor` where it is. `anchor` is in this
	// node's space - use convertToNodeSpace on a pointer location first.
	void zoomAt(const Vec2 &anchor, float factor);

	// Zoom about the middle of the surface (for buttons, where there is no cursor).
	void zoomBy(float factor);

	// Frame `worldBounds` in the surface. Framing has its own, wider zoom range: a world too big to
	// be framed inside the gesture range must still be framable.
	void fit(const sprt::geom::Bounds &worldBounds,
			const sprt::geom::FitConfig & = sprt::geom::FitConfig(),
			const sprt::geom::ZoomLimits & = sprt::geom::FramingZoom);

	/* Provider of the world bounds to frame, called on each framing button press.
	Without one the control's framing buttons are disabled, not hidden. */
	void setFitBounds(Function<sprt::geom::Bounds()> &&);
	bool hasFitBounds() const { return !!_fitBounds; }

	// Frame what `setFitBounds` answers, along the given axes. Does nothing without a provider.
	void fit(sprt::geom::FitAxis = sprt::geom::FitAxis::Both);

	// Set the zoom keeping the world point in the middle of the surface in place, like `zoomBy`.
	void setZoom(float);

	// 100 %.
	void resetZoom() { setZoom(1.0f); }

	// A location as an InputEvent carries it, in world coordinates.
	Vec2 worldLocation(const Vec2 &sceneLocation) const;

	// Pan on middle and right, zoom on the wheel, added to the caller's listener. Called once.
	void attachGestures(InputListener *);

	// Scissor clipping of the world to this node's box. On by default.
	void setClipped(bool);
	bool isClipped() const { return _clipped; }

	const sprt::geom::ZoomLimits &getZoomLimits() const { return _limits; }

	/* The floating zoom control: "-", the zoom as a percentage, "+", then fit-width, fit-height
	and 1:1. On by default, in the bottom-left corner. A step is `ZoomStepRatio`; the fit buttons
	need `setFitBounds`. */
	void setZoomControlEnabled(bool);
	bool isZoomControlEnabled() const { return _zoomControl != nullptr; }

	// Which corner it hangs in, as an anchor of this node's box: (0,0) is the bottom-left and the
	// default, (1,1) the top-right. `margin` is in points, on both axes.
	void setZoomControlPlacement(const Vec2 &corner, float margin = ZoomControlMargin);

	// The control itself, null while it is off. Its type is `canvas-zoom`, with ordinary `button`
	// and `label` children, so a stylesheet reaches it too.
	Node *getZoomControl() const { return _zoomControl; }

protected:
	// Rebuild the readout when the zoom has changed, and put the control back in its corner.
	// Called wherever the world's transform is written; nothing polls.
	void updateZoomControl();
	void layoutZoomControl();

	Node *_world = nullptr;
	DynamicStateSystem *_scissor = nullptr;
	sprt::geom::ZoomLimits _limits = sprt::geom::InteractiveZoom;
	bool _clipped = true;

	// This node's scale in scene units, including ancestors; gesture deltas are divided by it.
	// One until the first transform pass. Taken in the global transform phase, since the density
	// lives on the Scene.
	Vec2 _surfaceScale = Vec2(1.0f, 1.0f);

	// Bounds provider for the framing buttons; may be null.
	Function<sprt::geom::Bounds()> _fitBounds;

	// The floating control and its six parts. Null together.
	Panel *_zoomControl = nullptr;
	Button *_zoomOut = nullptr;
	Button *_zoomIn = nullptr;
	Button *_fitWidth = nullptr;
	Button *_fitHeight = nullptr;
	Button *_zoomReset = nullptr;
	basic2d::Label *_zoomLabel = nullptr;

	Vec2 _zoomCorner = Vec2(0.0f, 0.0f);
	float _zoomMargin = ZoomControlMargin;

	// The percentage the readout is showing, so a pan does not rewrite a label that has not changed.
	int32_t _zoomShown = -1;
};

} // namespace stappler::xenolith::ui

#endif /* XENOLITH_RENDERER_UI_VIEW_XLUICANVASVIEW_H_ */
