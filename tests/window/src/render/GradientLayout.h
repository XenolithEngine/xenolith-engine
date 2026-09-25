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


#ifndef TESTS_WINDOW_SRC_RENDER_GRADIENTLAYOUT_H_
#define TESTS_WINDOW_SRC_RENDER_GRADIENTLAYOUT_H_

#include "app/TestLayout.h"
#include "XL2dLayer.h"
#include "XLLinearGradient.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// A linear gradient and a shaded outline: the two things a Sprite draws from its extended state
// (basic2d::StateData) rather than from its vertex data.
//
// Both are what a remote client has to carry beside the vertices - the server draws the frame, and
// without the state extension on the wire it draws neither. And both change the picture without
// changing any vertex data, so a damage tracker that only compares data sets never repaints them:
// `gradient.step` swaps the gradient's colours (LinearGradient::updateWithData, a new generation)
// and nothing else.
class GradientLayout : public TestLayout {
public:
	static constexpr float BoxWidth = 320.0f;
	static constexpr float BoxHeight = 120.0f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	virtual void registerCommands() override;

	// The gradient's stops for step `n`: two colours that change with every step.
	Vector<GradientStep> makeSteps(uint32_t n) const;

	Value encodeState() const;

	basic2d::Layer *_gradient = nullptr;
	basic2d::Layer *_outlined = nullptr;
	Rc<LinearGradient> _linear;
	uint32_t _step = 0;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_RENDER_GRADIENTLAYOUT_H_
