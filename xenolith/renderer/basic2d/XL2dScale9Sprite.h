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

/** A sprite cut by four numbers: the corners keep their size, the edges stretch along one axis and
the middle along both.

A frame, a panel, a button background: one small picture that has to come out right at any size.
Sprite stretches everything, which rounds the corners of a frame into ovals; this one stretches only
what may be stretched.

THE FOUR NUMBERS ARE PIXELS OF THE IMAGE, IN CSS ORDER (top, right, bottom, left). That is how
`border-image-slice` names them and how an asset catalogue entry carries them, so the numbers an
author sees in one place are the numbers they type in the other. Normalized fractions would have to
be recomputed every time the picture moves inside an atlas.

PIXELS OF THE FRAGMENT, NOT OF THE TEXTURE. A sprite may look into a sub-rect of an atlas
(Sprite::setTextureRect, normalized); the slice is measured against THAT fragment. Otherwise the
same picture, moved into an atlas, would need different four numbers.

AUTOFIT IS REFUSED, NOT IGNORED. A nine-slice covers the whole content rect by construction, so
there is nothing left for autofit to decide; setTextureAutofit reports the refusal and keeps None.

The refusal that matters is an authoring one: slices that do not leave a middle (left + right >= the
fragment's width, or top + bottom >= its height) have nothing to stretch. That is reported once per
change, with the numbers, and the sprite falls back to a single quad - a visibly wrong picture
rather than nothing at all.

A content rect SMALLER than the slices is not an error: layout is allowed to hand out any size. The
corners shrink proportionally and the bands between them collapse to nothing. */
class SP_PUBLIC Scale9Sprite : public Sprite {
public:
	virtual ~Scale9Sprite() { }

	// Slice in pixels of the image fragment. Padding, rather than four loose floats or a Vec4,
	// because it is already the engine's name for four sides in CSS order - and a named field
	// cannot be passed in the wrong order.
	virtual void setSlice(const Padding &);
	virtual void setSlice(float top, float right, float bottom, float left);
	virtual const Padding &getSlice() const { return _slice; }

	// Draw the middle piece. A frame that is only a border turns it off; the eight other pieces are
	// unaffected.
	virtual void setFillCenter(bool);
	virtual bool isCenterFilled() const { return _fillCenter; }

	// Refused: see the class comment. Kept in the interface so the refusal is reported rather than
	// silently doing nothing.
	virtual void setTextureAutofit(Autofit) override;

protected:
	// No `using Sprite::init` here: this class adds no init() of its own, so the base's overloads
	// are not hidden - and re-exporting them from a protected section would make Rc::create() stop
	// seeing init(StringView) and try to construct the sprite from the texture name instead.

	virtual void initVertexes() override;
	virtual void updateVertexes(FrameInfo &frame) override;

	// The texture's own size takes part in the geometry here (the slice is in pixels of it), while
	// the base class only watches it when autofit is on. So it is watched here instead.
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

	// The refusal is reported once per change, not once per frame: a sprite that draws at 60 Hz
	// would otherwise fill the log with the same line.
	mutable bool _sliceReported = false;
};

} // namespace stappler::xenolith::basic2d

#endif /* XENOLITH_RENDERER_BASIC2D_XL2DSCALE9SPRITE_H_ */
