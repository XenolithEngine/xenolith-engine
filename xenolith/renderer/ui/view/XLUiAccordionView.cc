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

	// The flex layout is built here so a header measures from its title without a stylesheet.
	// No SystemManagedLayout marker: the resolver keeps a layout it did not create. A stylesheet
	// refines padding and gaps only in a rule that also declares `display: flex`.
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

	// The grab point: the only part of the header a drag may start from.
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
		// The icon is swapped rather than rotated, since the CSS subset has no transform.
		_chevron->setIconName(_expanded ? IconExpanded : IconCollapsed);
	}
}

void AccordionHeader::setIcon(IconName name) {
	Button::setIcon(name);
	// Order 2: after the chevron and the grip, before the title. The default 0 would tie with the
	// chevron and the two could swap between frames.
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
	// isTouched, not the hit-test registry: this runs in a live input callback on our own subtree;
	// the registry is for drop targets resolved against a committed frame.
	return _grip && _grip->isTouched(worldLocation);
}

bool AccordionHeader::handleLeftTap() {
	if (isDragging()) {
		return false; // this pointer belongs to a drag; a tap on release would be a second action
	}
	if (_host && _section) {
		// Toggle rather than activate.
		if (auto view = dynamic_cast<AccordionView *>(_host)) {
			view->togglePanel(_panelId);
			return true;
		}
	}
	return false;
}

void AccordionHeader::updatePanelDragOffer(DragOffer &, DockPanelPayload &payload) {
	if (auto view = dynamic_cast<AccordionView *>(_host)) {
		// The position in the stack; `source` stays empty, so a dock knows there is no frame handle.
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

	// The header has the higher ZOrder to draw over the body's edge; `order` puts it first in flow.
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
	// The body needs a layout, or a parked panel keeps its build-time (zero) size.
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
		// Hidden and capped at zero while collapsed; updateSectionFlex rewrites this item.
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

	// No LayoutSystem on this node. The viewport is the flex column and the only child, sized from
	// handleContentSizeDirty.
	_viewport = addChild(Rc<Node>::create(), ZOrder(0));
	_viewport->setType("accordion-viewport");
	_viewport->setAnchorPoint(Anchor::BottomLeft);
	_viewport->setPosition(Vec2::ZERO);
	_viewport->addSystem(Rc<LayoutSystem>::create(FlexLayoutInfo{
		.direction = FlexDirection::Column,
		.alignItems = FlexAlign::Stretch,
	}));

	// Built here since Fit sizing needs it; no StyleManagedScroll marker, so the resolver keeps it.
	// The horizontal axis is Visible: `Hidden` would let content exceed the cross axis and stop the
	// flex pass stretching sections to the viewport width. The vertical axis still scissors the box.
	_scroll = _viewport->addSystem(
			Rc<ScrollSystem>::create(document::Overflow::Visible, document::Overflow::Auto));

	// One drop target on the view (not per section, so the append gap is covered; not on the
	// clipped viewport). The index is resolved from section geometry.
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

	// Needs a scene, so not in init(). Scoped to TargetInside by default; on the viewport, since the
	// edge band is measured against the scrollport.
	DragScrollSystem::acquireForNode(_viewport);
}

void AccordionView::handleExit() {
	clearDropIndicator();

	// Release all claims without touching the nodes; panels keep their content.
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
	// Like a close but without the closed callback: the panel moves to another host, and the
	// registry hands its node over.
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

	// Single mode always keeps one section open.
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

	// Reduce to exactly one open section: the first open one, or the first section if none is.
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
		// Fill does not scroll. Visible, not Hidden: on a Hidden axis content is laid out at its
		// natural size and sections could not grow to fill the box.
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

void AccordionView::setSectionSizing(StringView id, AccordionSizing value) {
	auto key = id.str<Interface>();
	auto it = _sectionSizing.find(key);
	if (it != _sectionSizing.end() && it->second == value) {
		return;
	}
	_sectionSizing.emplace(sp::move(key), value).first->second = value;

	// The section may not exist yet; updateSectionFlex reads the map when it is built.
	if (auto section = getSection(id)) {
		updateSectionFlex(section);
	}
}

void AccordionView::clearSectionSizing(StringView id) {
	auto it = _sectionSizing.find(id.str<Interface>());
	if (it == _sectionSizing.end()) {
		return;
	}
	_sectionSizing.erase(it);
	if (auto section = getSection(id)) {
		updateSectionFlex(section);
	}
}

AccordionSizing AccordionView::getSectionSizing(StringView id) const {
	auto it = _sectionSizing.find(id.str<Interface>());
	return it != _sectionSizing.end() ? it->second : _sizing;
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

	// Reuse by panel id, so a reorder keeps hover state and any drag in flight on a header.
	Vector<AccordionSection *> kept;
	kept.reserve(_order.size());

	for (auto &id : _order) {
		auto section = getSection(id);
		if (!section) {
			auto created = Rc<AccordionSection>::create(this, this, id);
			// Parent it before the local Rc goes out of scope: `kept` holds raw pointers.
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

			// New sections start open in Multi mode; Single mode is settled below.
			if (_expansion == AccordionExpansion::Multi) {
				section->setExpanded(true);
			}
		}
		kept.emplace_back(section);
	}

	// Sections that fell out of the order; their panel nodes stay alive in the registry.
	Vector<AccordionSection *> gone;
	for (auto &[id, section] : _sections) {
		if (sprt::find(kept.begin(), kept.end(), section) == kept.end()) {
			gone.emplace_back(section);
		}
	}
	for (auto section : gone) {
		auto id = section->getPanelId().str<Interface>();
		// Take the panel out before the section is cleaned: Node::cleanup() recurses into children
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

	// Distinct, increasing ZOrder: child order is flow order and sortAllChildren is not stable.
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

	// The floor of a section: its header, plus the panel's declared minSize when open.
	float floor = section->getHeaderHeight();
	if (section->isExpanded()) {
		if (auto desc = _registry->getPanelDescriptor(section->getPanelId())) {
			floor += desc->minSize.height;
		}
	}

	if (!section->isExpanded()) {
		// Just the header, whatever the policy.
		LayoutSystem::setItem(section,
				FlexItemInfo{
					.grow = 0.0f,
					.shrink = 0.0f,
					.basis = FlexItemInfo::FitContent,
					.minMain = floor,
				});
		return;
	}

	// The section's override or the view's policy; read only here.
	const auto sizing = getSectionSizing(section->getPanelId());

	/* The body and the panel follow the same policy. Measurement resolves a definite basis without
	`grow`, so under `Fit` the body and panel need `FitContent` to be measured at all - but only when
	the panel can measure itself (MeasureComponent, HandleMeasure system): otherwise measureNode
	returns last frame's size and the section would never shrink, so it stays at its floor.
	Rewritten on every policy change; this overrides what AccordionSection::setExpanded wrote. */
	if (auto body = section->getBody()) {
		auto content = body->getChildren().empty() ? nullptr : body->getChildren().front();
		const bool measurable = sizing == AccordionSizing::Fit && content
				&& LayoutSystem::canMeasure(content);

		LayoutSystem::setItem(body,
				FlexItemInfo{
					.grow = 1.0f,
					.shrink = 1.0f,
					.basis = measurable ? FlexItemInfo::FitContent : 0.0f,
					.order = 1,
				});

		if (content) {
			LayoutSystem::setItem(content,
					FlexItemInfo{
						.grow = 1.0f,
						.shrink = 1.0f,
						.basis = measurable ? FlexItemInfo::FitContent : 0.0f,
					});
		}
	}

	switch (sizing) {
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
		// The open sections share what the collapsed headers left; `basis = 0` makes it even.
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

	// Only an open section holds its panel; acquiring here keeps the builder lazy.
	Node *content = section->isExpanded() ? _registry->acquireContent(section->getPanelId(), this)
										  : nullptr;

	// Take out whatever else is in there, without cleanup: the node stays alive in the registry,
	// and Node::cleanup() would destroy its systems, which handleEnter reads on re-entry.
	auto children = body->getChildren();
	for (auto &it : Vector<Rc<Node>>(children.begin(), children.end())) {
		if (it.get() != content) {
			it->removeFromParent(false);
		}
	}

	if (content && content->getParent() != body) {
		content->removeFromParent(false);
		body->addChild(content);
		// Initial item only; updateSectionFlex rewrites it on every policy change.
		LayoutSystem::setItem(content,
				FlexItemInfo{
					.grow = 1.0f,
					.shrink = 1.0f,
					.basis = getSectionSizing(section->getPanelId()) == AccordionSizing::Fit
							? FlexItemInfo::FitContent
							: 0.0f,
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

	// Midpoint comparison, top-down: Y points up, so the first section has the highest y.
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
		// the boundary above that section
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
	// The node flag and the component have to change together.
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

	// Dragging the only section within its own stack is a no-op.
	if (payload->host == this && _order.size() == 1) {
		return DragResponse();
	}

	// Pure: runs during hit testing. Panels are only moved, never copied.
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
	// Out of the flow, or the viewport's flex column would lay it out as a section.
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

	// Read everything before mutating: a drop from this stack destroys the header that sent it.
	const auto panelId = payload->panelId;
	const bool fromHere = (payload->host == this);
	const size_t index = getDropIndexAt(_viewport->convertToNodeSpace(event.worldLocation));

	if (!_registry->getPanelDescriptor(panelId)) {
		return false;
	}

	clearDropIndicator();

	if (fromHere) {
		// A reorder. The index includes this section, so one past its position shifts down by one.
		const auto from = getSectionIndex(panelId);
		size_t to = index;
		if (from != maxOf<size_t>() && to > from) {
			--to;
		}
		return movePanel(panelId, to);
	}

	// From another host: the registry evicts it on the acquire in updateSectionContent.
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

	// Build the candidate order first, dropping unknown and duplicate panels, then apply it.
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
