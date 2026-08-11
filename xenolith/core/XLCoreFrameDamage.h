/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_CORE_XLCOREFRAMEDAMAGE_H_
#define XENOLITH_CORE_XLCOREFRAMEDAMAGE_H_

#include "XLCoreInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

// What a render queue opts into at build time. All three are implemented, on both the Vulkan and
// the software backend; Scene2d turns the whole set on for the flat queue and leaves it off for the
// full one, because preserving an image between frames is what the last two rest on.
enum class QueueDamageFlags : uint32_t {
	None = 0,

	// track damage and pass it to the platform present call as a compositor hint
	PresentHint = 1 << 0,

	// restrict rendering to the damaged area (requires a queue whose attachments can be preserved).
	// The render area is what bounds the load/store ops, and the load is most of the saving - see
	// QueuePassHandle::preparePartialRedraw.
	PartialRedraw = 1 << 1,

	// drop a frame entirely when nothing changed: with the image already holding what the frame
	// would draw, nothing is recorded. On Vulkan the frame still submits and presents, so the
	// semaphore chain and the presentation pacing are unchanged.
	SkipEmptyFrames = 1 << 2,
};

SP_DEFINE_ENUM_AS_MASK(QueueDamageFlags)

// What a renderer reports about one drawn element for a single frame. Comparing two snapshots of
// these tells us which screen regions changed.
struct SP_PUBLIC DamageEntry {
	// identity of the copy-on-write data set behind the element
	uint64_t id = 0;

	// bumped whenever the contents of that set may have changed
	uint32_t generation = 0;

	// hash of everything that changes the pixels without changing the geometry: material, the
	// resolved draw state (gradient, outline), rendering level, instance count. Must never
	// include a per-frame index such as StateId, which is not stable between frames.
	uint32_t signature = 0;

	// AABB in swapchain-image pixels (y-down, post-rotation), already transformed and clipped
	Rect bounds;
};

// Per-frame snapshot of everything drawn, produced by the renderer and consumed by the swapchain's
// damage tracker. Malloc-backed on purpose: it must outlive the frame's memory pool.
class SP_PUBLIC FrameDamageState : public Ref {
public:
	virtual ~FrameDamageState() = default;

	// sorted by (id, occurrence) so a diff against a previous snapshot is a linear merge
	Vector<DamageEntry> entries;

	// elements excluded from the generation comparison; their bounds are always damaged
	Vector<Rect> alwaysDirty;

	// the frame cannot be described by the entries above (an element with unknown bounds, or the
	// element count blew past any sane limit) - the whole surface must be treated as damaged
	bool full = false;

	// stable, so repeated ids keep their emission order and pair up run-by-run with a snapshot
	void sort() {
		sprt::stable_sort(entries.begin(), entries.end(),
				[](const DamageEntry &l, const DamageEntry &r) { return l.id < r.id; });
	}

	// true when this frame is guaranteed to be visually identical to whatever produced the same
	// entry list; used by the frame-skip path
	bool empty() const { return !full && alwaysDirty.empty(); }
};

} // namespace stappler::xenolith::core

#endif /* XENOLITH_CORE_XLCOREFRAMEDAMAGE_H_ */
