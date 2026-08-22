/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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

#include "XLUiProgressBar.h"
#include "XLUiControlLock.h" // applyControlIndeterminate: the writer of every control state bit
#include "XLUiStyleSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {


ProgressBar::~ProgressBar() { }

bool ProgressBar::init() { return init(0.0f); }

bool ProgressBar::init(float progress) {
	if (!Panel::init()) {
		return false;
	}

	setType("progress-bar");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-progress-bar");
	// the same fill / outline / border-radius appliers Panel registers for itself, under
	// "progress-bar"
	registerStyleAppliers("progress-bar");

	// This widget places its own child; a stylesheet must not add a second writer of its geometry.
	setComponent<SystemManagedLayout>();

	_fill = addChild(Rc<Panel>::create(), ZOrder(1));
	_fill->setType("progress-fill");
	_fill->removeStyleClass("xl-ui-panel");
	_fill->addStyleClass("xl-ui-progress-fill");
	_fill->registerStyleAppliers("progress-fill");
	_fill->setAnchorPoint(Anchor::BottomLeft);
	_fill->setPosition(Vec2::ZERO);

	setProgress(progress);
	return true;
}

void ProgressBar::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();

	if (!_fill) {
		return;
	}

	// Geometry only - the colour is the stylesheet's. Anchor and position are re-stamped here
	// because a style pass is free to have written its own.
	_fill->setAnchorPoint(Anchor::BottomLeft);
	_fill->setPosition(Vec2::ZERO);
	_fill->setContentSize(Size2(_contentSize.width * (isIndeterminate() ? 0.0f : _progress),
			_contentSize.height));
}

void ProgressBar::setProgress(float value) {
	// Compared before clamping so that nan() -> nan() is caught here: nan == nan is false, which
	// would otherwise re-run the whole update on every progress tick of an indeterminate bar.
	const bool wasIndeterminate = isIndeterminate();
	if (!sprt::isnan(value)) {
		value = sprt::clamp(value, 0.0f, 1.0f);
		if (!wasIndeterminate && _progress == value) {
			return;
		}
	} else if (wasIndeterminate) {
		return;
	}

	_progress = value;

	applyControlIndeterminate(this, isIndeterminate());

	if (_fill) {
		_fill->setVisible(!isIndeterminate());
	}
	markContentSizeDirty();
}

bool ProgressBar::isIndeterminate() const { return sprt::isnan(_progress); }

} // namespace stappler::xenolith::ui
