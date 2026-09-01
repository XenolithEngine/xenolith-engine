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
	// Each open section takes the height its content asks for, and the view scrolls when the total
	// runs past it. The natural reading of "accordion", and the one a list of unrelated panels wants.
	Fit,

	// Open sections share the height that is left after the collapsed headers. Nothing scrolls.
	// What a side pane of working panels wants - an editor is not useful at its minimum height.
	Fill,
};

/** The header of one section: the chevron, the panel's icon and title, a grip, and a close button.

It is a PanelHandle, so the drag that pulls the panel out is the SAME one a dock tab uses, down to
the threshold, the pointer capture and the abort when the node goes away. What differs is only where
that drag may start.

A TAP TOGGLES THE SECTION; ONLY THE GRIP DRAGS IT. That split is the reason canBeginDragAt exists on
the base at all. The whole header cannot be the grab point here the way a whole tab is: a header's
press already means "open this", so a drag starting anywhere on it would make every slightly
imprecise click a drag, and the panel would come loose when the user only meant to look inside it.
A dock tab has no such conflict - its press means "show this", which is what a drag ends up doing
anyway - so it keeps the whole node.

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

	// The panel's icon, placed between the grip and the title. Overridden only for that placement:
	// ui::Button creates the sprite lazily on the first setIcon, so there is nothing to order until
	// this has been called at least once.
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

Collapsing does not hide the body, it TAKES THE PANEL OUT of it - detached without cleanup, so the
node keeps everything it had. A hidden body would keep the panel's whole subtree alive in the layout
and the style passes for as long as the section stayed shut, which for a panel that is expensive to
lay out is the cost the collapse was supposed to avoid.

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

WHAT IT IS NOT. It is not a ui::TreeView with two levels. A tree is driven by a data::Model whose
shape it discovers, and it virtualizes rows because it cannot know how many there are. An accordion's
sections are declared in advance, in the application's own source, and there are a dozen of them at
most - so the model, the virtualization and the row recycling would all be machinery for a problem
this does not have.

WHAT IT SHARES WITH THE DOCK, and why that is the whole point. A section IS a parked panel, in
exactly the sense ui::DockSystem means: same ui::DockPanelDescriptor, same lazy builder, same
ui::PanelRegistry, same ui::PanelHandle drag with the same "xl/dock-panel" payload. Hand this view
and a DockSystem the SAME registry and a panel can be dragged from a dock frame into this stack and
back, keeping its node - and with it its scroll position, its selection and its half-typed text -
across the move. Neither container knows anything about the other; the registry is the only thing
they share.

    auto registry = Rc<ui::PanelRegistry>::create();
    registry->registerPanel({.id = "console", .title = "Console", .minSize = Size2(240, 100),
        .builder = [] { return Rc<ConsolePanel>::create(); }});

    auto dock = dockRoot->addSystem(Rc<ui::DockSystem>::create(Rc<ui::PanelRegistry>(registry)));
    auto side = addChild(Rc<ui::AccordionView>::create(Rc<ui::PanelRegistry>(registry)));
    side->setSections({StringView("explorer"), StringView("console")});

STRUCTURE, and the one thing that is easy to get wrong. This node carries NO LayoutSystem: it holds a
single viewport child, which it sizes itself, and the viewport is the flex column that runs the
sections. That is not tidiness - it is what makes this node a legal `decoratorParent`. A drag ghost
parked in a flex container becomes a flex item and gets laid out into the stack; parked in the
viewport it would be clipped away by the scroll the moment it left. An unclipped parent with no
layout is the only node that is neither, which is exactly what a dock root is for the dock.

CSS type "accordion-view", with `drop-active` while a drag is over it. */
class SP_PUBLIC AccordionView : public Panel, public PanelHost {
public:
	// Distinct bands: sortAllChildren is not a stable sort, so siblings sharing a ZOrder permute
	// between frames - and inside the viewport the child order IS the flow order, so a permutation
	// would reshuffle the sections on screen. Each section therefore gets its own increasing order
	// from SectionZOrder, and the indicator sits far above every plausible section count.
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

	// Declare the sections, in order. The tabs of an accordion are known in advance - this is how
	// they are named. Sections already present keep their node and their expanded state; the rest
	// are built, and any that fall out of the list are released.
	//
	// A Vector rather than a SpanView, so the call site reads like the dock's own
	// DockLayoutSpec::leaf({"editor", "console"}) - SpanView has no initializer-list constructor.
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

	// --- callbacks ---------------------------------------------------------

	void setPanelOpenedCallback(PanelCallback &&);
	void setPanelClosedCallback(PanelCallback &&);
	void setPanelExpandedCallback(PanelCallback &&);

	// --- receiving a dragged panel -----------------------------------------

	/* Where a panel dropped at `viewportLocal` would land, as an insertion index into the section
	list. An insertion index and nothing else: a section holds exactly one panel, so unlike a dock
	frame there is no "into" and no way to subdivide - the only question a drop can answer here is
	"between which two".

	Answers with no drag in flight, so a test can drive the zone rule without synthesizing a pointer. */
	size_t getDropIndexAt(const Vec2 &viewportLocal) const;

	// The caret for an insertion index, in the viewport's space; false when there is nothing to draw.
	bool getDropIndicatorRect(size_t index, Rect &out) const;

	void setDropEnabled(bool);
	bool isDropEnabled() const { return _dropEnabled; }

	// --- measurement -------------------------------------------------------

	/* What this view would need to show everything it holds without scrolling: every header, plus
	the declared minimum of every OPEN panel.

	It has to be asked for rather than propagated, and that is worth knowing before parking an
	accordion inside a dock. DockSystem::measureLeaf floors a frame from the `minSize` in each
	parked panel's DESCRIPTOR - it never measures the node - so an accordion parked as a dock panel
	contributes only the minimum its own descriptor declares, whatever it happens to be holding.
	Feed this back into that descriptor if the frame has to grow with the stack. */
	Size2 getNaturalMinSize() const;

	// --- persistence -------------------------------------------------------

	// The order and which sections stand open. Never a title, an icon or a minimum: those come back
	// from the registry, and a stale copy of them would be worse than none.
	//
	// With a registry shared with a dock, this and DockSystem::save() are two HALVES of one
	// arrangement and have to be restored against that same registry - each half claims its own
	// panels, and a panel named by neither simply stays closed.
	Value save() const;
	bool restore(const Value &);

protected:
	using Panel::init;

	static constexpr uint32_t SaveVersion = 1;

	// Bring the section nodes in line with `_order`, reusing by panel id: a reorder or an arrival
	// beside one must not rebuild a section, which would drop its hover state and, worse, the drag
	// quite possibly in flight on it right now.
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

	// The clipped, scrolling flex column the sections live in. This node holds nothing else, which
	// is what keeps IT free to be a decoratorParent - see the class comment.
	Node *_viewport = nullptr;
	ScrollSystem *_scroll = nullptr;

	Vector<String> _order;
	Map<String, AccordionSection *> _sections;

	// alive only between a drag entering this view and leaving it
	basic2d::Layer *_indicator = nullptr;

	AccordionExpansion _expansion = AccordionExpansion::Multi;
	AccordionSizing _sizing = AccordionSizing::Fit;
	bool _dropEnabled = true;

	PanelCallback _panelOpenedCallback;
	PanelCallback _panelClosedCallback;
	PanelCallback _panelExpandedCallback;
};

} // namespace stappler::xenolith::ui

#endif // XENOLITH_RENDERER_UI_VIEW_XLUIACCORDIONVIEW_H_
