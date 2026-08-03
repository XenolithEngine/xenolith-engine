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

#include "InstallerSceneContent.h"
#include "InstallerLayout.h"

#include "XLUiStyleResolver.h"
#include "XLWindowDecorations.h"
#include "XLAction.h"
#include "XLAppThread.h"
#include "XLDirector.h"
#include "XLScheduler.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Height of the title bar the overlay must not cover. Mirrors --title-bar-h in
// resources/style.css; the overlay is positioned here because it lives outside the flex tree.
static constexpr float kTitleBarHeight = 32.0f;

// How long to keep the overlay up after the catalogue resolves, so the pre-warmed rows get a frame
// to render before the user sees them.
static constexpr auto kOverlayLingerMs = 300;

InstallerSceneContent::~InstallerSceneContent() { }

bool InstallerSceneContent::init() {
	if (!basic2d::SceneContent2d::init()) {
		return false;
	}

	setWindowDecorationsContructor([](NotNull<SceneContent>) -> Rc<WindowDecorations> {
		return Rc<WindowDecorations>::create();
	});

	_rootStyle = addSystem(
			Rc<ui::StyleSystem>::create(FileInfo{"resources/style.css", FileCategory::Bundled}));

	_globalBackground =
			addChild(Rc<basic2d::Layer>::create(Color::Black), ZOrder::min() + ZOrder(2));
	_globalBackground->setName("global-background");
	_globalBackground->addSystem(Rc<ui::StyleResolver>::create());

	_layout = Rc<InstallerLayout>::create();
	pushLayout(_layout);

	// Loading overlay. Three SIBLING nodes (not parent/child) so the render order is unambiguous:
	// the overlay is a semi-opaque dimming Layer over the CONTENT area only (it stops below the
	// title bar, so the traffic lights stay visible); the spinner + caption are added AFTER it
	// with a higher z and the Transparent rendering level, so they paint in the same pass as the
	// overlay but AFTER it (higher z-path) — they sit on top, fully visible, never dimmed/covered.
	// (Earlier the spinner was a CHILD of the overlay and vanished: the overlay's Transparent pass
	// painted over the Surface-level spinner. And the overlay covered the title bar because it was
	// full-height.)
	//
	// They are not part of the layout's flex tree, so each carries its own non-recursive resolver:
	// that is what makes the `.loading-*` rules in resources/style.css reach them.
	_loadingOverlay = addChild(Rc<basic2d::Layer>::create(Color::Black), ZOrder::max() - ZOrder(3));
	_loadingOverlay->setName("loading-overlay");
	_loadingOverlay->addStyleClass("loading-overlay");
	_loadingOverlay->setAnchorPoint(Anchor::Middle);
	_loadingOverlay->addSystem(Rc<ui::StyleResolver>::create());

	_spinner =
			addChild(Rc<basic2d::IconSprite>::create(basic2d::IconName::Navigation_refresh_solid),
					ZOrder::max() - ZOrder(1));
	_spinner->setType("icon");
	_spinner->setName("loading-spinner");
	_spinner->addStyleClass("loading-spinner");
	_spinner->setRenderingLevel(RenderingLevel::Transparent);
	_spinner->setContentSize(Size2(64.0f, 64.0f));
	_spinner->setAnchorPoint(Anchor::Middle);
	_spinner->addSystem(Rc<ui::StyleResolver>::create());

	_loadingLabel = addChild(Rc<basic2d::Label>::create(), ZOrder::max() - ZOrder(1));
	_loadingLabel->setType("label");
	_loadingLabel->setName("loading-label");
	_loadingLabel->addStyleClass("loading-label");
	_loadingLabel->setRenderingLevel(RenderingLevel::Transparent);
	_loadingLabel->setString("Loading catalogue…");
	_loadingLabel->setAnchorPoint(Anchor::Middle);
	_loadingLabel->addSystem(Rc<ui::StyleResolver>::create());

	return true;
}

void InstallerSceneContent::handleContentSizeDirty() {
	basic2d::SceneContent2d::handleContentSizeDirty();

	// Overlay covers the content area only — the title bar stays exposed. Spinner + caption are
	// centred in that same content area.
	const float contentHeight = _contentSize.height - kTitleBarHeight;
	const Vec2 centre(_contentSize.width / 2.0f, contentHeight / 2.0f);

	if (_loadingOverlay) {
		_loadingOverlay->setContentSize(Size2(_contentSize.width, contentHeight));
		_loadingOverlay->setPosition(centre);
	}
	if (_spinner) {
		_spinner->setPosition(Vec2(centre.x, centre.y + 24.0f));
	}
	if (_loadingLabel) {
		_loadingLabel->setPosition(Vec2(centre.x, centre.y - 48.0f));
	}
}

void InstallerSceneContent::handleEnter(Scene *scene) {
	basic2d::SceneContent2d::handleEnter(scene);

	// Spin the loading icon while it is visible. Two parts:
	//  - RenderContinuously action: the renderer otherwise only produces frames on demand (a dirty
	//    region / live input), so without this the spinner advances ONLY while you move the mouse.
	//    Run it on the spinner while it is shown, stop it once hidden to return to low-power
	//    rendering.
	//  - schedulePerFrame: the actual rotation (there is no RotateBy action). It lives on Scheduler,
	//    not Node; grab the node's scheduler and target the spinner.
	if (!_spinnerScheduled && _spinner && _spinner->getScheduler()) {
		_spinnerScheduled = true;
		_spinner->runAction(Rc<RenderContinuously>::create(), "RenderContinuously"_tag);
		_spinner->getScheduler()->schedulePerFrame([this](const UpdateTime &) {
			if (_spinner && _spinner->isVisible()) {
				_spinner->setRotation(_spinner->getRotation() + 0.25f);
			}
		}, _spinner, 0, false);
	}

	if (_controller) {
		return;
	}

	_controller = Rc<InstallerController>::create(getDirector()->getApplication());
	if (!_controller) {
		return;
	}

	_controller->loadCatalog([this](bool, String) {
		if (!_layout || !_controller) {
			return;
		}
		_layout->onCatalogReady(_controller);

		// Hide the overlay only once the catalogue has loaded and the table is built — NOT on a
		// fixed timer. The catalogue comes over FTP and can take many seconds; a timer would lift
		// the overlay onto an empty table. A short extra delay lets the pre-warmed rows render.
		if (auto looper = getDirector()->getApplication()->getLooper()) {
			looper->schedule(sprt::dispatch::TimeInterval::milliseconds(kOverlayLingerMs),
					[this](sprt::dispatch::Handle *, bool) { hideLoadingState(); }, this);
		}
	});
	_controller->queryEngine([](const EngineStatusInfo &) { });
}

void InstallerSceneContent::hideLoadingState() {
	if (_layout) {
		_layout->dropScrollWarmup();
	}
	if (_spinner) {
		_spinner->stopActionByTag("RenderContinuously"_tag);
		_spinner->setVisible(false);
	}
	if (_loadingOverlay) {
		_loadingOverlay->setVisible(false);
	}
	if (_loadingLabel) {
		_loadingLabel->setVisible(false);
	}
}

} // namespace stappler::xenolith::installer
