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
#include "XLUiAuxSession.h"

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
	using TipState = ui::AuxSession::TipState;
	auto &session = ui::AuxSession::instance();
	check(session.getTipState() == TipState::Ready, "tip Ready after replace");
	check(session.getTipId().starts_with("tooltip-"), "tip id is overlay tooltip-*");
	check(ui::AuxWindow::hasOverlay(session.getTipId()), "tip overlay still parented");
	check(_nativeTipCreates == 0, "no native Tooltip window");
	check(_poisonFirstFrame == 0, "no tooltip firstFrame success=0");
	check(_submitFails == 0, "no MaterialSwapchainPass fail");
}

void AuxSelfTest::finish() {
	if (_done) {
		return;
	}
	_done = true;

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

	const float parentH = 768.0f;
	auto *rootPtr = root.get();

	scene->runAction(Rc<Sequence>::create(0.35f, [rootPtr, parentH] {
		ui::AuxSession::instance().showTip(rootPtr, "Tip: hovering the heading",
				Vec2(24.0f, parentH - 32.0f), parentH, kLongHide);
	}));
	scene->runAction(Rc<Sequence>::create(0.7f, [rootPtr, parentH] {
		ui::AuxSession::instance().showTip(rootPtr, "Tip: shown from the button",
				Vec2(144.0f, parentH - 132.0f), parentH, kLongHide);
	}));
	scene->runAction(Rc<Sequence>::create(1.1f, [life = _life] {
		if (life && life->test) {
			life->test->evaluateReplacePhase();
		}
	}));

	scene->runAction(Rc<Sequence>::create(1.4f, [] { ui::AuxSession::instance().dismissTip(); }));
	scene->runAction(Rc<Sequence>::create(1.7f, [rootPtr, parentH] {
		ui::AuxSession::instance().showTip(rootPtr, "Tip: race create",
				Vec2(24.0f, parentH - 32.0f), parentH, kLongHide);

		sprt::window::WindowPlacement placement;
		placement.anchorRect = IRect(200, 100, 0, 0);
		placement.anchor = sprt::window::WindowAnchor::BottomRight;
		placement.gravity = sprt::window::WindowAnchor::TopLeft;
		ui::AuxSession::instance().openPopup(rootPtr, placement, Extent2(220, 220),
				[](StringView) -> Rc<basic2d::SceneLayout2d> { return nullptr; },
				"auxui SelfTest Menu");
	}));

	scene->runAction(Rc<Sequence>::create(3.5f, [life = _life] {
		if (life && life->test) {
			life->test->finish();
		}
	}));
}

} // namespace stappler::xenolith::app
