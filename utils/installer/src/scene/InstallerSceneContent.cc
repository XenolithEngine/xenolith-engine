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
#include "XLWindowDecorations.h"
#include "XLUiStyleSystem.h"
#include "XLAction.h"
#include "XLDirector.h"
#include "XLAppThread.h"
#include "XLScheduler.h"
#include "XL2dIcons.h"

#include <sprt/runtime/dispatch/looper.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

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
			addChild(Rc<basic2d::Layer>::create(Color::White), ZOrder::min() + ZOrder(2));
	_globalBackground->setName("global-background");
	_globalBackground->addSystem(Rc<ui::StyleResolver>::create());

	_layout = Rc<InstallerLayout>::create();
	pushLayout(_layout);

	// Loading overlay. Three SIBLING nodes (not parent/child) so the render order is unambiguous:
	// the overlay is a semi-opaque dimming Layer over the CONTENT area only (it stops below the
	// 32px title bar, so the traffic lights stay visible); the spinner + caption are added AFTER it
	// with a higher z and the Transparent rendering level, so they paint in the same pass as the
	// overlay but AFTER it (higher z-path) — they sit on top, fully visible, never dimmed/covered.
	// (Earlier the spinner was a CHILD of the overlay and vanished: the overlay's Transparent pass
	// painted over the Surface-level spinner. And the overlay covered the title bar because it was
	// full-height.)
	_loadingOverlay = addChild(Rc<basic2d::Layer>::create(Color4F(0.02f, 0.03f, 0.03f, 0.85f)),
			ZOrder::max() - ZOrder(3));
	_loadingOverlay->setName("loading-overlay");
	_loadingOverlay->setAnchorPoint(Vec2(0.5f, 0.5f));

	_spinner = addChild(
			Rc<basic2d::IconSprite>::create(basic2d::IconName::Navigation_refresh_solid),
			ZOrder::max() - ZOrder(1));
	_spinner->setType("icon");
	_spinner->setName("loading-spinner");
	_spinner->setRenderingLevel(RenderingLevel::Transparent);
	_spinner->setColor(Color4F(0.988f, 0.706f, 0.0f, 1.0f)); // #FCB400
	_spinner->setContentSize(Size2(64.0f, 64.0f));
	_spinner->setAnchorPoint(Vec2(0.5f, 0.5f));

	_loadingLabel = addChild(Rc<basic2d::Label>::create(), ZOrder::max() - ZOrder(1));
	_loadingLabel->setType("label");
	_loadingLabel->setRenderingLevel(RenderingLevel::Transparent);
	_loadingLabel->setString("Loading catalogue…");
	_loadingLabel->setColor(Color::White);
	_loadingLabel->setAnchorPoint(Vec2(0.5f, 0.5f));

	return true;
}

void InstallerSceneContent::handleContentSizeDirty() {
	basic2d::SceneContent2d::handleContentSizeDirty();

	// Overlay covers the content area only — the top 32px (title bar) stays exposed. Spinner +
	// caption are centred in that same content area.
	constexpr float kTitleBarH = 32.0f;
	const float contentH = _contentSize.height - kTitleBarH;
	if (_loadingOverlay) {
		_loadingOverlay->setContentSize(Size2(_contentSize.width, contentH));
		_loadingOverlay->setPosition(Vec2(_contentSize.width / 2.0f, contentH / 2.0f));
	}
	Vec2 c(_contentSize.width / 2.0f, contentH / 2.0f);
	if (_spinner) { _spinner->setPosition(Vec2(c.x, c.y + 24.0f)); }
	if (_loadingLabel) { _loadingLabel->setPosition(Vec2(c.x, c.y - 48.0f)); }
}

void InstallerSceneContent::handleEnter(Scene *scene) {
	basic2d::SceneContent2d::handleEnter(scene);

	// Spin the loading icon while it is visible. Two parts:
	//  - RenderContinuously action: the renderer otherwise only produces frames on demand (a dirty
	//    region / input), so without this the spinner advances ONLY while you move the mouse (the
	//    KioskScene in xenolith-os uses the same action for its clock). Run it on the spinner while
	//    it is shown, stop it once hidden to return to low-power rendering.
	//  - schedulePerFrame: the actual rotation (there is no RotateBy action). It lives on Scheduler,
	//    not Node; grab the node's scheduler and target the spinner.
	if (!_spinnerScheduled && _spinner && _spinner->getScheduler()) {
		_spinnerScheduled = true;
		_spinner->runAction(Rc<RenderContinuously>::create(), "RenderContinuously"_tag);
		_spinner->getScheduler()->schedulePerFrame(
				[this](const UpdateTime &) {
					if (_spinner && _spinner->isVisible()) {
						_spinner->setRotation(_spinner->getRotation() + 0.25f);
					}
				},
				_spinner, 0, false);
	}

	if (!_controller) {
		auto app = getDirector()->getApplication();
		_controller = Rc<InstallerController>::create(app);
		if (_controller) {
		_controller->loadCatalog([this](bool, String) {
			if (!_layout || !_controller) { return; }
			_layout->onCatalogReady(_controller);
			// Hide the overlay only once the catalogue has loaded and the table is built — NOT on a
			// fixed timer. The catalogue comes over FTP and can take many seconds; a timer would lift
			// the overlay onto an empty table. A short extra delay lets the pre-warmed rows render.
			auto looper = getDirector()->getApplication()->getLooper();
			if (looper) {
				looper->schedule(sprt::dispatch::TimeInterval::milliseconds(300),
						[this](sprt::dispatch::Handle *, bool) {
							if (_layout) { _layout->dropScrollWarmup(); }
							if (_spinner) { _spinner->stopActionByTag("RenderContinuously"_tag); }
							if (_loadingOverlay) { _loadingOverlay->setVisible(false); }
							if (_spinner) { _spinner->setVisible(false); }
							if (_loadingLabel) { _loadingLabel->setVisible(false); }
						},
						this);
			}
		});
		_controller->queryEngine([](const EngineStatusInfo &) {});
	}
}
}

} // namespace stappler::xenolith::installer
