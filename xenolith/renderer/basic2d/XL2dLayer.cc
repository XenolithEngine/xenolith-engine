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

#include "XL2dLayer.h"
#include "XL2dFrameContext.h"
#include "XLTexture.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

const Vec2 SimpleGradient::Horizontal(0.0f, 1.0f);
const Vec2 SimpleGradient::Vertical(-1.0f, 0.0f);

SimpleGradient SimpleGradient::progress(const SimpleGradient &a, const SimpleGradient &b, float p) {
	SimpleGradient ret;
	ret.colors[0] = stappler::progress(a.colors[0], b.colors[0], p);
	ret.colors[1] = stappler::progress(a.colors[1], b.colors[1], p);
	ret.colors[2] = stappler::progress(a.colors[2], b.colors[2], p);
	ret.colors[3] = stappler::progress(a.colors[3], b.colors[3], p);
	return ret;
}

SimpleGradient::SimpleGradient() {
	colors[0] = Color4B(255, 255, 255, 255);
	colors[1] = Color4B(255, 255, 255, 255);
	colors[2] = Color4B(255, 255, 255, 255);
	colors[3] = Color4B(255, 255, 255, 255);
}

SimpleGradient::SimpleGradient(ColorRef color) {
	colors[0] = colors[1] = colors[2] = colors[3] = color;
}

SimpleGradient::SimpleGradient(ColorRef start, ColorRef end, const Vec2 &alongVector) {
	float h = alongVector.length();
	if (h == 0) {
		return;
	}

	float c = sqrtf(2.0f);
	Vec2 u(alongVector.x / h, alongVector.y / h);

	// Compressed Interpolation mode
	float h2 = 1 / (fabsf(u.x) + fabsf(u.y));
	u = u * (h2 * c);

	Color4B S(start.r, start.g, start.b, start.a);
	Color4B E(end.r, end.g, end.b, end.a);

	// (-1, -1)
	colors[0].r = E.r + (S.r - E.r) * ((c + u.x + u.y) / (2.0f * c));
	colors[0].g = E.g + (S.g - E.g) * ((c + u.x + u.y) / (2.0f * c));
	colors[0].b = E.b + (S.b - E.b) * ((c + u.x + u.y) / (2.0f * c));
	colors[0].a = E.a + (S.a - E.a) * ((c + u.x + u.y) / (2.0f * c));
	// (1, -1)
	colors[1].r = E.r + (S.r - E.r) * ((c - u.x + u.y) / (2.0f * c));
	colors[1].g = E.g + (S.g - E.g) * ((c - u.x + u.y) / (2.0f * c));
	colors[1].b = E.b + (S.b - E.b) * ((c - u.x + u.y) / (2.0f * c));
	colors[1].a = E.a + (S.a - E.a) * ((c - u.x + u.y) / (2.0f * c));
	// (-1, 1)
	colors[2].r = E.r + (S.r - E.r) * ((c + u.x - u.y) / (2.0f * c));
	colors[2].g = E.g + (S.g - E.g) * ((c + u.x - u.y) / (2.0f * c));
	colors[2].b = E.b + (S.b - E.b) * ((c + u.x - u.y) / (2.0f * c));
	colors[2].a = E.a + (S.a - E.a) * ((c + u.x - u.y) / (2.0f * c));
	// (1, 1)
	colors[3].r = E.r + (S.r - E.r) * ((c - u.x - u.y) / (2.0f * c));
	colors[3].g = E.g + (S.g - E.g) * ((c - u.x - u.y) / (2.0f * c));
	colors[3].b = E.b + (S.b - E.b) * ((c - u.x - u.y) / (2.0f * c));
	colors[3].a = E.a + (S.a - E.a) * ((c - u.x - u.y) / (2.0f * c));
}

SimpleGradient::SimpleGradient(ColorRef bl, ColorRef br, ColorRef tl, ColorRef tr) {
	colors[0] = bl;
	colors[1] = br;
	colors[2] = tl;
	colors[3] = tr;
}

bool SimpleGradient::hasAlpha() const {
	return colors[0].a != 255 || colors[1].a != 255 || colors[2].a != 255 || colors[3].a != 255;
}

bool SimpleGradient::isMono() const {
	return colors[0] == colors[1] && colors[2] == colors[3] && colors[1] == colors[2];
}

bool SimpleGradient::operator==(const SimpleGradient &other) const {
	return memcmp(colors, other.colors, sizeof(Color4B) * 4) == 0;
}

bool SimpleGradient::operator!=(const SimpleGradient &other) const {
	return memcmp(colors, other.colors, sizeof(Color4B) * 4) != 0;
}

bool Layer::init() { return init(Color4F::WHITE); }

bool Layer::init(const Color4F &c) {
	if (!Sprite::init(core::SolidTextureName)) {
		return false;
	}

	setColor(c, true);
	setColorMode(core::ColorMode(core::ComponentMapping::R, core::ComponentMapping::One));

	return true;
}

bool Layer::init(const SimpleGradient &grad) {
	if (!Sprite::init(core::SolidTextureName)) {
		return false;
	}

	setColor(Color4F::WHITE, true);
	setGradient(grad);
	setColorMode(core::ColorMode(core::ComponentMapping::R, core::ComponentMapping::One));

	return true;
}

void Layer::handleContentSizeDirty() { Sprite::handleContentSizeDirty(); }

void Layer::setGradient(const SimpleGradient &g) {
	_gradient = g;
	_contentSizeDirty = true;
}

const SimpleGradient &Layer::getGradient() const { return _gradient; }

void Layer::updateVertexes(FrameInfo &frame) {
	_vertexes.clear();
	auto quad = _vertexes.addQuad()
						.setGeometry(Vec4(Vec2::ZERO, _textureLayer, 1.0f), _contentSize)
						.setTextureRect(_texturePlacement.textureRect, 1.0f, 1.0f, _flippedX,
								_flippedY, _rotated);

	Color4F color[4];
	for (int i = 0; i < 4; i++) {
		color[i] = Color4F(_displayedColor.r * (_gradient.colors[i].r / 255.0f),
				_displayedColor.g * (_gradient.colors[i].g / 255.0f),
				_displayedColor.b * (_gradient.colors[i].b / 255.0f),
				_displayedColor.a * _gradient.colors[i].a / 255.0f);
	}

	quad.setColor(makeSpanView(color, 4));
}

void Layer::updateVertexesColor() {
	if (!_vertexes.empty()) {
		Color4F color[4];
		for (int i = 0; i < 4; i++) {
			color[i] = Color4F(_displayedColor.r * (_gradient.colors[i].r / 255.0f),
					_displayedColor.g * (_gradient.colors[i].g / 255.0f),
					_displayedColor.b * (_gradient.colors[i].b / 255.0f),
					_displayedColor.a * _gradient.colors[i].a / 255.0f);
		}

		_vertexes.getQuad(0, 0).setColor(makeSpanView(color, 4));
	}
}

RenderingLevel Layer::getRealRenderingLevel() const {
	auto level = _renderingLevel;
	if (level == RenderingLevel::Default) {
		if (_displayedColor.a < 1.0f || _gradient.hasAlpha() || !_texture
				|| _materialInfo.getLineWidth() != 0.0f) {
			level = RenderingLevel::Transparent;
		} else if (_colorMode.getMode() == core::ColorMode::Solid) {
			if (_texture->hasAlpha()) {
				level = RenderingLevel::Transparent;
			} else {
				level = RenderingLevel::Solid;
			}
		} else {
			auto alphaMapping = _colorMode.getA();
			switch (alphaMapping) {
			case core::ComponentMapping::Identity:
				if (_texture->hasAlpha()) {
					level = RenderingLevel::Transparent;
				} else {
					level = RenderingLevel::Solid;
				}
				break;
			case core::ComponentMapping::Zero: level = RenderingLevel::Transparent; break;
			case core::ComponentMapping::One: level = RenderingLevel::Solid; break;
			default: level = RenderingLevel::Transparent; break;
			}
		}
	}
	return level;
}

} // namespace stappler::xenolith::basic2d
