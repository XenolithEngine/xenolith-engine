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

#ifndef TESTS_WINDOW_SRC_LAYOUT_OVERFLOWLAYOUT_H_
#define TESTS_WINDOW_SRC_LAYOUT_OVERFLOWLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for CSS `overflow`. Four boxes, each a flex column or row that holds more
// content than fits:
//
// - `scroll`: the items keep their declared sizes (flex-shrink must NOT crush them), the container
//   reports a scroll range, and a wheel moves it and clamps at both ends;
// - `visible` (the control): the same content IS crushed, i.e. nothing about a container that did
//   not ask for overflow has changed;
// - `hidden`: an oversized child keeps its size and is clipped, and it stops taking clicks once it
//   is outside the scissor;
// - a `flex-grow: 1` filler in an `overflow-y: auto` box whose content FITS: the filler must still
//   grow and the range must stay zero. That is the guard against freeing the axis unconditionally.
//
// Plus an `overflow-x: visible; overflow-y: hidden` box, which must compute BOTH axes to
// non-visible (the scissor is one rect and cannot clip a single axis).
class OverflowLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runPhase1();
	void runPhase2();
	void runPhase3();
	void runPhase4();
	void runPhase5();

	basic2d::Layer *_scrollBox = nullptr;
	basic2d::Layer *_scrollFirst = nullptr;
	basic2d::Layer *_visibleBox = nullptr;
	basic2d::Layer *_visibleFirst = nullptr;
	basic2d::Layer *_hiddenBox = nullptr;
	basic2d::Layer *_hiddenChild = nullptr;
	basic2d::Layer *_fitBox = nullptr;
	basic2d::Layer *_fitFiller = nullptr;
	basic2d::Layer *_coercedBox = nullptr;
	basic2d::Layer *_tearBox = nullptr;
	basic2d::Layer *_tearFirst = nullptr;

	// where the touch drag left off, before the fling had a chance to coast
	float _flingFrom = 0.0f;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LAYOUT_OVERFLOWLAYOUT_H_
