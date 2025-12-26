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

#include "MaterialIconSprite.h"
#include "MaterialStyleContainer.h"
#include "XLAction.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::material2d {

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

void IconSprite::setBlendColor(ColorRole rule, float value) {
	if (_blendColorRule != rule || _blendValue != value) {
		_blendColorRule = rule;
		_blendValue = value;
	}
}

void IconSprite::setBlendColor(const Color4F &c, float value) {
	setBlendColor(ColorRole::Undefined, 0.0f);
	_blendColor = c;
	_blendValue = value;
}

void IconSprite::setPreserveOpacity(bool value) { _preserveOpacity = value; }

bool IconSprite::isPreserveOpacity() const { return _preserveOpacity; }

bool IconSprite::visitDraw(FrameInfo &frame, NodeVisitFlags parentFlags) {
	if (!_visible) {
		return false;
	}

	auto style = frame.getSystem<SurfaceInterior>(SurfaceInterior::SystemFrameTag);
	auto styleContainer = frame.getSystem<StyleContainer>(StyleContainer::SystemFrameTag);
	if (style) {
		auto &s = style->getStyle();

		if (styleContainer) {
			if (auto scheme = styleContainer->getScheme(s.schemeTag)) {
				if (_blendValue > 0.0f && _blendColorRule != ColorRole::Undefined) {
					auto c = scheme->get(_blendColorRule);
					if (c != _blendColor) {
						_blendColor = c;
					}
				}
			}
		}

		auto color = s.colorOn.asColor4F();
		if (_blendValue > 0.0f) {
			color = color * (1.0f - _blendValue) + _blendColor * _blendValue;
		}

		if (color != getColor()) {
			setColor(color, !_preserveOpacity);
		}
	}

	return VectorSprite::visitDraw(frame, parentFlags);
}

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

} // namespace stappler::xenolith::material2d
