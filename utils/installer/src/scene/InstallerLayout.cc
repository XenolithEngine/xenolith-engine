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
#include "XLUiButton.h"
#include "XLUiStyleResolver.h"
#include "XL2dLabel.h"
#include "XL2dLayer.h"
#include "XL2dIconSprite.h"
#include "XL2dScrollController.h"
#include "XLInputListener.h"
#include "XLNode.h"
#include "XLScheduler.h"
#include "XLDirector.h"
#include "XLAppThread.h"

#include "SPITriple.h"
#include "SPICatalogue.h"

#include "SPLog.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

static constexpr const char *kLogTag = "installer-ui";

namespace {
constexpr Color4F kAccent(0.988f, 0.706f, 0.0f, 1.0f); // #FCB400
constexpr Color4F kSecondary(0.72f, 0.72f, 0.72f, 1.0f); // #B8B8B8
constexpr Color4F kCheckboxOff(0.16f, 0.16f, 0.16f, 1.0f); // #292929 — OPAQUE (alpha 1 keeps opacity 1)

// A working checkbox: a plain square Layer (NOT Panel/LayerRounded — those route background-color
// through a type applier that never fired, leaving the box white) with an OPAQUE fill set directly
// via setColor. Opaque colour => alpha 1 => the setColor(withOpacity) cascade keeps opacity 1, so
// the check icon child stays visible. Tap toggles fill colour + check icon. No CSS dependency.
Rc<basic2d::Layer> makeCheckbox() {
	auto box = Rc<basic2d::Layer>::create(kCheckboxOff);
	box->setType("layer");
	box->addStyleClass("c-checkbox-box");
	box->setContentSize(Size2(17.0f, 17.0f));
	box->setAnchorPoint(Anchor::Middle);

	auto check = box->addChild(Rc<basic2d::IconSprite>::create(), ZOrder(1));
	check->setType("icon");
	check->setIconName(basic2d::IconName::Navigation_check_solid);
	check->setColor(Color4F(0.10f, 0.10f, 0.10f, 1.0f)); // dark check on the accent fill
	check->setContentSize(Size2(14.0f, 14.0f));
	check->setAnchorPoint(Anchor::Middle);
	check->setPosition(Vec2(8.5f, 8.5f));
	check->setVisible(false);

	auto listener = box->addSystem(Rc<InputListener>::create());
	listener->addTapRecognizer(
			[box, check](const GestureTap &tap) {
				if (tap.event != GestureEvent::Activated) { return true; }
				bool on = !check->isVisible();
				check->setVisible(on);
				box->setColor(on ? kAccent : kCheckboxOff, true);
				return true;
			},
			InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	return box;
}

// Trash button for installed components (the Rust shell's `.trash` — calls uninstall). Geometry
// MIRRORS makeCheckbox exactly: a 17x17 hit area, anchor Middle, so it lands on the SAME spot as
// the checkbox in the not-installed rows. The trash glyph itself is drawn larger (20x20) than the
// checkbox's check mark so it reads at a glance. Opaque colour (alpha 1) keeps the icon's opacity
// cascade at 1.
Rc<Node> makeTrashButton() {
	auto wrap = Rc<Node>::create();
	wrap->setContentSize(Size2(17.0f, 17.0f));
	wrap->setAnchorPoint(Anchor::Middle); // same anchor as the checkbox box → identical placement

	auto icon = wrap->addChild(Rc<basic2d::IconSprite>::create(), ZOrder(1));
	icon->setType("icon");
	icon->addStyleClass("c-trash");
	icon->setIconName(basic2d::IconName::Action_delete_solid);
	icon->setColor(kSecondary);
	icon->setContentSize(Size2(20.0f, 20.0f)); // bigger than the 14px check mark
	icon->setAnchorPoint(Anchor::Middle);
	icon->setPosition(Vec2(8.5f, 8.5f)); // centre of the 17x17 box

	auto listener = wrap->addSystem(Rc<InputListener>::create());
	listener->addTapRecognizer(
			[](const GestureTap &tap) {
				if (tap.event != GestureEvent::Activated) { return true; }
				log::info(kLogTag, "trash tapped — uninstall requested");
				// TODO: wire to InstallerController::uninstallComponent once actions are wired
				return true;
			},
			InputTapInfo{makeButtonMask({InputMouseButton::Touch, InputMouseButton::MouseLeft}), 1});

	return wrap;
}

// IMPORTANT: row/section containers MUST be plain Node, not Layer(transparent).
// Node::setColor(color, /*withOpacity*/true) — which Layer::create(transparent) ends up calling —
// writes the colour's alpha into the node opacity and CASCADES it to children. A transparent fill
// therefore flips the whole subtree to opacity 0 and blanks it (the "central area is dark" bug).
// A plain Node has opacity 1, renders no fill, and hides nothing. (A background fill, when wanted,
// must use an opaque colour — like .pkg-header does — so the alpha resets the cascade to 1.)
Rc<Node> makePkgRow(StringView name, StringView sizeText, StringView statusVariant,
		StringView statusText, bool isHead, bool isNative) {
	auto row = Rc<Node>::create();
	row->setType("node");
	row->addStyleClass("pkg-row");
	if (isHead) { row->addStyleClass("head"); }
	if (isNative) { row->addStyleClass("native"); }

	// c-check slot mirrors the Rust shell: an INSTALLED component shows a trash button (uninstall),
	// a not-installed component shows a checkbox (select to install). The header row shows nothing.
	auto checkSlot = row->addChild(Rc<Node>::create());
	checkSlot->addStyleClass("c-check");
	if (!isHead) {
		const bool installed = (statusVariant == "installed");
		if (installed) {
			auto trash = checkSlot->addChild(makeTrashButton());
			trash->setPosition(Vec2(8.0f, 0.0f)); // centre within the slot
		} else {
			auto cb = checkSlot->addChild(makeCheckbox());
			cb->setPosition(Vec2(8.0f, 0.0f)); // centre within the slot
		}
	}

	auto nameL = row->addChild(Rc<basic2d::Label>::create());
	nameL->setType("label");
	nameL->addStyleClass("c-name");
	nameL->setString(name);
	nameL->setColor(isHead ? kSecondary : (isNative ? kAccent : Color::White));

	auto sizeL = row->addChild(Rc<basic2d::Label>::create());
	sizeL->setType("label");
	sizeL->addStyleClass("c-size");
	sizeL->setAlignment(font::TextAlign::Right);
	sizeL->setString(sizeText);
	sizeL->setColor(kSecondary);

	// c-status: plain Label, NOT a Badge. Badge is a Panel (LayerRounded / VectorImage); the scroll
	// recreates every row as it virtualizes in/out of view, and recreating LayerRounded objects per
	// row was the source of the multi-GB leak while scrolling. A Label is cheap and leak-free.
	const bool installed = (statusVariant == "installed");
	auto statusL = row->addChild(Rc<basic2d::Label>::create());
	statusL->setType("label");
	statusL->addStyleClass("c-status");
	statusL->setAlignment(font::TextAlign::Right);
	statusL->setString(statusText);
	statusL->setColor(isHead ? kSecondary : (installed ? kAccent : kSecondary));

	// NOTE: row separators are not added here. CSS `border-bottom` is unsupported by this engine's
	// CSS subset, and a separator Layer as a flex child would either take flex space (shifting the
	// columns) or need -xl-position hacks. They'll come back as a dedicated pass once the columns
	// are stable.

	return row;
}

Rc<Node> makeGroupRow(StringView title) {
	auto g = Rc<Node>::create();
	g->setType("node");
	g->addStyleClass("pkg-group");
	auto lbl = g->addChild(Rc<basic2d::Label>::create());
	lbl->setType("label");
	lbl->setString(title);
	lbl->setColor(Color::White);
	return g;
}
} // namespace

InstallerLayout::~InstallerLayout() { }

bool InstallerLayout::init() {
	if (!basic2d::SceneLayout2d::init()) {
		return false;
	}

	setName("installer-layout");
	addSystem(Rc<ui::StyleResolver>::create(true));

	_titleBar = addChild(Rc<TitleBar>::create());
	_titleBar->setName("title-bar");

	_packagesArea = addChild(Rc<basic2d::Layer>::create(Color4F(0.02f, 0.03f, 0.03f, 1.0f)));
	_packagesArea->setName("packages-area");
	_packagesArea->addStyleClass("xl-ui-flex");

	{
		_header = _packagesArea->addChild(Rc<basic2d::Layer>::create(Color4F(0.06f, 0.07f, 0.05f, 1.0f)));
		_header->setName("pkg-header");
		_header->addStyleClass("pkg-header");

		// Title on the left (the Rust shell's <h1>Xenolith Installer</h1>).
		_titleLabel = _header->addChild(Rc<basic2d::Label>::create());
		_titleLabel->setType("label");
		_titleLabel->addStyleClass("pkg-title");
		_titleLabel->setString("Xenolith Installer");
		_titleLabel->setColor(Color::White);

		// A grow-only spacer pushes the meta labels to the right edge of the flex row.
		auto spacer = _header->addChild(Rc<Node>::create());
		spacer->addStyleClass("pkg-spacer");

		// Meta labels get their REAL text right here at init — synchronously, from the host triple
		// and the default release (no FTP wait). This is the CRITICAL fix: a label whose text is set
		// once at init and never changed renders perfectly, but a setString AFTER the first layout
		// (e.g. in onCatalogReady) corrupts the render (overlapping/overflowing glyphs) due to an
		// engine bug. nativeId/release are local data (the running machine + the build's release
		// channel), so we do not need the catalogue for them.
		auto host = resolve_host(native_arch(), native_os());

		_releaseLabel = _header->addChild(Rc<basic2d::Label>::create());
		_releaseLabel->setType("label");
		_releaseLabel->addStyleClass("pkg-release");
		_releaseLabel->setString(toString(default_release()));
		_releaseLabel->setColor(kSecondary);

		_nativeLabel = _header->addChild(Rc<basic2d::Label>::create());
		_nativeLabel->setType("label");
		_nativeLabel->addStyleClass("pkg-native");
		_nativeLabel->setString(host.native);
		_nativeLabel->setColor(kAccent);
	}

	{
		_scroll =
				_packagesArea->addChild(Rc<basic2d::ScrollView>::create(basic2d::ScrollView::Vertical));
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
		_footer = _packagesArea->addChild(Rc<basic2d::Layer>::create(Color4F(0.07f, 0.08f, 0.09f, 1.0f)));
		_footer->setName("pkg-footer");
		_footer->addStyleClass("pkg-footer");

		auto btn = _footer->addChild(Rc<ui::Button>::create([] {}));
		btn->setString("⚡ Install everything");
		btn->addStyleClass("primary");
	}

	return true;
}

void InstallerLayout::handleContentSizeDirty() {
	basic2d::SceneLayout2d::handleContentSizeDirty();
	// Header / scroll / footer are laid out by CSS flex (#packages-area is display:flex column;
	// .pkg-scroll is flex:1). Do NOT setContentSize/setPosition them here — that fights the
	// layout pass. The loading overlay is owned by InstallerSceneContent (this engine's
	// position:absolute does not take a node out of the flex flow, so it cannot live in here).
}

void InstallerLayout::onCatalogReady(InstallerController *controller) {
	_controller = controller;
	if (!controller || !controller->catalog()) {
		return;
	}
	// NOTE: the header meta labels (release/host) are set ONCE at init from synchronous host data —
	// do NOT setString them here. A setString after the first layout triggers an engine render bug
	// (overlapping/overflowing glyphs). The catalogue gives the same release/native values anyway.
	log::info(kLogTag, "onCatalogReady: rebuilding packages table, rows=", controller->catalog()->rows.size());
	rebuildPackages();

	// PRE-WARM the scroll: keepNodes makes the virtualizer build each row lazily as it scrolls into
	// view, so the FIRST scroll would freeze while ~20 rows are built, text-shaped and icon-tessellated
	// on the main thread. Force-build every row NOW (hidden under the loading overlay) by widening the
	// virtual window to cover the entire content via a huge animation padding; dropScrollWarmup()
	// (called when the overlay hides) restores the real window so off-screen rows just hide, not die.
	if (_scrollController) {
		_scrollController->setAnimationPadding(8000.0f);
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

bool InstallerLayout::labelsReady() const {
	// The meta labels start empty (width 0); the engine shapes them on visit. Ready once native has
	// a real width — it is the longer of the two, and both are set together.
	return _nativeLabel && _nativeLabel->getContentSize().width > 0.0f;
}

Rc<Node> InstallerLayout::makeRow(StringView name, StringView sizeText, StringView statusVariant,
		StringView statusText, bool isHead, bool isNative) {
	return makePkgRow(name, sizeText, statusVariant, statusText, isHead, isNative);
}

void InstallerLayout::rebuildPackages() {
	if (!_scrollController || !_controller || !_controller->catalog()) {
		return;
	}
	const auto *cat = _controller->catalog();

	_scrollController->clear();
	_scrollController->addPlaceholder(8.0f);

	_scrollController->addItem(
			[](const basic2d::ScrollController::Item &) -> Rc<Node> {
				return makePkgRow("Name", "Size", "", "Status", true, false);
			},
			36.0f);

	auto addComponentRow = [](const CatalogRow &r) -> basic2d::ScrollController::NodeFunction {
		String name = r.variant.empty() ? r.triple : (r.triple + " +" + r.variant);
		char sizeBuf[32];
		snprintf(sizeBuf, sizeof(sizeBuf), "%llu MB", (unsigned long long)(r.size / 1000000));
		String sizeText = toString(sizeBuf);
		String variant = (r.status == RowStatus::Installed) ? "installed" : "not-installed";
		String text = (r.status == RowStatus::Installed) ? "Installed" : "Not Installed";
		bool native = r.isNative;
		return [name, sizeText, variant, text, native](
					   const basic2d::ScrollController::Item &) -> Rc<Node> {
			return makePkgRow(name, sizeText, variant, text, false, native);
		};
	};

	constexpr float kRowH = 44.0f;

	_scrollController->addItem(
			[](const basic2d::ScrollController::Item &) -> Rc<Node> {
				return makeGroupRow("Development Tools (hosts)");
			},
			40.0f);
	for (const auto &r : cat->rows) {
		if (r.kind == Kind::Host) { _scrollController->addItem(addComponentRow(r), kRowH); }
	}

	_scrollController->addItem(
			[](const basic2d::ScrollController::Item &) -> Rc<Node> {
				return makeGroupRow("Runtime Platforms (targets)");
			},
			40.0f);
	for (const auto &r : cat->rows) {
		if (r.kind == Kind::Target) { _scrollController->addItem(addComponentRow(r), kRowH); }
	}

	_scrollController->addPlaceholder(16.0f);
	_scrollController->commitChanges();
}

} // namespace stappler::xenolith::installer
