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

/* Raw resolved style for a node: the merged CSS parameters plus the media environment and the
nearest sheet's string table. Accessors compute lazily, only what is asked for.

Move-only; owns a pool with the merged parameter list. Views reference the originating sheet,
so consume it while that sheet is alive (within a single apply()). Appliers must not touch
widget properties whose parameters are absent (`has()`, media-filtered). */
class SP_PUBLIC ResolvedStyle {
public:
	using ParameterName = document::ParameterName;
	using StyleValue = document::StyleValue;

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

	// did any sheet in scope use a structural pseudo-class (`:nth-child` and friends)? Then the
	// sibling set must be invalidated when the parent's child list changes.
	bool hasStructuralSelectors() const { return _structural; }

	// raw text of the custom property `--name` in effect for this node (own and inherited),
	// empty when not declared. Parameters already have `var()` substituted.
	StringView getCustomProperty(StringView name) const;

	// every custom property in effect, in no particular order
	void foreachCustomProperty(const Callback<void(StringView, StringView)> &) const;

	// digest of every custom property in effect; 0 when there are none. Custom properties are
	// inherited, so a node whose digest changed has invalidated its whole subtree.
	uint64_t getCustomPropertiesHash() const;

	// was a parameter defined for this node (media-filtered)?
	bool has(ParameterName) const;
	// last matching raw value for a parameter (media-filtered); false if absent
	bool getValue(ParameterName, document::StyleValue &out) const;
	// resolve a string-valued parameter into std memory ("" if absent)
	String getString(ParameterName) const;

	void foreach (const Callback<void(ParameterName, const StyleValue &)> &) const;

	// compiled views: each builds a whole parameter block on demand. For a single property,
	// prefer the individual accessors below.
	document::FontStyleParameters font() const;
	document::TextLayoutParameters text() const;
	document::ParagraphLayoutParameters paragraph() const;
	document::BlockModelParameters block() const;
	document::BackgroundParameters background() const;
	document::OutlineParameters outline() const;

	// individual property accessors: resolve one parameter (mirroring the compiled block field);
	// each returns the CSS default when the parameter is absent.
	document::FontSize fontSize() const; // font-size (+ font-size-increment, like compileFontStyle)
	document::FontStyle fontStyle() const;
	document::FontWeight fontWeight() const;
	document::FontStretch fontStretch() const;
	String fontFamily() const; // font-family ("default" when absent/empty)
	Color3B color() const; // color
	uint8_t opacity() const; // opacity (0-255)
	document::TextAlign textAlign() const; // text-align
	document::TextDirection direction() const; // direction (inherited)
	document::BidiMode unicodeBidi() const; // unicode-bidi
	document::TextTransform textTransform() const; // text-transform
	document::TextDecoration textDecoration() const; // text-decoration
	document::WhiteSpace whiteSpace() const; // white-space
	document::Hyphens hyphens() const; // hyphens
	document::VerticalAlign verticalAlign() const; // vertical-align
	document::FontVariant fontVariant() const; // font-variant
	// line-height as the raw metric; a unitless number ("line-height: 1.5") is
	// stored as Units::Auto with the factor in `value`
	document::Metric lineHeight() const;
	document::Display display() const; // display
	document::Visibility visibility() const; // visibility
	document::Overflow overflowX() const;
	document::Overflow overflowY() const;
	document::Metric width() const;
	document::Metric height() const;
	// min-/max-width/height. Only the flex main axis is enforced (FlexItemInfo::minMain/maxMain);
	// on the cross axis they are read but nothing applies them yet.
	document::Metric minWidth() const;
	document::Metric minHeight() const;
	document::Metric maxWidth() const;
	document::Metric maxHeight() const;
	document::Metric marginTop() const;
	document::Metric marginRight() const;
	document::Metric marginBottom() const;
	document::Metric marginLeft() const;
	document::Metric paddingTop() const;
	document::Metric paddingRight() const;
	document::Metric paddingBottom() const;
	document::Metric paddingLeft() const;

	// inline-axis box properties; applyLayout maps each to a physical side by the node's
	// computed `direction`
	document::Metric paddingInlineStart() const;
	document::Metric paddingInlineEnd() const;
	document::Metric marginInlineStart() const;
	document::Metric marginInlineEnd() const;
	document::Metric insetInlineStart() const;
	document::Metric insetInlineEnd() const;

	// positioning (position/top/right/bottom/left, -xl-anchor-point, -xl-position)
	document::Position position() const;
	document::Metric top() const;
	document::Metric right() const;
	document::Metric bottom() const;
	document::Metric left() const;
	Vec2 anchorPoint() const;
	document::Metric xlPositionX() const;
	document::Metric xlPositionY() const;
	int32_t xlZOrder() const; // -xl-z-order (Node ZOrder)

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

	// table. Columns come from `grid-template-columns`; per-side borders for border collapsing
	// come from outline().
	document::TableLayout tableLayout() const;
	document::BorderCollapse borderCollapse() const;
	document::Metric borderSpacingHorizontal() const;
	document::Metric borderSpacingVertical() const;
	uint32_t columnSpan() const; // -xl-column-span
	uint32_t rowSpan() const; // -xl-row-span

private:
	friend class StyleResolver;

	// custom properties resolved for the node, plus strings interned by `var()` substitution;
	// pool-allocated with the parameter list
	struct VariableTable;

	// create the variable table, seeding its string overlay from the nearest sheet's table so
	// ids that already exist keep their meaning. Must run inside this style's pool.
	void initVariables(SpanView<StringView> sheetStrings);

	// intern a string produced by var() substitution; the id is valid against this style only
	document::StringId internSubstitutedString(StringView);

	// expand and parse one matched rule's deferred var() declarations into `dst`, at the point
	// in the cascade where that rule is being merged
	void expandPendingRule(document::StyleList &dst, const document::StyleContainer::MatchedRule &);

	bool _valid = false;
	bool _structural = false; // see hasStructuralSelectors()
	memory::pool_t *_pool = nullptr;
	document::StyleList *_style = nullptr; // merged parameters, allocated in _pool (AllocPool)
	VariableTable *_variables = nullptr; // null when no sheet in scope declares one
	// non-owning views into the nearest sheet's media bits + string table; a plain value
	// (not pool-allocated: SimpleStyleInterface is not an AllocPool)
	document::SimpleStyleInterface _iface;
	const document::MediaParameters *_media;
};

/* Auto-applies resolved styles to its owner node.

Initial application happens on scene enter; re-application rides the ComponentsDirty cascade
(SystemFlags::HandleAncestorComponents). The optional callback runs before the defaults;
returning true suppresses them.

registerTypeApplier(type, attr, applier) registers per-attribute appliers for a node type;
unregistered attributes fall through to the default mapping below.

An applier whose mask lists `document::ParameterName::CmdReset` receives it first on every
pass: drop what the previous pass applied, since a rule that stopped matching is not in the
new pass. The reset undoes only styling, not what the widget set on itself from code; keep the
two layers apart (see `ui::Panel` / `PanelStyleComponent`, pinned by `XL_PANEL_TEST`).

A recursive resolver (init(true)) styles its whole subtree: it publishes on the frame stack and
resolves each descendant as its content-size / layout-children event arrives, once per version.

Default property mapping:
 - opacity -> Node::setOpacity
 - display: none, visibility: hidden -> VisibilityComponent (wrapVisit skips the subtree
   like setVisible(false); layout engines collapse display:none, visibility:hidden keeps
   its box; the node's explicit setVisible state is never touched)
 - background-color -> Layer/Button color
 - color, font-size/-family/-weight/-style/-stretch/-variant, text-align/-transform/
   -decoration, white-space, hyphens, vertical-align, line-height -> Inherited*Style
   components on the node (see XLInheritedStyle.h; Label accumulates them over the
   parent chain, a defined value overrides the label's explicit one)
 - width -> Label (non-inheritable, still pushed directly)
 - width/height (non-Label) -> setContentSize (percent resolved against the parent size;
   percent-styled nodes re-resolve on parent resize, see _nodesUpdated and
   NodeEventFlags::HandleParentContentSize)
 - margin-* -> FlexItemInfo::margin (when the parent is a flex container)
 - padding-* -> owner's FlexLayoutInfo::padding (when the node is one)
Component writes are equality-guarded to avoid dirty loops. */
class SP_PUBLIC StyleResolver : public System {
public:
	using ApplyCallback = Function<bool(Node *, const ResolvedStyle &)>;

	// Applies a single CSS attribute to a node (the handler reads its own ParameterName value
	// from the ResolvedStyle; the StyleResolver is passed so it can reuse media()/helpers)
	using AttrApplier = Function<bool(StyleResolver &, Node *, const ResolvedStyle &,
			document::ParameterName, const document::StyleValue &)>;

	using ParameterMask = sprt::bitset<toInt(document::ParameterName::Max)>;

	// Frame-stack tag: a recursive resolver publishes itself here so descendants deliver their
	// content-size / layout-children events back to it (recursive styling, see init(recursive))
	static uint64_t SystemFrameTag;

	static void registerTypeApplier(StringView type, AttrApplier &&, ParameterMask &&);

	static ParameterMask makeParameterMask(sprt::initializer_list<document::ParameterName> &&);

	/* Resolve the style for a node against all stylesheet scopes on its ancestor
	chain (nearest StyleSystem and above).

	Resolution: for every ancestor with a StyleIdentity, inheritable parameters
	cascade down (outermost ancestor first); then the node's own matches are
	applied (outer sheets first, inner override), then the node's inline style.
	Returns `valid == false` when no stylesheet scope is present. */
	static ResolvedStyle resolveStyleForNode(NotNull<Node>);

	/* Drop the per-node match cache of resolveStyleForNode. The cache self-validates against each
	node's match stamp; this exists for tests measuring a cold resolve. */
	static void dropMatchCache();

	virtual ~StyleResolver() = default;

	virtual bool init(bool recursive = false);
	virtual bool init(ApplyCallback &&, bool recursive = false);

	virtual void handleAdded(Node *) override;
	virtual void handleEnter(Scene *) override;

	// a component changed on this node or (via HandleAncestorComponents) on a styled ancestor,
	// e.g. the StyleSystemState version bump on sheet reload; apply() re-resolves when needed
	virtual void handleComponentsDirty(const ComponentMask &) override;

	// owner's own content size changed - re-resolve when stale (own-size-relative
	// paddings/gaps); gated by SystemFlags::HandleNodeEvents
	virtual void handleContentSizeDirty() override;

	// owner's parent content size changed - re-resolve when stale (percent metrics); gated by
	// SystemFlags::HandleNodeEvents. apply() won't do: a resize changes neither version nor mask
	virtual void handleLayoutInParent(Node *) override;

	// recursive styling: a descendant's content-size event arrived via the frame stack - resolve
	// it when stale. Styles descendants initially, after a CSS reload, and on parent resizes
	// (percent-styled nodes carry NodeEventFlags::HandleParentContentSize)
	virtual void handleChildContentSizeDirty(Node *) override;

	// recursive styling: a descendant's own components changed (via the frame stack) - re-resolve
	// it on identity, interactive state or style variable changes
	virtual void handleChildComponentsDirty(Node *, const ComponentMask &) override;

	void apply();

protected:
	// look up and run the type-registered attribute appliers for `node`; every attribute a handler
	// consumed is inserted into `handled` so applyDefault can skip its built-in mapping for it
	void applyTypeAttributes(Node *, const ResolvedStyle &,
			sprt::bitset<toInt(document::ParameterName::Max)> &handled);

	void resolveForNode(Node *);

	// re-resolve the owner when its freshness key (parent/own size) no longer matches
	void resolveOwnerIfStale();

	// true when the node's style is fresh: resolved this version against the same sizes
	bool isNodeFresh(Node *) const;

	void applyDefault(Node *, const ResolvedStyle &);

	// add/remove/configure the node's LayoutSystem from its CSS display + flex/grid
	// properties, and map this node's flex/grid item properties onto its parent
	// container's per-item component
	void applyLayout(Node *, const ResolvedStyle &);

	// Freshness key of an applied style: parent size (percent metrics), own size (paddings/gaps),
	// the parent's child-list version (structural selectors) and the style source version.
	struct StyleFreshness {
		Size2 parentSize;
		Size2 ownSize;
		uint32_t parentChildrenVersion = 0;

		// style source version: a sheet reload or a media flag flip (e.g. `rtl`) must invalidate
		// nodes whose geometry did not change
		uint32_t sourceVersion = 0;

		bool operator==(const StyleFreshness &) const = default;
	};

	StyleFreshness makeStyleFreshness(Node *) const;

	bool _recursive = false;
	ApplyCallback _callback;

	// does any sheet in scope use a structural pseudo-class? Learned from each resolve; puts
	// the parent's child-list version into the freshness key.
	bool _structuralSelectors = false;

	// local copy of the owner's InteractiveComponent state bits at the last resolve, so
	// handleInteractiveState() can skip rebuilds when the mask is unchanged
	uint32_t _interactiveMask = 0;
	uint32_t _sourceSystemVersion = 0;
	uint64_t _sourceSystemId = 0;

	/* Freshness map: a node's style is fresh while the sheet version is unchanged (cleared in
	apply() on change) and its recorded StyleFreshness matches. Pointer keys share alignment bits,
	so a spreading hasher is required to avoid bucket collisions. */
	sprt::__malloc_unordered_map<Node *, StyleFreshness, sprt::hash_spread<>, sprt::equal_to<void>>
			_nodesUpdated;

	// digest of the custom properties each node resolved to (nodes that have any). Properties are
	// inherited, so a changed digest makes every descendant stale without an event of its own.
	sprt::__malloc_unordered_map<Node *, uint64_t, sprt::hash_spread<>, sprt::equal_to<void>>
			_nodeCustomProperties;

	// applyDefault can re-enter this resolver (a nested handleChildComponentsDirty, a scroll row
	// attached mid-visit); nested resolves are queued and drained when the outer apply returns.
	bool _inResolve = false;
	Vector<Node *> _pendingResolve;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_STYLE_XLUISTYLERESOLVER_H_
