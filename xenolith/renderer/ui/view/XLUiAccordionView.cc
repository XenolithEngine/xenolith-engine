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

#include "XLUiAccordionView.h"
#include "XLUiLayoutSystem.h"
#include "XLUiDragScrollSystem.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::ui {

// --- AccordionHeader -------------------------------------------------------

bool AccordionHeader::init(NotNull<AccordionSection> section, NotNull<PanelHost> host,
		StringView panelId) {
	if (!PanelHandle::init(host, panelId)) {
		return false;
	}

	_section = section;

	setType("accordion-header");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-accordion-header");
	registerStyleAppliers("accordion-header");

	setAnchorPoint(Anchor::BottomLeft);

	// A header sizes itself from its own title, and a ui::Button does not: like every button in this
	// kit it is arranged by a flex layout, which normally comes from `display: flex` in a stylesheet.
	// The view cannot require an application to write that rule for a widget it did not create, so
	// the layout is built here - and with it the measurement protocol, through which the section and
	// then the whole stack derive their floor from the actual titles.
	//
	// NO SystemManagedLayout marker, on purpose: the resolver only ever tears down a layout it
	// created itself, so this one survives. A stylesheet can still refine it, but only through a
	// rule that ALSO declares `display: flex` - padding and the gaps are read inside the resolver's
	// flex branch, and a rule without `display` never enters it.
	addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Row,
		.alignItems = FlexAlign::Center,
		.columnGap = 6.0f,
		.padding = Padding(4.0f, 8.0f),
	}));

	_chevron = addChild(Rc<basic2d::IconSprite>::create(IconCollapsed), ZOrder(1));
	_chevron->setType("icon");
	_chevron->addStyleClass("accordion-chevron");
	LayoutSystem::setItem(_chevron, FlexItemInfo{.grow = 0.0f, .shrink = 0.0f, .order = 0});

	// The grab point. It is the ONLY part of the header a drag may start from, which is what keeps
	// an imprecise click on a header from pulling its panel out - see the class comment.
	_grip = addChild(Rc<basic2d::IconSprite>::create(IconName::Editor_drag_handle_outline),
			ZOrder(2));
	_grip->setType("icon");
	_grip->addStyleClass("accordion-grip");
	LayoutSystem::setItem(_grip, FlexItemInfo{.grow = 0.0f, .shrink = 0.0f, .order = 1});

	// The label comes from Button; it takes the free space so the close button sits at the far end.
	if (auto label = getLabel()) {
		LayoutSystem::setItem(label, FlexItemInfo{.grow = 1.0f, .shrink = 1.0f, .order = 3});
	}

	return true;
}

void AccordionHeader::setExpanded(bool value) {
	if (value == _expanded) {
		return;
	}
	_expanded = value;
	if (_expanded) {
		addStyleClass("expanded");
		removeStyleClass("collapsed");
	} else {
		addStyleClass("collapsed");
		removeStyleClass("expanded");
	}
	if (_chevron) {
		// The icon is swapped rather than the sprite rotated: the CSS subset has no transform, so a
		// rotation would be invisible to a stylesheet that wanted to restyle the two states apart.
		_chevron->setIconName(_expanded ? IconExpanded : IconCollapsed);
	}
}

void AccordionHeader::setIcon(IconName name) {
	Button::setIcon(name);
	// Order 2: after the chevron and the grip, before the title. Without it the sprite keeps the
	// default order of 0 and ties with the chevron - and a tie is resolved by child order, which
	// follows ZOrder, so the two would swap places between frames.
	if (auto icon = getIconSprite()) {
		LayoutSystem::setItem(icon, FlexItemInfo{.grow = 0.0f, .shrink = 0.0f, .order = 2});
	}
}

void AccordionHeader::setClosable(bool value) {
	if (value && !_close) {
		_close = addChild(Rc<Button>::create([this] {
			if (_host) {
				_host->closePanel(_panelId);
			}
		}),
				ZOrder(4));
		_close->setType("accordion-close");
		_close->addStyleClass("xl-ui-accordion-close");
		_close->setIcon(IconName::Navigation_close_solid);
		LayoutSystem::setItem(_close, FlexItemInfo{.grow = 0.0f, .shrink = 0.0f, .order = 4});
	} else if (!value && _close) {
		_close->removeFromParent(true);
		_close = nullptr;
	}
}

bool AccordionHeader::canBeginDragAt(const Vec2 &worldLocation) const {
	// isTouched, not the hit-test registry: this runs inside a live input callback on our own
	// subtree, where the tree as it is right now IS what the user pressed on. The registry's test is
	// for a drop target being resolved against a frame that has already been committed.
	return _grip && _grip->isTouched(worldLocation);
}

bool AccordionHeader::handleLeftTap() {
	if (isDragging()) {
		return false; // this pointer belongs to a drag; a tap on release would be a second action
	}
	if (_host && _section) {
		// Toggle rather than activate: a header's press means "open this so I can see inside", and
		// pressing it again means "shut it". A dock tab has no second state to return to.
		if (auto view = dynamic_cast<AccordionView *>(_host)) {
			view->togglePanel(_panelId);
			return true;
		}
	}
	return false;
}

void AccordionHeader::updatePanelDragOffer(DragOffer &, DockPanelPayload &payload) {
	if (auto view = dynamic_cast<AccordionView *>(_host)) {
		// A linear container: the position in the stack, not a frame handle - `source` stays empty,
		// which is how a dock reading this payload knows the handle would mean nothing in its tree.
		payload.sourceIndex = view->getSectionIndex(_panelId);
	}
}

// --- AccordionSection ------------------------------------------------------

bool AccordionSection::init(NotNull<AccordionView> view, NotNull<PanelHost> host,
		StringView panelId) {
	if (!Panel::init()) {
		return false;
	}

	_view = view;
	_panelId = panelId.str<Interface>();

	setType("accordion-section");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-accordion-section");
	registerStyleAppliers("accordion-section");

	setAnchorPoint(Anchor::BottomLeft);

	addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Column,
		.alignItems = FlexAlign::Stretch,
	}));

	// The header keeps the higher ZOrder because it draws over the body's edge, while the FLOW has
	// to put it first. Child order follows ZOrder, so `order` is what separates the two - the same
	// reason DockFrame sets it on its tab strip.
	_header = addChild(Rc<AccordionHeader>::create(this, host, panelId), ZOrder(1));
	LayoutSystem::setItem(_header,
			FlexItemInfo{
				.grow = 0.0f,
				.shrink = 0.0f,
				.basis = FlexItemInfo::FitContent,
				.order = 0,
			});

	_body = addChild(Rc<Node>::create(), ZOrder(0));
	_body->setType("accordion-body");
	_body->setAnchorPoint(Anchor::BottomLeft);
	LayoutSystem::setItem(_body,
			FlexItemInfo{
				.grow = 1.0f,
				.shrink = 1.0f,
				.basis = 0.0f,
				.order = 1,
			});
	// A panel parked in a node with no layout would keep whatever size it was built with - none.
	_body->addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Column,
		.alignItems = FlexAlign::Stretch,
	}));

	setExpanded(false);
	return true;
}

void AccordionSection::setExpanded(bool value) {
	_expanded = value;
	if (_expanded) {
		addStyleClass("expanded");
		removeStyleClass("collapsed");
	} else {
		addStyleClass("collapsed");
		removeStyleClass("expanded");
	}
	if (_header) {
		_header->setExpanded(value);
	}
	if (_body) {
		// The body is taken out of the flow rather than resized to nothing: a zero-height flex item
		// still participates in every pass, and its own content would go on being measured.
		_body->setVisible(_expanded);
		LayoutSystem::setItem(_body,
				FlexItemInfo{
					.grow = _expanded ? 1.0f : 0.0f,
					.shrink = _expanded ? 1.0f : 0.0f,
					.basis = _expanded ? 0.0f : 0.0f,
					.order = 1,
					.maxMain = _expanded ? FlexItemInfo::Auto : 0.0f,
				});
	}
}

float AccordionSection::getHeaderHeight() const {
	return _header ? _header->getContentSize().height : 0.0f;
}

// --- AccordionView ---------------------------------------------------------

bool AccordionView::init() { return init(Rc<PanelRegistry>::create()); }

bool AccordionView::init(Rc<PanelRegistry> &&registry) {
	if (!registry) {
		return false;
	}
	if (!Panel::init()) {
		return false;
	}

	_registry = sp::move(registry);

	setType("accordion-view");
	removeStyleClass("xl-ui-panel");
	addStyleClass("xl-ui-accordion-view");
	registerStyleAppliers("accordion-view");

	setAnchorPoint(Anchor::BottomLeft);

	// This widget places its own child, so a CSS width/height on the viewport must become an
	// intrinsic hint rather than a committed size that would fight handleContentSizeDirty.
	setComponent<SystemManagedLayout>();

	// NO LayoutSystem on THIS node - see the class comment. The viewport is the flex column, and it
	// is the only child, sized from handleContentSizeDirty.
	_viewport = addChild(Rc<Node>::create(), ZOrder(0));
	_viewport->setType("accordion-viewport");
	_viewport->setAnchorPoint(Anchor::BottomLeft);
	_viewport->setPosition(Vec2::ZERO);
	_viewport->addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Column,
		.alignItems = FlexAlign::Stretch,
	}));

	// Built here rather than left to a stylesheet: the scroll is what makes Fit sizing work at all,
	// and an application cannot be asked to write `overflow` for a widget it did not create. It
	// carries no StyleManagedScroll marker, so the resolver - which only ever removes a system it
	// added itself - leaves it alone.
	//
	// The HORIZONTAL axis stays Visible, and that is not the same as "unclipped". ScrollSystem hands
	// LayoutSystem `setOverflowAxes(clipsX(), clipsY())`, and `Hidden` clips - so declaring it would
	// mark the cross axis as one the content may exceed, which is exactly what tells the flex pass
	// to leave the sections at their own width instead of stretching them to the viewport. The box
	// is still scissored, because that is gated on clipsX() OR clipsY().
	_scroll = _viewport->addSystem(
			Rc<ScrollSystem>::create(document::Overflow::Visible, document::Overflow::Auto));

	// How this stack receives dragged panels. ONE target on the view, never one per section: a
	// per-section target could not answer for the gap after the last one, which is the append
	// position, and the drop resolves an index by arithmetic over the sections anyway.
	//
	// On the VIEW rather than the viewport, because the view is the node whose drawn rect should
	// accept a drop - the viewport carries the same rect but is clipped, and a target only exists
	// where it was drawn.
	setDropTarget(this,
			DropTargetSlots{
				.accept = [this](const DragEvent &event) { return handleDragAccept(event); },
				.enter = [this](const DragEvent &event) { handleDragEnter(event); },
				.over = [this](const DragEvent &event) { handleDragOver(event); },
				.leave = [this](const DragEvent &event) { handleDragLeave(event); },
				.drop = [this](const DragEvent &event,
								DragActions action) { return handleDragDrop(event, action); },
			});

	setSizing(_sizing);
	return true;
}

void AccordionView::handleEnter(Scene *scene) {
	Panel::handleEnter(scene);

	_registry->addHost(this);

	// A node has no scene until it is added to one, so this cannot happen in init(). Scoped to
	// TargetInside by default, or dragging a dock tab clear across the screen would scroll every
	// accordion it passed over. On the VIEWPORT: the edge band is measured against the scrollport.
	DragScrollSystem::acquireForNode(_viewport);
}

void AccordionView::handleExit() {
	clearDropIndicator();

	// Give up every claim without touching a node: the panels keep their content, so re-opening one
	// anywhere brings back exactly what was there.
	_registry->releaseHost(this);

	Panel::handleExit();
}

void AccordionView::handleContentSizeDirty() {
	Panel::handleContentSizeDirty();
	if (_viewport) {
		_viewport->setContentSize(_contentSize);
	}
}

// --- panels ----------------------------------------------------------------

void AccordionView::registerPanel(DockPanelDescriptor &&desc) {
	_registry->registerPanel(sp::move(desc));
}

AccordionSection *AccordionView::getSection(StringView id) const {
	auto it = _sections.find(id.str<Interface>());
	return (it != _sections.end()) ? it->second : nullptr;
}

size_t AccordionView::getSectionIndex(StringView id) const {
	for (size_t i = 0; i < _order.size(); ++i) {
		if (_order[i] == id) {
			return i;
		}
	}
	return maxOf<size_t>();
}

bool AccordionView::isPanelOpen(StringView id) const {
	return getSectionIndex(id) != maxOf<size_t>();
}

bool AccordionView::isPanelExpanded(StringView id) const {
	auto section = getSection(id);
	return section && section->isExpanded();
}

void AccordionView::setSections(Vector<String> &&ids) {
	Vector<String> next;
	next.reserve(ids.size());
	for (auto &id : ids) {
		if (!_registry->getPanelDescriptor(id)) {
			log::source().warn("ui::AccordionView", "unknown panel '", id, "' in the section list");
			continue;
		}
		if (sprt::find(next.begin(), next.end(), id) != next.end()) {
			log::source().warn("ui::AccordionView", "panel '", id, "' listed twice");
			continue;
		}
		next.emplace_back(sp::move(id));
	}
	_order = sp::move(next);
	syncSections();
}

bool AccordionView::openPanel(StringView id, size_t index) {
	if (!_registry->getPanelDescriptor(id)) {
		log::source().error("ui::AccordionView", "openPanel: unknown panel '", id, "'");
		return false;
	}

	if (isPanelOpen(id)) {
		// already here: honour an explicit position, otherwise just bring it forward
		return (index == maxOf<size_t>()) ? activatePanel(id) : movePanel(id, index);
	}

	const size_t at = sprt::min(index, _order.size());
	_order.emplace(_order.begin() + at, id.str<Interface>());
	syncSections();

	if (_panelOpenedCallback) {
		_panelOpenedCallback(id);
	}
	return true;
}

bool AccordionView::movePanel(StringView id, size_t index) {
	auto from = getSectionIndex(id);
	if (from == maxOf<size_t>()) {
		return false;
	}

	auto key = _order[from];
	_order.erase(_order.begin() + from);
	const size_t at = sprt::min(index, _order.size());
	if (at == from) {
		_order.emplace(_order.begin() + at, sp::move(key));
		return true; // nothing actually moved; no rebuild, no callback
	}
	_order.emplace(_order.begin() + at, sp::move(key));
	syncSections();
	return true;
}

bool AccordionView::closePanel(StringView id) {
	auto index = getSectionIndex(id);
	if (index == maxOf<size_t>()) {
		return false;
	}
	_order.erase(_order.begin() + index);
	syncSections();

	if (_panelClosedCallback) {
		_panelClosedCallback(id);
	}
	return true;
}

void AccordionView::releasePanel(StringView id) {
	// Structurally the same as a close - the section goes - but NOT reported as one: the panel is
	// moving to another container, and an application treating `closed` as "the user is done with
	// this" would act on something that did not happen. The node is not touched here; the registry
	// hands it to the new host.
	auto index = getSectionIndex(id);
	if (index == maxOf<size_t>()) {
		return;
	}
	_order.erase(_order.begin() + index);
	syncSections();
}

bool AccordionView::activatePanel(StringView id) {
	if (!expandPanel(id)) {
		// already open: still bring it into view, which is what "activate" asks for
		if (!isPanelOpen(id)) {
			return false;
		}
	}
	if (auto section = getSection(id); section && _scroll) {
		_scroll->scrollNodeIntoView(section);
	}
	return true;
}

bool AccordionView::expandPanel(StringView id) {
	auto section = getSection(id);
	if (!section || section->isExpanded()) {
		return false;
	}

	section->setExpanded(true);
	if (_expansion == AccordionExpansion::Single) {
		collapseOthers(id);
	}
	updateSectionContent(section);
	updateSectionFlex(section);

	if (_panelExpandedCallback) {
		_panelExpandedCallback(id);
	}
	return true;
}

bool AccordionView::collapsePanel(StringView id) {
	auto section = getSection(id);
	if (!section || !section->isExpanded()) {
		return false;
	}

	// In Single mode the open section is the only one; collapsing it would leave the stack with
	// nothing showing, which is a state the mode does not have. Refuse rather than invent one.
	if (_expansion == AccordionExpansion::Single) {
		return false;
	}

	section->setExpanded(false);
	updateSectionContent(section);
	updateSectionFlex(section);

	if (_panelExpandedCallback) {
		_panelExpandedCallback(id);
	}
	return true;
}

bool AccordionView::togglePanel(StringView id) {
	auto section = getSection(id);
	if (!section) {
		return false;
	}
	return section->isExpanded() ? collapsePanel(id) : expandPanel(id);
}

void AccordionView::collapseOthers(StringView keep) {
	for (auto &id : _order) {
		if (id == keep) {
			continue;
		}
		if (auto section = getSection(id); section && section->isExpanded()) {
			section->setExpanded(false);
			updateSectionContent(section);
			updateSectionFlex(section);
		}
	}
}

// --- policy ----------------------------------------------------------------

void AccordionView::setExpansion(AccordionExpansion value) {
	if (value == _expansion) {
		return;
	}
	_expansion = value;
	if (_expansion != AccordionExpansion::Single) {
		return;
	}

	// Single means exactly one, so an existing arrangement has to be reduced to one - and to a
	// DEFINITE one: with nothing open the stack would show only headers, which is the state this
	// mode exists to rule out. The first open section wins, or the first section if none is.
	StringView keep;
	for (auto &id : _order) {
		if (auto section = getSection(id); section && section->isExpanded()) {
			keep = id;
			break;
		}
	}
	if (keep.empty() && !_order.empty()) {
		keep = _order.front();
		expandPanel(keep);
	} else if (!keep.empty()) {
		collapseOthers(keep);
	}
}

void AccordionView::setSizing(AccordionSizing value) {
	_sizing = value;
	if (_scroll) {
		// Fill leaves nothing to scroll: the open sections absorb whatever height there is. It has
		// to be Visible rather than Hidden for the same reason the horizontal axis is (see init) -
		// on a Hidden axis the layout lays the content out at its NATURAL size so there is something
		// to clip, and a section that sizes itself can no longer be grown to fill the box.
		_scroll->setOverflow(document::Overflow::Visible,
				_sizing == AccordionSizing::Fit ? document::Overflow::Auto
												: document::Overflow::Visible);
	}
	for (auto &id : _order) {
		if (auto section = getSection(id)) {
			updateSectionFlex(section);
		}
	}
}

void AccordionView::setPanelOpenedCallback(PanelCallback &&cb) {
	_panelOpenedCallback = sp::move(cb);
}

void AccordionView::setPanelClosedCallback(PanelCallback &&cb) {
	_panelClosedCallback = sp::move(cb);
}

void AccordionView::setPanelExpandedCallback(PanelCallback &&cb) {
	_panelExpandedCallback = sp::move(cb);
}

// --- section nodes ---------------------------------------------------------

void AccordionView::syncSections() {
	if (!_viewport) {
		return;
	}

	// Reuse by panel id, wherever in the stack it was: a reorder or a panel arriving beside one must
	// not destroy and rebuild a section - that would drop its hover state and, worse, the drag that
	// is quite possibly in flight on its header right now.
	Vector<AccordionSection *> kept;
	kept.reserve(_order.size());

	for (auto &id : _order) {
		auto section = getSection(id);
		if (!section) {
			auto created = Rc<AccordionSection>::create(this, this, id);
			// Parent it BEFORE the local Rc goes out of scope: `kept` holds raw pointers, so letting
			// the only reference die at the end of this block would leave the entry dangling.
			_viewport->addChild(created, SectionZOrder);
			if (auto desc = _registry->getPanelDescriptor(id)) {
				if (auto header = created->getHeader()) {
					header->setString(
							desc->title.empty() ? StringView(desc->id) : StringView(desc->title));
					header->setIcon(desc->icon);
					header->setClosable(hasFlag(desc->flags, DockPanelFlags::Closable));
				}
			}
			section = created;
			_sections.emplace(id, section);

			// A new section starts open in Single mode only if nothing else is - collapseOthers
			// below settles it either way.
			if (_expansion == AccordionExpansion::Multi) {
				section->setExpanded(true);
			}
		}
		kept.emplace_back(section);
	}

	// Sections that fell out of the order. Their panels go back to being parked nowhere: the node
	// survives in the registry, so whatever picks one up next gets it whole.
	Vector<AccordionSection *> gone;
	for (auto &[id, section] : _sections) {
		if (sprt::find(kept.begin(), kept.end(), section) == kept.end()) {
			gone.emplace_back(section);
		}
	}
	for (auto section : gone) {
		auto id = section->getPanelId().str<Interface>();
		// Take the panel OUT before the section is cleaned: Node::cleanup() recurses into children
		// and would destroy the panel's systems while the registry still holds it.
		if (auto body = section->getBody()) {
			auto children = body->getChildren();
			for (auto &it : Vector<Rc<Node>>(children.begin(), children.end())) {
				it->removeFromParent(false);
			}
		}
		_sections.erase(id);
		section->removeFromParent(true);
	}

	// Distinct, increasing ZOrder: child order follows ZOrder and IS the flow order here, and
	// sortAllChildren is not a stable sort - siblings sharing an order would reshuffle on screen
	// between frames.
	for (size_t i = 0; i < kept.size(); ++i) {
		kept[i]->setLocalZOrder(SectionZOrder + ZOrder(int32_t(i)));
		updateSectionContent(kept[i]);
		updateSectionFlex(kept[i]);
	}

	if (_expansion == AccordionExpansion::Single) {
		StringView keep;
		for (auto &id : _order) {
			if (auto section = getSection(id); section && section->isExpanded()) {
				keep = id;
				break;
			}
		}
		if (keep.empty() && !_order.empty()) {
			expandPanel(_order.front());
		} else if (!keep.empty()) {
			collapseOthers(keep);
		}
	}
}

void AccordionView::updateSectionFlex(AccordionSection *section) {
	if (!section) {
		return;
	}

	// The floor of a section: its header, plus - when it is open - the minimum its panel declared.
	// This is where the registry's minSize means something on this side, the way it floors a dock
	// frame on the other.
	float floor = section->getHeaderHeight();
	if (section->isExpanded()) {
		if (auto desc = _registry->getPanelDescriptor(section->getPanelId())) {
			floor += desc->minSize.height;
		}
	}

	if (!section->isExpanded()) {
		// Just the header, whatever the policy: a collapsed section has nothing else to show.
		LayoutSystem::setItem(section,
				FlexItemInfo{
					.grow = 0.0f,
					.shrink = 0.0f,
					.basis = FlexItemInfo::FitContent,
					.minMain = floor,
				});
		return;
	}

	switch (_sizing) {
	case AccordionSizing::Fit:
		// The content decides, and the viewport scrolls when the total runs past it.
		LayoutSystem::setItem(section,
				FlexItemInfo{
					.grow = 0.0f,
					.shrink = 0.0f,
					.basis = FlexItemInfo::FitContent,
					.minMain = floor,
				});
		break;
	case AccordionSizing::Fill:
		// The open sections divide what the collapsed headers left. `basis = 0` is what makes them
		// share it evenly rather than in proportion to whatever they happen to contain.
		LayoutSystem::setItem(section,
				FlexItemInfo{
					.grow = 1.0f,
					.shrink = 1.0f,
					.basis = 0.0f,
					.minMain = floor,
				});
		break;
	}
}

void AccordionView::updateSectionContent(AccordionSection *section) {
	if (!section) {
		return;
	}
	auto body = section->getBody();
	if (!body) {
		return;
	}

	// Only an OPEN section holds its panel. Acquiring here rather than when the section is built is
	// what makes the builder lazy in the same sense the dock's is: a section nobody has opened has
	// never built anything.
	Node *content = section->isExpanded() ? _registry->acquireContent(section->getPanelId(), this)
										  : nullptr;

	// Take out whatever else is in there. Detach WITHOUT cleanup: the node stays alive in the
	// registry, and Node::cleanup() would destroy its systems (a Label's EventListener among them),
	// which handleEnter then reads as freed memory on the next present. A plain detach fires
	// handleExit, which is exactly how a system pauses while its node leaves the scene; re-entry
	// replays it through handleEnter.
	auto children = body->getChildren();
	for (auto &it : Vector<Rc<Node>>(children.begin(), children.end())) {
		if (it.get() != content) {
			it->removeFromParent(false);
		}
	}

	if (content && content->getParent() != body) {
		content->removeFromParent(false);
		body->addChild(content);
		// Fill the body; a panel that wants less says so with CSS on its own node.
		LayoutSystem::setItem(content,
				FlexItemInfo{
					.grow = 1.0f,
					.shrink = 1.0f,
					.basis = _sizing == AccordionSizing::Fit ? FlexItemInfo::FitContent : 0.0f,
				});
	}
}

Size2 AccordionView::getNaturalMinSize() const {
	Size2 result;
	for (auto &id : _order) {
		auto section = getSection(id);
		if (!section) {
			continue;
		}
		float height = section->getHeaderHeight();
		if (section->isExpanded()) {
			if (auto desc = _registry->getPanelDescriptor(id)) {
				height += desc->minSize.height;
				result.width = sprt::max(result.width, desc->minSize.width);
			}
		}
		result.height += height;
	}
	return result;
}

// --- dropping --------------------------------------------------------------

size_t AccordionView::getDropIndexAt(const Vec2 &viewportLocal) const {
	if (_order.empty()) {
		return 0;
	}

	// Midpoint comparison, top-down. The scene's Y axis points UP while the stack reads downwards,
	// so the first section is at the HIGHEST y - which is why this walks the order and compares
	// against each section's own middle rather than dividing the extent.
	for (size_t i = 0; i < _order.size(); ++i) {
		auto section = getSection(_order[i]);
		if (!section) {
			continue;
		}
		const float top = section->getPosition().y + section->getContentSize().height;
		const float middle = top - section->getContentSize().height * 0.5f;
		if (viewportLocal.y > middle) {
			return i; // above this section's midpoint: insert before it
		}
	}
	return _order.size();
}

bool AccordionView::getDropIndicatorRect(size_t index, Rect &out) const {
	if (!_viewport) {
		return false;
	}
	const float width = _viewport->getContentSize().width;
	const float half = DefaultIndicatorThickness * 0.5f;

	if (_order.empty()) {
		out = Rect(0.0f, _viewport->getContentSize().height - half, width,
				DefaultIndicatorThickness);
		return true;
	}

	if (index < _order.size()) {
		auto section = getSection(_order[index]);
		if (!section) {
			return false;
		}
		// the boundary ABOVE that section
		const float top = section->getPosition().y + section->getContentSize().height;
		out = Rect(0.0f, top - half, width, DefaultIndicatorThickness);
		return true;
	}

	auto last = getSection(_order.back());
	if (!last) {
		return false;
	}
	out = Rect(0.0f, last->getPosition().y - half, width, DefaultIndicatorThickness);
	return true;
}

void AccordionView::setDropEnabled(bool value) {
	if (value == _dropEnabled) {
		return;
	}
	_dropEnabled = value;
	// The flag on the node is a cache of the component's presence: a node carrying one and not the
	// other wins a hit test and then offers nothing, so both have to move together.
	setDropTargetEnabled(this, value);
	if (!_dropEnabled) {
		clearDropIndicator();
	}
}

DockPanelPayload *AccordionView::payloadOf(const DragEvent &event) {
	if (!event.data || !event.data->isLocal(DockPanelPayload::TypeName)) {
		return nullptr; // somebody else's drag; this view has nothing to say about it
	}
	return dynamic_cast<DockPanelPayload *>(event.data->getLocal());
}

DragResponse AccordionView::handleDragAccept(const DragEvent &event) {
	if (!_dropEnabled) {
		return DragResponse();
	}
	auto payload = payloadOf(event);
	if (!payload) {
		return DragResponse();
	}

	auto desc = _registry->getPanelDescriptor(payload->panelId);
	if (!desc) {
		return DragResponse(); // a panel from a registry we do not share: not ours to take
	}

	// A section holds one panel and there is nothing to subdivide, so the only no-op is dragging the
	// ONLY section of this stack around inside it: every insertion index puts it back where it was.
	if (payload->host == this && _order.size() == 1) {
		return DragResponse();
	}

	// Pure: the index is resolved and thrown away. Nothing is drawn and nothing is remembered - this
	// runs during hit testing, several times a frame, for candidates that may never become current.
	// A panel is MOVED between containers, never copied: one node, one identity.
	return DragResponse{event.allowed & DragActions::Move};
}

void AccordionView::handleDragEnter(const DragEvent &event) {
	if (_indicator) {
		return;
	}
	addStyleClass("drop-active");

	_indicator = _viewport->addChild(Rc<basic2d::Layer>::create(), IndicatorZOrder);
	_indicator->setType("accordion-drop-indicator");
	_indicator->setAnchorPoint(Anchor::BottomLeft);
	// Out of the flow: the viewport is a flex column, and an indicator left in it would be laid out
	// as one more section instead of floating over the boundary it is pointing at.
	_indicator->setComponent<OutOfFlowComponent>();

	handleDragOver(event);
}

void AccordionView::handleDragOver(const DragEvent &event) {
	if (!payloadOf(event) || !_indicator) {
		return;
	}
	updateDropIndicator(_viewport->convertToNodeSpace(event.worldLocation));
}

void AccordionView::handleDragLeave(const DragEvent &) { clearDropIndicator(); }

void AccordionView::updateDropIndicator(const Vec2 &viewportLocal) {
	Rect rect;
	if (!_indicator) {
		return;
	}
	if (!getDropIndicatorRect(getDropIndexAt(viewportLocal), rect)) {
		_indicator->setVisible(false);
		return;
	}
	_indicator->setVisible(true);
	_indicator->setPosition(rect.origin);
	_indicator->setContentSize(rect.size);
}

void AccordionView::clearDropIndicator() {
	removeStyleClass("drop-active");
	if (_indicator) {
		_indicator->removeFromParent(true);
		_indicator = nullptr;
	}
}

bool AccordionView::handleDragDrop(const DragEvent &event, DragActions) {
	auto payload = payloadOf(event);
	if (!_dropEnabled || !payload) {
		return false;
	}

	// Read everything the drop needs BEFORE anything mutates: applying it destroys the header that
	// delivered the drag whenever the panel came from this very stack.
	const auto panelId = payload->panelId;
	const bool fromHere = (payload->host == this);
	const size_t index = getDropIndexAt(_viewport->convertToNodeSpace(event.worldLocation));

	if (!_registry->getPanelDescriptor(panelId)) {
		return false;
	}

	clearDropIndicator();

	if (fromHere) {
		// A reorder. The index was resolved against the stack WITH this section still in it, so an
		// index past its own position counts one slot too many once it is taken out.
		const auto from = getSectionIndex(panelId);
		size_t to = index;
		if (from != maxOf<size_t>() && to > from) {
			--to;
		}
		return movePanel(panelId, to);
	}

	// From somewhere else: the registry evicts the previous host as part of handing over the node,
	// on the acquire updateSectionContent does at the end of this.
	return openPanel(panelId, index);
}

// --- persistence -----------------------------------------------------------

Value AccordionView::save() const {
	Value result;
	result.setInteger(SaveVersion, "version");
	result.setInteger(toInt(_expansion), "expansion");
	result.setInteger(toInt(_sizing), "sizing");

	auto &sections = result.emplace("sections");
	for (auto &id : _order) {
		Value entry;
		entry.setString(id, "id");
		entry.setBool(isPanelExpanded(id), "expanded");
		sections.addValue(sp::move(entry));
	}
	return result;
}

bool AccordionView::restore(const Value &value) {
	if (value.getInteger("version") != SaveVersion) {
		log::source().warn("ui::AccordionView", "a saved layout of an unknown version, ignored");
		return false;
	}

	// Build the whole candidate first and swap it in only when it holds together, so a malformed
	// file leaves what is on screen untouched - the same rule DockSystem::restore follows.
	Vector<String> order;
	Vector<bool> expanded;
	for (auto &entry : value.getArray("sections")) {
		auto id = entry.getString("id");
		if (id.empty() || !_registry->getPanelDescriptor(id)) {
			log::source().warn("ui::AccordionView", "unknown panel '", id,
					"' in the saved layout, dropped");
			continue;
		}
		if (sprt::find(order.begin(), order.end(), id) != order.end()) {
			continue; // the first one wins
		}
		order.emplace_back(id);
		expanded.emplace_back(entry.getBool("expanded"));
	}

	_expansion = AccordionExpansion(value.getInteger("expansion"));
	setSizing(AccordionSizing(value.getInteger("sizing")));

	_order = sp::move(order);
	syncSections();

	for (size_t i = 0; i < _order.size(); ++i) {
		if (expanded[i]) {
			expandPanel(_order[i]);
		} else {
			collapsePanel(_order[i]);
		}
	}
	return true;
}

} // namespace stappler::xenolith::ui
