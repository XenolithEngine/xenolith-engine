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

#include "InstallerStatusBar.h"
#include "InstallerAppController.h"

#include "XLEventListener.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::installer {

InstallerStatusBar::~InstallerStatusBar() { }

bool InstallerStatusBar::init() {
	if (!ui::Panel::init()) {
		return false;
	}

	setName("status-bar");
	removeStyleClass("xl-ui-panel");
	registerStyleAppliers("status-bar");

	_arch = addChild(Rc<basic2d::Label>::create(), ZOrder(1));
	_arch->setType("label");
	_arch->setName("status-arch");

	auto spacer = addChild(Rc<Node>::create(), ZOrder(2));
	spacer->addStyleClass("status-spacer");

	_message = addChild(Rc<basic2d::Label>::create(), ZOrder(3));
	_message->setType("label");
	_message->setName("status-message");

	_progress = addChild(Rc<ui::ProgressBar>::create(), ZOrder(4));
	_progress->setName("status-progress");
	_progress->setVisible(false);

	_count = addChild(Rc<basic2d::Label>::create(), ZOrder(5));
	_count->setType("label");
	_count->setName("status-jobs");

	return true;
}

void InstallerStatusBar::handleEnter(Scene *scene) {
	ui::Panel::handleEnter(scene);

	auto listener = addSystem(Rc<EventListener>::create());
	// One listener, four events: everything this widget shows is derived state, so there is nothing
	// finer to react to than "the registry moved".
	listener->listenForEvent(AppController::onJobStarted, [this](const Event &) { refresh(); });
	listener->listenForEvent(AppController::onJobProgress, [this](const Event &) { refresh(); });
	listener->listenForEvent(AppController::onJobFinished, [this](const Event &) { refresh(); });
	listener->listenForEvent(AppController::onReady, [this](const Event &) { refresh(); });

	refresh();
}

void InstallerStatusBar::refresh() {
	auto controller = AppController::getInstance();
	if (!controller) {
		return;
	}

	if (_arch) {
		auto arch = toString(controller->getNativeId());
		if (arch.empty()) {
			arch = "unsupported host";
		} else if (controller->isNativeViaEmulation()) {
			// resolveHost already works this out and nothing has ever surfaced it; it matters,
			// because it explains why the "native" toolchain is not the machine's own architecture.
			arch += " (via emulation)";
		}
		_arch->setString(arch);
	}

	const auto jobs = controller->getJobs();

	// The newest ACTIVE job names what is happening; with none, the newest finished one says what
	// just happened. That is "the progress of the latest actions" from design.md - the bar is not
	// blank the moment a download ends.
	const Job *headline = nullptr;
	for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
		if (it->isActive()) {
			headline = &(*it);
			break;
		}
	}
	if (!headline && !jobs.empty()) {
		headline = &jobs.back();
	}

	if (_message) {
		String text;
		if (headline) {
			text = headline->title;
			if (headline->phase == JobPhase::Failed) {
				text += ": ";
				text += headline->error.empty() ? String("failed") : headline->error;
			} else if (headline->phase == JobPhase::Done) {
				text += " — done";
			}
		}
		_message->setString(text);
	}

	const auto active = controller->getActiveJobCount();
	if (_count) {
		_count->setString(active > 1 ? toString(active, " running") : String());
	}

	if (_progress) {
		const auto value = controller->getAggregateProgress();
		// Hidden rather than empty when nothing determinate is running: an empty bar reads as
		// "0% done", which is a claim the registry cannot make for a clone.
		_progress->setVisible(!sprt::isnan(value));
		if (!sprt::isnan(value)) {
			_progress->setProgress(value);
		}
	}
}

} // namespace stappler::xenolith::installer
