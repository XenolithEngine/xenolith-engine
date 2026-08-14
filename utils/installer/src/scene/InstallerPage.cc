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

#include "InstallerPage.h"
#include "InstallerActionCell.h"
#include "InstallerStrings.h"

#include "XLUiButton.h"
#include "XLUiBadge.h"
#include "XLUiLayoutSystem.h"
#include "XLEventListener.h"
#include "XL2dScrollController.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// --- InstallerPage ----------------------------------------------------------

InstallerPage::~InstallerPage() { }

bool InstallerPage::init(StringView title, StringView note) {
	if (!ui::Panel::init()) {
		return false;
	}

	setName("content-page");
	removeStyleClass("xl-ui-panel");
	registerStyleAppliers("content-page");
	setComponent<ui::SystemManagedLayout>();

	_scroll = addChild(Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical));
	_scroll->setName("page-scroll");
	_scroll->setAnchorPoint(Anchor::BottomLeft);
	_scroll->setPosition(Vec2::ZERO);

	// The controller is kept only for the scrollable extent: a page has a handful of long-lived
	// blocks rather than a virtualized list, so the content is one node inside the scroll's root.
	// The virtualization that matters happens one level down, inside the table's own rows.
	_scrollController = Rc<basic2d::ScrollController>::create();
	_scroll->setController(_scrollController);

	// The column itself is an ordinary flex container - `#page-body` in the stylesheet declares
	// `display: flex; flex-direction: column`, and the style resolver installs the LayoutSystem for
	// it. That is what puts the page's padding, its row-gap and the stacking order in CSS instead of
	// in this file, and it is also what measures the column: a flex container answers
	// LayoutSystem::measureNode with the height of its content, which is exactly the number the
	// outer ScrollView needs.
	_body = _scroll->getRoot()->addChild(Rc<Node>::create());
	_body->setName("page-body");
	_body->setAnchorPoint(Anchor::TopLeft);

	_title = static_cast<basic2d::Label *>(addBlock(Rc<basic2d::Label>::create()));
	_title->setType("label");
	_title->addStyleClass("page-title");
	_title->setString(title);

	_note = static_cast<basic2d::Label *>(addBlock(Rc<basic2d::Label>::create()));
	_note->setType("label");
	_note->addStyleClass("page-note");
	_note->setString(note);
	_note->setVisible(!note.empty());

	return true;
}

Node *InstallerPage::addBlock(Rc<Node> &&node) {
	// Distinct, increasing ZOrder: the flex column stacks in document order, and sortAllChildren is
	// an unstable sort - equal orders would leave the order of the blocks up to chance.
	auto ret = _body->addChild(sp::move(node), ZOrder(_blockCount++));
	ret->setAnchorPoint(Anchor::TopLeft);
	return ret;
}

void InstallerPage::handleContentSizeDirty() {
	ui::Panel::handleContentSizeDirty();

	if (_scroll) {
		_scroll->setAnchorPoint(Anchor::BottomLeft);
		_scroll->setPosition(Vec2::ZERO);
		_scroll->setContentSize(_contentSize);
	}
	updateScrollArea();
}

void InstallerPage::handleShown() {
	/* Re-establish the scroll geometry, unconditionally.

	A page is laid out while it is HIDDEN: they are all built at startup, and the data that fills
	them lands whenever the network answers. A hidden node is not visited, and a ScrollView settles
	its own bounds from a visit - the scroll root's POSITION is the scroll offset, and it is clamped
	against the scrollable extent there. So a page whose table grew while it was away comes back with
	an offset that was clamped against the extent it had before, which is what put the Engines page's
	heading at the bottom of the viewport with its table entirely out of view.

	Clearing the remembered extent is what forces it back onto the controller: updateScrollArea()
	skips the call when the number has not changed, and the number has not - what changed is that
	nobody applied it while the page was hidden. */
	_scrollHeight = nan();
	updateScrollArea();
}

void InstallerPage::updateScrollArea() {
	if (!_body || !_scroll || _contentSize.width <= 0.0f) {
		return;
	}

	const float width = _contentSize.width;

	/* Ask the column how tall it wants to be at this width, and give it exactly that.

	This is one call rather than a loop because every block answers for itself: a Label re-wraps to
	the width it is offered and reports the height of the wrapped text, and ui::TableView answers
	from its MODEL through getIntrinsicHeight() - without a single row node existing - so the column
	is right on the first frame instead of reflowing once the rows materialize. Padding and the gaps
	between blocks are the flex container's, hence the stylesheet's. */
	MeasureConstraints constraints;
	constraints.maxWidth = width;
	const float total = ui::LayoutSystem::measureNode(_body, constraints).height;

	/* The scroll root is deliberately ZERO-HIGH and pinned to the top of the viewport:
	ScrollViewBase::updateScrollBounds() re-imposes that on every content-size change and uses the
	root's Y purely as the scroll offset. So the content hangs BELOW the root's origin at negative
	Y, and the root's own size must not be touched - setting it is what left this page rendered
	above the viewport. The column's own anchor is what keeps it there: with TopLeft at the origin
	its top edge sits at the top of the viewport and it grows downwards. */
	_body->setAnchorPoint(Anchor::TopLeft);
	_body->setContentSize(Size2(width, total));
	_body->setPosition(Vec2::ZERO);

	if (_scrollHeight != total) {
		_scrollHeight = total;
		// How far the view may scroll. The controller is not building the content here - there are a
		// handful of long-lived blocks, not a virtualized list - so this is the only thing it is
		// told, and it is what gives the scrollbar its extent.
		_scrollController->setScrollableArea(0.0f, total);
	}
}

// --- InstallerToolsPage -----------------------------------------------------

InstallerToolsPage::~InstallerToolsPage() { }

bool InstallerToolsPage::init(PageId page) {
	StringView title, note;
	switch (page) {
	case PageId::Engines:
		title = strings::pageEnginesTitle();
		note = strings::pageEnginesNote();
		break;
	case PageId::Hosts:
		title = strings::pageHostsTitle();
		note = strings::pageHostsNote();
		break;
	default:
		title = strings::pageTargetsTitle();
		note = strings::pageTargetsNote();
		break;
	}

	if (!InstallerPage::init(title, note)) {
		return false;
	}

	_page = page;

	_table = static_cast<ui::TableView *>(addBlock(Rc<ui::TableView>::create()));
	_table->setName("tools-table");
	// The whole point of the embedding: report the model's height, stop scrolling, and let this
	// page's ScrollView carry the heading, the note and the table as one column.
	_table->setAutoHeight(true);
	_table->setIntrinsicHeightCallback([this](float) { updateScrollArea(); });
	_table->setColumns(Vector<ui::TableView::Column>{
		{String("name"), toString(strings::colName()), String("col-name"), ui::GridTrack()},
		{String("status"), toString(strings::colStatus()), String("col-status"), ui::GridTrack()},
		{String("actions"), toString(strings::colActions()), String("col-actions"), ui::GridTrack()},
	});
	_table->setCellCallback([this](ui::TableView::CellBuilder &b) { buildCell(b); });
	_table->setRowCallback([](ui::TableView::RowBuilder &b) {
		if (b.getData().getBool("native")) {
			b.addStyleClass("native");
		}
	});

	return true;
}

void InstallerToolsPage::handleEnter(Scene *scene) {
	InstallerPage::handleEnter(scene);

	reload();
	checkTools();

	auto listener = addSystem(Rc<EventListener>::create());
	/* Exactly the two events that change WHICH rows exist: what the mirror offers, and which engine
	refs there are. Nothing else rebuilds this table.

	Notably NOT onInstalledStateChanged. Installing or removing a component does not change the row
	set here - the same components are still on offer, one of them just reads differently - and
	reloading for it is what made the whole table flash on every status change. The rows carry that
	themselves now, through onRowStatusChanged; see InstallerActionCell and InstallerStatusCell. */
	listener->listenForEvent(AppController::onCatalogueChanged,
			[this](const Event &) { reload(); });
	listener->listenForEvent(AppController::onEngineRefsChanged,
			[this](const Event &) { reload(); });
}

void InstallerToolsPage::handleShown() {
	// setSource() would be a no-op here - it early-outs on the same Source - so the model is
	// re-derived explicitly. This is what a hidden page misses: its listener fires, but nothing
	// visits it to act on that.
	if (_table) {
		_table->invalidateSource();
	}
	// After the model, not before: the base re-applies the extent, and the extent is the table's.
	InstallerPage::handleShown();
	checkTools();
}

void InstallerToolsPage::checkTools() {
	/* Opening a page is what re-checks the tools it lists: the store can be changed by anything -
	another window, the CLI, a directory removed by hand - and what a page shows must be what is
	there now, not what was there when it was last built.

	The call returns immediately. The check itself runs on the worker pool and dirties the Source
	when it lands, so the page is on screen either way; the rows whose answer has not arrived say
	"Checking…" and offer no action until it does. Repeated calls collapse into one run - see
	AppController::checkComponents. */
	if (auto controller = AppController::getInstance()) {
		controller->checkComponents();
	}
}

void InstallerToolsPage::reload() {
	auto controller = AppController::getInstance();
	if (!controller || !_table) {
		return;
	}
	auto *source = _page == PageId::Engines
			? controller->getEnginesSource()
			: controller->getToolsSource(_page == PageId::Hosts ? Kind::Host : Kind::Target);

	// setSource() early-outs when the Source is the one already bound, which is the usual case
	// here: reload() is what the change events call, and the Source object never changes - only its
	// contents do. So the model is re-derived explicitly, or an install would finish with the table
	// still showing the row as it was.
	if (_table->getSource() == source) {
		_table->invalidateSource();
	} else {
		_table->setSource(source);
	}
	updateScrollArea();
}

void InstallerToolsPage::buildCell(ui::TableView::CellBuilder &builder) {
	if (builder.isHeader()) {
		return; // the column title is the default
	}

	const auto *row = builder.getRow();
	if (!row) {
		return;
	}
	const auto &data = row->getData();
	const auto key = builder.getColumn().key;

	if (key == "name") {
		auto name = toString(data.getString(_page == PageId::Engines ? "name" : "id"));
		if (const auto size = data.getInteger("size"); size > 0) {
			// design.md gives the table three columns, so the download size rides along with the
			// name rather than claiming one of its own.
			name += toString("   ", size / 1'000'000, " MB");
		}
		builder.setLabel(name);
		if (data.getBool("native")) {
			// The running machine's toolchain, which is also always the first data row.
			builder.setIcon(basic2d::IconName::Hardware_computer_solid);
		} else if (data.getBool("active")) {
			builder.setIcon(basic2d::IconName::Action_check_circle_solid);
		}
		return;
	}

	if (key == "status") {
		// A node of its own rather than a badge built here: it keeps up with its row's status
		// without the table being rebuilt around it (InstallerStatusCell).
		builder.setNode(Rc<InstallerStatusCell>::create(_page, data).get());
		return;
	}

	if (key == "actions") {
		builder.setNode(Rc<InstallerActionCell>::create(_page, data).get());
	}
}

// --- InstallerWelcomePage ---------------------------------------------------

InstallerWelcomePage::~InstallerWelcomePage() { }

bool InstallerWelcomePage::init() {
	if (!InstallerPage::init(strings::pageWelcomeTitle(), strings::pageWelcomeNote())) {
		return false;
	}

	_summary = static_cast<basic2d::Label *>(addBlock(Rc<basic2d::Label>::create()));
	_summary->setType("label");
	_summary->addStyleClass("page-summary");

	auto install = Rc<ui::Button>::create([] {
		if (auto controller = AppController::getInstance()) {
			controller->installForSystem();
		}
	});
	install->setString(strings::actionInstallEverything());
	install->addStyleClass("primary");
	auto node = addBlock(install.get());
	node->setName("welcome-install");

	return true;
}

void InstallerWelcomePage::handleEnter(Scene *scene) {
	InstallerPage::handleEnter(scene);

	refresh();

	auto listener = addSystem(Rc<EventListener>::create());
	listener->listenForEvent(AppController::onEngineStatusChanged,
			[this](const Event &) { refresh(); });
	listener->listenForEvent(AppController::onCatalogueChanged,
			[this](const Event &) { refresh(); });
}

void InstallerWelcomePage::refresh() {
	auto controller = AppController::getInstance();
	if (!controller || !_summary) {
		return;
	}

	StringStream out;
	out << strings::welcomeArch() << ": " << controller->getNativeId();
	if (controller->isNativeViaEmulation()) {
		out << " (via emulation)";
	}
	out << "\n" << strings::welcomeEngine() << ": ";
	const auto &engine = controller->getEngineStatus();
	out << (engine.ready ? engine.reference : toString(strings::welcomeEngineMissing()));
	if (const auto *cat = controller->getCatalogue()) {
		out << "\n" << strings::welcomeRelease() << ": " << cat->release;
	}
	_summary->setString(out.str());
	updateScrollArea();
}

} // namespace stappler::xenolith::installer
