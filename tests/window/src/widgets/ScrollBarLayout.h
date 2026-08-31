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

#ifndef TESTS_WINDOW_SRC_WIDGETS_SCROLLBARLAYOUT_H_
#define TESTS_WINDOW_SRC_WIDGETS_SCROLLBARLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dScrollView.h"
#include "XL2dScrollController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

/* basic2d::ScrollView's own scroll bar: the thumb, the track it runs in, and what a pointing
device changes about both.

Nothing here is checkable from a screenshot, which is why the stand exists. A thumb one pixel
too short and one exactly right are the same picture; a bar that follows the pointer at half rate
looks like a bar that follows it; and "the press reached the row behind the bar" is invisible by
construction. So every claim below is a number `scrollbar.state` reports:

  * the thumb's LENGTH is `viewport * viewport/content`, floored at the minimum. A bar whose length
    does not encode the ratio is telling the user how much there is to scroll, wrongly;
  * the thumb's POSITION and getIndicatorRelativePosition() are one expression and its inverse. The
    check drags and compares by equality rather than by tolerance: a doubled response - the bar and
    the content both taking the swipe - shows up as 2x, and a forward/inverse mismatch as a constant
    offset. Neither is visible to the eye;
  * the bar is grabbable only where a POINTING DEVICE exists. `--headless-no-pointer` is the whole
    reason that flag was added, and this is what reads it back.

The last section is the paint. basic2d builds the bar out of nodes that draw a fill and one radius,
so `background-color` reaches it and `outline` has nowhere to land; `ui::useStyledScrollIndicator`
swaps them for Panels that draw all of it. Both halves of that are claims about what a rule DID,
which is why `scrollbar.state` reports the resolved paint of each node rather than a screenshot:
before the swap the radius must still be the widget's own and the outline must be absent, and after
it both must be what the sheet asked for.

The content is deliberately taller than the viewport by a round factor, so every expected number is
exact rather than "about right". */
class ScrollBarLayout : public TestLayout {
public:
	// Duplicated by scrollbar-check.py on purpose: a check that reads its expectations out of the
	// thing it is checking cannot fail.
	static constexpr size_t RowCount = 40;
	static constexpr float RowHeight = 40.0f;
	static constexpr float ViewWidth = 400.0f;
	static constexpr float ViewHeight = 320.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	Value encodeState() const;
	static Value encodeRect(const Node *);

	// What the node would PAINT with: the fill, the radius, the outline, and which kind of node is
	// answering. Read out of the style component where there is one and off the node itself where
	// there is not, so "the rule never arrived" and "the rule arrived and said this" are different
	// answers rather than both being a default.
	static Value encodePaint(const Node *);

	basic2d::ScrollView *_scroll = nullptr;
	Rc<basic2d::ScrollController> _controller;

	// Presses that reached the rows, and presses the scroll view itself recognized. The bar sits
	// over the last few pixels of every row, so the first is how "the bar swallowed it" is told
	// from "the bar was not there" - and the second is what tells BOTH of those from "the press
	// never arrived", which is what a check reads when it aims at the boundary between two rows.
	uint32_t _rowTaps = 0;
	uint32_t _viewTaps = 0;

	// Whether useStyledScrollIndicator has been called. One-way, like the call itself: the control
	// case is the state before it, which is where the stand starts.
	bool _styled = false;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_WIDGETS_SCROLLBARLAYOUT_H_
