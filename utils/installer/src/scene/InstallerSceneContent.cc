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
#include "InstallerDialogs.h"
#include "InstallerStrings.h"

#include "XLAppWindow.h"
#include "XLSceneInspector.h"
#include "XLUiStyleResolver.h"
#include "XLWindowDecorations.h"
#include "XLAction.h"
#include "XLAppThread.h"
#include "XLDirector.h"
#include "XLScheduler.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

// Height of the title bar the overlay must not cover. Mirrors --title-bar-h in
// resources/style.css; the overlay is positioned here because it lives outside the flex tree.
static constexpr float kTitleBarHeight = 32.0f;

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
	//  - RenderContinuously on THIS content (not only the spinner): schedulePerFrame and
	//    Node rotation only advance when frames are produced; on-demand rendering otherwise
	//    freezes the spinner until the mouse moves over the window.
	//  - schedulePerFrame: the actual rotation (there is no RotateBy action).
	if (!_spinnerScheduled && _spinner && _spinner->getScheduler()) {
		_spinnerScheduled = true;
		runAction(Rc<RenderContinuously>::create(), "LoadingRender"_tag);
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

	_controller->loadCatalog([this](bool ok, String) {
		if (!_layout || !_controller) {
			return;
		}
		_layout->onCatalogReady(_controller);

		// Hide loading BEFORE any confirm/onboarding. Dialogs used to snapshot+restore the
		// loading chrome on dismiss, which left "Loading catalogue…" stuck forever after the
		// first onboard dismiss — and without RenderContinuously the restored spinner froze.
		hideLoadingState();

		// Onboarding confirm is opt-in via inspector for now — opening it immediately after
		// catalogue rebuild has been crashing the headless process (and used to re-show the
		// stuck loader). Packages UI must be usable first.
		(void)ok;
	});
	_controller->queryEngine([this](const EngineStatusInfo &info) {
		if (_layout) {
			_layout->setEngineStatus(info);
		}
	});

	// Inspector: open a confirm dialog without a pointer click. Both tones are exposed — the
	// primary one is what looked empty on first open when StyleResolver painted labels black.
	auto addConfirmCommand = [this](StringView name, StringView help, ConfirmTone tone) {
		inspector::addCommand(this, name, help,
				[this, tone](Value &&, Function<void(Value &&)> &&done) {
			auto *window = static_cast<AppWindow *>(getDirector()->getRenderServer());
			if (window) {
				if (tone == ConfirmTone::Danger) {
					showConfirmDialog(window, strings::confirmDeleteTitle(),
							strings::confirmDeleteMessage("aarch64-apple-macosx"),
							strings::actionDelete(), tone, [] { });
				} else {
					showConfirmDialog(window, strings::confirmInstallTitle(),
							strings::confirmInstallMessage(), strings::actionInstall(), tone,
							[] { });
				}
			}
			Value r;
			r.setBool(window != nullptr, "opened");
			done(sp::move(r));
		});
	};
	addConfirmCommand("open-confirm", "Open Delete confirm dialog (danger)", ConfirmTone::Danger);
	addConfirmCommand("open-confirm-install", "Open Install confirm dialog (primary)",
			ConfirmTone::Primary);
}

void InstallerSceneContent::hideLoadingState() {
	stopActionByTag("LoadingRender"_tag);
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

void InstallerSceneContent::presentOverlay(basic2d::SceneLayout2d *overlay) {
	if (_modalOverlay) {
		popOverlay(_modalOverlay);
		_modalOverlay = nullptr;
		stopActionByTag("OverlayRender"_tag);
	}
	_modalOverlay = overlay;
	if (overlay) {
		pushOverlay(overlay);
		// A dialog has hover states and a relayout pass; on-demand rendering would otherwise
		// leave it half-painted until the mouse moves.
		runAction(Rc<RenderContinuously>::create(), "OverlayRender"_tag);
	}
}

void InstallerSceneContent::dismissOverlay(basic2d::SceneLayout2d *overlay) {
	if (overlay && _modalOverlay == overlay) {
		presentOverlay(nullptr);
	}
}

InstallerSceneContent *getSceneContent(NotNull<AppWindow> window) {
	auto *director = window->getDirector();
	auto *scene = director ? director->getScene() : nullptr;
	return scene ? dynamic_cast<InstallerSceneContent *>(scene->getContent()) : nullptr;
}

} // namespace stappler::xenolith::installer
