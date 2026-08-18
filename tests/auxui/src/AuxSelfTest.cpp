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

#include "XLCommon.h"

#include "AuxSelfTest.h"

#include "XLAction.h"
#include "XLAppWindow.h"
#include "XLScene.h"
#include "XL2dSceneLayout.h"
#include "XLUiSubWindowSession.h"
#include "XLUiTooltipSystem.h"
#include "AuxPopupScene.h"

#include <cstdio>

#include <sprt/runtime/window/window_info.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::app {

static constexpr const char *kStatusPath = "/tmp/auxui_selftest.status";
static constexpr TimeInterval kLongHide = TimeInterval::seconds(12);

AuxSelfTest &AuxSelfTest::instance() {
	static AuxSelfTest s;
	return s;
}

static bool lineHas(StringView line, StringView needle) {
	return line.find(needle) != maxOf<size_t>();
}

static bool selfTestLogHook(log::LogType, StringView tag, const sprt::source_location &,
		log::CustomLog::Type t, log::CustomLog::VA &va) {
	auto &self = AuxSelfTest::instance();
	if (self.isDone() || tag != "WindowDiag") {
		return true;
	}
	StringView text;
	char buf[1024];
	if (t == log::CustomLog::Text) {
		text = va.text;
	} else {
		__sprt_va_list tmp;
		__sprt_va_copy(tmp, va.format.args);
		auto n = __sprt_vsnprintf(buf, sizeof(buf) - 1, va.format.format, tmp);
		__sprt_va_end(tmp);
		if (n <= 0) {
			return true;
		}
		text = StringView(buf, size_t(n));
	}
	self.onLogLine(text);
	return true;
}

void AuxSelfTest::install() {
	if (_hook) {
		return;
	}
	_life = Rc<Lifetime>::alloc();
	_life->test = this;
	_hook = new log::CustomLog(selfTestLogHook);
	std::remove(kStatusPath);
}

void AuxSelfTest::onLogLine(StringView line) {
	if (lineHas(line, "firstFrame id=tooltip-") && lineHas(line, "success=0")) {
		++_poisonFirstFrame;
		fail(toString("poison firstFrame: ", line));
	}
	if (lineHas(line, "Fail to submit") || lineHas(line, "MaterialSwapchainPass")) {
		++_submitFails;
		fail(toString("present fail: ", line));
	}
	if (lineHas(line, "MacosWindow: init type=Tooltip")
			|| lineHas(line, "createWindow begin type=Tooltip")) {
		++_nativeTipCreates;
		fail(toString("native Tooltip window created: ", line));
	}
}

void AuxSelfTest::fail(StringView reason) {
	++_failures;
	log::source().error("AuxSelfTest", "FAIL: ", reason);
}

void AuxSelfTest::check(bool ok, StringView name) {
	++_checks;
	if (!ok) {
		fail(name);
	}
}

void AuxSelfTest::writeStatus(int code) {
	if (FILE *f = std::fopen(kStatusPath, "w")) {
		std::fprintf(f, "%d\n", code);
		std::fclose(f);
	}
}

void AuxSelfTest::evaluateReplacePhase() {
	auto session = _root ? ui::SubWindowSession::get(_root) : nullptr;
	check(session != nullptr, "root window has a SubWindowSession");
	check(session && session->hasTip(), "tip still up after replace");
	check(session && !session->getTipText().empty(), "tip carries its text");
	check(_nativeTipCreates == 0, "no native Tooltip window");
	check(_poisonFirstFrame == 0, "no tooltip firstFrame success=0");
	check(_submitFails == 0, "no MaterialSwapchainPass fail");
}

// computeWindowPlacement is a pure function shared by every backend, so it is checked here rather
// than through a window: this harness already links runtime_window, and tests/runtime does not.
void AuxSelfTest::evaluatePlacement() {
	using sprt::window::WindowAnchor;
	using sprt::window::WindowPlacement;
	using sprt::window::WindowPlacementAdjustment;

	// A 1000x1000 work area, and a 200x20 anchor rect sitting 40px off the bottom of it: the
	// classic "a hint under a widget near the screen edge" geometry.
	const IRect work(0, 0, 1'000, 1'000);
	const IRect anchorRect(100, 940, 200, 20);
	const Extent2 size(120, 30);

	auto place = [&](WindowAnchor a, WindowAnchor g, IVec2 offset, WindowPlacementAdjustment adj) {
		WindowPlacement p;
		p.anchorRect = anchorRect;
		p.anchor = a;
		p.gravity = g;
		p.offset = offset;
		p.adjustment = adj;
		return sprt::window::computeWindowPlacement(p, size, work, work);
	};

	// Unconstrained: below the rect's bottom edge, 8px clear of it. (gravity names the edge of the
	// window that lands on the anchor point, so Top means the window hangs below.)
	auto below = place(WindowAnchor::Bottom, WindowAnchor::Top, IVec2{0, 8},
			WindowPlacementAdjustment::None);
	check(below.y == 968, "placement: unflipped sits below the anchor rect");
	check(below.x == 140, "placement: unflipped is centred on the anchor rect");

	// Same thing with FlipY: 968+30 = 998 still fits, so nothing flips.
	auto noFlip = place(WindowAnchor::Bottom, WindowAnchor::Top, IVec2{0, 8},
			WindowPlacementAdjustment::FlipY);
	check(noFlip.y == 968, "placement: no flip while it fits");

	// Now a window too tall to fit below (940+20+8+80 = 1048 > 1000). The flip must invert the
	// anchor to the rect's TOP edge, the gravity to Bottom, and the offset to -8 - putting the
	// window's bottom 8px ABOVE the rect: 940 - 8 - 80 = 852.
	//
	// The old rect-mirroring shortcut answered 892 here, i.e. 40px lower, straight over the anchor
	// rect it was supposed to avoid - off by the rect's height plus twice the offset.
	WindowPlacement tall;
	tall.anchorRect = anchorRect;
	tall.anchor = WindowAnchor::Bottom;
	tall.gravity = WindowAnchor::Top;
	tall.offset = IVec2{0, 8};
	tall.adjustment = WindowPlacementAdjustment::FlipY;
	auto flipped = sprt::window::computeWindowPlacement(tall, Extent2(120, 80), work, work);
	check(flipped.y == 852, "placement: flip inverts anchor, gravity and offset");

	// A zero-sized anchor rect with no offset is the degenerate case every existing caller uses,
	// and there the flip is a plain mirror around the point. Pinned so the fix above cannot have
	// moved any of them.
	WindowPlacement pt;
	pt.anchorRect = IRect(500, 990, 0, 0);
	pt.anchor = WindowAnchor::TopLeft;
	pt.gravity = WindowAnchor::Top;
	pt.adjustment = WindowPlacementAdjustment::FlipY;
	auto point = sprt::window::computeWindowPlacement(pt, size, work, work);
	check(point.y == 960, "placement: point anchor still mirrors around itself");

	// Sliding still clamps into the work area.
	auto slid = place(WindowAnchor::BottomRight, WindowAnchor::Top, IVec2{900, 0},
			WindowPlacementAdjustment::SlideX);
	check(slid.x == 880, "placement: slide clamps to the work area");
}

void AuxSelfTest::evaluateDwellPhase(NotNull<Scene> scene, bool afterDelay) {
	auto *tips = ui::TooltipSystem::findForNode(scene->getContent());
	if (!afterDelay) {
		check(tips != nullptr, "a hovered target installed a TooltipSystem");
		// The dwell has been running for less than hoverDelay, and the pointer has been moving,
		// which restarts it. Nothing may be up yet.
		check(tips && !tips->isVisible(), "no hint before the dwell elapses");
		check(tips && tips->getPendingTarget() != nullptr, "the dwell is armed");
		return;
	}
	check(tips && tips->isVisible(), "hint is up after the dwell");
	check(tips && tips->getCurrentTarget() != nullptr, "the hint names its target");
}

void AuxSelfTest::noteMenuClosed() { _menuCloseFired = true; }

void AuxSelfTest::finish() {
	if (_done) {
		return;
	}
	_done = true;

	// The opener must have heard that its popup went away. On the native path this callback had no
	// caller at all before the window data carried it, so it never ran.
	check(_menuCloseFired, "popup close callback fired");

	check(_nativeTipCreates == 0, "no native Tooltip after popup race");
	check(_poisonFirstFrame == 0, "no tooltip firstFrame success=0 after popup race");
	check(_submitFails == 0, "no submit fail after popup race");

	const int code = _failures == 0 ? 0 : 1;
	log::source().warn("AuxSelfTest", "SUMMARY: ", _checks, " checks, ", _failures, " failures");
	writeStatus(code);

	if (_root) {
		_root->close(true);
	}
}

void AuxSelfTest::startScenario(NotNull<AppWindow> root, NotNull<Scene> scene) {
	install();
	_root = root.get();

	// Pure-function checks first; they depend on nothing the scenario sets up.
	evaluatePlacement();

	const float parentH = 768.0f;
	auto *rootPtr = root.get();

	scene->runAction(Rc<Sequence>::create(0.35f, [rootPtr, parentH] {
		if (auto session = ui::SubWindowSession::get(rootPtr)) {
			session->showTip("Tip: hovering the heading", Vec2(24.0f, parentH - 32.0f), parentH,
					kLongHide);
		}
	}));
	scene->runAction(Rc<Sequence>::create(0.7f, [rootPtr, parentH] {
		if (auto session = ui::SubWindowSession::get(rootPtr)) {
			session->showTip("Tip: shown from the button", Vec2(144.0f, parentH - 132.0f), parentH,
					kLongHide);
		}
	}));
	scene->runAction(Rc<Sequence>::create(1.1f, [life = _life] {
		if (life && life->test) {
			life->test->evaluateReplacePhase();
		}
	}));

	scene->runAction(Rc<Sequence>::create(1.4f, [rootPtr] {
		if (auto session = ui::SubWindowSession::get(rootPtr)) {
			session->dismissTip();
		}
	}));

	// --- hover dwell, driven through the real input path ---------------------------------------
	//
	// The heading sits at the top-left; resting on it must NOT produce a hint until hoverDelay has
	// passed, and moving within it must keep pushing that out.
	auto sceneRef = Rc<Scene>(scene.get());
	auto hoverAt = [rootPtr](float x, float y) {
		core::InputEventData ev;
		ev.event = sprt::window::InputEventName::MouseMove;
		ev.id = 0;
		ev.input.x = x;
		ev.input.y = y;
		ev.point.density = 1.0f;

		Vector<core::InputEventData> data;
		data.emplace_back(ev);
		// Native, like the inspector's own injection: hover is gated on the window reporting
		// WindowState::Pointer, which only the window-system path sets.
		rootPtr->handleNativeInputEvents(sp::move(data));
	};

	// Jiggle across the heading for the whole window before the check: every move restarts the
	// dwell, so a hint appearing here means the reset is broken.
	for (uint32_t i = 0; i < 8; ++i) {
		scene->runAction(Rc<Sequence>::create(3.6f + float(i) * 0.05f,
				[hoverAt, i] { hoverAt(60.0f + float(i), 736.0f); }));
	}
	scene->runAction(Rc<Sequence>::create(4.02f, [life = _life, sceneRef] {
		if (life && life->test) {
			life->test->evaluateDwellPhase(sceneRef, false);
		}
	}));

	// Now stop moving and let the dwell run out. AuxRootScene sets it to 500ms.
	scene->runAction(Rc<Sequence>::create(4.9f, [life = _life, sceneRef] {
		if (life && life->test) {
			life->test->evaluateDwellPhase(sceneRef, true);
		}
	}));
	scene->runAction(Rc<Sequence>::create(1.7f, [life = _life, rootPtr, parentH] {
		auto session = ui::SubWindowSession::get(rootPtr);
		if (!session) {
			return;
		}
		session->showTip("Tip: race create", Vec2(24.0f, parentH - 32.0f), parentH, kLongHide);

		ui::SubWindow::Config config;
		config.type = sprt::window::WindowType::Popup;
		config.placement.anchorRect = IRect(200, 100, 0, 0);
		config.placement.anchor = sprt::window::WindowAnchor::BottomRight;
		config.placement.gravity = sprt::window::WindowAnchor::TopLeft;
		config.size = Extent2(220, 220);
		config.title = StringView("auxui SelfTest Menu");
		config.idPrefix = StringView("menu");
		config.scene = [](NotNull<ui::SubWindow> surface, NotNull<AppThread> app,
								 NotNull<core::RenderServerChannel> window,
								 const core::FrameConstraints &c) -> Rc<Scene> {
			return AuxPopupScene::create(app, window, c, surface, 1);
		};
		// The close callback is the fix this test now covers: on the native path it used to have no
		// caller at all, so an opener could never learn that its popup was gone.
		config.onClose = [life](NotNull<ui::SubWindow>) {
			if (life && life->test) {
				life->test->noteMenuClosed();
			}
		};

		if (life && life->test) {
			life->test->_menu = session->openPopup(sp::move(config));
		}
	}));

	// Dismiss and assert are separate steps on purpose: on the native path dismiss() routes through
	// the context thread and back, so the close callback cannot have fired by the time it returns.
	// Checking in the same tick passes on the overlay path and fails on the native one - which is
	// exactly what it did the first time this ran with a window system attached.
	scene->runAction(Rc<Sequence>::create(2.8f, [life = _life] {
		if (life && life->test) {
			if (auto menu = sp::move(life->test->_menu)) {
				life->test->_menu = nullptr;
				menu->dismiss();
			}
		}
	}));

	// After the hover-dwell phase, which is the last thing this scenario checks.
	scene->runAction(Rc<Sequence>::create(5.4f, [life = _life] {
		if (life && life->test) {
			life->test->finish();
		}
	}));
}

} // namespace stappler::xenolith::app
