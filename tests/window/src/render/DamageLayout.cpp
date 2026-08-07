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

#include "XLCommon.h"

#include "render/DamageLayout.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

bool DamageLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	_static = addChild(Rc<basic2d::Layer>::create(Color::Grey_400), ZOrder(1));
	_static->setContentSize(Size2(200.0f, 200.0f));
	_static->setAnchorPoint(Anchor::BottomLeft);

	_moving = addChild(Rc<basic2d::Layer>::create(Color::Red_500), ZOrder(2));
	_moving->setContentSize(Size2(200.0f, 200.0f));
	_moving->setAnchorPoint(Anchor::BottomLeft);

	return true;
}

void DamageLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	const float y = getWorkSize().height / 2.0f - 100.0f;

	// The static square sits above the moving one, but must stay clear of the caption strip: it is
	// the control, and it has to be visible in full to show that it never contributes damage.
	_static->setPosition(Vec2(80.0f, sprt::min(y + 260.0f, getWorkTop() - 220.0f)));

	if (!_started) {
		// Start only once, and only after the node has been placed: MoveStep captures the start
		// position in startWithTarget, so re-positioning afterwards would fight the action.
		_moving->setPosition(Vec2(80.0f, y));

		// The node holds each position for StepDelay seconds and then teleports, with no
		// interpolated frames in between - so a screenshot pair taken more than StepDelay apart
		// captures two clean, non-overlapping positions.
		_moving->runAction(
				Rc<MoveStep>::create(StepDelay * MoveSteps, Vec2(MoveDistance, 0.0f), MoveSteps));
		_started = true;
	}
}

} // namespace stappler::xenolith::app
