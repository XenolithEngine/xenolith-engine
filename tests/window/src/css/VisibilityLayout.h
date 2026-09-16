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

#ifndef TESTS_WINDOW_SRC_CSS_VISIBILITYLAYOUT_H_
#define TESTS_WINDOW_SRC_CSS_VISIBILITYLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for CSS `display: none` / `visibility: hidden` via VisibilityComponent.
// Two CSS flex rows of three items each: the middle item of row 1 is `display: none` (row
// collapses its box - the third item shifts left), the middle item of row 2 is
// `visibility: hidden` (not drawn, but its layout box is preserved - a visible gap remains).
// Then both classes are removed: the components must disappear and the items must render
// again in identical positions, verifying that a style-hidden node stays reachable by the
// styling protocol (its own data phases keep running while the subtree is skipped).
class VisibilityLayout : public TestLayout {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	void runPhase1();
	void runPhase2();

	basic2d::Layer *_rowNone = nullptr;
	basic2d::Layer *_rowHidden = nullptr;
	basic2d::Layer *_midNone = nullptr;
	basic2d::Layer *_midHidden = nullptr;
	basic2d::Layer *_lastNone = nullptr;
	basic2d::Layer *_lastHidden = nullptr;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_CSS_VISIBILITYLAYOUT_H_
