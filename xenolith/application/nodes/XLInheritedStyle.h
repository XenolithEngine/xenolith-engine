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

Each component stores only the values actually defined by some styling source, plus a
bitmask of which fields are defined. Consumers (Label first of all) read the component
from their own node first, then walk the parent chain (Node::findParentWithComponent),
taking the nearest defined value per field, until the mask is complete — see
accumulateInheritedStyle(). A defined inherited value takes priority over the consumer's
own explicitly-set value; when the component is removed, the consumer falls back to its
explicit values.

NOTE: there is NO built-in reactivity for these components. ui::StyleResolver keeps them
up to date through its own styling protocol (stylesheet version bump -> re-resolve ->
component rewrite on the node itself -> the node's own components-dirty pass). Any OTHER
producer that writes these components must itself trigger re-evaluation of the consumers:
a Label only reacts to changes of the components on its OWN node (handleComponentsDirty);
a change on an ancestor is picked up only when the label is re-laid-out for some other
reason. */

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

	/* CSS `direction` and `unicode-bidi`. `direction` rides in this component and not in a
	   component of its own because it IS an inherited text property, so the walk above already
	   carries it - and because `direction` is inherited, the resolver stamps this component on
	   every node under a root that declares one, which lets a layout container read its own
	   direction with a single getComponent and no parent walk in the layout path.

	   `unicode-bidi` is NOT inherited and is only ever written on the node that declared it. */
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

/* THE INLINE DIRECTION IN FORCE AT A NODE, for the code that asks OUTSIDE a style pass.

The layout backends, the scroll indicator and the dock tree all need the direction and none of them
runs inside StyleResolver. The node's own component answers immediately in the common case, because
`direction` is inherited and the resolver therefore stamps InheritedTextStyle on every node under a
root that declares one; the parent walk is the fallback for a subtree with no stylesheet at all.

`LeftToRight` when nothing says otherwise, which keeps every application that never heard of
`direction` laid out exactly as before. */
SP_PUBLIC font::TextDirection getInlineDirection(const Node *);

inline bool isInlineRtl(const Node *node) {
	return getInlineDirection(node) == font::TextDirection::RightToLeft;
}

/* PLACE A CHILD BY ITS INLINE EDGE, for a widget that lays its own row out in code.

A widget like `ui::Select` walks a cursor in from the start of its box (icon, then label) and
another in from the end (the arrow), then gives the label whatever is between them. Both cursors
are DISTANCES, and only the last step turns a distance into an x - so these two functions are the
whole of what such a widget needs to work in both directions: keep the arithmetic, swap the edge.

`inset` is measured from the named edge. The anchor is set to match, so the child grows into the
box rather than out of it. */
inline void placeInlineStart(Node *child, float inset, float y, float boxWidth, bool rtl) {
	child->setAnchorPoint(Vec2(rtl ? 1.0f : 0.0f, 0.5f));
	child->setPosition(Vec2(rtl ? boxWidth - inset : inset, y));
}

inline void placeInlineEnd(Node *child, float inset, float y, float boxWidth, bool rtl) {
	child->setAnchorPoint(Vec2(rtl ? 0.0f : 1.0f, 0.5f));
	child->setPosition(Vec2(rtl ? inset : boxWidth - inset, y));
}

/* Which way a widget's own caption hugs its box.

NOT `TextAlign::Start`, which resolves against the TEXT's base direction: a Latin caption inside a
right-to-left control would then hug the left of its box, and the box's left is the far side. What
a caption follows is the CONTROL's direction, which is this. */
inline font::TextAlign inlineStartAlign(bool rtl) {
	return rtl ? font::TextAlign::Right : font::TextAlign::Left;
}

} // namespace stappler::xenolith

#endif // XENOLITH_APPLICATION_NODES_XLINHERITEDSTYLE_H_
