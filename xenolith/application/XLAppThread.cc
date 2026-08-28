/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>
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

#include "XLAppThread.h"
#include "resources/XLQueueCache.h"
#include "XLEvent.h"
#include "XLRemoteBlockTransfer.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(AppThread, onNetworkState)
XL_DECLARE_EVENT_CLASS(AppThread, onThemeInfo)

AppThread::~AppThread() { }

void AppThread::run() { Thread::run(); }

void AppThread::threadInit() {
	_requests.reserve(16);

	// Bidirectional block-transfer manager (Domain::Data); both subclasses share it.
	_blockTransfer = Rc<BlockTransferManager>::create(this);

	_thisThreadId = getCurrentThreadId();

	_appLooper = sprt::dispatch::Looper::acquire(sprt::dispatch::LooperInfo{
		.name = StringView("App"),
		.workersCount = getContextInfo()->appThreadsCount,

#if SPRT_HOSTED_RTOS
		.engineMask = sprt::dispatch::QueueEngine::None,
#else
		// Disable ALooper for internal queue, it can not be stopped gracefully
		.engineMask = sprt::dispatch::QueueEngine::Any & ~sprt::dispatch::QueueEngine::ALooper,
#endif
	});

	// Steady app-event heartbeat: an infinite Looper timer at appUpdateInterval (default 1s, an app-event
	// cadence -- NOT the screen/frame interval). It drives performAppUpdate regardless of frame
	// production, which in the remote subclasses pumps the connection and runs the ~1s keepalive
	// (ping/pong) even while the window is idle. See ServerAppThread::pumpListener /
	// ClientAppThread::pumpConnection.
	_timer = _appLooper->scheduleTimer(sprt::dispatch::TimerInfo{
		.completion = sprt::dispatch::TimerInfo::Completion::create<AppThread>(this,
				[](AppThread *data, sprt::dispatch::TimerHandle *self, uint32_t value,
						Status status) {
		data->performUpdate(false); //
	}),
		.interval = getContextInfo()->appUpdateInterval,
		.count = sprt::dispatch::TimerInfo::Infinite,
	});

	loadExtensions();

	handleThreadInitialized();

	initializeExtensions();

	_time.delta = 0;
	_time.global = sp::platform::clock(ClockType::Monotonic);
	_time.app = 0;
	_time.dt = 0.0f;

	performUpdate(true);

	Thread::threadInit();
}

void AppThread::threadDispose() {
	handleThreadDisposed();

	_timer->cancel();
	_timer = nullptr;

	finalizeExtensions();

	_appLooper->poll();
	_appLooper = nullptr;

	Thread::threadDispose();
}

bool AppThread::worker() {
	_startTime = _lastUpdate = _clock = _time.global;

	_appLooper->run();

	return _continueExecution.test_and_set();
}

void AppThread::stop() {
	Thread::stop();

	_appLooper->wakeup(
			sprt::dispatch::WakeupFlags::Graceful | sprt::dispatch::WakeupFlags::SuspendThreads);
}

void AppThread::wakeup(Function<void()> &&fn) {
	performOnAppThread([this, fn = sp::move(fn)] {
		if (fn) {
			fn();
		}
		performUpdate(true);
	}, this, true);
}

void AppThread::handleNetworkStateChanged(NetworkFlags flags) {
	performOnAppThread([this, flags] {
		if (flags != _networkFlags) {
			_networkFlags = flags;
			for (auto &it : _extensions) { it.second->handleNetworkStateChanged(flags); }
			onNetworkState(this, int64_t(toInt(flags)));
		}
	}, this);
}

void AppThread::handleThemeInfoChanged(const ThemeInfo &theme) {
	performOnAppThread([this, theme] {
		if (theme != _themeInfo) {
			_themeInfo = theme;
			for (auto &it : _extensions) { it.second->handleThemeInfoChanged(theme); }
			onThemeInfo(this, encodeThemeInfo(_themeInfo));
		}
	}, this);
}

void AppThread::handleMatrialsUpdated(NotNull<core::MaterialSet> set) { }

void AppThread::performOnAppThread(Function<void()> &&func, Ref *target, bool onNextFrame,
		StringView tag) {
	if (isOnThisThread() && !onNextFrame) {
		func();
	} else {
		waitRunning();
		if (_appLooper) {
			_appLooper->performOnThread(sp::move(func), target, !onNextFrame, tag);
		}
	}
}

void AppThread::performOnAppThread(Rc<Task> &&task, bool onNextFrame) {
	if (isOnThisThread() && !onNextFrame) {
		task->handleCompleted();
	} else {
		waitRunning();
		if (_appLooper) {
			_appLooper->performOnThread(sp::move(task));
		}
	}
}

void AppThread::perform(ExecuteCallback &&exec, CompleteCallback &&complete, Ref *obj) const {
	perform(Rc<Task>::create(sp::move(exec), sp::move(complete), obj));
}

void AppThread::perform(Rc<Task> &&task) const { _appLooper->performAsync(sp::move(task)); }

void AppThread::perform(Rc<Task> &&task, bool performFirst) const {
	_appLooper->performAsync(sp::move(task), performFirst);
}

bool AppThread::addListener(NotNull<Ref> ref, Function<void(const UpdateTime &, bool)> &&cb) {
	auto it = _listeners.find(ref);
	if (it == _listeners.end()) {
		_listeners.emplace(ref, sp::move(cb));
		return true;
	}
	return false;
}

bool AppThread::removeListener(NotNull<Ref> ref) {
	auto it = _listeners.find(ref);
	if (it != _listeners.end()) {
		_listeners.erase(it);
		return true;
	}
	return false;
}

// Context-bridge hooks: base defaults are no-ops; the server/client subclasses route them to their
// own context.
void AppThread::handleThreadInitialized() { }
void AppThread::handleThreadDisposed() { }
void AppThread::handleThreadUpdated(const UpdateTime &) { }

// Window lifecycle seams: no-ops on a context-free / client thread (no native windows).
Rc<Director> AppThread::handleAppWindowCreated(NotNull<AppWindow>, const core::FrameConstraints &) {
	return nullptr;
}

void AppThread::handleAppWindowDestroyed(NotNull<AppWindow>, Rc<Director> &&) { }

// Listener seams: no-ops on a context-free / client thread (no server listener).
bool AppThread::isServerThread() const { return false; }
bool AppThread::isListening() const { return false; }
bool AppThread::setListenAddress(StringView) { return false; }

bool AppThread::shareWindow(AppWindow *, SpanView<core::Queue *>,
		const HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> &) {
	return false;
}

bool AppThread::startListening() { return false; }
bool AppThread::stopListening() { return false; }
bool AppThread::setBearerKey(BytesView) { return false; }
bool AppThread::setCompressionDictionary(BytesView) { return false; }

// Connection send facade: no connection on the base, so everything fails. Overridden by the server /
// client subclasses to route through their active connection.
bool AppThread::remoteSendCbor(remote::Domain, uint8_t, const Value &, uint32_t *) { return false; }
bool AppThread::remoteSendRaw(remote::Domain, uint8_t, BytesView, uint32_t *) { return false; }
bool AppThread::remoteSendCborReply(uint32_t, remote::Domain, uint8_t, const Value &) {
	return false;
}
bool AppThread::remoteSendError(remote::Domain, uint8_t, uint32_t) { return false; }
bool AppThread::remoteSendCborWithReply(remote::Domain, uint8_t, const Value &,
		Function<void(const remote::MessageHeader &, BytesView)> &&, uint64_t) {
	return false;
}

void AppThread::waitForReply(uint32_t serial,
		Function<void(const remote::MessageHeader &, BytesView payload)> &&cb, uint64_t timeoutUs) {
	uint64_t deadline = timeoutUs ? sp::platform::clock(ClockType::Monotonic) + timeoutUs : 0;
	_requests.insert_or_assign(serial, PendingReply{sp::move(cb), deadline});
}

bool AppThread::failTimedOutRequests() {
	if (_requests.empty()) {
		return false;
	}

	auto now = sp::platform::clock(ClockType::Monotonic);

	// Collect the expired serials first: invoking a waiter's callback may register new requests (or
	// erase this one), so we must not iterate _requests while calling back into it.
	Vector<uint32_t> expired;
	for (auto &it : _requests) {
		if (it.second.deadline != 0 && now >= it.second.deadline) {
			expired.emplace_back(it.first);
		}
	}
	if (expired.empty()) {
		return false;
	}

	// Synthesize a local protocol-error reply: the peer that owed us this reply is the opposite role, so
	// tag the error as coming from it. code == NetworkBackend marks a local/transport-level failure.
	auto errType =
			isServerThread() ? remote::MessageType::ClientError : remote::MessageType::ServerError;
	for (auto serial : expired) {
		auto it = _requests.find(serial);
		if (it == _requests.end()) {
			continue;
		}
		auto cb = sp::move(it->second.cb);
		_requests.erase(it);

		log::source().warn("AppThread", "request ", serial,
				" timed out without a reply; failing with local protocol error");

		if (cb) {
			remote::MessageHeader h{};
			h.msgtype = toInt(errType);
			h.domain = toInt(remote::Domain::Error);
			h.code = toInt(remote::GlobalError::NetworkBackend);
			h.serial = serial;
			cb(h, BytesView());
		}
	}
	return true;
}

void AppThread::performAppUpdate(const UpdateTime &time, bool wakeup) {
	handleThreadUpdated(time);
	for (auto &it : _extensions) { it.second->update(this, time, wakeup); }

	auto listeners = _listeners;
	for (auto &it : listeners) { it.second(time, wakeup); }
}

void AppThread::performUpdate(bool wakeup) {
	_clock = sp::platform::clock(ClockType::Monotonic);

	_time.delta = _clock - _lastUpdate;
	// Clamp the frame delta. When the tab is backgrounded the browser throttles/pauses the
	// worker clock, so on the next tick `_clock - _lastUpdate` can be many seconds; feeding that
	// into the action/animation system makes it lurch far past the end of an interval and appear
	// to freeze. Cap the step so time keeps flowing smoothly after a resume.
	if (_lastUpdate != 0 && _time.delta > 100'000 /* 100 ms */) {
		_time.delta = 100'000;
	}
	_time.global = _clock;
	_time.app = _startTime - _clock;
	_time.dt = float(_time.delta) / 1'000'000;

	performAppUpdate(_time, wakeup);

	_lastUpdate = _clock;
}

void AppThread::loadExtensions() {
	_resourceCache = addExtension(Rc<ResourceCache>::create(this));
	addExtension(Rc<QueueCache>::create(this));
}

void AppThread::initializeExtensions() {
	for (auto &it : _extensions) { it.second->initialize(this); }
	_extensionsInitialized = true;
}

void AppThread::finalizeExtensions() {
	for (auto &it : _extensions) { it.second->invalidate(this); }
}

bool AppThread::dispatchMessage(const remote::MessageHeader &h, BytesView payload) {
	if (remote::isReplyOrError(h)) {
		auto reqIt = _requests.find(h.serial);
		if (reqIt != _requests.end()) {
			auto cb = sp::move(reqIt->second.cb);
			_requests.erase(reqIt);
			if (cb) {
				cb(h, payload);
			}
			return true;
		}
	}
	return false;
}

} // namespace stappler::xenolith
