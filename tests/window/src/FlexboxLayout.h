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

#ifndef EXAMPLES_VK_GUI_SRC_FLEXBOXLAYOUT_H_
#define EXAMPLES_VK_GUI_SRC_FLEXBOXLAYOUT_H_

#include "XL2dSceneLayout.h"
#include "XL2dLayer.h"
#include "XLSimpleFlexLayout.h"
#include "XLSimpleButton.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Demonstration layout for the simpleui flexbox placement engine.
//
// A control bar (itself a flex container) lets the user cycle the demo
// container's flex parameters at runtime, while the demo container below holds
// a handful of colored boxes whose per-item parameters are described through
// FlexItemInfo components.
class FlexboxLayout : public basic2d::SceneLayout2d {
public:
	virtual ~FlexboxLayout() = default;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	basic2d::Layer *addControlButton(StringView, Function<void()> &&);
	void updateControlLabels();

	void cycleDirection();
	void cycleWrap();
	void cycleJustify();
	void cycleAlign();

	// control bar: a horizontal flex container holding the buttons
	basic2d::Layer *_controls = nullptr;
	simpleui::FlexLayout *_controlsFlex = nullptr;

	// demonstration container, reconfigured by the control buttons
	basic2d::Layer *_demo = nullptr;
	simpleui::FlexLayout *_demoFlex = nullptr;

	simpleui::ButtonWithLabel *_btnDirection = nullptr;
	simpleui::ButtonWithLabel *_btnWrap = nullptr;
	simpleui::ButtonWithLabel *_btnJustify = nullptr;
	simpleui::ButtonWithLabel *_btnAlign = nullptr;

	float _controlsHeight = 44.0f;
};

} // namespace stappler::xenolith::app

#endif // EXAMPLES_VK_GUI_SRC_FLEXBOXLAYOUT_H_
