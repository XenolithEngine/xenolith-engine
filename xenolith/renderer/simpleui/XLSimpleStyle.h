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

#ifndef XENOLITH_RENDERER_SIMPLEUI_XLSIMPLESTYLE_H_
#define XENOLITH_RENDERER_SIMPLEUI_XLSIMPLESTYLE_H_

#include "XLSimpleStyleSheet.h"
#include "XLNode.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::simpleui {

/* Marker component on a stylesheet owner node.

Serves two purposes: descendants discover stylesheet scopes with
Node::findParentWithComponent<StyleSheetState>, and bumping the version via
setOrUpdateComponent triggers the ComponentsDirty cascade that re-resolves
styles in the owner's subtree. Managed by StyleSheetSystem. */
struct SP_PUBLIC StyleSheetState {
	static ComponentId Id;

	uint32_t version = 0;
};

// Marker recording that THIS StyleApplier created the node's LayoutSystem. The
// applier only removes layouts it added, so pug `flex` tags and programmatic
// LayoutSystems (which carry no marker) are left untouched.
struct StyleManagedLayout {
	static ComponentId Id;
};

/* Attaches a StyleSheet to a node: the sheet applies to the owner and its subtree.

Multiple systems may be present on the ancestor chain - outer sheets apply
first, inner (nearer) sheets override. Media parameters default from the
Director's frame constraints on enter (surface size in points, density for
fonts) and may be set explicitly. */
class SP_PUBLIC StyleSheetSystem : public System {
public:
	virtual ~StyleSheetSystem() = default;

	virtual bool init() override;
	virtual bool init(Rc<StyleSheet> &&);
	virtual bool init(StringView css);
	virtual bool init(const FileInfo &);

	bool addStyle(StringView css);
	bool addStyle(const FileInfo &);

	void setStyleSheet(Rc<StyleSheet> &&);
	StyleSheet *getStyleSheet() const { return _sheet; }

	void setMediaParameters(const document::MediaParameters &);
	const document::MediaParameters &getMediaParameters() const { return _media; }

	// lazily re-evaluated when the sheet version changes
	SpanView<bool> getMediaResolved();

	// force re-resolution in the owner's subtree (bumps StyleSheetState)
	void invalidateStyles();

	virtual void handleAdded(Node *) override;
	virtual void handleRemoved() override;
	virtual void handleEnter(Scene *) override;

protected:
	void updateMedia();

	Rc<StyleSheet> _sheet;
	document::MediaParameters _media;
	Vector<bool> _mediaResolved;
	uint32_t _resolvedForVersion = maxOf<uint32_t>();
	bool _mediaExplicit = false;
};

/* Style resolved for a single node: compiled document parameters in std memory,
safe to keep after resolution (nothing points into stylesheet pools; the string
values live in the `fontFamily`/`backgroundImage` members, the corresponding
StringViews inside `font`/`background` are cleared).

`has()` reports whether the stylesheet actually defined a given parameter -
appliers must not touch widget properties whose parameters are absent. */
struct SP_PUBLIC ResolvedStyle {
	document::FontStyleParameters font;
	String fontFamily;
	document::TextLayoutParameters text;
	document::ParagraphLayoutParameters paragraph;
	document::BlockModelParameters block;
	document::BackgroundParameters background;
	String backgroundImage;

	// positioning (simpleui-only CSS: position/top/right/bottom/left, -xl-anchor-point)
	document::Position position = document::Position::Static;
	document::Metric top;
	document::Metric right;
	document::Metric bottom;
	document::Metric left;
	Vec2 anchorPoint;
	document::Metric xlPositionX;
	document::Metric xlPositionY;

	// flexbox / grid CSS (consumed by the LayoutSystem-management step; the raw
	// document enums/metrics are translated into the simpleui layout components)
	document::FlexDirection flexDirection = document::FlexDirection::Row;
	document::FlexWrap flexWrap = document::FlexWrap::NoWrap;
	document::GridAutoFlow gridAutoFlow = document::GridAutoFlow::Row;
	document::Align justifyContent = document::Align::Auto;
	document::Align alignContent = document::Align::Auto;
	document::Align justifyItems = document::Align::Auto;
	document::Align alignItems = document::Align::Auto;
	document::Align justifySelf = document::Align::Auto;
	document::Align alignSelf = document::Align::Auto;
	float flexGrow = 0.0f;
	float flexShrink = 1.0f;
	document::Metric flexBasis;
	int32_t order = 0;
	document::Metric rowGap;
	document::Metric columnGap;
	// grid track/line strings, resolved to std memory (raw CSS text)
	String gridTemplateColumns;
	String gridTemplateRows;
	String gridAutoColumns;
	String gridAutoRows;
	String gridColumnStart;
	String gridColumnEnd;
	String gridRowStart;
	String gridRowEnd;

	// media environment of the nearest stylesheet scope (for Metric computation)
	document::MediaParameters media;

	Vector<uint16_t> present; // toInt(ParameterName) actually defined by the sheets
	bool valid = false;

	bool has(document::ParameterName name) const;
};

/* Resolve the style for a node against all stylesheet scopes on its ancestor
chain (nearest StyleSheetSystem and above).

Resolution: for every ancestor with a StyleIdentity, inheritable parameters
cascade down (outermost ancestor first); then the node's own matches are
applied (outer sheets first, inner override), then the node's inline style.
Returns `valid == false` when no stylesheet scope is present. */
SP_PUBLIC ResolvedStyle resolveStyleForNode(NotNull<Node>);

/* Auto-applies resolved styles to its owner node.

Add to any styleable node. Initial application happens on scene enter (the
ancestor chain is complete there); re-application rides the ComponentsDirty
cascade (the system opts in via SystemFlags::HandleAncestorComponents).

The optional callback runs before the default property application; returning
true suppresses the defaults (widget-specific extension point).

Default v1 property mapping:
 - opacity -> Node::setOpacity
 - color -> Label color (node color)
 - background-color -> Layer/Button color
 - font-size/-family/-weight/-style/-stretch, text-align, width -> Label
 - width/height (non-Label) -> setContentSize (percent/vw/vh resolved against
   the parent size at apply time; parent resizes are not tracked in v1)
 - margin-* -> FlexItemInfo::margin (when the parent is a flex container)
 - padding-* -> owner's FlexLayoutInfo::padding (when the node is one)
Component writes are equality-guarded to avoid dirty loops. */
class SP_PUBLIC StyleApplier : public System {
public:
	using ApplyCallback = Function<bool(Node *, const ResolvedStyle &)>;

	virtual ~StyleApplier() = default;

	virtual bool init() override;
	virtual bool init(ApplyCallback &&);

	virtual void handleAdded(Node *) override;
	virtual void handleEnter(Scene *) override;
	virtual void handleComponentsDirty(const ComponentMask &mask) override;

	void apply();

protected:
	void applyDefault(Node *, const ResolvedStyle &);

	// add/remove/configure the node's LayoutSystem from its CSS display + flex/grid
	// properties, and map this node's flex/grid item properties onto its parent
	// container's per-item component
	void applyLayout(Node *, const ResolvedStyle &);

	ApplyCallback _callback;
};

} // namespace stappler::xenolith::simpleui

#endif /* XENOLITH_RENDERER_SIMPLEUI_XLSIMPLESTYLE_H_ */
