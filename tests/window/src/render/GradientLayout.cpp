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

#include "render/GradientLayout.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

bool GradientLayout::init() {
	if (!TestLayout::init()) {
		return false;
	}

	// White, so the gradient is all there is to see.
	_gradient = addChild(Rc<basic2d::Layer>::create(Color::White), ZOrder(1));
	_gradient->setContentSize(Size2(BoxWidth, BoxHeight));
	_gradient->setAnchorPoint(Anchor::Middle);

	// Start and end are in the node's own space: left edge to right edge.
	_linear = Rc<LinearGradient>::create(Vec2(0.0f, 0.0f), Vec2(BoxWidth, 0.0f), makeSteps(0));
	_gradient->setLinearGradient(Rc<LinearGradient>(_linear));

	_outlined = addChild(Rc<basic2d::Layer>::create(Color::Teal_300), ZOrder(2));
	_outlined->setContentSize(Size2(BoxWidth, BoxHeight));
	_outlined->setAnchorPoint(Anchor::Middle);
	_outlined->setShadedOutlineOffset(4.0f);
	_outlined->setShadedOutlineColor(Color::Grey_900);

	return true;
}

void GradientLayout::handleContentSizeDirty() {
	TestLayout::handleContentSizeDirty();

	auto work = getWorkSize();
	_gradient->setPosition(Vec2(work.width / 2.0f, work.height / 2.0f + BoxHeight * 0.75f));
	_outlined->setPosition(Vec2(work.width / 2.0f, work.height / 2.0f - BoxHeight * 0.75f));
}

Vector<GradientStep> GradientLayout::makeSteps(uint32_t n) const {
	static const Color4F colors[] = {
		Color::Red_500,
		Color::Blue_500,
		Color::Green_500,
		Color::Amber_500,
	};
	return Vector<GradientStep>{
		GradientStep(0.0f, colors[n % 4]),
		GradientStep(1.0f, colors[(n + 1) % 4]),
	};
}

Value GradientLayout::encodeState() const {
	Value ret;
	ret.setInteger(int64_t(_step), "step");
	return ret;
}

void GradientLayout::registerCommands() {
	addCommand("state", "Which gradient step is shown: { step }",
			[this](Value &&) { return encodeState(); });

	addCommand("step",
			"Swap the gradient's colours - a new gradient generation, no new vertex data",
			[this](Value &&) {
		++_step;
		_linear->updateWithData(_linear->getStart(), _linear->getEnd(), makeSteps(_step));
		return encodeState();
	});
}

} // namespace stappler::xenolith::app
