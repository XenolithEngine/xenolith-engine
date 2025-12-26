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

#ifndef EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTOBJECT_H_
#define EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTOBJECT_H_

#include "SPDocLayout.h" // IWYU pragma: keep
#include "SPDocStyle.h"
#include "SPFontTextLayout.h"
#include "SPVectorPath.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::document {

struct Link;
struct Label;
struct Background;
struct PathObject;
struct LayoutBlock;

class LayoutResult;

using ZOrder = ValueWrapper<uint32_t, class LayoutZOrderFlag>;

static constexpr ZOrder ZOrderBackground = ZOrder(0);
static constexpr ZOrder ZOrderLabel = ZOrder(64);
static constexpr ZOrder ZOrderBorder = ZOrder(128);
static constexpr ZOrder ZOrderOutline = ZOrder(192);
static constexpr ZOrder ZOrderForeground = ZOrder(256);

struct SP_PUBLIC Object : memory::AllocPool {
	enum class Type : uint8_t {
		Empty,
		Background,
		Path,
		Label,
		Ref,

		Link = Ref,
	};

	enum class Context : uint8_t {
		None,
		Normal,
	};

	Rect bbox;
	Type type = Type::Empty;
	Context context = Context::Normal;
	uint16_t depth = 0;
	uint32_t zIndex = 0;
	size_t index = 0;

	bool isLink() const;
	bool isPath() const;
	bool isLabel() const;
	bool isBackground() const;

	const Link *asLink() const;
	const PathObject *asPath() const;
	const Label *asLabel() const;
	const Background *asBackground() const;
};

struct SP_PUBLIC Link : Object {
	StringView target;
	StringView mode;
	WideStringView text;
	Background *source = nullptr;
};

struct SP_PUBLIC BorderParams {
	BorderStyle style = BorderStyle::None;
	float width = 0.0f;
	Color4B color;

	bool merge(const BorderParams &p);
	bool compare(const BorderParams &p) const;
	bool isVisible() const;
};

struct SP_PUBLIC PathObject : Object {
	vg::PathData<memory::PoolInterface> path;

	static void makeBorder(LayoutResult *, LayoutBlock &, const Rect &, const OutlineParameters &,
			float w, ZOrder zOrder, const MediaParameters &);

	void drawOutline(const Rect &, const Color4B &, float = 0.0f, BorderStyle = BorderStyle::None);
	void drawRect(const Rect &, const Color4B &);

	void drawVerticalLineSegment(const Vec2 &origin, float height, const Color4B &, float border,
			BorderStyle, float wTopLeft, float wTop, float wTopRight, float wBottomRight,
			float wBottom, float wBottomLeft);
	void drawHorizontalLineSegment(const Vec2 &origin, float width, const Color4B &, float border,
			BorderStyle, float wLeftBottom, float wLeft, float wLeftTop, float wRightTop,
			float wRight, float wRightBottom);
};

struct SP_PUBLIC Label : Object {
	font::TextLayoutData<memory::PoolInterface> layout;
	float height = 0.0f;
	bool preview = false;
	StringView hash;
	size_t sourceIndex = 0;
};

struct SP_PUBLIC Background : Object {
	BackgroundParameters background;
	Link *link = nullptr;
};

} // namespace stappler::document

#endif /* EXTRA_DOCUMENT_LAYOUT_SPDOCLAYOUTOBJECT_H_ */
