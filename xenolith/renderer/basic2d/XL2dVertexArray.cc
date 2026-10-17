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

VertexData::Bounds VertexData::getBounds(const core::DataAtlas *atlas) const {
	if (hasLayoutBounds()) {
		return Bounds{layoutBounds, 0};
	}

	const uint64_t atlasSerial = atlas ? atlas->getSerial() : 0;
	{
		sprt::unique_lock<sprt::mutex> lock(cachedBoundsMutex);
		if (cachedBoundsGeneration == identity.generation && cachedBoundsAtlas == atlasSerial) {
			return cachedBounds;
		}
	}

	// an atlas entry as the shader reads it (xl_2d_flat.vert): the offset of the vertex, and where
	// it samples
	struct AtlasData {
		Vec2 pos;
		Vec2 tex;
	};

	Bounds ret;
	bool first = true;
	float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f;
	for (auto &it : data) {
		float x = it.pos.x;
		float y = it.pos.y;
		if (atlas && it.object != 0) {
			// not in the atlas, the shader leaves the vertex where it is
			if (auto d = reinterpret_cast<const AtlasData *>(atlas->getObjectByName(it.object))) {
				x += d->pos.x;
				y += d->pos.y;
			} else {
				ret.unresolved = (ret.unresolved ^ it.object) * 0x100'0000'01b3ULL;
			}
		}
		if (first) {
			minX = maxX = x;
			minY = maxY = y;
			first = false;
		} else {
			minX = sprt::min(minX, x);
			maxX = sprt::max(maxX, x);
			minY = sprt::min(minY, y);
			maxY = sprt::max(maxY, y);
		}
	}
	if (!first) {
		ret.box = Rect(minX, minY, maxX - minX, maxY - minY);
	}

	sprt::unique_lock<sprt::mutex> lock(cachedBoundsMutex);
	cachedBounds = ret;
	cachedBoundsAtlas = atlasSerial;
	cachedBoundsGeneration = identity.generation;
	return ret;
}

/* The four corner texture coordinates a fragment maps to, tl bl tr br, with flips and the
quarter turn a frame laid sideways on a sheet needs. Quad and Polygon answer through the same
helper so the two paths cannot drift: a rectangle ring and a quad of the same arguments sample
the same texels. */
static void VertexArray_texCorners(const Rect &texRect, float texWidth, float texHeight,
		bool flippedX, bool flippedY, bool rotated, Vec2 out[4]) {
	float texLeft = texRect.origin.x / texWidth;
	float texRight = (texRect.origin.x + texRect.size.width) / texWidth;
	float texTop = texRect.origin.y / texHeight;
	float texBottom = (texRect.origin.y + texRect.size.height) / texHeight;

	if (flippedX) {
		sprt::swap(texLeft, texRight);
	}

	if (flippedY) {
		sprt::swap(texTop, texBottom);
	}

	if (!rotated) {
		// tl bl tr br
		out[0] = Vec2(texLeft, texTop);
		out[1] = Vec2(texLeft, texBottom);
		out[2] = Vec2(texRight, texTop);
		out[3] = Vec2(texRight, texBottom);
	} else {
		// tl bl tr br
		out[0] = Vec2(texLeft, texTop);
		out[1] = Vec2(texRight, texTop);
		out[2] = Vec2(texLeft, texBottom);
		out[3] = Vec2(texRight, texBottom);
	}
}

VertexArray::Quad &VertexArray::Quad::setTextureRect(const Rect &texRect, float texWidth,
		float texHeight, bool flippedX, bool flippedY, bool rotated) {

	Vertex *data = const_cast<Vertex *>(vertexes.data());

	Vec2 tex[4];
	VertexArray_texCorners(texRect, texWidth, texHeight, flippedX, flippedY, rotated, tex);
	for (uint32_t i = 0; i < 4; ++i) {
		data[i].tex = tex[i];
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
		sprt::initializer_list<Color4F> &&colors) { // tl bl tr br
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
	mutate();

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
		_data->identity.invalidate();
		_data->data.clear();
		_data->indexes.clear();
	}
}

VertexArray::Quad VertexArray::addQuad() {
	mutate();

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
	mutate();

	return Quad({SpanView<Vertex>(_data->data.data() + firstVertex, 4),
		SpanView<uint32_t>(_data->indexes.data() + firstIndex, 6), firstVertex, firstIndex});
}

/* Ear clipping over a ring in image space (x right, y down). Everything the caller may hand over
is normalized to [0..1], so one epsilon serves every ring; a distance or an area below it is
sub-pixel at any size a sprite fragment has.

Deterministic by construction: the ring is canonicalized to one orientation (the quad's own
facing, negative shoelace in image space) and the ear scan always starts from the same vertex of
the canonical ring, so the same shape walked in either direction yields the same triangles.

`ring` receives the kept points in canonical order, `out` the triangle corners as positions into
that ring. False when the ring cannot be a mesh - and then neither output is used. */
static bool VertexArray_triangulate(SpanView<const Vec2> points, Vector<Vec2> &ring,
		Vector<uint32_t> &out) {
	constexpr float Eps = 1e-7f;

	ring.clear();
	for (size_t i = 0; i < points.size(); ++i) {
		const auto &p = points[i];
		if (!ring.empty() && sprt::abs(p.x - ring.back().x) < Eps
				&& sprt::abs(p.y - ring.back().y) < Eps) {
			continue;
		}
		ring.push_back(p);
	}
	// A closed ring repeats its first point last; both repeats fall to the dedup above.
	while (ring.size() > 1 && sprt::abs(ring.front().x - ring.back().x) < Eps
			&& sprt::abs(ring.front().y - ring.back().y) < Eps) {
		ring.pop_back();
	}

	// Straight points can never be an ear and would stall the walk; drop them before it starts.
	auto crossAt = [&](size_t i) {
		const auto &p = ring[(i + ring.size() - 1) % ring.size()];
		const auto &c = ring[i];
		const auto &n = ring[(i + 1) % ring.size()];
		return (c.x - p.x) * (n.y - c.y) - (c.y - p.y) * (n.x - c.x);
	};
	bool dropped = true;
	while (dropped && ring.size() > 3) {
		dropped = false;
		for (size_t i = 0; i < ring.size(); ++i) {
			if (sprt::abs(crossAt(i)) < Eps) {
				ring.erase(ring.begin() + i);
				dropped = true;
				break;
			}
		}
	}

	if (ring.size() < 3) {
		return false;
	}

	// The quad's triangles run clockwise in image space (y down); the mesh must face with them.
	float shoelace = 0.0f;
	for (size_t i = 0; i < ring.size(); ++i) {
		const auto &a = ring[i];
		const auto &b = ring[(i + 1) % ring.size()];
		shoelace += a.x * b.y - b.x * a.y;
	}
	if (sprt::abs(shoelace) < Eps) {
		// Zero area: every kept point is on one line, whatever their count.
		return false;
	}
	if (shoelace > 0.0f) {
		sprt::reverse(ring.begin(), ring.end());
	}

	/* The walk runs over POSITIONS into the canonical ring, and only the walk list shrinks - the
	ring itself is what the caller receives, and the triangle corners must stay positions into it
	from the first clip to the last. */
	Vector<uint32_t> loop;
	loop.reserve(ring.size());
	for (size_t j = 0; j < ring.size(); ++j) {
		loop.push_back(uint32_t(j));
	}

	size_t i = 0;
	size_t lap = 0;
	while (loop.size() > 3) {
		const auto m = loop.size();
		const auto prevIdx = loop[(i + m - 1) % m];
		const auto curIdx = loop[i];
		const auto nextIdx = loop[(i + 1) % m];
		const auto &p = ring[prevIdx];
		const auto &c = ring[curIdx];
		const auto &nx = ring[nextIdx];
		const auto cross = (c.x - p.x) * (nx.y - c.y) - (c.y - p.y) * (nx.x - c.x);

		bool ear = cross < -Eps;
		for (size_t j = 0; j < m && ear; ++j) {
			const auto o = loop[j];
			if (o == prevIdx || o == curIdx || o == nextIdx) {
				continue;
			}
			const auto &q = ring[o];
			const auto d0 = (c.x - p.x) * (q.y - p.y) - (c.y - p.y) * (q.x - p.x);
			const auto d1 = (nx.x - c.x) * (q.y - c.y) - (nx.y - c.y) * (q.x - c.x);
			const auto d2 = (p.x - nx.x) * (q.y - nx.y) - (p.y - nx.y) * (q.x - nx.x);
			// Inside strictly is all three on the inner side; a point on an edge does not block.
			if (d0 < -Eps && d1 < -Eps && d2 < -Eps) {
				ear = false;
			}
		}

		if (ear) {
			out.push_back(prevIdx);
			out.push_back(curIdx);
			out.push_back(nextIdx);
			loop.erase(loop.begin() + i);
			if (i >= loop.size()) {
				i = 0;
			}
			lap = 0;
		} else {
			i = (i + 1) % loop.size();
			++lap;
			if (lap > loop.size()) {
				// A whole lap without an ear: the ring is not a simple polygon.
				out.clear();
				return false;
			}
		}
	}

	out.push_back(loop[0]);
	out.push_back(loop[1]);
	out.push_back(loop[2]);
	return true;
}

VertexArray::Polygon VertexArray::addPolygon(SpanView<const Vec2> points) {
	mutate();

	Vector<Vec2> ring;
	Vector<uint32_t> indexes;
	if (!VertexArray_triangulate(points, ring, indexes)) {
		return Polygon({Vector<Vec2>(), SpanView<Vertex>(), SpanView<uint32_t>(),
			_data->data.size(), _data->indexes.size(), false});
	}

	const auto firstVertex = uint32_t(_data->data.size());
	const auto firstIndex = uint32_t(_data->indexes.size());
	const auto count = ring.size();

	_data->data.resize(_data->data.size() + count);
	for (size_t j = 0; j < indexes.size(); ++j) {
		_data->indexes.push_back(firstVertex + indexes[j]);
	}

	return Polygon({sp::move(ring), SpanView<Vertex>(_data->data.data() + firstVertex, count),
		SpanView<uint32_t>(_data->indexes.data() + firstIndex, indexes.size()), firstVertex,
		firstIndex, true});
}

VertexArray::Polygon &VertexArray::Polygon::setGeometry(const Vec4 &origin, const Size2 &size) {
	if (!valid) {
		return *this;
	}

	Vertex *data = const_cast<Vertex *>(vertexes.data());
	for (size_t i = 0; i < ring.size(); ++i) {
		const auto &p = ring[i];
		data[i].pos = Vec4(origin.x + p.x * size.width,
				origin.y + size.height - p.y * size.height, origin.z, origin.w);
	}

	return *this;
}

VertexArray::Polygon &VertexArray::Polygon::setTextureRect(const Rect &texRect, float texWidth,
		float texHeight, bool flippedX, bool flippedY, bool rotated) {
	if (!valid) {
		return *this;
	}

	Vertex *data = const_cast<Vertex *>(vertexes.data());

	Vec2 tex[4];
	VertexArray_texCorners(texRect, texWidth, texHeight, flippedX, flippedY, rotated, tex);

	// The corner mapping is affine (the quarter turn included), so the corner answers at the
	// corners are exact and every inner point is the same bilinear combination of them.
	const auto &tl = tex[0];
	const auto &bl = tex[1];
	const auto &tr = tex[2];
	for (size_t i = 0; i < ring.size(); ++i) {
		const auto &p = ring[i];
		data[i].tex = Vec2(tl.x + p.x * (tr.x - tl.x) + p.y * (bl.x - tl.x),
				tl.y + p.x * (tr.y - tl.y) + p.y * (bl.y - tl.y));
	}

	return *this;
}

VertexArray::Polygon &VertexArray::Polygon::setColor(const Color4F &color) {
	if (!valid) {
		return *this;
	}

	Vertex *data = const_cast<Vertex *>(vertexes.data());
	for (size_t i = 0; i < ring.size(); ++i) {
		data[i].color = color;
	}

	return *this;
}

void VertexArray::updateColor(const Color4F &color) {
	mutateColor();

	for (auto &it : _data->data) { it.color = color; }
}

void VertexArray::updateColor(const Color4F &color, const Vector<ColorMask> &mask) {
	mutateColor();

	auto count = sprt::min(_data->data.size(), mask.size());

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
	mutateColor();

	auto quadsCount = _data->data.size() / 4;
	auto count = sprt::min(quadsCount, mask.size());

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

void VertexArray::setLayoutBounds(const Rect &r) {
	// not mutate(): supplying bounds is not a content change
	_data->layoutBounds = r;
	_data->layoutBoundsGeneration = _data->identity.generation;
}

void VertexArray::mutateColor() {
	// a colour change moves no vertex, so the layout box carries over to the new generation
	const bool bounded = _data->hasLayoutBounds();
	const auto box = _data->layoutBounds;
	mutate();
	if (bounded) {
		setLayoutBounds(box);
	}
}

void VertexArray::mutate() {
	if (_copyOnWrite) {
		// detaching allocates a fresh VertexData, which carries a fresh identity already
		copy();
	} else {
		// we own the set exclusively: nothing else observes the change, so the generation is
		// the only signal damage tracking can see
		_data->identity.invalidate();
	}
}

} // namespace stappler::xenolith::basic2d
