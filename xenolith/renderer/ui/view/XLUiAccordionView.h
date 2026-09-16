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

#ifndef XENOLITH_RENDERER_UI_VIEW_XLUIACCORDIONVIEW_H_
#define XENOLITH_RENDERER_UI_VIEW_XLUIACCORDIONVIEW_H_

#include "XLUiPanelHandle.h"
#include "XLUiScrollSystem.h"
#include "XLDropTarget.h"
#include "XL2dLayer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

class AccordionView;
class AccordionSection;

// How many sections may stand open at once.
enum class AccordionExpansion : uint8_t {
	Multi, // any number; the classic accordion
	Single, // exactly one - a vertical set of tabs, which is what a DockFrame is horizontally
};

// What an OPEN section's height is made of. A collapsed one is always just its header.
enum class AccordionSizing : uint8_t {
	// Each open section takes the height its content asks for; the view scrolls on overflow.
	Fit,

	// Open sections share the height left after the collapsed headers. Nothing scrolls.
	Fill,
};

/** The header of one section: the chevron, the panel's icon and title, a grip, and a close button.

A PanelHandle, so the panel drag is the same one a dock tab uses. A tap toggles the section;
only the grip starts a drag (canBeginDragAt), so an imprecise click does not pull the panel out.

CSS type "accordion-header", with the class `expanded` or `collapsed`; the children are
"accordion-chevron", "accordion-grip" and "accordion-close". */
class SP_PUBLIC AccordionHeader : public PanelHandle {
public:
	static constexpr auto IconCollapsed = IconName::Navigation_chevron_right_solid;
	static constexpr auto IconExpanded = IconName::Navigation_expand_more_solid;

	virtual ~AccordionHeader() = default;

	virtual bool init(NotNull<AccordionSection>, NotNull<PanelHost>, StringView panelId);

	virtual void setExpanded(bool);
	bool isExpanded() const { return _expanded; }

	// The panel's icon, placed between the grip and the title. Overridden for that placement, since
	// ui::Button creates the sprite lazily on the first setIcon.
	virtual void setIcon(IconName) override;

	// mirrors DockPanelFlags::Closable; hides the close affordance when off
	virtual void setClosable(bool);

	Node *getGrip() const { return _grip; }

protected:
	using PanelHandle::init;

	// only on the grip - see the class comment
	virtual bool canBeginDragAt(const Vec2 &worldLocation) const override;

	virtual bool handleLeftTap() override;

	virtual void updatePanelDragOffer(DragOffer &, DockPanelPayload &) override;

	AccordionSection *_section = nullptr; // non-owning: it is our parent
	basic2d::IconSprite *_chevron = nullptr;
	basic2d::IconSprite *_grip = nullptr;
	Button *_close = nullptr;
	bool _expanded = false;
};

/** One section: a header, and a body that holds the panel's node while the section is open.

Collapsing detaches the panel from the body without cleanup, so the node keeps its state and a
closed section costs no layout or style passes.

CSS type "accordion-section", class `expanded` or `collapsed`; the body is "accordion-body". */
class SP_PUBLIC AccordionSection : public Panel {
public:
	virtual ~AccordionSection() = default;

	virtual bool init(NotNull<AccordionView>, NotNull<PanelHost>, StringView panelId);

	StringView getPanelId() const { return _panelId; }

	AccordionHeader *getHeader() const { return _header; }

	// where the panel's node is parented while this section is open
	Node *getBody() const { return _body; }

	virtual void setExpanded(bool);
	bool isExpanded() const { return _expanded; }

	// Height of the header alone, as of the last layout: the floor of a collapsed section, and the
	// part of an open one that is not the panel.
	float getHeaderHeight() const;

protected:
	using Panel::init;

	AccordionView *_view = nullptr; // non-owning: it is our parent
	AccordionHeader *_header = nullptr;
	Node *_body = nullptr;
	String _panelId;
	bool _expanded = false;
};

/** A vertical stack of named sections, each holding one panel: the accordion.

Sections are declared up front (no model, no virtualization). A section is a parked panel in the
ui::DockSystem sense: same descriptor, builder, ui::PanelRegistry and "xl/dock-panel" drag
payload. With a registry shared with a DockSystem, a panel moves between the two keeping its node.

    auto registry = Rc<ui::PanelRegistry>::create();
    registry->registerPanel({.id = "console", .title = "Console", .minSize = Size2(240, 100),
        .builder = [] { return Rc<ConsolePanel>::create(); }});

    auto dock = dockRoot->addSystem(Rc<ui::DockSystem>::create(Rc<ui::PanelRegistry>(registry)));
    auto side = addChild(Rc<ui::AccordionView>::create(Rc<ui::PanelRegistry>(registry)));
    side->setSections({StringView("explorer"), StringView("console")});

This node has no LayoutSystem: it sizes a single viewport child, which is the scrolling flex
column of sections. That keeps this node usable as the `decoratorParent` (unclipped, no layout,
so a drag ghost is neither laid out nor clipped).

CSS type "accordion-view", with `drop-active` while a drag is over it. */
class SP_PUBLIC AccordionView : public Panel, public PanelHost {
public:
	// Each section gets its own increasing ZOrder from SectionZOrder: sortAllChildren is not
	// stable, and child order is flow order. The indicator sits above any plausible count.
	static constexpr ZOrder SectionZOrder = ZOrder(1);
	static constexpr ZOrder IndicatorZOrder = ZOrder(1024);

	// Thickness of the caret drawn between two sections to show where a drop would land.
	static constexpr float DefaultIndicatorThickness = 2.0f;

	using PanelCallback = Function<void(StringView id)>;

	virtual ~AccordionView() = default;

	virtual bool init() override;

	// Run against a registry somebody else owns - a DockSystem's - so panels can move between them.
	virtual bool init(Rc<PanelRegistry> &&);

	virtual void handleEnter(Scene *) override;
	virtual void handleExit() override;
	virtual void handleContentSizeDirty() override;

	// --- PanelHost ---------------------------------------------------------

	virtual PanelRegistry *getPanelRegistry() const override { return _registry; }
	virtual Ref *getPanelHostRef() override { return this; }

	virtual bool isPanelOpen(StringView id) const override;

	// Expand the section and scroll it into view. In Single mode this is what closes the other one.
	virtual bool activatePanel(StringView id) override;

	virtual bool closePanel(StringView id) override;
	virtual void releasePanel(StringView id) override;

	// This node: unclipped, and with no layout of its own. See the class comment.
	virtual Node *getPanelDecoratorParent() const override { return const_cast<AccordionView *>(this); }

	// --- panels ------------------------------------------------------------

	// Convenience forward; the registry is the real home of a descriptor.
	void registerPanel(DockPanelDescriptor &&);

	// Declare the sections, in order. Existing sections keep their node and expanded state, new
	// ones are built, and those not in the list are released.
	virtual void setSections(Vector<String> &&ids);

	SpanView<String> getSections() const { return _order; }
	AccordionSection *getSection(StringView id) const;

	// Index of a section, or maxOf<size_t>() when there is none.
	size_t getSectionIndex(StringView id) const;

	// Add one section at `index` (clamped, and appended when past the end). Takes the panel from
	// whatever container is holding it.
	virtual bool openPanel(StringView id, size_t index = maxOf<size_t>());

	// Move an existing section to `index`. Reordering only; use openPanel to bring one in.
	virtual bool movePanel(StringView id, size_t index);

	virtual bool expandPanel(StringView id);
	virtual bool collapsePanel(StringView id);
	virtual bool togglePanel(StringView id);
	bool isPanelExpanded(StringView id) const;

	// --- policy ------------------------------------------------------------

	virtual void setExpansion(AccordionExpansion);
	AccordionExpansion getExpansion() const { return _expansion; }

	virtual void setSizing(AccordionSizing);
	AccordionSizing getSizing() const { return _sizing; }

	/* Per-section override of the view's sizing, e.g. a `Fit` section in a `Fill` view takes its
	declared minimum and does not grow, while the others share the rest. `clearSectionSizing`
	removes the override; `setSizing` keeps overrides. */
	virtual void setSectionSizing(StringView id, AccordionSizing);
	virtual void clearSectionSizing(StringView id);

	// The section's override, or the view's policy; also valid for unknown ids.
	AccordionSizing getSectionSizing(StringView id) const;

	// --- callbacks ---------------------------------------------------------

	void setPanelOpenedCallback(PanelCallback &&);
	void setPanelClosedCallback(PanelCallback &&);
	void setPanelExpandedCallback(PanelCallback &&);

	// --- receiving a dragged panel -----------------------------------------

	/* Where a panel dropped at `viewportLocal` would land, as an insertion index into the section
	list (no "into" zone). Works without a drag in flight. */
	size_t getDropIndexAt(const Vec2 &viewportLocal) const;

	// The caret for an insertion index, in the viewport's space; false when there is nothing to draw.
	bool getDropIndicatorRect(size_t index, Rect &out) const;

	void setDropEnabled(bool);
	bool isDropEnabled() const { return _dropEnabled; }

	// --- measurement -------------------------------------------------------

	/* The size needed to show everything without scrolling: every header plus the declared minimum
	of every open panel. Not propagated: a dock measures parked panels by descriptor `minSize`, so
	feed this into the descriptor when an accordion is docked. */
	Size2 getNaturalMinSize() const;

	// --- persistence -------------------------------------------------------

	// The order and which sections are open; titles, icons and minimums come from the registry.
	// With a registry shared with a dock, restore both halves against that registry.
	Value save() const;
	bool restore(const Value &);

protected:
	using Panel::init;

	static constexpr uint32_t SaveVersion = 1;

	// Bring the section nodes in line with `_order`, reusing by panel id so a reorder does not
	// rebuild a section (and lose its hover state or an in-flight drag).
	void syncSections();

	// Flex item of one section, from the expansion state and the sizing policy.
	void updateSectionFlex(AccordionSection *);

	// Put the panel's node into an open section's body, take it out of a closed one's.
	void updateSectionContent(AccordionSection *);

	// Apply the Single-expansion rule after `keep` was opened.
	void collapseOthers(StringView keep);

	void updateDropIndicator(const Vec2 &viewportLocal);
	void clearDropIndicator();

	// --- drop-target slots -------------------------------------------------

	static DockPanelPayload *payloadOf(const DragEvent &);

	// Pure: called during hit testing, possibly several times a frame, for candidates that may never
	// become current. Resolves the index and answers whether it would take the panel
	DragResponse handleDragAccept(const DragEvent &);

	void handleDragEnter(const DragEvent &);
	void handleDragOver(const DragEvent &);
	void handleDragLeave(const DragEvent &);
	bool handleDragDrop(const DragEvent &, DragActions);

	Rc<PanelRegistry> _registry;

	// The clipped, scrolling flex column the sections live in; this node's only child.
	Node *_viewport = nullptr;
	ScrollSystem *_scroll = nullptr;

	Vector<String> _order;
	Map<String, AccordionSection *> _sections;

	// alive only between a drag entering this view and leaving it
	basic2d::Layer *_indicator = nullptr;

	AccordionExpansion _expansion = AccordionExpansion::Multi;
	AccordionSizing _sizing = AccordionSizing::Fit;

	// Per-section sizing overrides, keyed by panel id since section nodes may be rebuilt.
	Map<String, AccordionSizing> _sectionSizing;

	bool _dropEnabled = true;

	PanelCallback _panelOpenedCallback;
	PanelCallback _panelClosedCallback;
	PanelCallback _panelExpandedCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_VIEW_XLUIACCORDIONVIEW_H_
