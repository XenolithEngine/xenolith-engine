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

#ifndef XENOLITH_RENDERER_UI_STYLE_XLUISTYLERESOLVER_H_
#define XENOLITH_RENDERER_UI_STYLE_XLUISTYLERESOLVER_H_

#include "XLUiStyleSheet.h" // IWYU pragma: keep

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

/* Raw resolved style for a node: the merged CSS parameters plus the context needed to
interpret them (media environment + the nearest sheet's string table). Nothing is compiled
up front - every accessor computes only what it is asked for, so a consumer that needs one
property pays only for that property (the default applier, a widget callback, or a test each
read their own slice).

Move-only; owns a private memory pool holding the merged parameter list. The compiled views
and string values reference the originating stylesheet, so a ResolvedStyle must be consumed
while that sheet is alive - it is produced and read within a single apply().

`has()` reports whether the sheets actually defined a parameter (media-filtered); appliers
must not touch widget properties whose parameters are absent. */
class SP_PUBLIC ResolvedStyle {
public:
	ResolvedStyle() = default;
	~ResolvedStyle();

	ResolvedStyle(ResolvedStyle &&) noexcept;
	ResolvedStyle &operator=(ResolvedStyle &&) noexcept;
	ResolvedStyle(const ResolvedStyle &) = delete;
	ResolvedStyle &operator=(const ResolvedStyle &) = delete;

	bool valid() const { return _valid; }
	explicit operator bool() const { return _valid; }

	// media environment, for Metric computation by consumers
	const document::MediaParameters &media() const { return *_media; }

	// the merged parameter list, for consumers that iterate raw parameters directly
	const document::StyleList *parameters() const { return _style; }

	// was a parameter defined for this node (media-filtered)?
	bool has(document::ParameterName) const;
	// last matching raw value for a parameter (media-filtered); false if absent
	bool getValue(document::ParameterName, document::StyleValue &out) const;
	// resolve a string-valued parameter into std memory ("" if absent)
	String getString(document::ParameterName) const;

	// compiled views - each compiled on demand from the raw parameters
	document::FontStyleParameters font() const;
	document::TextLayoutParameters text() const;
	document::ParagraphLayoutParameters paragraph() const;
	document::BlockModelParameters block() const;
	document::BackgroundParameters background() const;

	// positioning (position/top/right/bottom/left, -xl-anchor-point, -xl-position)
	document::Position position() const;
	document::Metric top() const;
	document::Metric right() const;
	document::Metric bottom() const;
	document::Metric left() const;
	Vec2 anchorPoint() const;
	document::Metric xlPositionX() const;
	document::Metric xlPositionY() const;

	// flexbox / grid (the raw document enums/metrics; the applier maps them to layout)
	document::FlexDirection flexDirection() const;
	document::FlexWrap flexWrap() const;
	document::GridAutoFlow gridAutoFlow() const;
	document::Align justifyContent() const;
	document::Align alignContent() const;
	document::Align justifyItems() const;
	document::Align alignItems() const;
	document::Align justifySelf() const;
	document::Align alignSelf() const;
	float flexGrow() const;
	float flexShrink() const;
	document::Metric flexBasis() const;
	int32_t order() const;
	document::Metric rowGap() const;
	document::Metric columnGap() const;
	String gridTemplateColumns() const;
	String gridTemplateRows() const;
	String gridAutoColumns() const;
	String gridAutoRows() const;
	String gridColumnStart() const;
	String gridColumnEnd() const;
	String gridRowStart() const;
	String gridRowEnd() const;

private:
	friend class StyleResolver;

	bool _valid = false;
	memory::pool_t *_pool = nullptr;
	document::StyleList *_style = nullptr; // merged parameters, allocated in _pool (AllocPool)
	// non-owning views into the nearest sheet's media bits + string table; a plain value
	// (NOT pool-allocated: SimpleStyleInterface is not an AllocPool)
	document::SimpleStyleInterface _iface;
	const document::MediaParameters *_media;
};

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
class SP_PUBLIC StyleResolver : public System {
public:
	using ApplyCallback = Function<bool(Node *, const ResolvedStyle &)>;

	/* Resolve the style for a node against all stylesheet scopes on its ancestor
	chain (nearest StyleSheetSystem and above).

	Resolution: for every ancestor with a StyleIdentity, inheritable parameters
	cascade down (outermost ancestor first); then the node's own matches are
	applied (outer sheets first, inner override), then the node's inline style.
	Returns `valid == false` when no stylesheet scope is present. */
	static ResolvedStyle resolveStyleForNode(NotNull<Node>);

	virtual ~StyleResolver() = default;

	virtual bool init(bool recursive = false);
	virtual bool init(ApplyCallback &&, bool recursive = false);

	virtual void handleAdded(Node *) override;
	virtual void handleEnter(Scene *) override;
	virtual void handleComponentsDirty() override;
	virtual void handleReorderChildDirty() override;

	void apply();

protected:
	void resolveForNode(Node *);

	void applyDefault(Node *, const ResolvedStyle &);

	// add/remove/configure the node's LayoutSystem from its CSS display + flex/grid
	// properties, and map this node's flex/grid item properties onto its parent
	// container's per-item component
	void applyLayout(Node *, const ResolvedStyle &);

	bool _recursive = false;
	ApplyCallback _callback;

	// local copy of the owner's InteractiveComponent state bits at the last resolve, so
	// handleInteractiveState() can skip rebuilds when the mask is unchanged
	uint32_t _interactiveMask = 0;
	uint32_t _sourceSystemVersion = 0;
	uint64_t _sourceSystemId = 0;

	// For which nodes we already resolve most recent styles
	HashSet<Node *> _nodesUpdated;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_STYLE_XLUISTYLERESOLVER_H_
