/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

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

#include "XLSubscriptionListener.h"
#include "XLDirector.h"
#include "XLScheduler.h"
#include "XLScene.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

bool SubscriptionListener::init(DirtyCallback &&cb) {
	if (!System::init()) {
		return false;
	}

	_callback = sp::move(cb);
	return true;
}

void SubscriptionListener::handleEnter(Scene *scene) {
	System::handleEnter(scene);

	_scheduler = scene->getDirector()->getScheduler();

	updateScheduler();
}

void SubscriptionListener::handleExit() {
	if (_scheduled) {
		unschedule();
	}

	_scheduler = nullptr;

	System::handleExit();
}

void SubscriptionListener::setCallback(DirtyCallback &&cb) { _callback = sp::move(cb); }

void SubscriptionListener::setDirty() { _dirty = true; }

void SubscriptionListener::update(UpdateTime dt) { }

void SubscriptionListener::check() { update(UpdateTime()); }

void SubscriptionListener::updateScheduler() {
	if (_subscription && !_scheduled) {
		schedule();
	} else if (!_subscription && _scheduled) {
		unschedule();
	}
}

void SubscriptionListener::schedule() {
	if (_subscription && !_scheduled && _scheduler) {
		_scheduler->scheduleUpdate(this, 0, false);
		_scheduled = true;
	}
}

void SubscriptionListener::unschedule() {
	if (_scheduled && _scheduler) {
		_scheduler->unschedule(this);
		_scheduled = false;
	}
}

} // namespace stappler::xenolith
