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

#if MODULE_XENOLITH_FONT
// Downstream module, reached only through the font::FontController extension type.
#include "XLFontController.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

XL_DECLARE_EVENT_CLASS(AppThread, onNetworkState)
XL_DECLARE_EVENT_CLASS(AppThread, onThemeInfo)

AppThread::~AppThread() { }

void AppThread::run() { Thread::run(); }

void AppThread::threadInit() {
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

	// App-event heartbeat: an infinite Looper timer at appUpdateInterval (default 1s, not the frame
	// interval). It drives performAppUpdate regardless of frames, which pumps the connection and
	// the keepalive in the remote subclasses.
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

void AppThread::flushPendingFontGlyphs() {
#if MODULE_XENOLITH_FONT
	if (auto fc = getExtension<font::FontController>()) {
		fc->flushPendingGlyphs(this);
	}
#endif
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

// Connection send facade: the base has no connection, so everything fails; subclasses route
// through their active connection.
bool AppThread::remoteSendCbor(remote::Domain, uint8_t, const Value &, uint32_t *) { return false; }
bool AppThread::remoteSendRaw(remote::Domain, uint8_t, BytesView, uint32_t *) { return false; }
bool AppThread::remoteSendCborReply(uint32_t, remote::Domain, uint8_t, const Value &) {
	return false;
}
bool AppThread::remoteSendError(remote::Domain, uint8_t, uint32_t) { return false; }
bool AppThread::remoteSendCborWithReply(remote::Domain, uint8_t, const Value &, ReplyCallback &&,
		uint64_t) {
	return false;
}

size_t AppThread::cancelOutgoingTransfers() {
	return _blockTransfer ? _blockTransfer->cancelAllTransfers() : 0;
}

void AppThread::waitForReply(uint32_t serial,
		Function<void(const remote::MessageHeader &, BytesView payload)> &&cb, uint64_t timeoutUs) {
	uint64_t deadline = timeoutUs ? sp::platform::clock(ClockType::Monotonic) + timeoutUs : 0;
	_replies.wait(serial, sp::move(cb), deadline);
}

Rc<sprt::dispatch::Handle> AppThread::watchTransport(sprt::dispatch::NativeHandle handle,
		remote::TransportWaitAddress wait, Function<void()> &&cb) {
	if (handle.fd >= 0) {
		return _appLooper->listenPollableHandle(handle, sprt::dispatch::PollFlags::In,
				[cb = sp::move(cb)](auto, auto) -> Status {
			cb();
			return Status::Ok;
		}, this);
	}
	if (wait.address) {
		return _appLooper->waitOnAddress(wait.address, wait.value,
				[cb = sp::move(cb)](uint32_t) -> Status {
			cb();
			return Status::Ok;
		}, this);
	}
	return nullptr;
}

bool AppThread::failTimedOutRequests() {
	// The waiter is told the error came from the peer role that owed the reply.
	return _replies.failExpired(sp::platform::clock(ClockType::Monotonic),
			isServerThread() ? remote::MessageType::ClientError : remote::MessageType::ServerError);
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
	// Clamp the frame delta: a backgrounded browser tab throttles the worker clock, and a delta of
	// many seconds would push animations far past their intervals.
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
	return _replies.dispatch(h, payload);
}

} // namespace stappler::xenolith
