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

#include "XL2dPixelGrid.h"
#include "XL2dFrameContext.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

namespace {

// Levels above the base that still differ in colour; anything higher is drawn as the top one.
constexpr int32_t PixelGridToneLevels = 3;

// A build never emits more lines than this per axis, whatever the mapping says.
constexpr int64_t PixelGridMaxLines = 16'384;

bool PixelGrid_isAxisAligned(const Mat4 &m) {
	return m.m[1] == 0.0f && m.m[4] == 0.0f && m.m[0] != 0.0f && m.m[5] != 0.0f;
}

float PixelGrid_snap(float v) { return sprt::floor(v + 0.5f); }

} // namespace

Vec2 snapToPixels(const Mat4 &m, const Vec2 &point) {
	if (!PixelGrid_isAxisAligned(m)) {
		return point;
	}
	const float px = PixelGrid_snap(m.m[0] * point.x + m.m[12]);
	const float py = PixelGrid_snap(m.m[5] * point.y + m.m[13]);
	return Vec2((px - m.m[12]) / m.m[0], (py - m.m[13]) / m.m[5]);
}

bool PixelGrid::init() {
	if (!Sprite::init(core::SolidTextureName)) {
		return false;
	}

	setColorMode(core::ColorMode(core::ComponentMapping::R, core::ComponentMapping::One));

	// The solid texture is opaque, so without this the per-vertex alpha would get the Solid pipeline.
	setRenderingLevel(RenderingLevel::Transparent);
	return true;
}

void PixelGrid::setMapping(const Vec2 &origin, float unit) {
	if (_origin != origin || _unit != unit) {
		_origin = origin;
		_unit = unit;
		_vertexesDirty = true;
	}
}

void PixelGrid::setMappingSource(Node *node) {
	_mappingSource = node;
	_vertexesDirty = true;
}

void PixelGrid::setBounds(const Rect &rect) {
	if (!_bounds.equals(rect)) {
		_bounds = rect;
		_vertexesDirty = true;
	}
}

void PixelGrid::setEdgesVisible(bool value) {
	if (_edges != value) {
		_edges = value;
		_vertexesDirty = true;
	}
}

void PixelGrid::setMinLevel(int32_t value) {
	if (_minLevel != value) {
		_minLevel = value;
		_vertexesDirty = true;
	}
}

void PixelGrid::setMinStep(float value) {
	value = sprt::max(value, 1.0f);
	if (_minStep != value) {
		_minStep = value;
		_vertexesDirty = true;
	}
}

void PixelGrid::setColors(const Color4F &minor, const Color4F &major) {
	if (_minor != minor || _major != major) {
		_minor = minor;
		_major = major;
		_vertexesDirty = true;
	}
}

void PixelGrid::draw(FrameInfo &frame, NodeVisitFlags flags) {
	if (_mappingSource) {
		setMapping(_mappingSource->getPosition().xy(), _mappingSource->getScale().x);
	}

	const auto &transform = frame.modelTransformStack.back();
	if (transform != _builtTransform || _displayedColor != _builtColor) {
		_builtTransform = transform;
		_builtColor = _displayedColor;
		_vertexesDirty = true;
	}

	Sprite::draw(frame, flags);
}

void PixelGrid::pushCommands(FrameInfo &frame, NodeVisitFlags flags) {
	if (!_state.snapped) {
		Sprite::pushCommands(frame, flags);
		return;
	}

	// The vertices are in surface pixels already.
	auto handle = static_cast<FrameContextHandle2d *>(frame.currentContext);
	handle->commands->pushVertexArray(_vertexes.pop(), frame.viewProjectionStack.back(),
			buildCmdInfo(frame), _commandFlags);
}

/* Built in surface pixels when the transform is axis-aligned, so that every line is one whole
device pixel; otherwise in node space, unsnapped. `k * node + b` is the space the quads go to. */
void PixelGrid::updateVertexes(FrameInfo &frame) {
	_vertexes.clear();
	_vertexColorDirty = false;

	++_state.rebuilds;
	_state.lines = 0;
	_state.baseLevel = -1;
	_state.baseStep = 0.0f;

	const auto &m = _builtTransform;
	_state.snapped = PixelGrid_isAxisAligned(m);

	const Rect area = (_bounds.size.width > 0.0f && _bounds.size.height > 0.0f)
			? _bounds
			: Rect(Vec2::ZERO, _contentSize);
	if (area.size.width <= 0.0f || area.size.height <= 0.0f || _unit <= 0.0f) {
		return;
	}

	Vec2 k(1.0f, 1.0f);
	Vec2 b(0.0f, 0.0f);
	float scale = Vec2(m.m[0], m.m[1]).length();
	if (_state.snapped) {
		k = Vec2(m.m[0], m.m[5]);
		b = Vec2(m.m[12], m.m[13]);
		scale = sprt::min(sprt::fabs(k.x), sprt::fabs(k.y));
	}
	if (scale <= 0.0f) {
		return;
	}

	_state.surfaceOrigin = Vec2(k.x * _origin.x + b.x, k.y * _origin.y + b.y);
	_state.surfaceUnit = Vec2(k.x * _unit, k.y * _unit);

	// The finest level whose step is at least MinStep device pixels.
	int32_t level = _minLevel;
	float step = _unit * scale * powf(float(LevelRatio), float(level));
	while (step < _minStep && level < 64) {
		++level;
		step *= float(LevelRatio);
	}
	if (!(step >= _minStep)) {
		return;
	}

	_state.baseLevel = level;
	_state.baseStep = step;

	const double period = double(_unit) * pow(double(LevelRatio), double(level));
	const float lineWidth = _state.snapped ? 1.0f : 1.0f / scale;

	auto toTarget = [&](float v, bool x) {
		const float t = x ? k.x * v + b.x : k.y * v + b.y;
		return _state.snapped ? PixelGrid_snap(t) : t;
	};

	// The area in target space, per axis, low to high.
	float lo[2] = {toTarget(area.getMinX(), true), toTarget(area.getMinY(), false)};
	float hi[2] = {toTarget(area.getMaxX(), true), toTarget(area.getMaxY(), false)};
	for (size_t i = 0; i < 2; ++i) {
		if (lo[i] > hi[i]) {
			sprt::swap(lo[i], hi[i]);
		}
	}

	/* How many levels above the threshold a line's step is, as a real number. The base level's
	own part stays under one: only a base held by `minLevel` goes past it, and a closer zoom then
	has no finer level to hand the tones down to. */
	const float baseOver = sprt::min(log2f(step / _minStep) / log2f(float(LevelRatio)), 1.0f);

	auto colorOf = [&](int32_t lineLevel) {
		const float over = baseOver + float(lineLevel - level);
		const float fade = sprt::clamp(over * 2.0f, 0.0f, 1.0f);
		const float tone = sprt::clamp(over / float(PixelGridToneLevels), 0.0f, 1.0f);
		auto c = _minor * (1.0f - tone) + _major * tone;
		c.a *= fade;
		return Color4F(c.r * _displayedColor.r, c.g * _displayedColor.g, c.b * _displayedColor.b,
				c.a * _displayedColor.a);
	};

	const Color4F edgeColor(_major.r * _displayedColor.r, _major.g * _displayedColor.g,
			_major.b * _displayedColor.b, _major.a * _displayedColor.a);

	// One line across the other axis at `at`: vertical when `x`.
	auto emit = [&](bool x, float at, const Color4F &color) {
		if (color.a <= 0.0f) {
			return;
		}
		const size_t other = x ? 1 : 0;
		const float length = hi[other] - lo[other];
		auto quad = _vertexes.addQuad();
		if (x) {
			quad.setGeometry(Vec4(at, lo[other], _textureLayer, 1.0f), Size2(lineWidth, length));
		} else {
			quad.setGeometry(Vec4(lo[other], at, _textureLayer, 1.0f), Size2(length, lineWidth));
		}
		quad.setTextureRect(Rect(0.0f, 0.0f, 1.0f, 1.0f), 1.0f, 1.0f, false, false)
				.setColor(color);
		++_state.lines;
	};

	const float origin[2] = {_origin.x, _origin.y};
	const float areaMin[2] = {area.getMinX(), area.getMinY()};
	const float areaMax[2] = {area.getMaxX(), area.getMaxY()};

	for (size_t axis = 0; axis < 2; ++axis) {
		const bool x = axis == 0;

		const auto first = int64_t(ceil((double(areaMin[axis]) - origin[axis]) / period));
		const auto last = int64_t(floor((double(areaMax[axis]) - origin[axis]) / period));
		if (last < first || last - first > PixelGridMaxLines) {
			continue;
		}

		_vertexes.reserve(uint32_t((last - first + 3) * 4), uint32_t((last - first + 3) * 6));

		for (int64_t n = first; n <= last; ++n) {
			const float at = toTarget(float(double(origin[axis]) + double(n) * period), x);
			if (_edges && (at == lo[axis] || at == hi[axis])) {
				continue;
			}

			// The highest level dividing this line's index.
			int32_t lineLevel = level;
			auto rest = n;
			while (lineLevel < level + PixelGridToneLevels
					&& (rest == 0 || rest % int64_t(LevelRatio) == 0)) {
				rest /= int64_t(LevelRatio);
				++lineLevel;
			}

			emit(x, at, colorOf(lineLevel));
		}

		if (_edges) {
			// Outside the area on both sides, so an edge never covers what it frames.
			emit(x, lo[axis] - lineWidth, edgeColor);
			emit(x, hi[axis], edgeColor);
		}
	}
}

} // namespace stappler::xenolith::basic2d
