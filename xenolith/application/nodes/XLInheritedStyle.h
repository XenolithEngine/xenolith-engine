/**
 Copyright (c) 2026 Stappler Team <admin@stappler.org>

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

#ifndef XENOLITH_APPLICATION_NODES_XLINHERITEDSTYLE_H_
#define XENOLITH_APPLICATION_NODES_XLINHERITEDSTYLE_H_

#include "XLNode.h"
#include "XLFontConfig.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith {

/** Inherited-style data components ("values + defined-mask").

Each component stores only the values defined by some styling source, plus a mask of defined
fields. Consumers (e.g. Label) take the nearest defined value per field from their node and its
parents - see accumulateInheritedStyle(). A defined inherited value overrides the consumer's own
explicit value; without the component the consumer uses its explicit values.

There is no built-in reactivity: ui::StyleResolver rewrites the components on the node itself.
Any other producer must trigger re-evaluation: a Label reacts only to components on its own node
(handleComponentsDirty), not to changes on an ancestor. */

struct SP_PUBLIC InheritedColorStyle {
	static ComponentId Id;

	enum Defined : uint32_t {
		DefinedColor = 1 << 0,
		DefinedOpacity = 1 << 1,
	};

	static constexpr uint32_t DefinedAll = DefinedColor | DefinedOpacity;

	Color3B color = Color3B::BLACK;
	uint8_t opacity = 255;
	uint32_t defined = 0;

	bool complete() const { return (defined & DefinedAll) == DefinedAll; }

	// copy parent's defined fields that are not defined here (nearest value wins)
	void merge(const InheritedColorStyle &);

	bool operator==(const InheritedColorStyle &) const = default;
};

struct SP_PUBLIC InheritedFontStyle {
	static ComponentId Id;

	enum Defined : uint32_t {
		DefinedFontSize = 1 << 0,
		DefinedFontStyle = 1 << 1,
		DefinedFontWeight = 1 << 2,
		DefinedFontStretch = 1 << 3,
		DefinedFontGrade = 1 << 4,
		DefinedFontVariant = 1 << 5,
		DefinedFontFamily = 1 << 6,
	};

	static constexpr uint32_t DefinedAll = DefinedFontSize | DefinedFontStyle | DefinedFontWeight
			| DefinedFontStretch | DefinedFontGrade | DefinedFontVariant | DefinedFontFamily;

	font::FontSize fontSize = font::FontSize(14);
	font::FontStyle fontStyle = font::FontStyle::Normal;
	font::FontWeight fontWeight = font::FontWeight::Normal;
	font::FontStretch fontStretch = font::FontStretch::Normal;
	font::FontGrade fontGrade = font::FontGrade::Normal;
	font::FontVariant fontVariant = font::FontVariant::Normal;
	String fontFamily; // owning copy (font::FontParameters::fontFamily is a non-owning view)
	uint32_t defined = 0;

	bool complete() const { return (defined & DefinedAll) == DefinedAll; }

	// copy parent's defined fields that are not defined here (nearest value wins)
	void merge(const InheritedFontStyle &);

	bool operator==(const InheritedFontStyle &) const = default;
};

struct SP_PUBLIC InheritedTextStyle {
	static ComponentId Id;

	enum Defined : uint32_t {
		DefinedTextTransform = 1 << 0,
		DefinedTextDecoration = 1 << 1,
		DefinedWhiteSpace = 1 << 2,
		DefinedHyphens = 1 << 3,
		DefinedVerticalAlign = 1 << 4,
		DefinedTextAlign = 1 << 5,
		DefinedLineHeight = 1 << 6,
		DefinedDirection = 1 << 7,
		DefinedBidi = 1 << 8,
	};

	static constexpr uint32_t DefinedAll = DefinedTextTransform | DefinedTextDecoration
			| DefinedWhiteSpace | DefinedHyphens | DefinedVerticalAlign | DefinedTextAlign
			| DefinedLineHeight | DefinedDirection | DefinedBidi;

	font::TextTransform textTransform = font::TextTransform::None;
	font::TextDecoration textDecoration = font::TextDecoration::None;
	font::WhiteSpace whiteSpace = font::WhiteSpace::Normal;
	font::Hyphens hyphens = font::Hyphens::Manual;
	font::VerticalAlign verticalAlign = font::VerticalAlign::Baseline;
	font::TextAlign textAlign = font::TextAlign::Left;

	/* CSS `direction` and `unicode-bidi`. `direction` is inherited, so the resolver stamps this
	   component on every node under a root that declares one, and a layout container reads it
	   with a single getComponent. `unicode-bidi` is not inherited: only on the declaring node. */
	font::TextDirection direction = font::TextDirection::LeftToRight;
	font::BidiMode bidi = font::BidiMode::Normal;

	float lineHeight = 0.0f; // px when `lineHeightAbsolute`, factor of font size otherwise
	bool lineHeightAbsolute = false;
	uint32_t defined = 0;

	bool complete() const { return (defined & DefinedAll) == DefinedAll; }

	// copy parent's defined fields that are not defined here (nearest value wins)
	void merge(const InheritedTextStyle &);

	bool operator==(const InheritedTextStyle &) const = default;
};

// Accumulate an inherited-style component of type T for `node`: the node's own component
// first, then the parent chain (nearest defined field wins); stops walking as soon as
// every field is defined.
template <typename T>
inline T accumulateInheritedStyle(NotNull<const Node> node) {
	T ret;
	if (auto own = node->getComponent<T>()) {
		ret = *own;
	}
	if (!ret.complete()) {
		node->findParentWithComponent<T>([&](NotNull<Node>, NotNull<const T> c, uint32_t) {
			ret.merge(*c);
			return !ret.complete(); // false stops the walk
		});
	}
	return ret;
}

/* The inline direction in force at a node, for code outside a style pass (layout backends, scroll
indicator, dock tree). Reads the node's own component, then walks parents; `LeftToRight` if nothing
declares a direction. */
SP_PUBLIC font::TextDirection getInlineDirection(const Node *);

inline bool isInlineRtl(const Node *node) {
	return getInlineDirection(node) == font::TextDirection::RightToLeft;
}

/* Place a child by its inline edge, for a widget that lays its own row out in code (e.g.
`ui::Select`). `inset` is a distance from the named edge; the anchor is set to match, so the child
grows into the box. */
inline void placeInlineStart(Node *child, float inset, float y, float boxWidth, bool rtl) {
	child->setAnchorPoint(Vec2(rtl ? 1.0f : 0.0f, 0.5f));
	child->setPosition(Vec2(rtl ? boxWidth - inset : inset, y));
}

inline void placeInlineEnd(Node *child, float inset, float y, float boxWidth, bool rtl) {
	child->setAnchorPoint(Vec2(rtl ? 0.0f : 1.0f, 0.5f));
	child->setPosition(Vec2(rtl ? inset : boxWidth - inset, y));
}

/* Which way a widget's own caption hugs its box: follows the control's direction, not
`TextAlign::Start`, which resolves against the text's base direction. */
inline font::TextAlign inlineStartAlign(bool rtl) {
	return rtl ? font::TextAlign::Right : font::TextAlign::Left;
}

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_NODES_XLINHERITEDSTYLE_H_
