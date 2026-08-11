/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#ifndef TESTS_WINDOW_SRC_LAYOUT_FLEXBOXLAYOUT_H_
#define TESTS_WINDOW_SRC_LAYOUT_FLEXBOXLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"
#include "XLUiLayoutSystem.h"
#include "XLUiButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Demonstration layout for the ui LayoutSystem placement engine.
//
// A control bar (itself a flex container) lets the user cycle the demo
// container's parameters at runtime. The demo container below holds a handful of
// colored boxes; the "Mode" button flips it between flexbox and grid, so the two
// backends of the unified LayoutSystem can be compared on the same boxes (flex
// reads their FlexItemInfo, grid reads their GridItemInfo components).
class FlexboxLayout : public TestLayout {
public:
	virtual ~FlexboxLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	ui::Button *addControlButton(StringView, Function<void()> &&);
	void updateControlLabels();

	// What the control bar currently shows, as data: the answer of every cycle command, so a
	// headless caller can drive the container to a known configuration without reading the screen.
	Value getLayoutState() const;

	void cycleMode();
	void cycleDirection();
	void cycleWrap();
	void cycleJustify();
	void cycleAlign();

	// grid parameters used when the demo container is in grid mode
	ui::GridLayoutInfo makeDemoGridInfo() const;

	// control bar: a horizontal flex container holding the buttons
	basic2d::Layer *_controls = nullptr;
	ui::LayoutSystem *_controlsFlex = nullptr;

	// demonstration container, reconfigured by the control buttons
	basic2d::Layer *_demo = nullptr;
	ui::LayoutSystem *_demoFlex = nullptr;

	ui::Button *_btnMode = nullptr;
	ui::Button *_btnDirection = nullptr;
	ui::Button *_btnWrap = nullptr;
	ui::Button *_btnJustify = nullptr;
	ui::Button *_btnAlign = nullptr;

	float _controlsHeight = 44.0f;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_LAYOUT_FLEXBOXLAYOUT_H_
