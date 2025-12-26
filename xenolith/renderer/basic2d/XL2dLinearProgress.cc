/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#include "XL2dLinearProgress.h"
#include "XL2dLayer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool LinearProgress::init() {
	if (!Node::init()) {
		return false;
	}

	_line = addChild(Rc<Layer>::create());
	_line->setPosition(Vec2::ZERO);
	_line->setAnchorPoint(Vec2::ZERO);

	_bar = addChild(Rc<Layer>::create());
	_bar->setPosition(Vec2::ZERO);
	_bar->setAnchorPoint(Vec2::ZERO);

	setCascadeOpacityEnabled(true);

	return true;
}
void LinearProgress::handleContentSizeDirty() {
	Node::handleContentSizeDirty();
	layoutSubviews();
}

void LinearProgress::handleEnter(Scene *scene) {
	Node::handleEnter(scene);

	if (_animated) {
		updateAnimations();
	}
}

void LinearProgress::handleExit() {
	stopAllActions();
	Node::handleExit();
}

void LinearProgress::setAnimated(bool value) {
	if (_animated != value) {
		_animated = value;
		updateAnimations();
	}
}
bool LinearProgress::isAnimated() const { return _animated; }

void LinearProgress::setProgress(float value) {
	if (_progress != value) {
		_progress = value;
		_contentSizeDirty = true;
	}
}
float LinearProgress::getProgress() const { return _progress; }

void LinearProgress::layoutSubviews() {
	_line->setContentSize(_contentSize);
	if (!_animated) {
		auto barSize = Size2(_progress * _contentSize.width, _contentSize.height);
		_bar->setPosition(Vec2::ZERO);
		_bar->setContentSize(barSize);
	} else {
		const float sep = 0.60f;
		float p = 0.0f;
		bool invert = false;
		if (_progress < sep) {
			p = _progress / sep;
		} else {
			p = (_progress - sep) / (1.0f - sep);
			invert = true;
		}

		float start = 0.0f;
		float end = _contentSize.width;

		const float ePos = invert ? 0.15f : 0.45f;
		const float sPos = invert ? 0.35f : 0.20f;

		if (p < (1.0f - ePos)) {
			end = _contentSize.width * p / (1.0f - ePos);
		}

		if (p > sPos) {
			start = _contentSize.width * (p - sPos) / (1.0f - sPos);
		}

		_bar->setPosition(Vec2(start, 0.0f));
		_bar->setContentSize(Size2(end - start, _contentSize.height));
	}
}

void LinearProgress::updateAnimations() {
	if (_running) {
		if (_animated) {
			auto a = Rc<RepeatForever>::create(Rc<ActionProgress>::create(2.0f, 1.0f,
					[this](float time) { setProgress(time); }));
			a->setTag(2);
			runAction(a);
		} else {
			stopActionByTag(2);
		}
	}
}

void LinearProgress::setLineColor(const Color &c) { _line->setColor(c); }

void LinearProgress::setLineOpacity(float op) { _line->setOpacity(op); }

void LinearProgress::setBarColor(const Color &c) { _bar->setColor(c); }

void LinearProgress::setBarOpacity(float op) { _bar->setOpacity(op); }

} // namespace stappler::xenolith::basic2d
