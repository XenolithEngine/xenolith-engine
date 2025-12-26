/**
 Copyright (c) 2024 Stappler LLC <admin@stappler.dev>
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

#include "SPDocLayoutObject.h"
#include "SPDocLayoutResult.h"
#include "SPDocLayoutBlock.h"

namespace STAPPLER_VERSIONIZED stappler::document {

bool Object::isLink() const { return type == Type::Link; }

bool Object::isLabel() const { return type == Type::Label; }

bool Object::isBackground() const { return type == Type::Background; }

const Link *Object::asLink() const {
	if (type == Type::Link) {
		return static_cast<const Link *>(this);
	}
	return nullptr;
}
const PathObject *Object::asPath() const {
	if (type == Type::Path) {
		return static_cast<const PathObject *>(this);
	}
	return nullptr;
}
const Label *Object::asLabel() const {
	if (type == Type::Label) {
		return static_cast<const Label *>(this);
	}
	return nullptr;
}
const Background *Object::asBackground() const {
	if (type == Type::Background) {
		return static_cast<const Background *>(this);
	}
	return nullptr;
}

bool BorderParams::merge(const BorderParams &p) {
	if (p.width > width) {
		*this = p;
		return true;
	}
	return false;
}

bool BorderParams::compare(const BorderParams &p) const {
	return p.style == style && p.width == width && p.color == color;
}

bool BorderParams::isVisible() const {
	return style != BorderStyle::None && width >= 0.5f && color.a > 0;
}

void PathObject::makeBorder(LayoutResult *res, LayoutBlock &l, const Rect &bb,
		const OutlineParameters &style, float w, ZOrder zIndex, const MediaParameters &media) {
	BorderParams left, top, right, bottom;
	if (style.top.style != BorderStyle::None) {
		top = BorderParams{style.top.style, media.computeValueAuto(style.top.width, w),
			style.top.color};
	}
	if (style.right.style != BorderStyle::None) {
		right = BorderParams{style.right.style, media.computeValueAuto(style.right.width, w),
			style.right.color};
	}
	if (style.bottom.style != BorderStyle::None) {
		bottom = BorderParams{style.bottom.style, media.computeValueAuto(style.bottom.width, w),
			style.bottom.color};
	}
	if (style.left.style != BorderStyle::None) {
		left = BorderParams{style.left.style, media.computeValueAuto(style.left.width, w),
			style.left.color};
	}

	if (left.compare(right) && top.compare(bottom) && left.compare(top)) {
		l.objects.emplace_back(res->emplaceOutline(l,
				Rect(bb.origin.x + left.width / 2.0f, bb.origin.y + left.width / 2.0f,
						bb.size.width - left.width, bb.size.height - left.width),
				left.color, zIndex, left.width, left.style));
	} else {
		if (left.isVisible()) {
			auto p = res->emplacePath(l, zIndex);
			p->drawVerticalLineSegment(bb.origin, bb.size.height, left.color, left.width,
					left.style, 0.0f, 0.0f, top.width, bottom.width, 0.0f, 0.0f);
			l.objects.emplace_back(p);
		}
		if (top.isVisible()) {
			auto p = res->emplacePath(l, zIndex);
			p->drawHorizontalLineSegment(bb.origin, bb.size.width, top.color, top.width, top.style,
					left.width, 0.0f, 0.0f, 0.0f, 0.0f, right.width);
			l.objects.emplace_back(p);
		}
		if (right.isVisible()) {
			auto p = res->emplacePath(l, zIndex);
			p->drawVerticalLineSegment(Vec2(bb.origin.x + bb.size.width, bb.origin.y),
					bb.size.height, right.color, right.width, right.style, top.width, 0.0f, 0.0f,
					0.0f, 0.0f, bottom.width);
			l.objects.emplace_back(p);
		}
		if (bottom.isVisible()) {
			auto p = res->emplacePath(l, zIndex);
			p->drawHorizontalLineSegment(Vec2(bb.origin.x, bb.origin.y + bb.size.height),
					bb.size.width, bottom.color, bottom.width, bottom.style, 0.0f, 0.0f, left.width,
					right.width, 0.0f, 0.0f);
			l.objects.emplace_back(p);
		}
	}
}

void PathObject::drawOutline(const Rect &bb, const Color4B &color, float width, BorderStyle style) {
	Rect rect(bb.origin.x - width, bb.origin.y - width, bb.size.width + width * 2.0f,
			bb.size.height + width * 2.0f);
	bbox = rect;
	path.params.winding = vg::Winding::EvenOdd;
	path.params.style = vg::DrawFlags::Fill;
	path.params.fillColor = color;

	path.getWriter()
			.moveTo(0.0f, 0.0f)
			.lineTo(0.0f, rect.size.height)
			.lineTo(rect.size.width, rect.size.height)
			.lineTo(rect.size.width, 0.0f)
			.closePath()

			.moveTo(width, width)
			.lineTo(rect.size.width - width, width)
			.lineTo(rect.size.width - width, rect.size.height - width)
			.lineTo(width, rect.size.height - width)
			.closePath();
}

void PathObject::drawRect(const Rect &rect, const Color4B &color) {
	bbox = rect;
	path.params.style = vg::DrawFlags::Fill;
	path.params.fillColor = color;
	path.getWriter().addRect(0.0f, 0.0f, rect.size.width, rect.size.height);
}

void PathObject::drawVerticalLineSegment(const Vec2 &origin, float height, const Color4B &color,
		float border, BorderStyle style, float wTopLeft, float wTop, float wTopRight,
		float wBottomRight, float wBottom, float wBottomLeft) {
	path.params.style = vg::DrawFlags::Fill;
	path.params.fillColor = color;

	const float extraTopSize = (wTop == 0.0f
									   && ((wTopLeft == 0.0f && wTopRight != 0.0f)
											   || (wTopLeft != 0.0f && wTopRight == 0.0f)))
			? max(wTopLeft, wTopRight) / 2.0f
			: 0.0f;
	const float extraBottomSize = (wBottom == 0.0f
										  && ((wBottomLeft == 0.0f && wBottomRight != 0.0f)
												  || (wBottomLeft != 0.0f && wBottomRight == 0.0f)))
			? max(wBottomLeft, wBottomRight) / 2.0f
			: 0.0f;

	const float originX = origin.x - border / 2.0f;
	const float originY = origin.y - extraTopSize;

	bbox = Rect(originX, originY, border, height + extraTopSize + extraBottomSize);

	auto writer = path.getWriter();

	if (wBottom == 0.0f) {
		if ((wBottomLeft == 0.0f && wBottomRight != 0.0f)
				|| (wBottomLeft != 0.0f && wBottomRight == 0.0f)) {
			if (wBottomLeft == 0.0f) {
				writer.moveTo(0.0f, 0.0f);
				writer.lineTo(border, wBottomRight);
			} else {
				writer.moveTo(0.0f, wBottomLeft);
				writer.lineTo(border, 0.0f);
			}
		} else {
			writer.moveTo(0.0f, (wBottomLeft == 0.0f) ? border / 2.0f : wBottomLeft / 2.0f);
			writer.lineTo(border / 2.0f, 0.0f);
			writer.lineTo(border, (wBottomRight == 0.0f) ? border / 2.0f : wBottomRight / 2.0f);
		}
	} else {
		if (wBottomLeft > 0.0f) {
			writer.moveTo(0.0f, wBottomLeft / 2.0f);
		} else {
			writer.moveTo(0.0f, 0.0f);
		}
		writer.lineTo(border / 2.0f, 0.0f);
		if (wBottomRight > 0.0f) {
			writer.lineTo(border, wBottomRight / 2.0f);
		} else {
			writer.lineTo(border, 0.0f);
		}
	}

	if (wTop == 0.0f) {
		if ((wTopLeft == 0.0f && wTopRight != 0.0f) || (wTopLeft != 0.0f && wTopRight == 0.0f)) {
			if (wTopRight > 0.0f) {
				writer.lineTo(border, bbox.size.height - wTopRight);
				writer.lineTo(0.0f, bbox.size.height);
			} else {
				writer.lineTo(border, bbox.size.height);
				writer.lineTo(0.0f, bbox.size.height - wTopLeft);
			}
		} else {
			writer.lineTo(border,
					bbox.size.height - ((wTopRight == 0.0f) ? border : wTopRight) / 2.0f);
			writer.lineTo(border / 2.0f, bbox.size.height);
			writer.lineTo(0.0f, bbox.size.height - ((wTopLeft == 0.0f) ? border : wTopLeft) / 2.0f);
		}
	} else {
		if (wTopRight > 0.0f) {
			writer.lineTo(border, bbox.size.height - wTopRight / 2.0f);
		} else {
			writer.lineTo(border, bbox.size.height);
		}
		writer.lineTo(border / 2.0f, bbox.size.height);
		if (wTopLeft > 0.0f) {
			writer.lineTo(0.0f, bbox.size.height - wTopLeft / 2.0f);
		} else {
			writer.lineTo(0.0f, bbox.size.height);
		}
	}
	writer.closePath();
}

void PathObject::drawHorizontalLineSegment(const Vec2 &origin, float width, const Color4B &color,
		float border, BorderStyle style, float wLeftBottom, float wLeft, float wLeftTop,
		float wRightTop, float wRight, float wRightBottom) {
	path.params.style = vg::DrawFlags::Fill;
	path.params.fillColor = color;

	const float extraLeftSize = (wLeft == 0.0f
										&& ((wLeftBottom == 0.0f && wLeftTop != 0.0f)
												|| (wLeftBottom != 0.0f && wLeftTop == 0.0f)))
			? max(wLeftTop, wLeftBottom) / 2.0f
			: 0.0f;
	const float extraRightSize = (wRight == 0.0f
										 && ((wRightBottom == 0.0f && wRightTop != 0.0f)
												 || (wRightBottom != 0.0f && wRightTop == 0.0f)))
			? max(wRightTop, wRightBottom) / 2.0f
			: 0.0f;

	const float originX = origin.x - extraLeftSize;
	const float originY = origin.y - border / 2.0f;

	bbox = Rect(originX, originY, width + extraLeftSize + extraRightSize, border);

	auto writer = path.getWriter();

	if (wLeft == 0.0f) {
		if ((wLeftBottom == 0.0f && wLeftTop != 0.0f)
				|| (wLeftBottom != 0.0f && wLeftTop == 0.0f)) {
			if (wLeftTop > 0.0f) {
				writer.moveTo(0.0f, 0.0f);
				writer.lineTo(wLeftTop, border);
			} else {
				writer.moveTo(wLeftBottom, 0.0f);
				writer.lineTo(0.0f, border);
			}
		} else {
			writer.moveTo((wLeftBottom == 0.0f) ? border / 2.0f : wLeftBottom / 2.0f, 0.0f);
			writer.lineTo(0.0f, border / 2.0f);
			writer.lineTo((wLeftTop == 0.0f) ? border / 2.0f : wLeftTop / 2.0f, border);
		}
	} else {
		if (wLeftBottom > 0.0f) {
			writer.moveTo(wLeftBottom / 2.0f, 0.0f);
		} else {
			writer.moveTo(0.0f, 0.0f);
		}
		writer.lineTo(0.0f, border / 2.0f);
		if (wLeftTop > 0.0f) {
			writer.lineTo(wLeftTop / 2.0f, border);
		} else {
			writer.lineTo(0.0f, border);
		}
	}

	if (wRight == 0.0f) {
		if ((wRightBottom == 0.0f && wRightTop != 0.0f)
				|| (wRightBottom != 0.0f && wRightTop == 0.0f)) {
			if (wRightTop > 0.0f) {
				writer.lineTo(bbox.size.width - wRightTop, border);
				writer.lineTo(bbox.size.width, 0.0f);
			} else {
				writer.lineTo(bbox.size.width, border);
				writer.lineTo(bbox.size.width - wRightBottom, 0.0f);
			}
		} else {
			writer.lineTo(bbox.size.width - ((wRightTop == 0.0f) ? border : wRightTop) / 2.0f,
					border);
			writer.lineTo(bbox.size.width, border / 2.0f);
			writer.lineTo(bbox.size.width - ((wRightBottom == 0.0f) ? border : wRightBottom) / 2.0f,
					0.0f);
		}
	} else {
		if (wRightTop > 0.0f) {
			writer.lineTo(bbox.size.width - wRightTop / 2.0f, border);
		} else {
			writer.lineTo(bbox.size.width, border);
		}
		writer.lineTo(bbox.size.width, border / 2.0f);
		if (wRightBottom > 0.0f) {
			writer.lineTo(bbox.size.width - wRightBottom / 2.0f, 0.0f);
		} else {
			writer.lineTo(bbox.size.width, 0.0f);
		}
	}
	writer.closePath();
}

} // namespace stappler::document
