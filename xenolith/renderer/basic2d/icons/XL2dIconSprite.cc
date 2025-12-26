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

#include "XL2dIconSprite.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

bool IconSprite::init(IconName icon) {
	if (!VectorSprite::init(Size2(24.0f, 24.0f))) {
		return false;
	}

	setContentSize(Size2(24.0f, 24.0f));

	_iconName = icon;

	if (_iconName != IconName::None) {
		updateIcon();
	}

	return true;
}

void IconSprite::setIconName(IconName name) {
	if (_iconName != name) {
		_iconName = name;
		updateIcon();
	}
}

void IconSprite::setProgress(float pr) {
	if (_progress != pr) {
		_progress = pr;
		updateIcon();
	}
}

float IconSprite::getProgress() const { return _progress; }

void IconSprite::animate() {
	// TODO
}

void IconSprite::animate(float targetProgress, float duration) {
	stopAllActionsByTag("IconSprite::animate"_tag);
	runAction(Rc<ActionProgress>::create(duration, _progress, targetProgress,
					  [this](float value) { setProgress(value); }),
			"IconSprite::animate"_tag);
}

void IconSprite::updateIcon() {
	_image->clear();
	drawIcon(*_image, _iconName, _progress);
}

} // namespace stappler::xenolith::basic2d
