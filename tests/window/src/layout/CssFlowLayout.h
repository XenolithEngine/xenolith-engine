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

#ifndef TESTS_WINDOW_SRC_LAYOUT_CSSFLOWLAYOUT_H_
#define TESTS_WINDOW_SRC_LAYOUT_CSSFLOWLAYOUT_H_

#include "app/TestLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// How a flex item's size, its place in the flow, and its place in the draw order are controlled -
// three separate things that are easy to mistake for one.
//
// `min-width` / `max-width` (and their height counterparts on a column) reach the item's own
// main-axis clamps, so they bound both the base size and the size a grow/shrink produces. Only the
// MAIN axis is enforced - the cross axis has nowhere to put them.
//
// `position: absolute` takes the box out of the flow entirely, the way CSS says: the container
// sizes and spaces its remaining items as if the absolute one were not there, and the offsets the
// stylesheet gave it survive the container's next layout pass. An overlay no longer has to be kept
// outside the flex container to avoid displacing its siblings.
//
// A child added to a container that has ALREADY been laid out is the fourth thing here, and the
// only one that is about timing rather than about geometry. Its item terms - `flex-grow` and the
// rest - reach it from the stylesheet, which is resolved after the container's layout pass has come
// and gone for it. The container has to be told; see LayoutSystem::markItemDirty.
//
// `-xl-z-order` and `order` are independent knobs over the same children. z-order sorts the
// children themselves, so it drives DRAWING (and, as a side effect, the sequence the layout
// receives them in); `order` is then applied inside the layout, on top of that sequence, and
// decides PLACEMENT. Setting both gives an arbitrary combination of the two - which is the only
// way to overlap items in one direction while laying them out in another.
class CssFlowLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runPhase1();
	void runPhase2();
	void runPhase3();

	void expectNear(StringView what, float actual, float expected);
	void expect(bool, StringView what);

	basic2d::Layer *_clampRow = nullptr;
	basic2d::Layer *_clampMin = nullptr; // grows, but min-width holds its floor
	basic2d::Layer *_clampMax = nullptr; // grows, but max-width holds its ceiling
	basic2d::Layer *_clampFree = nullptr; // grows without a clamp, for comparison

	basic2d::Layer *_flowRow = nullptr;
	basic2d::Layer *_flowFirst = nullptr;
	basic2d::Layer *_flowSecond = nullptr;
	basic2d::Layer *_flowOverlay = nullptr; // position: absolute, inside the same container

	// a row that gains a child after it has been laid out; the newcomer is sized by the sheet alone
	basic2d::Layer *_lateRow = nullptr;
	basic2d::Layer *_lateFixed = nullptr;
	basic2d::Layer *_lateGrown = nullptr; // added in phase 1, measured in phase 2

	// overlapping boxes whose placement order and draw order are deliberately reversed
	basic2d::Layer *_stackRow = nullptr;
	basic2d::Layer *_stackA = nullptr; // placed last, drawn first (bottom)
	basic2d::Layer *_stackB = nullptr; // placed first, drawn last (top)
	basic2d::Layer *_stackC = nullptr; // in the middle of both

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LAYOUT_CSSFLOWLAYOUT_H_
