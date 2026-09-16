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

#ifndef TESTS_WINDOW_SRC_CSS_SPECIFICITYLAYOUT_H_
#define TESTS_WINDOW_SRC_CSS_SPECIFICITYLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for CSS specificity-weighted cascade sorting. Each swatch matches
// several rules of different specificity (and some with conflicting source order); the
// resolved background-color must be the one from the highest-specificity rule (ties broken
// by source order). The key case (`#id` beating a lower-specificity descendant rule) is what
// the old fixed lookup order got wrong.
class SpecificityLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	struct Row {
		basic2d::Label *name = nullptr;
		basic2d::Layer *swatch = nullptr;
	};
	Vector<Row> _rows;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_CSS_SPECIFICITYLAYOUT_H_
