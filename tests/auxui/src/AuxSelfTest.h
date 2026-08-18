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

#ifndef TESTS_AUXUI_SRC_AUXSELFTEST_H_
#define TESTS_AUXUI_SRC_AUXSELFTEST_H_

#include "XLCommon.h"

#include "SPLog.h"

#include "XLUiSubWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class AppWindow;
class Scene;

namespace app {

// In-process tip-slot contract checks. Enable with AUXUI_SELFTEST=1.
// Tips are in-scene overlays — fail on native tooltip windows or present poison.
class AuxSelfTest {
public:
	static AuxSelfTest &instance();

	void install();
	void startScenario(NotNull<AppWindow> root, NotNull<Scene> scene);

	void onLogLine(StringView line);

	bool isDone() const { return _done; }

	// Called from the menu's close callback. Public because that callback is a plain closure
	// registered on the SubWindow, not a member.
	void noteMenuClosed();

	// The menu the scenario opened. Public for the same reason.
	Rc<ui::SubWindow> _menu;

protected:
	struct Lifetime : public Ref {
		AuxSelfTest *test = nullptr;
	};

	AuxSelfTest() = default;

	void fail(StringView reason);
	void check(bool ok, StringView name);
	void evaluateReplacePhase();

	// `afterDelay` false: mid-jiggle, nothing may be up. True: the pointer has rested, so it must be.
public:
	void evaluateDwellPhase(NotNull<Scene>, bool afterDelay);

	// Pure-function checks on sprt::window::computeWindowPlacement; no window involved.
	void evaluatePlacement();

protected:
	void finish();
	void writeStatus(int code);

	Rc<Lifetime> _life;
	log::CustomLog *_hook = nullptr;
	AppWindow *_root = nullptr;

	uint32_t _checks = 0;
	uint32_t _failures = 0;
	bool _done = false;

	// Set by noteMenuClosed(): proves the close callback fired on whichever path materialized.
	bool _menuCloseFired = false;

	uint32_t _nativeTipCreates = 0;
	uint32_t _poisonFirstFrame = 0;
	uint32_t _submitFails = 0;
};

} // namespace app
} // namespace stappler::xenolith

#endif // TESTS_AUXUI_SRC_AUXSELFTEST_H_
