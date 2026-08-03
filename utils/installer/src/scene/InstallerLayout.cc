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

#include "InstallerLayout.h"
#include "InstallerTitleBar.h"

#include "XLUiBadge.h"
#include "XLUiButton.h"
#include "XLUiCheckbox.h"
#include "XLUiStyleResolver.h"
#include "XL2dIcons.h"
#include "XL2dScrollController.h"

#include "SPICatalogue.h"
#include "SPITriple.h"
#include "SPLog.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

static constexpr auto kLogTag = StringView("installer-ui");

// The virtualizer needs a row's height before the row node exists to measure, so these mirror
// --row-h / --row-head-h / --group-h in resources/style.css.
static constexpr float kRowHeight = 44.0f;
static constexpr float kHeadRowHeight = 36.0f;
static constexpr float kGroupRowHeight = 40.0f;

namespace {

// One table row. Text and identity only: colours, sizes, spacing and alignment all come from
// resources/style.css.
struct RowSpec {
	StringView name;
	StringView size;
	StringView status; // badge text on a component row, column caption on the head row
	bool head = false; // the column-caption row
	bool native = false; // the component matching the running machine's host triple
	bool installed = false;
};

// IMPORTANT: row/section containers MUST be plain Node, not Layer(transparent).
// Node::setColor(color, /*withOpacity*/true) — which Layer::create(transparent) ends up calling —
// writes the colour's alpha into the node opacity and CASCADES it to children. A transparent fill
// therefore flips the whole subtree to opacity 0 and blanks it. A plain Node has opacity 1, renders
// no fill, and hides nothing. (A background fill, when wanted, must use an opaque colour.)
Rc<Node> makePkgRow(const RowSpec &spec) {
	auto row = Rc<Node>::create();
	row->setType("node");
	row->addStyleClass("pkg-row");
	if (spec.head) {
		row->addStyleClass("head");
	}
	if (spec.native) {
		row->addStyleClass("native");
	}

	// The check slot mirrors the Rust shell: an installed component offers a trash button
	// (uninstall), a not-installed one a checkbox (select to install). The caption row shows
	// neither, but keeps the slot so the columns line up. `.c-check` is a centring flex container,
	// so neither widget needs a position of its own.
	auto checkSlot = row->addChild(Rc<Node>::create());
	checkSlot->addStyleClass("c-check");
	if (!spec.head) {
		if (spec.installed) {
			auto trash = checkSlot->addChild(Rc<ui::Button>::create([] {
				// TODO: call InstallerController::uninstallComponent once the row knows its id
				log::info(kLogTag, "trash tapped - uninstall requested");
			}));
			trash->addStyleClass("c-trash");
			trash->setIcon(basic2d::IconName::Action_delete_solid);
		} else {
			checkSlot->addChild(Rc<ui::Checkbox>::create());
		}
	}

	auto nameLabel = row->addChild(Rc<basic2d::Label>::create());
	nameLabel->setType("label");
	nameLabel->addStyleClass("c-name");
	nameLabel->setString(spec.name);

	auto sizeLabel = row->addChild(Rc<basic2d::Label>::create());
	sizeLabel->setType("label");
	sizeLabel->addStyleClass("c-size");
	sizeLabel->setAlignment(font::TextAlign::Right);
	sizeLabel->setString(spec.size);

	auto statusSlot = row->addChild(Rc<Node>::create());
	statusSlot->addStyleClass("c-status");
	if (spec.head) {
		// the caption row wants plain text, not a pill
		auto caption = statusSlot->addChild(Rc<basic2d::Label>::create());
		caption->setType("label");
		caption->setString(spec.status);
	} else {
		auto badge = statusSlot->addChild(Rc<ui::Badge>::create());
		badge->setText(spec.status);
		badge->setVariant(spec.installed ? "installed" : "not-installed");
	}

	return row;
}

Rc<Node> makeGroupRow(StringView title) {
	auto group = Rc<Node>::create();
	group->setType("node");
	group->addStyleClass("pkg-group");

	auto label = group->addChild(Rc<basic2d::Label>::create());
	label->setType("label");
	label->setString(title);
	return group;
}

// Captures the row's text by value: the virtualizer may build (and rebuild) the node long after the
// catalogue vector it came from has been replaced.
basic2d::ScrollController::NodeFunction makeComponentRow(const CatalogRow &row) {
	const bool installed = (row.status == RowStatus::Installed);
	auto name = row.variant.empty() ? row.triple : toString(row.triple, " +", row.variant);
	auto size = toString(row.size / 1'000'000, " MB");

	return [name = sp::move(name), size = sp::move(size), installed, native = row.isNative](
				   const basic2d::ScrollController::Item &) -> Rc<Node> {
		return makePkgRow(RowSpec{
			.name = name,
			.size = size,
			.status = installed ? StringView("Installed") : StringView("Not Installed"),
			.native = native,
			.installed = installed,
		});
	};
}

} // namespace

InstallerLayout::~InstallerLayout() { }

bool InstallerLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	setName("installer-layout");

	// One recursive resolver for the whole screen: every node below — including the rows the scroll
	// virtualizer builds later — is styled from resources/style.css.
	addSystem(Rc<ui::StyleResolver>::create(true));

	_titleBar = addChild(Rc<TitleBar>::create());

	// Every Layer here is created black on purpose: CSS owns the paint (`background-color`), and the
	// style pass runs before the first frame is drawn. The placeholder must be OPAQUE — Layer maps
	// its colour's alpha onto the node opacity and cascades it into the subtree.
	_packagesArea = addChild(Rc<basic2d::Layer>::create(Color::Black));
	_packagesArea->setName("packages-area");

	{
		_header = _packagesArea->addChild(Rc<basic2d::Layer>::create(Color::Black));
		_header->setName("pkg-header");
		_header->addStyleClass("pkg-header");

		auto title = _header->addChild(Rc<basic2d::Label>::create());
		title->setType("label");
		title->addStyleClass("pkg-title");
		title->setString("Xenolith Installer");

		// A grow-only spacer pushes the meta labels to the right edge of the flex row.
		auto spacer = _header->addChild(Rc<Node>::create());
		spacer->addStyleClass("pkg-spacer");

		// Seed the meta labels from local data (the running machine's triple and the build's
		// release channel) so the header is never blank while the catalogue is in flight;
		// onCatalogReady overwrites them with the catalogue's own values.
		auto host = resolveHost(getNativeArch(), getNativeOs());

		_releaseLabel = _header->addChild(Rc<basic2d::Label>::create());
		_releaseLabel->setType("label");
		_releaseLabel->addStyleClass("pkg-release");
		_releaseLabel->setString(toString(getDefaultRelease()));

		_nativeLabel = _header->addChild(Rc<basic2d::Label>::create());
		_nativeLabel->setType("label");
		_nativeLabel->addStyleClass("pkg-native");
		_nativeLabel->setString(host.native);
	}

	{
		_scroll = _packagesArea->addChild(
				Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical));
		_scroll->setName("pkg-scroll");
		_scroll->addStyleClass("pkg-scroll");

		_scrollController = Rc<basic2d::ScrollController>::create();
		// KeepNodes: rows are created ONCE and reused (hidden when out of view), never destroyed and
		// recreated. Without this the virtualizer thrashes every frame — a row's labels finish shaping
		// AFTER it is created, which changes the row's content size, which re-triggers virtualization,
		// which destroys and recreates the rows, which shape again… an unbounded create/destroy loop
		// that leaked gigabytes while scrolling (and even while idle). The catalogue is tiny (~33
		// items), so keeping every node live is far cheaper than the destroy/recreate churn.
		_scrollController->setKeepNodes(true);
		_scroll->setController(_scrollController);
		_scroll->setIndicatorColor(Color4B(255, 255, 255, 77));
	}

	{
		_footer = _packagesArea->addChild(Rc<basic2d::Layer>::create(Color::Black));
		_footer->setName("pkg-footer");
		_footer->addStyleClass("pkg-footer");

		auto install = _footer->addChild(Rc<ui::Button>::create([] {
			// TODO: call InstallerController::installForSystem once the progress UI exists
		}));
		install->addStyleClass("primary");
		install->setString("⚡ Install everything");
	}

	// Header / scroll / footer are laid out by CSS flex (#packages-area is display:flex column,
	// .pkg-scroll is flex:1), so this class overrides no geometry hook — a setContentSize or
	// setPosition here would fight the layout pass. The loading overlay is owned by
	// InstallerSceneContent: this engine's position:absolute does not take a node out of the flex
	// flow, so it cannot live in here.
	return true;
}

void InstallerLayout::onCatalogReady(InstallerController *controller) {
	_controller = controller;
	if (!controller || !controller->catalog()) {
		return;
	}

	const auto *cat = controller->catalog();
	log::info(kLogTag, "onCatalogReady: rebuilding packages table, rows=", cat->rows.size());

	// The catalogue is authoritative for the header meta, so refresh what init() seeded. Re-shaping
	// a label after the first layout now re-measures its fit-content ancestors correctly
	// (XL_LABEL_UPDATE_TEST); it used to corrupt the header, which is why this was once set once.
	_releaseLabel->setString(cat->release);
	if (!cat->nativeId.empty()) {
		_nativeLabel->setString(cat->nativeId);
	}

	rebuildPackages();

	// PRE-WARM the scroll: keepNodes makes the virtualizer build each row lazily as it scrolls into
	// view, so the FIRST scroll would freeze while ~20 rows are built, text-shaped and icon-tessellated
	// on the main thread. Force-build every row NOW (hidden under the loading overlay) by widening the
	// virtual window to cover the entire content via a huge animation padding; dropScrollWarmup()
	// (called when the overlay hides) restores the real window so off-screen rows just hide, not die.
	if (_scrollController) {
		_scrollController->setAnimationPadding(8'000.0f);
		_scrollController->commitChanges();
	}
}

void InstallerLayout::dropScrollWarmup() {
	// Restore the real virtual window once the rows have been built and rendered under the overlay.
	if (_scrollController) {
		_scrollController->dropAnimationPadding();
		_scrollController->commitChanges();
	}
}

void InstallerLayout::rebuildPackages() {
	if (!_scrollController || !_controller || !_controller->catalog()) {
		return;
	}
	const auto *cat = _controller->catalog();

	_scrollController->clear();
	_scrollController->addPlaceholder(8.0f);

	_scrollController->addItem([](const basic2d::ScrollController::Item &) -> Rc<Node> {
		return makePkgRow(
				RowSpec{.name = "Name", .size = "Size", .status = "Status", .head = true});
	}, kHeadRowHeight);

	auto addRows = [&](Kind kind) {
		for (const auto &row : cat->rows) {
			if (row.kind == kind) {
				_scrollController->addItem(makeComponentRow(row), kRowHeight);
			}
		}
	};

	_scrollController->addItem([](const basic2d::ScrollController::Item &) -> Rc<Node> {
		return makeGroupRow("Development Tools (hosts)");
	}, kGroupRowHeight);
	addRows(Kind::Host);

	_scrollController->addItem([](const basic2d::ScrollController::Item &) -> Rc<Node> {
		return makeGroupRow("Runtime Platforms (targets)");
	}, kGroupRowHeight);
	addRows(Kind::Target);

	_scrollController->addPlaceholder(16.0f);
	_scrollController->commitChanges();
}

} // namespace stappler::xenolith::installer
