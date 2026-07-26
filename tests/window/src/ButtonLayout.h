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

#ifndef TESTS_WINDOW_SRC_BUTTONLAYOUT_H_
#define TESTS_WINDOW_SRC_BUTTONLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for the ui::Button type-applier + recursive StyleResolver work:
// a StyleSystem sheet gives `button` its background-color / outline-color / outline-width (drawn
// into the button's VectorImage as fill + stroke, via the registered "button" attribute appliers)
// and gives the button's `label` child its color/font. Each button carries a single recursive
// StyleResolver that styles both itself and its label child through the frame-stack child events.
//
// Reach via XL_BUTTON_TEST.
class ButtonLayout : public basic2d::SceneLayout2d {
public:
	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	Vector<ui::Button *> _buttons;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_BUTTONLAYOUT_H_
