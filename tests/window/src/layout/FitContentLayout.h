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

#ifndef TESTS_WINDOW_SRC_LAYOUT_FITCONTENTLAYOUT_H_
#define TESTS_WINDOW_SRC_LAYOUT_FITCONTENTLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"
#include "XL2dLabel.h"
#include "XLUiLayoutSystem.h"
#include "XLSimpleButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Demonstration/verification layout for fit-content sizing in the ui-module
// LayoutSystem: labels sized from their text (FlexItemInfo::FitContent), a
// nested fit-content flex container measured through its own LayoutSystem,
// upward invalidation on runtime text changes, and the CSS
// `flex-basis: fit-content` path through ui::StyleSystem.
class FitContentLayout : public TestLayout {
public:
	virtual ~FitContentLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	basic2d::Layer *addControlButton(StringView, Function<void()> &&);

	// extend the nested chip's label: exercises the child -> chip -> demo
	// container invalidation chain, including the bubbled nested case
	void appendText();
	void toggleWrap();

	basic2d::Layer *_controls = nullptr;
	basic2d::Layer *_demo = nullptr;
	ui::LayoutSystem *_demoFlex = nullptr;

	basic2d::Label *_appendTarget = nullptr;
	uint32_t _appendCount = 0;

	float _controlsHeight = 44.0f;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LAYOUT_FITCONTENTLAYOUT_H_
