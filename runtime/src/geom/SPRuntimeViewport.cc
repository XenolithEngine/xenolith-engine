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

// The transform and the framing. The header carries the argument; this is the arithmetic, unchanged
// by the two moves that brought it here.

#include <sprt/runtime/geom/viewport.h>

namespace sprt::geom {

// ---- the transform -------------------------------------------------------------------------------

Rect rectFromPoints(Vec2 a, Vec2 b) {
	const float x = sprt::min(a.x, b.x);
	const float y = sprt::min(a.y, b.y);
	return Rect(x, y, sprt::abs(b.x - a.x), sprt::abs(b.y - a.y));
}

Rect Viewport::visibleWorld(float marginScreen) const {
	const Vec2 a = toWorld(Vec2(-marginScreen, -marginScreen));
	const Vec2 b = toWorld(Vec2(screenSize.x + marginScreen, screenSize.y + marginScreen));
	return rectFromPoints(a, b);
}

float clampZoom(float zoom, const ZoomLimits &limits) {
	// Two comparisons, not sprt::min/max: a NaN fails both and comes out unchanged, whereas min/max
	// would hand back a limit and hide the fault that produced it.
	if (zoom < limits.min) {
		return limits.min;
	}
	if (zoom > limits.max) {
		return limits.max;
	}
	return zoom;
}

float wheelZoomFactor(float delta, float divisor) {
	if (!(divisor > 0.0f)) {
		return 1.0f;
	}
	return sprt::exp(-delta / divisor);
}

Viewport zoomAt(const Viewport &view, Vec2 screenAnchor, float factor, const ZoomLimits &limits) {
	Viewport out = view;

	// Clamp FIRST. See the header: the offset is solved against the zoom the view actually takes,
	// which is what keeps the anchor exact at a limit instead of creeping.
	out.zoom = clampZoom(view.zoom * factor, limits);

	// The world point under the anchor, read from the OLD view.
	const Vec2 world = view.toWorld(screenAnchor);

	out.offset = Vec2(screenAnchor.x - world.x * out.zoom, screenAnchor.y - world.y * out.zoom);
	return out;
}

// ---- bounds and framing --------------------------------------------------------------------------

void Bounds::add(const Rect &r) {
	if (!valid) {
		min = Vec2(r.getMinX(), r.getMinY());
		max = Vec2(r.getMaxX(), r.getMaxY());
		valid = true;
		return;
	}
	min = Vec2(sprt::min(min.x, r.getMinX()), sprt::min(min.y, r.getMinY()));
	max = Vec2(sprt::max(max.x, r.getMaxX()), sprt::max(max.y, r.getMaxY()));
}

Viewport fitBounds(const Bounds &bounds, Vec2 screenSize, const FitConfig &config,
		const ZoomLimits &limits) {
	Viewport out;
	out.screenSize = screenSize;

	if (!bounds.valid) {
		// Nothing to frame, and a viewport is still owed. The origin at the centre is the one answer
		// that does not depend on numbers nobody supplied.
		out.zoom = clampZoom(1.0f, limits);
		out.offset = Vec2(screenSize.x * 0.5f, screenSize.y * 0.5f);
		return out;
	}

	// Floored at one pixel rather than at zero: this is a guard against a division by zero, not a
	// claim that a one-pixel surface is meaningful.
	const float availW = sprt::max(1.0f, screenSize.x - config.padding * 2.0f);
	const float availH = sprt::max(1.0f, screenSize.y - config.padding * 2.0f);

	const float boundsW = bounds.max.x - bounds.min.x;
	const float boundsH = bounds.max.y - bounds.min.y;

	// Zero extent on either axis - one object with no size, or a world in a single row - goes to the
	// limit's maximum rather than to 1. Any other answer either divides by zero or invents a scale.
	const float fitZoom = (boundsW > 0.0f && boundsH > 0.0f)
			? sprt::min(availW / boundsW, availH / boundsH)
			: limits.max;

	out.zoom = clampZoom(fitZoom, limits);

	// Centred on the FULL surface, not on the padded area: padding shrinks what the zoom is fitted
	// to and nothing else, so the margins are symmetric by construction.
	const Vec2 centre = bounds.centre();
	out.offset = Vec2(screenSize.x * 0.5f - centre.x * out.zoom,
			screenSize.y * 0.5f - centre.y * out.zoom);
	return out;
}

} // namespace sprt::geom
