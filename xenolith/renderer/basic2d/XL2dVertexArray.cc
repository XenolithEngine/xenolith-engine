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

#include "XL2dVertexArray.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

VertexArray::Quad &VertexArray::Quad::setTextureRect(const Rect &texRect, float texWidth,
		float texHeight, bool flippedX, bool flippedY, bool rotated) {

	Vertex *data = const_cast<Vertex *>(vertexes.data());

	float texLeft = texRect.origin.x / texWidth;
	float texRight = (texRect.origin.x + texRect.size.width) / texWidth;
	float texTop = texRect.origin.y / texHeight;
	float texBottom = (texRect.origin.y + texRect.size.height) / texHeight;

	if (flippedX) {
		std::swap(texLeft, texRight);
	}

	if (flippedY) {
		std::swap(texTop, texBottom);
	}

	if (!rotated) {
		// tl bl tr br
		data[0].tex = Vec2(texLeft, texTop);
		data[1].tex = Vec2(texLeft, texBottom);
		data[2].tex = Vec2(texRight, texTop);
		data[3].tex = Vec2(texRight, texBottom);
	} else {
		// tl bl tr br
		data[0].tex = Vec2(texLeft, texTop);
		data[1].tex = Vec2(texRight, texTop);
		data[2].tex = Vec2(texLeft, texBottom);
		data[3].tex = Vec2(texRight, texBottom);
	}

	return *this;
}

VertexArray::Quad &VertexArray::Quad::setTexturePoints(const Vec2 &tl, const Vec2 &bl,
		const Vec2 &tr, const Vec2 &br, float texWidth, float texHeight) {

	Vertex *data = const_cast<Vertex *>(vertexes.data());

	// tl bl tr br
	data[0].tex = Vec2(tl.x / texWidth, tl.y / texHeight);
	data[1].tex = Vec2(bl.x / texWidth, bl.y / texHeight);
	data[2].tex = Vec2(tr.x / texWidth, tr.y / texHeight);
	data[3].tex = Vec2(br.x / texWidth, br.y / texHeight);
	return *this;
}

VertexArray::Quad &VertexArray::Quad::setGeometry(const Vec4 &pos, const Size2 &size,
		const Mat4 &transform) {
	const float x1 = pos.x;
	const float y1 = pos.y;

	const float x2 = x1 + size.width;
	const float y2 = y1 + size.height;
	const float x = transform.m[12];
	const float y = transform.m[13];

	const float cr = transform.m[0];
	const float sr = transform.m[1];
	const float cr2 = transform.m[5];
	const float sr2 = -transform.m[4];

	// d - c
	// |   |
	// a - b

	const float ax = x1 * cr - y1 * sr2 + x;
	const float ay = x1 * sr + y1 * cr2 + y;

	const float bx = x2 * cr - y1 * sr2 + x;
	const float by = x2 * sr + y1 * cr2 + y;

	const float cx = x2 * cr - y2 * sr2 + x;
	const float cy = x2 * sr + y2 * cr2 + y;

	const float dx = x1 * cr - y2 * sr2 + x;
	const float dy = x1 * sr + y2 * cr2 + y;

	Vertex *data = const_cast<Vertex *>(vertexes.data());

	// tl bl tr br
	data[0].pos = Vec4(dx, dy, pos.z, pos.w);
	data[1].pos = Vec4(ax, ay, pos.z, pos.w);
	data[2].pos = Vec4(cx, cy, pos.z, pos.w);
	data[3].pos = Vec4(bx, by, pos.z, pos.w);

	return *this;
}

VertexArray::Quad &VertexArray::Quad::setGeometry(const Vec4 &pos, const Size2 &size) {
	const float x1 = pos.x;
	const float y1 = pos.y;

	const float x2 = x1 + size.width;
	const float y2 = y1 + size.height;

	Vertex *data = const_cast<Vertex *>(vertexes.data());

	// (x1, y2) - (x2, y2)
	// |          |
	// (x1, y1) - (x2, y1)

	// tl bl tr br
	data[0].pos = Vec4(x1, y2, pos.z, pos.w);
	data[1].pos = Vec4(x1, y1, pos.z, pos.w);
	data[2].pos = Vec4(x2, y2, pos.z, pos.w);
	data[3].pos = Vec4(x2, y1, pos.z, pos.w);

	return *this;
}

VertexArray::Quad &VertexArray::Quad::setColor(const Color4F &color) {
	Vertex *data = const_cast<Vertex *>(vertexes.data());
	data[0].color = color;
	data[1].color = color;
	data[2].color = color;
	data[3].color = color;
	return *this;
}

VertexArray::Quad &VertexArray::Quad::setColor(SpanView<Color4F> colors) { // tl bl tr br
	if (colors.size() != 4) {
		return *this;
	}

	Vertex *data = const_cast<Vertex *>(vertexes.data());
	data[0].color = colors[0];
	data[1].color = colors[1];
	data[2].color = colors[2];
	data[3].color = colors[3];
	return *this;
}

VertexArray::Quad &VertexArray::Quad::setColor(
		std::initializer_list<Color4F> &&colors) { // tl bl tr br
	return setColor(SpanView<Color4F>(colors.begin(), colors.size()));
}

VertexArray::Quad &VertexArray::Quad::drawChar(const font::Metrics &m, char32_t charID,
		int16_t charX, int16_t charY, const Color4B &color, font::TextDecoration, uint16_t face,
		float layer) {

	setGeometry(Vec4(charX, charY - m.descender, layer, 1.0f),
			Size2(0, 0)); // char placing based on atlas data
	setColor(Color4F(color));

	Vertex *data = const_cast<Vertex *>(vertexes.data());

	const float texLeft = 0.0f;
	const float texRight = 1.0f;
	const float texTop = 0.0f;
	const float texBottom = 1.0f;

	data[0].tex = Vec2(texLeft, texTop);
	data[1].tex = Vec2(texLeft, texBottom);
	data[2].tex = Vec2(texRight, texTop);
	data[3].tex = Vec2(texRight, texBottom);

	data[0].object = font::CharId::getCharId(face, charID, font::CharAnchor::BottomLeft);
	data[1].object = font::CharId::getCharId(face, charID, font::CharAnchor::TopLeft);
	data[2].object = font::CharId::getCharId(face, charID, font::CharAnchor::BottomRight);
	data[3].object = font::CharId::getCharId(face, charID, font::CharAnchor::TopRight);

	return *this;
}

VertexArray::Quad &VertexArray::Quad::drawUnderlineRect(int16_t charX, int16_t charY,
		uint16_t width, uint16_t height, const Color4B &color, float layer) {
	setGeometry(Vec4(charX, charY, layer, 1.0f), Size2(width, height));
	setColor(Color4F(color));

	Vertex *data = const_cast<Vertex *>(vertexes.data());

	const float texLeft = 0.0f;
	const float texRight = 1.0f;
	const float texTop = 0.0f;
	const float texBottom = 1.0f;

	data[0].tex = Vec2(texLeft, texTop);
	data[1].tex = Vec2(texLeft, texBottom);
	data[2].tex = Vec2(texRight, texTop);
	data[3].tex = Vec2(texRight, texBottom);

	data[0].object =
			font::CharId::getCharId(font::CharId::SourceMax, 0, font::CharAnchor::BottomLeft);
	data[1].object = font::CharId::getCharId(font::CharId::SourceMax, 0, font::CharAnchor::TopLeft);
	data[2].object =
			font::CharId::getCharId(font::CharId::SourceMax, 0, font::CharAnchor::BottomRight);
	data[3].object =
			font::CharId::getCharId(font::CharId::SourceMax, 0, font::CharAnchor::TopRight);

	return *this;
}

VertexArray::~VertexArray() { _data = nullptr; }

bool VertexArray::init(uint32_t bufferCapacity, uint32_t indexCapacity) {
	_data = Rc<VertexData>::alloc();
	_data->data.reserve(bufferCapacity);
	_data->indexes.reserve(indexCapacity);
	return true;
}

bool VertexArray::init(const Rc<VertexData> &data) {
	_data = data;
	_copyOnWrite = true;
	return true;
}

void VertexArray::reserve(uint32_t bufferCapacity, uint32_t indexCapacity) {
	if (_copyOnWrite) {
		copy();
	}

	if (_data->data.capacity() > bufferCapacity) {
		_data->data.reserve(bufferCapacity);
	}
	if (_data->indexes.capacity() > indexCapacity) {
		_data->indexes.reserve(indexCapacity);
	}
}

Rc<VertexData> VertexArray::pop() {
	_copyOnWrite = true;
	return _data;
}

Rc<VertexData> VertexArray::dup() {
	auto data = Rc<VertexData>::alloc();
	data->data = _data->data;
	data->indexes = _data->indexes;
	return data;
}

bool VertexArray::empty() const { return _data->indexes.empty() || _data->data.empty(); }

void VertexArray::clear() {
	if (_copyOnWrite) {
		_data = Rc<VertexData>::alloc();
	} else {
		_data->data.clear();
		_data->indexes.clear();
	}
}

VertexArray::Quad VertexArray::addQuad() {
	if (_copyOnWrite) {
		copy();
	}

	auto firstVertex = uint32_t(_data->data.size());
	auto firstIndex = uint32_t(_data->indexes.size());

	_data->data.resize(_data->data.size() + 4);
	_data->indexes.resize(_data->indexes.size() + 6);

	// 0 - 2
	// |   |
	// 1 - 3
	//
	// counter-clockwise:

	_data->indexes[firstIndex + 0] = firstVertex + 0;
	_data->indexes[firstIndex + 1] = firstVertex + 1;
	_data->indexes[firstIndex + 2] = firstVertex + 2;
	_data->indexes[firstIndex + 3] = firstVertex + 3;
	_data->indexes[firstIndex + 4] = firstVertex + 2;
	_data->indexes[firstIndex + 5] = firstVertex + 1;

	return Quad({SpanView<Vertex>(_data->data.data() + firstVertex, 4),
		SpanView<uint32_t>(_data->indexes.data() + firstIndex, 6), firstVertex, firstIndex});
}

VertexArray::Quad VertexArray::getQuad(size_t firstVertex, size_t firstIndex) {
	if (_copyOnWrite) {
		copy();
	}

	return Quad({SpanView<Vertex>(_data->data.data() + firstVertex, 4),
		SpanView<uint32_t>(_data->indexes.data() + firstIndex, 6), firstVertex, firstIndex});
}

void VertexArray::updateColor(const Color4F &color) {
	if (_copyOnWrite) {
		copy();
	}

	for (auto &it : _data->data) { it.color = color; }
}

void VertexArray::updateColor(const Color4F &color, const Vector<ColorMask> &mask) {
	if (_copyOnWrite) {
		copy();
	}

	auto count = std::min(_data->data.size(), mask.size());

	for (size_t i = 0; i < count; ++i) {
		switch (mask[i]) {
		case ColorMask::None: break;
		case ColorMask::All: _data->data[i].color = color; break;
		case ColorMask::Color:
			_data->data[i].color.x = color.r;
			_data->data[i].color.y = color.g;
			_data->data[i].color.z = color.b;
			break;
		case ColorMask::A: _data->data[i].color.w = color.a; break;
		default:
			if ((mask[i] & ColorMask::R) != ColorMask::None) {
				_data->data[i].color.x = color.r;
			}
			if ((mask[i] & ColorMask::G) != ColorMask::None) {
				_data->data[i].color.y = color.g;
			}
			if ((mask[i] & ColorMask::B) != ColorMask::None) {
				_data->data[i].color.z = color.b;
			}
			if ((mask[i] & ColorMask::A) != ColorMask::None) {
				_data->data[i].color.w = color.a;
			}
			break;
		}
	}
}

void VertexArray::updateColorQuads(const Color4F &color, const Vector<ColorMask> &mask) {
	if (_copyOnWrite) {
		copy();
	}

	auto quadsCount = _data->data.size() / 4;
	auto count = std::min(quadsCount, mask.size());

	for (size_t i = 0; i < count; ++i) {
		switch (mask[i]) {
		case ColorMask::None: break;
		case ColorMask::All:
			for (size_t j = 0; j < 4; ++j) { _data->data[i * 4 + j].color = color; }
			break;
		case ColorMask::Color:
			for (size_t j = 0; j < 4; ++j) {
				_data->data[i * 4 + j].color.x = color.r;
				_data->data[i * 4 + j].color.y = color.g;
				_data->data[i * 4 + j].color.z = color.b;
			}
			break;
		case ColorMask::A:
			for (size_t j = 0; j < 4; ++j) { _data->data[i * 4 + j].color.w = color.a; }
			break;
		default:
			for (size_t j = 0; j < 4; ++j) {
				if ((mask[i] & ColorMask::R) != ColorMask::None) {
					_data->data[i * 4 + j].color.x = color.r;
				}
				if ((mask[i] & ColorMask::G) != ColorMask::None) {
					_data->data[i * 4 + j].color.y = color.g;
				}
				if ((mask[i] & ColorMask::B) != ColorMask::None) {
					_data->data[i * 4 + j].color.z = color.b;
				}
				if ((mask[i] & ColorMask::A) != ColorMask::None) {
					_data->data[i * 4 + j].color.w = color.a;
				}
			}
			break;
		}
	}
}

size_t VertexArray::getVertexCount() const { return _data->data.size(); }

size_t VertexArray::getIndexCount() const { return _data->indexes.size(); }

void VertexArray::copy() {
	if (_copyOnWrite) {
		auto data = Rc<VertexData>::alloc();
		data->data = _data->data;
		data->indexes = _data->indexes;
		_data = data;
		_copyOnWrite = false;
	}
}

} // namespace stappler::xenolith::basic2d
