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

#ifndef RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_VIEWPORT_H_
#define RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_VIEWPORT_H_

#include <sprt/runtime/geom/geom.h>
#include <sprt/runtime/geom/vec2.h>

namespace sprt::geom {

/* WHERE A WORLD IS ON A SURFACE, AND HOW A GESTURE MOVES IT.

Pure arithmetic over a uniform scale and a translation, with nothing of any application in reach: a
canvas that decided for itself which point of the world is under the cursor would be a canvas whose
answer cannot be checked without a window.

---- WHY IT IS HERE, IN THE RUNTIME, AND NOT IN THE UI --------------------------------------------

Because the answer has to be checkable with no window at all. This math was written for one editor's
canvas, moved down once when a second appeared, and moves down again here on the third and fourth -
and `ui::CanvasView` is what needs it, which would ordinarily put it beside that widget. It does not,
for one reason: the consumer that PROVES it is a console test harness which links no UI module and is
built for a second ABI, and math inside a renderer stops being provable that way the day it moves
there. `Vec2` and `Rect` are already here; nothing below allocates, opens a window or names a
document.

Nothing here touches a stored view. A file may carry one; this is the math a LIVE view is moved by,
and keeping them apart is what lets the stored one stay a plain carry-through value. */

// The rectangle two visited corners describe, in either order.
//
// Four lines of min/abs, and here rather than beside the rubber band that first needed it because
// `visibleWorld` below is the second caller and neither knows what a document is.
SPRT_API Rect rectFromPoints(Vec2 a, Vec2 b);

// ---- the transform -------------------------------------------------------------------------------

// screen = world * zoom + offset.
//
// A uniform scale and a translation, with no matrix: the world is never rotated, and a Mat4 here
// would be a dependency this header has so far done without. `offset` is where the world's origin
// lands on the surface, which is exactly what a scene's world node carries as its position.
//
// The surface size lives here and does NOT belong in a saved viewport - a file has no business
// knowing how big the window was.
struct SPRT_API Viewport {
	Vec2 offset;
	float zoom = 1.0f;
	Vec2 screenSize;

	Vec2 toScreen(Vec2 world) const {
		return Vec2(world.x * zoom + offset.x, world.y * zoom + offset.y);
	}

	Vec2 toWorld(Vec2 screen) const {
		return Vec2((screen.x - offset.x) / zoom, (screen.y - offset.y) / zoom);
	}

	// The world rectangle the surface shows, grown by `marginScreen` SCREEN pixels on every side.
	//
	// Screen pixels rather than world units, because what the margin buys is a constant band of
	// already-built content around the visible area, and "constant" is a statement about the screen.
	// In the world it is marginScreen / zoom, which is the point.
	//
	// Built from the two corners through min/max rather than from a top-left corner and a size: the
	// axes here point up, some references' point down, and a formula that assumed either would be
	// wrong on the other.
	Rect visibleWorld(float marginScreen = 0.0f) const;
};

// What a zoom may reach. Two named pairs below, because there are two different jobs.
struct SPRT_API ZoomLimits {
	float min = 0.25f;
	float max = 2.0f;
};

// A gesture's range, and a framing's range.
//
// Declaring both and then handing the interactive pair to `fitBounds` as well is a recorded mistake:
// a world too big to be framed at 0.25 is then never framed at all. A wheel that could reach 0.02
// would be a wheel that loses the document; a fit that could not would be a fit that does not fit.
// So: two pairs, each with a stated job.
inline constexpr ZoomLimits InteractiveZoom{0.25f, 2.0f};
inline constexpr ZoomLimits FramingZoom{0.02f, 1.0f};

// Two comparisons rather than min/max, so that a NaN passes through unchanged instead of coming out
// as a limit. A NaN zoom is a fault further up, and one that reaches the clamp should still look
// like one rather than be quietly turned into a plausible number.
SPRT_API float clampZoom(float zoom, const ZoomLimits &);

// A wheel notch to a zoom factor: exp(-delta / divisor).
//
// Exponential because equal notches must be equal RATIOS. A linear step is coarse when zoomed out
// and imperceptible when zoomed in, which is the same complaint from both ends of the range.
//
// The PIXEL form, for a device that reports scrolling as a distance (a touchpad). Where the input is
// counted in notches, `wheelZoomRatio` below says the same thing in the units a person picks.
SPRT_API float wheelZoomFactor(float delta, float divisor = 500.0f);

// The same curve stated as the RATIO one notch is worth: ratioPerNotch^notches.
//
// Identical arithmetic to wheelZoomFactor and identical reasoning for it - equal notches, equal
// ratios - but parameterised by the number anyone actually chooses. "A notch is a tenth" is 1.1;
// nobody has to know what a tenth is in pixels, and the two constants can no longer drift apart by
// one of them being retuned.
//
// A ratio of zero or less is a fault further up and answers 1.0 rather than a zoom of nothing.
SPRT_API float wheelZoomRatio(float notches, float ratioPerNotch);

// Zoom by `factor` while the world point under `screenAnchor` stays under it.
//
//   world_after == world_before  for  screen == screenAnchor
//   (screenAnchor - offset') / zoom' == (screenAnchor - offset) / zoom
//   offset' = screenAnchor - worldAnchor * zoom'
//
// The ORDER is the one thing here that is easy to get wrong, and getting it wrong is invisible until
// somebody scrolls at a limit: clamp first, then read the world anchor from the OLD view, then solve
// the offset against the ALREADY CLAMPED zoom. Solving first and clamping after leaves an offset
// that belongs to a zoom the view did not take, and the pan creeps every time the wheel runs into a
// stop. No early return is needed when the clamp bites, because at the clamp the formula reproduces
// the view exactly.
SPRT_API Viewport zoomAt(const Viewport &, Vec2 screenAnchor, float factor,
		const ZoomLimits & = InteractiveZoom);

// Rect has no empty state - merging into a default-constructed one drags its corner to the origin -
// so an accumulator needs a flag of its own.
struct SPRT_API Bounds {
	Vec2 min;
	Vec2 max;
	bool valid = false;

	Rect rect() const { return Rect(min.x, min.y, max.x - min.x, max.y - min.y); }
	Vec2 centre() const { return Vec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f); }

	void add(const Rect &);
};

struct SPRT_API FitConfig {
	// Screen pixels kept clear on every side.
	float padding = 64.0f;
};

// The viewport that frames `bounds` centred in a surface of `screenSize`.
//
// Padding shrinks the area the zoom is fitted TO and does not move the centre, so the margins come
// out symmetric by construction rather than by arithmetic. A padding wider than the surface degrades
// to an available area of one pixel - a guard against a division by zero, not a claim that one pixel
// is sensible.
//
// Degenerate inputs, all of which a caller can reach:
//   * bounds not valid (an empty document) -> the origin at the centre of the surface, zoom clamped
//     from 1. There is nothing to frame, and a viewport is still owed.
//   * bounds of zero extent (one object with no size) -> the zoom goes to the limit's maximum rather
//     than to 1, which is the only answer that does not divide by zero.
//
// The caller must not frame into a surface that has no size yet: every fit run inside a "ready"
// callback centres the world in a 1x1 frame, and a separate "the host has real dimensions now" flag
// is what the callers carry for it.
SPRT_API Viewport fitBounds(const Bounds &, Vec2 screenSize, const FitConfig & = FitConfig(),
		const ZoomLimits & = FramingZoom);

} // namespace sprt::geom

#endif /* RUNTIME_INCLUDE_SPRT_RUNTIME_GEOM_VIEWPORT_H_ */
