/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
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

#include "XL2dRoundedProgress.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool RoundedProgress::init(Layout l) {
	if (!LayerRounded::init(Color::Grey_500, 0.0f)) {
		return false;
	}

	setOpacity(1.0f);

	_layout = l;
	setCascadeOpacityEnabled(true);

	_bar = addChild(Rc<LayerRounded>::create(Color::Black, 0.0f), ZOrder(1));
	_bar->setPosition(Vec2::ZERO);
	_bar->setAnchorPoint(Vec2::ZERO);
	_bar->setOpacity(1.0f);

	return true;
}

void RoundedProgress::setLayout(Layout l) {
	if (_layout != l) {
		_layout = l;
		_contentSizeDirty = true;
	}
}

RoundedProgress::Layout RoundedProgress::getLayout() const { return _layout; }

void RoundedProgress::setInverted(bool value) {
	if (_inverted != value) {
		_inverted = value;
		_contentSizeDirty = true;
	}
}

bool RoundedProgress::isInverted() const { return _inverted; }

void RoundedProgress::handleContentSizeDirty() {
	LayerRounded::handleContentSizeDirty();

	auto l = _layout;
	if (l == Auto) {
		if (_contentSize.width > _contentSize.height) {
			l = Horizontal;
		} else {
			l = Vertical;
		}
	}

	if (l == Horizontal) {
		float width = _contentSize.width * _barScale;
		if (width < _contentSize.height) {
			width = _contentSize.height;
		}
		if (width > _contentSize.width) {
			width = _contentSize.width;
		}

		float diff = _contentSize.width - width;

		_bar->setContentSize(Size2(width, _contentSize.height));
		_bar->setPosition(Vec2(diff * (_inverted ? (1.0f - _progress) : _progress), 0));
	} else {
		float height = _contentSize.height * _barScale;
		if (height < _contentSize.width) {
			height = _contentSize.width;
		}
		if (height > _contentSize.height) {
			height = _contentSize.height;
		}

		float diff = _contentSize.height - height;

		_bar->setContentSize(Size2(_contentSize.width, height));
		_bar->setPosition(Vec2(0.0f, diff * (_inverted ? (1.0f - _progress) : _progress)));
	}
}

void RoundedProgress::setBorderRadius(float value) {
	LayerRounded::setBorderRadius(value);
	_bar->setBorderRadius(value);
}

void RoundedProgress::setProgress(float value, float anim) {
	if (value < 0.0f) {
		value = 0.0f;
	} else if (value > 1.0f) {
		value = 1.0f;
	}
	if (_progress != value) {
		if (!_actionManager || anim == 0.0f) {
			_progress = value;
			_contentSizeDirty = true;
		} else {
			stopActionByTag(129);
			auto a = Rc<ActionProgress>::create(anim, value, [this](float time) {
				_progress = time;
				_contentSizeDirty = true;
			});
			a->setSourceProgress(_progress);
			a->setTag(129);
			runAction(a);
		}
	}
}

float RoundedProgress::getProgress() const { return _progress; }

void RoundedProgress::setBarScale(float value) {
	if (_barScale != value) {
		_barScale = value;
		_contentSizeDirty = true;
	}
}

float RoundedProgress::getBarScale() const { return _barScale; }

void RoundedProgress::setLineColor(const Color4F &c) { setColor(c); }

void RoundedProgress::setLineOpacity(float o) { setOpacity(o); }

void RoundedProgress::setBarColor(const Color4F &c) { _bar->setColor(c); }

void RoundedProgress::setBarOpacity(float o) { _bar->setOpacity(o); }

} // namespace stappler::xenolith::basic2d
