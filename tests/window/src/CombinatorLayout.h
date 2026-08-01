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

#ifndef TESTS_WINDOW_SRC_COMBINATORLAYOUT_H_
#define TESTS_WINDOW_SRC_COMBINATORLAYOUT_H_

#include "TestLayout.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Demonstration/verification layout for CSS combinator selectors in the ui module
// (descendant `A B`, child `A > B`, adjacent sibling `A + B`, general sibling `A ~ B`).
//
// A single ui::StyleSystem stylesheet is attached to this layout node; below it a set
// of Layer "swatches" is built so that exactly one swatch per combinator SHOULD match
// (colored) and one SHOULD NOT (stays grey base). Each swatch carries a StyleResolver so
// the resolved `background-color` becomes its on-screen color, making the match visible.
// init() also asserts the resolved colors programmatically and logs a pass/fail summary.
class CombinatorLayout : public TestLayout {
public:
	virtual ~CombinatorLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	struct Row {
		basic2d::Label *name = nullptr;
		basic2d::Layer *hit = nullptr; // should receive the combinator color
		basic2d::Layer *miss = nullptr; // should stay the grey base color
	};

	// build the scene-graph structure a combinator needs and return the swatch layers
	basic2d::Layer *makeSwatch(Node *parent, StringView cls, ZOrder z);

	Vector<Row> _rows;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_COMBINATORLAYOUT_H_
