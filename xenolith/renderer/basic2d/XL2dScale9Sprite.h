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

#ifndef XENOLITH_RENDERER_BASIC2D_XL2DSCALE9SPRITE_H_
#define XENOLITH_RENDERER_BASIC2D_XL2DSCALE9SPRITE_H_

#include "XL2dSprite.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

/** A nine-slice sprite: the corners keep their size, the edges stretch along one axis and the
middle along both.

The slice is in pixels of the image fragment (the Sprite::setTextureRect sub-rect, not the whole
texture), in CSS order (top, right, bottom, left) as `border-image-slice`.

Autofit is refused: setTextureAutofit reports it and keeps None. Slices that leave no middle
(left + right >= fragment width, or top + bottom >= height) are reported once per change and the
sprite falls back to a single quad. A content rect smaller than the slices is valid: the corners
shrink proportionally and the bands collapse. */
class SP_PUBLIC Scale9Sprite : public Sprite {
public:
	virtual ~Scale9Sprite() { }

	// Slice in pixels of the image fragment, as Padding (four sides in CSS order).
	virtual void setSlice(const Padding &);
	virtual void setSlice(float top, float right, float bottom, float left);
	virtual const Padding &getSlice() const { return _slice; }

	// Draw the middle piece. A frame that is only a border turns it off; the eight other pieces are
	// unaffected.
	virtual void setFillCenter(bool);
	virtual bool isCenterFilled() const { return _fillCenter; }

	// Refused: see the class comment. Kept in the interface so the refusal is reported.
	virtual void setTextureAutofit(Autofit) override;

protected:
	// No `using Sprite::init` here: this class adds no init() of its own, and re-exporting the base
	// overloads from a protected section would make Rc::create() miss init(StringView).

	virtual void initVertexes() override;
	virtual void updateVertexes(FrameInfo &frame) override;

	// The texture's own size affects the geometry here (the slice is in its pixels), while the base
	// class watches it only with autofit on.
	virtual bool checkVertexDirty() const override;

	// Everything the geometry needs, resolved once: the pieces that survive, in view coordinates
	// and in normalized texture coordinates. Returns the number of pieces written; 0 means the
	// slice is unusable and the caller falls back to a single quad.
	struct Piece {
		Rect view;
		Rect texture;
	};

	uint32_t buildPieces(const ImagePlacementResult &, const Size2 &texSize, Piece *out) const;

	Padding _slice;
	bool _fillCenter = true;

	// The refusal is reported once per change, not once per frame.
	mutable bool _sliceReported = false;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DSCALE9SPRITE_H_ */
