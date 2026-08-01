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

#ifndef TESTS_WINDOW_SRC_DAMAGELAYOUT_H_
#define TESTS_WINDOW_SRC_DAMAGELAYOUT_H_

#include "TestLayout.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

// Verification layout for damage tracking: one node that moves in discrete jumps (MoveStep) and
// one that never moves.
//
// The moving node is the interesting case. It keeps its vertex data — and therefore its
// DataIdentity generation — across the jump, so the only thing that changes is its transform.
// A damage implementation that compares generations but not bounds reports nothing for it, and
// the old position is left on screen as a trail. The static node is the control: it must never
// contribute damage.
//
// Pair it with XL_DAMAGE_DEBUG=1 and the batch capture harness to get a before/after pair:
// XL_SCREENSHOT_TESTS=XL_DAMAGE_TEST,XL_DAMAGE_TEST names the test twice, which shoots the same
// layout again after another delay.
//
// Be careful about what those screenshots prove: captureScreenshot renders a fresh OFFSCREEN frame
// (OffscreenTarget | DoNotPresent), so it shows what the scene data produces and is blind to
// anything that goes wrong in the presentation path itself - a trail left by an under-reported
// damage rectangle lives only in the swapchain image, and the PNG will look perfect. This layout
// has to be watched on screen.
class DamageLayout : public TestLayout {
public:
	// Several jumps of MoveDistance each, spaced StepDelay apart, so a screenshot pair taken more
	// than StepDelay apart lands on two different positions. A step is slightly wider than the
	// 200x200 node, so consecutive positions never overlap; the last one still fits on screen,
	// which matters because the run ends with a long static tail (nothing moves, every frame is
	// skipped) whose final image must still be pixel-exact.
	static constexpr float MoveDistance = 210.0f;
	static constexpr uint32_t MoveSteps = 3;
	static constexpr float StepDelay = 0.5f;

	virtual bool init() override;
	virtual void handleContentSizeDirty() override;

protected:
	basic2d::Layer *_moving = nullptr;
	basic2d::Layer *_static = nullptr;
	bool _started = false;
};

} // namespace stappler::xenolith::app

#endif // TESTS_WINDOW_SRC_DAMAGELAYOUT_H_
