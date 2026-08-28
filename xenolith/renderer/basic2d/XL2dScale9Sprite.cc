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

#include "XL2dScale9Sprite.h"

#include "XLTexture.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

void Scale9Sprite::setSlice(const Padding &slice) {
	if (_slice != slice) {
		_slice = slice;
		_sliceReported = false;
		_vertexesDirty = true;
	}
}

void Scale9Sprite::setSlice(float top, float right, float bottom, float left) {
	setSlice(Padding(top, right, bottom, left));
}

void Scale9Sprite::setFillCenter(bool value) {
	if (_fillCenter != value) {
		_fillCenter = value;
		_vertexesDirty = true;
	}
}

void Scale9Sprite::setTextureAutofit(Autofit value) {
	if (value != Autofit::None) {
		// A nine-slice covers the whole content rect by construction, so autofit has nothing left
		// to decide. Refused rather than ignored: a setter that quietly does nothing is worse.
		log::source().warn("Scale9Sprite", "Autofit is not applicable to a nine-slice sprite; "
										   "the value is refused and autofit stays None");
		return;
	}
	Sprite::setTextureAutofit(value);
}

void Scale9Sprite::initVertexes() {
	// Nine quads is the maximum; a zero side or an unfilled centre only makes it fewer.
	_vertexes.init(9 * 4, 9 * 6);
	_vertexesDirty = true;
}

bool Scale9Sprite::checkVertexDirty() const {
	if (Sprite::checkVertexDirty()) {
		return true;
	}

	// Sprite::draw watches the texture's size only under autofit, because that is the only case in
	// which the base class needs it. Here the slice is measured in pixels of that texture, so a
	// texture that arrived (or was swapped for one of another size) changes the geometry.
	return _texture && _targetTextureSize != _texture->getExtent();
}

uint32_t Scale9Sprite::buildPieces(const ImagePlacementResult &placement, const Size2 &texSize,
		Piece *out) const {
	auto &fragment = placement.imageFragmentSize;

	// The authoring refusal: no middle to stretch. An all-zero slice is NOT this case - it is a
	// plain sprite, and comes out of the loop below as a single centre piece.
	if (_slice.top < 0.0f || _slice.right < 0.0f || _slice.bottom < 0.0f || _slice.left < 0.0f
			|| _slice.horizontal() >= fragment.width || _slice.vertical() >= fragment.height) {
		return 0;
	}

	auto &view = placement.viewRect;

	// A content rect narrower than its own corners is layout, not an error: the corners shrink
	// together, keeping their ratio, and the band between them comes out empty.
	float left = _slice.left, right = _slice.right;
	if (_slice.horizontal() > view.size.width) {
		auto k = view.size.width / _slice.horizontal();
		left *= k;
		right *= k;
	}

	float top = _slice.top, bottom = _slice.bottom;
	if (_slice.vertical() > view.size.height) {
		auto k = view.size.height / _slice.vertical();
		top *= k;
		bottom *= k;
	}

	// View edges, y growing UP: y[0] is the bottom of the sprite, y[3] its top.
	const float x[4] = {view.origin.x, view.origin.x + left,
		view.origin.x + view.size.width - right, view.origin.x + view.size.width};
	const float y[4] = {view.origin.y, view.origin.y + bottom,
		view.origin.y + view.size.height - top, view.origin.y + view.size.height};

	// Texture edges, v growing DOWN: v[0] is the top row of the fragment. The slice is in pixels of
	// the fragment while the rect is normalized against the whole texture, so it divides by the
	// texture's size - not by the fragment's.
	auto &tex = placement.textureRect;
	const float u[4] = {tex.origin.x, tex.origin.x + _slice.left / texSize.width,
		tex.origin.x + tex.size.width - _slice.right / texSize.width,
		tex.origin.x + tex.size.width};
	const float v[4] = {tex.origin.y, tex.origin.y + _slice.top / texSize.height,
		tex.origin.y + tex.size.height - _slice.bottom / texSize.height,
		tex.origin.y + tex.size.height};

	uint32_t count = 0;

	// Rows are walked TOP-DOWN, because that is the direction the texture is read in, while the
	// view is built bottom-up: row 0 is the top band, and it takes y[3]..y[2].
	for (uint32_t row = 0; row < 3; ++row) {
		const float yTop = y[3 - row];
		const float yBottom = y[2 - row];
		if (yTop <= yBottom) {
			continue; // a zero side, or a band squeezed out by shrunken corners
		}

		for (uint32_t col = 0; col < 3; ++col) {
			if (row == 1 && col == 1 && !_fillCenter) {
				continue;
			}

			const float xLeft = x[col];
			const float xRight = x[col + 1];
			if (xRight <= xLeft) {
				continue;
			}

			out[count].view = Rect(xLeft, yBottom, xRight - xLeft, yTop - yBottom);
			out[count].texture = Rect(u[col], v[row], u[col + 1] - u[col], v[row + 1] - v[row]);
			++count;
		}
	}

	return count;
}

void Scale9Sprite::updateVertexes(FrameInfo &frame) {
	auto extent = _texture->getExtent();
	_targetTextureSize = extent;

	auto texSize = Size2(extent.width, extent.height);
	auto placement = _texturePlacement.resolve(_contentSize, texSize);

	Piece pieces[9];
	auto count = buildPieces(placement, texSize, pieces);
	if (count == 0) {
		if (!_sliceReported) {
			log::source().warn("Scale9Sprite", "Slice (top ", _slice.top, ", right ", _slice.right,
					", bottom ", _slice.bottom, ", left ", _slice.left, ") leaves no middle in a ",
					placement.imageFragmentSize.width, "x", placement.imageFragmentSize.height,
					" fragment of texture '", _texture->getName(),
					"'; drawing it as a plain sprite");
			_sliceReported = true;
		}

		// Visibly wrong beats invisible: a frame drawn stretched is a bug an author can see, a
		// frame that is not drawn at all is a bug they have to look for.
		Sprite::updateVertexes(frame);
		return;
	}

	_vertexes.clear();

	for (uint32_t i = 0; i < count; ++i) {
		_vertexes.addQuad()
				.setGeometry(
						Vec4(pieces[i].view.origin.x, pieces[i].view.origin.y, _textureLayer, 1.0f),
						pieces[i].view.size)
				.setTextureRect(pieces[i].texture, 1.0f, 1.0f, _flippedX, _flippedY, _rotated)
				.setColor(_displayedColor);
	}

	_textureScale = placement.scale;
	_vertexColorDirty = false;
}

} // namespace stappler::xenolith::basic2d
