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
#include "InstallerShell.h"
#include "InstallerDialogs.h"
#include "InstallerSettingsPage.h"
#include "InstallerStrings.h"

#include "XLAppWindow.h"
#include "XLEventListener.h"
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

	_shell = Rc<InstallerShell>::create();
	pushLayout(_shell);

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
	// Attach BEFORE the base class propagates handleEnter into the subtree: the nav pane and the
	// pages bind to the controller's data::Sources in their own handleEnter, and those Sources only
	// exist once attach() has run. Doing it after would hand every child a null source and leave the
	// tree empty until something else happened to dirty it.
	if (!_controller) {
		// A never-destroyed singleton, so this is a lookup rather than a construction; what belongs
		// to the scene is the attachment, which handleExit gives back.
		_controller = AppController::getInstance();
		if (_controller && !_controller->attach(scene->getDirector()->getApplication())) {
			_controller = nullptr;
		}
	}

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

	if (!_controller || _commandsRegistered) {
		return;
	}
	_commandsRegistered = true;

	/* The catalogue, the engine refs and the engine status are all in flight already: attach()
	starts them, so they are fetched while this scene is still being built rather than when a page
	that wants them is first opened. What is left here is only the loading chrome.

	Hide it on the EVENT rather than through a completion callback, because by now the load may have
	already landed - hence the immediate check first, or the spinner would stay up forever over a
	catalogue that arrived before this listener existed. */
	auto listener = addSystem(Rc<EventListener>::create());
	listener->listenForEvent(AppController::onCatalogueChanged, [this](const Event &) {
		// Hide loading BEFORE any confirm/onboarding. Dialogs used to snapshot+restore the
		// loading chrome on dismiss, which left "Loading catalogue…" stuck forever after the
		// first onboard dismiss — and without RenderContinuously the restored spinner froze.
		hideLoadingState();
	});
	if (_controller->getCatalogue()) {
		hideLoadingState();
	}

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

	// The two system dialogs, reachable without hunting for a button. Both answer only once the
	// dialog is gone, so they double as a check that the callback really does arrive.
	inspector::addCommand(this, "pick-folder", "Open the system folder picker",
			[this](Value &&, Function<void(Value &&)> &&done) {
		auto *window = static_cast<AppWindow *>(getDirector()->getRenderServer());
		if (!window || !_controller) {
			Value r;
			r.setString("no window", "error");
			done(sp::move(r));
			return;
		}
		_controller->pickFolder(window, strings::projectChoose(),
				[done = sp::move(done)](String picked) mutable {
			Value r;
			r.setString(picked, "path");
			r.setBool(!picked.empty(), "ok");
			done(sp::move(r));
		});
	});

	inspector::addCommand(this, "open-folder", "Reveal the data directory in the file manager",
			[this](Value &&, Function<void(Value &&)> &&done) {
		auto *window = static_cast<AppWindow *>(getDirector()->getRenderServer());
		if (window && _controller) {
			_controller->openFolder(window, _controller->getLayout().data);
		}
		Value r;
		r.setBool(window != nullptr, "opened");
		done(sp::move(r));
	});

	// Both new views are virtualized, so inspect_scene only ever shows the rows that happen to be
	// on screen. The MODEL therefore has to be inspectable on its own — this is the whole of it:
	// settings, caches, reachability and the job registry.
	inspector::addCommand(this, "dump-state", "Dump the controller state (settings, jobs, caches)",
			[this](Value &&, Function<void(Value &&)> &&done) {
		if (!_controller) {
			Value r;
			r.setString("not attached", "error");
			done(sp::move(r));
			return;
		}
		done(_controller->encodeDebugState());
	});

	// Both new views are virtualized and the pages swap by visibility, so driving the UI from
	// outside needs a way in that does not depend on hit-testing a row.
	inspector::addCommand(this, "select-page",
			"Show a content page: welcome | engines | hosts | targets",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		const auto name = args.isString() ? args.getString() : args.getString("page");
		PageId page = PageId::Welcome;
		if (name == "engines") {
			page = PageId::Engines;
		} else if (name == "hosts") {
			page = PageId::Hosts;
		} else if (name == "targets") {
			page = PageId::Targets;
		}
		if (_shell) {
			_shell->showPage(page);
		}
		Value r;
		r.setString(name, "page");
		r.setBool(_shell != nullptr, "ok");
		done(sp::move(r));
	});

	inspector::addCommand(this, "probe-urls", "Re-check both configured sources for reachability",
			[this](Value &&, Function<void(Value &&)> &&done) {
		if (!_controller) {
			Value r;
			r.setString("not attached", "error");
			done(sp::move(r));
			return;
		}
		const auto &settings = _controller->getSettings();
		_controller->probeSource(SourceKind::EngineRepo, settings.sources.getEngineRepoUrl());
		_controller->probeSource(SourceKind::Releases, settings.sources.getReleasesRoot());
		// Answers immediately: a probe is asynchronous, and its result shows up in dump-state.
		Value r;
		r.setBool(true, "started");
		done(sp::move(r));
	});

	// Install without a click, so that "the button does nothing" can be told apart from "the core
	// install does nothing". Answers when the operation is over, not when it starts.
	inspector::addCommand(this, "install",
			"Install a component: {kind: host|target, id: <triple>}",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		if (!_controller) {
			Value r;
			r.setString("not attached", "error");
			done(sp::move(r));
			return;
		}
		const auto kind = args.getString("kind") == "host" ? Kind::Host : Kind::Target;
		const auto id = args.getString("id");
		_controller->installComponent(kind, id,
				nullptr, [done = sp::move(done)](bool ok, String err) mutable {
			Value r;
			r.setBool(ok, "ok");
			if (!err.empty()) {
				r.setString(err, "error");
			}
			done(sp::move(r));
		});
	});

	// The settings form lives in a SubWindow, which is a separate scene where there is no
	// Subwindows capability - so it is reachable neither by hit-testing the frame button nor by
	// select-page. This is the only way in from outside.
	inspector::addCommand(this, "open-settings", "Open the settings form",
			[this](Value &&, Function<void(Value &&)> &&done) {
		auto *window = static_cast<AppWindow *>(getDirector()->getRenderServer());
		if (window) {
			showSettingsPage(window);
		}
		Value r;
		r.setBool(window != nullptr, "opened");
		done(sp::move(r));
	});

	/* Hold the render loop open, for watching something that changes on its own.

	A window is redrawn only when something dirties it, and a change made from a callback - a probe
	landing, a download's progress, a scheduled step - does not count. Nobody is moving the mouse
	during an automated check, so those changes are computed and never drawn, which reads as "my fix
	did nothing". This makes the scene render regardless.

	{seconds: N} bounds it; no argument runs until the app exits. Re-invoking replaces the previous
	one - the action is tagged. */
	inspector::addCommand(this, "render-continuously",
			"Keep rendering frames: {seconds: N}, or endlessly with no argument",
			[this](Value &&args, Function<void(Value &&)> &&done) {
		const auto seconds = args.isDictionary() ? args.getDouble("seconds") : args.getDouble();
		stopActionByTag("InspectorRender"_tag);
		if (seconds > 0.0) {
			runAction(Rc<RenderContinuously>::create(float(seconds)), "InspectorRender"_tag);
		} else {
			runAction(Rc<RenderContinuously>::create(), "InspectorRender"_tag);
		}
		Value r;
		r.setDouble(seconds, "seconds");
		r.setBool(true, "running");
		done(sp::move(r));
	});

	// The other half: closing is the path that has to release the surface and fire the close
	// callback exactly once, and it is the one that can leave the window system waiting.
	inspector::addCommand(this, "close-settings", "Dismiss the settings form",
			[](Value &&, Function<void(Value &&)> &&done) {
		Value r;
		r.setBool(closeSettingsPage(), "closed");
		done(sp::move(r));
	});
}

void InstallerSceneContent::handleExit() {
	// The controller outlives the scene, so it is detach() - not a destructor - that releases the
	// dialog request, the spawned prompt and the data Sources. Skipping this would leave Rc's on
	// scene-lifetime objects alive until static destruction, long after the renderer is gone.
	if (_controller) {
		_controller->detach();
		_controller = nullptr;
	}
	basic2d::SceneContent2d::handleExit();
}

void InstallerSceneContent::hideLoadingState() {
	stopActionByTag("LoadingRender"_tag);
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
