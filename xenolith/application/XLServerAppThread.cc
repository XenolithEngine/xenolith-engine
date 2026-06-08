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

#include "XLServerAppThread.h"
#include "XLContext.h"
#include "SPSharedModule.h"
#include "XLAppWindow.h"
#include "XLRemoteListener.h"
#include "XLRemoteProtocol.h"
#include "XLRemoteRenderClient.h"
#include "XLDirector.h"
#include "XLScene.h"
#include "XLCorePresentationEngine.h"

#include <sprt/runtime/dispatch/handle.h>

#if MODULE_XENOLITH_FONT

#include "XLFontComponent.h"

#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Keepalive cadence: ping the connected client this often, and terminate it if it has not answered a
// pong within the timeout.
static constexpr uint64_t kKeepalivePingIntervalUs = 1'000'000; // 1s
static constexpr uint64_t kKeepalivePongTimeoutUs = 5'000'000; // 5s

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ServerAppThread::~ServerAppThread() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool ServerAppThread::init(NotNull<Context> ctx) {
	_context = ctx;
	return true;
}

const ContextInfo *ServerAppThread::getContextInfo() const { return _context->getInfo(); }

core::Loop *ServerAppThread::getGlLoop() const {
	return static_cast<core::Loop *>(_context->getGlLoop());
}

void ServerAppThread::handleThreadInitialized() { _context->handleAppThreadCreated(this); }

void ServerAppThread::handleThreadDisposed() { _context->handleAppThreadDestroyed(this); }

void ServerAppThread::handleThreadUpdated(const UpdateTime &time) {
	_context->handleAppThreadUpdate(this, time);
}

void ServerAppThread::readFromClipboard(Function<void(Status, BytesView, StringView)> &&cb,
		Function<StringView(SpanView<StringView>)> &&tcb, Ref *ref) {
	_context->performOnThread(
			[this, cb = sp::move(cb), tcb = sp::move(tcb), ref = Rc<Ref>(ref)]() mutable {
		_context->readFromClipboard(
				[this, cb = sp::move(cb), ref = sp::move(ref)](Status st, BytesView data,
						StringView type) mutable {
			performOnAppThread(
					[st, data = data.bytes<Interface>(), type = type.str<Interface>(),
							cb = sp::move(cb), ref = move(ref)]() mutable {
				cb(st, data, type);
				ref = nullptr;
			},
					this);
		},
				sp::move(tcb), this);
	}, this);
}

void ServerAppThread::probeClipboard(Function<void(Status, SpanView<StringView>)> &&cb, Ref *ref) {
	_context->performOnThread([this, cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		auto st = _context->probeClipboard(
				[this, cb = sp::move(cb), ref = sp::move(ref)](Status st,
						SpanView<StringView> types) mutable {
			Vector<String> typesData;
			typesData.reserve(types.size());
			for (auto it : types) { typesData.emplace_back(it.str<Interface>()); }
			performOnAppThread(
					[st, types = sp::move(typesData), cb = sp::move(cb),
							ref = move(ref)]() mutable {
				Vector<StringView> typesData;
				typesData.reserve(types.size());
				for (auto &it : types) { typesData.emplace_back(it); }
				cb(st, typesData);
				ref = nullptr;
			},
					this);
		},
				this);
		if (st != Status::Ok) {
			performOnAppThread([st, cb = sp::move(cb), ref = move(ref)]() mutable {
				cb(st, SpanView<StringView>());
				ref = nullptr;
			}, this);
		}
	}, this);
}

void ServerAppThread::writeToClipboard(BytesView data, StringView contentType, Ref *ref,
		StringView label) {
	_context->performOnThread(
			[this, data = data.bytes<sprt::window::Bytes>(), type = contentType.str<Interface>(),
					ref = Rc<Ref>(ref), label = label.str<Interface>()]() mutable {
		_context->writeToClipboard(
				[data = sp::move(data), t = type](StringView type) -> sprt::window::Bytes {
			if (t == type) {
				return data;
			}
			return sprt::window::Bytes();
		}, makeSpanView(&type, 1), ref, label);
	},
			this);
}

void ServerAppThread::writeToClipboard(sprt::window::Function<sprt::window::Bytes(StringView)> &&cb,
		SpanView<StringView> types, Ref *ref, StringView label) {
	Vector<String> vtypes;
	vtypes.reserve(types.size());
	for (auto &it : types) { vtypes.emplace_back(it.str<Interface>()); }
	_context->performOnThread(
			[this, cb = sp::move(cb), vtypes = sp::move(vtypes), ref = Rc<Ref>(ref),
					label = label.str<Interface>()]() mutable {
		_context->writeToClipboard(sp::move(cb), vtypes, ref, label);
	},
			this);
}

void ServerAppThread::acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&cb, Ref *ref) {
	_context->performOnThread([this, cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		auto info = _context->getScreenInfo();
		performOnAppThread([cb = sp::move(cb), ref = move(ref), info = move(info)]() mutable {
			cb(info);
			ref = nullptr;
			info = nullptr;
		}, this);
	}, this);
}

void ServerAppThread::openUrl(StringView str) {
	_context->performOnThread([str = str.str<Interface>(), ctx = _context] { ctx->openUrl(str); },
			_context);
}

Rc<Director> ServerAppThread::handleAppWindowCreated(NotNull<AppWindow> w,
		const core::FrameConstraints &c) {
	log::source().info("AppThread", "handleAppWindowCreated");

	addListener(w, [w](const UpdateTime &, bool wakeup) {
		if (wakeup) {
			w->setReadyForNextFrame();

			// force display link to update views
			w->update(core::PresentationUpdateFlags::DisplayLink);
		}
	});

	auto dir = makeDirector(w, c);
	if (dir) {
		_windows.emplace(w.get());
	}
	return dir;
}

void ServerAppThread::handleAppWindowDestroyed(NotNull<AppWindow> w, Rc<Director> &&d) {
	log::source().info("AppThread", "handleAppWindowDestroyed");

	_sharedObjects->drop(w);

	if (d) {
		if (shouldPreserveDirector(w, d)) {
			d->setServer(nullptr);
			preserveDirector(w, sp::move(d));
		} else {
			d->end();
		}
	}
	removeListener(w);
	_windows.erase(w.get());

	if (_windows.empty()) {
		// In practice, listening is started by loader Scene, that rxist if at least one window exists;
		// If no window exists - we should stop listening, or app can not be closed properly;
		// If new window will be spawned - it's loader scene can restart listening
		stopListening();
	}
}

bool ServerAppThread::isServerThread() const { return true; }

bool ServerAppThread::isListening() const { return _listener && _listener->isOpen(); }

bool ServerAppThread::setListenAddress(StringView addr) {
	auto newAddr = remote::Address::parse(addr);
	if (newAddr != _listenAddress) {
		if (_listener && _listener->isOpen()) {
			log::error("ServerAppThread", "Fail to assign listen address: listener already active");
			return false;
		}
		_listenAddress = newAddr;
	}
	return true;
}

bool ServerAppThread::shareWindow(AppWindow *w) {
	if (!isListening()) {
		if (_listenAddress.empty() || _expectedKey.empty()) {
			log::error("ServerAppThread",
					"Listen address and credentials should be set before sharing anything");
			return false;
		}

		if (!startListening()) {
			log::error("ServerAppThread", "Fail to start listener for shareWindow");
			return false;
		}
	}

	(void)_sharedObjects->getId(w);
	return true;
}

bool ServerAppThread::shareQueue(core::Queue *queue) {
	if (!isListening()) {
		if (_listenAddress.empty() || _expectedKey.empty()) {
			log::error("ServerAppThread",
					"Listen address and credentials should be set before sharing anything");
			return false;
		}

		if (!startListening()) {
			log::error("ServerAppThread", "Fail to start listener for shareQueue");
			return false;
		}
	}

	(void)_sharedObjects->getId(queue);
	return true;
}

bool ServerAppThread::setBearerKey(BytesView key) {
	if (isListening()) {
		log::error("ServerAppThread", "Fail to assign bearer key: listener already active");
		return false;
	}
	_expectedKey = key.bytes<Interface>();
	return true;
}

bool ServerAppThread::setCompressionDictionary(BytesView d) {
	if (isListening()) {
		log::error("ServerAppThread",
				"Fail to assign compression dictionary: listener already active");
		return false;
	}
	_dictionary = d.bytes<Interface>();
	return true;
}

bool ServerAppThread::startListening() {
	if (_listener && _listener->isOpen()) {
		log::error("AppThread", "startListening: already listening");
		return false;
	}
	if (_listenAddress.empty()) {
		log::source().error("AppThread", "startListening: no listen address set");
		return false;
	}
	if (_expectedKey.empty()) {
		log::source().error("AppThread", "startListening: no bearer key");
		return false;
	}
	_listener = Rc<remote::Listener>::alloc();
	if (!_listener || !_listener->open(_listenAddress)) {
		log::error("AppThread", "startListening: fail to start listening on address");
		_listener = nullptr;
		return false;
	}

	if (_sharedObjects) {
		_sharedObjects.clear();
		_sharedObjects = nullptr;
	}

	_sharedObjects = Rc<remote::ObjectRegistry>::create();

	// Drive accept on this looper: readiness on the listener socket gives a prompt wakeup; QUIC's
	// internal timers are pumped from performAppUpdate() (same appUpdateInterval cadence as the main
	// update timer), so no separate listen timer is needed.
	_listenPoll =
			_appLooper->listenPollableHandle(_listener->getPollFd(), sprt::dispatch::PollFlags::In,
					[this](sprt::dispatch::NativeHandle, sprt::dispatch::PollFlags) -> Status {
		pumpListener();
		return Status::Ok;
	}, this);
	pumpListener();
	return true;
}

bool ServerAppThread::stopListening() {
	if (_sharedObjects) {
		_sharedObjects.clear();
		_sharedObjects = nullptr;
	}

	if (_listenPoll) {
		_listenPoll->cancel();
		_listenPoll = nullptr;
	}
	if (_remoteClient) {
		_remoteClient->closeConnection();
	}
	if (_listener) {
		_listener->close();
		_listener = nullptr;
	}
	return true;
}

void ServerAppThread::pumpListener() {
	if (!_listener) {
		return;
	}
	// Pump the listener first: read the socket, route datagrams to the active connection, and accept
	// any new ones.
	_listener->handleEvents([this](Rc<remote::ServerConnection> &&conn) {
		handleRemoteConnection(sp::move(conn));
	});
	// Then service the active connection and detect a disconnect, freeing the single-connection slot
	// so a new client can connect after the previous one went away.
	if (_remoteClient && _remoteClient->isClosed()) {
		log::source().info("AppThread", "remote client disconnected; window reverts to fallback");
		_remoteClient->closeConnection();
		_remoteClient = nullptr; // free the single-connection slot for a new client
	}

	// Run the setup handshake for a freshly accepted connection (bounded, synchronous).
	if (_pendingConnection) {
		completePendingHandshake();
	}

	// Drain + dispatch any messages from the active connection; deferred ones (cb returns false) stay
	// queued in the connection's reader for a later poll.
	if (_remoteClient) {
		if (auto conn = _remoteClient->getConnection()) {
			conn->poll([this](const remote::MessageHeader &h, BytesView payload) -> bool {
				return dispatchMessage(h, payload);
			});
		}
	}

	// Keepalive: a pong resets _lastPongTime (see dispatchMessage). If the client has not answered for
	// kKeepalivePongTimeoutUs, terminate it (drops the connection, freeing the slot for a new client);
	// otherwise send a ping at most every kKeepalivePingIntervalUs.
	if (_remoteClient && !_remoteClient->isClosed()) {
		auto now = sp::platform::clock(ClockType::Monotonic);
		if (now - _lastPongTime >= kKeepalivePongTimeoutUs) {
			log::source().info("AppThread",
					"client keepalive timeout (no pong for 5s); terminating connection");
			_remoteClient->closeConnection();
			_remoteClient = nullptr;
		} else if (now - _lastPingTime >= kKeepalivePingIntervalUs) {
			if (auto conn = _remoteClient->getConnection()) {
				conn->ping();
			}
			_lastPingTime = now;
		}
	}
}

bool ServerAppThread::dispatchMessage(const remote::MessageHeader &h, BytesView) {
	if (remote::Domain(h.domain) == remote::Domain::Global) {
		switch (remote::GlobalCode(h.code)) {
		case remote::GlobalCode::Ping: {
			log::source().info("AppThread", "received ping (serial ", h.serial, "); replying pong");
			auto conn = _remoteClient ? _remoteClient->getConnection() : nullptr;
			if (conn) {
				conn->pong(h.serial);
			}
			return true;
		}
		case remote::GlobalCode::Pong:
			log::source().info("AppThread", "received pong (serial ", h.serial, ")");
			_lastPongTime = sp::platform::clock(ClockType::Monotonic);
			return true;
		default:
			log::source().warn("AppThread", "unhandled global message (code ", uint32_t(h.code),
					")");
			return true; // consume unknown control messages (don't defer indefinitely)
		}
	}
	log::source().warn("AppThread", "unhandled message domain (", uint32_t(h.domain), ")");
	return true;
}

void ServerAppThread::handleRemoteConnection(Rc<remote::ServerConnection> &&conn) {
	if (_remoteClient || _pendingConnection) {
		log::source().warn("AppThread",
				"remote client already connected; rejecting new connection (single connection)");
		return; // conn dropped
	}
	// Defer the handshake to pumpListener so it doesn't run nested inside the accept callback.
	_pendingConnection = sp::move(conn);
}

void ServerAppThread::completePendingHandshake() {
	auto conn = sp::move(_pendingConnection);
	_pendingConnection = nullptr;

	auto status = conn->handshake(_expectedKey, _dictionary);
	if (status != remote::ErrorCode::Ok) {
		log::source().error("AppThread", "client handshake failed (status ",
				uint32_t(toInt(status)), "); dropping connection");
		return; // conn dropped, slot stays free
	}

	log::source().info("AppThread", "client authenticated");

	_remoteClient = Rc<RemoteRenderClient>::create(sp::move(conn));
	if (_remoteClient) {
		_remoteClient->announce(_sharedObjects);
		// Start the keepalive clock fresh so the timeout is measured from connection establishment.
		_lastPingTime = _lastPongTime = sp::platform::clock(ClockType::Monotonic);
	}
}

void ServerAppThread::performAppUpdate(const UpdateTime &time, bool wakeup) {
	AppThread::performAppUpdate(time, wakeup);

	// Service the remote listener's QUIC timers on the regular app-update cadence (no-op unless a
	// scene started listening). Socket readiness is handled promptly via _listenPoll. The keepalive
	// ping/timeout is driven from pumpListener.
	pumpListener();
}

void ServerAppThread::loadExtensions() {
	AppThread::loadExtensions();

#if MODULE_XENOLITH_FONT
	auto createFontController = SharedModule::acquireTypedSymbol<
			decltype(&font::FontComponent::createDefaultController)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "FontComponent::createDefaultController");

	if (createFontController) {
		auto comp = _context->getComponent<font::FontComponent>();
		if (comp) {
			if (auto controller =
							createFontController(comp, _appLooper, "ApplicationFontController")) {
				addExtension(move(controller));
			}
		}
	}
#endif
}

bool ServerAppThread::shouldPreserveDirector(NotNull<AppWindow> w, NotNull<Director>) {
	return hasFlag(w->getCapabilities(), WindowCapabilities::PreserveDirector);
}

void ServerAppThread::preserveDirector(NotNull<AppWindow> w, Rc<Director> &&d) {
	_preservedDirectors.emplace(w->getId().str<Interface>(), sp::move(d));
}

bool ServerAppThread::hasPreservedDirector(NotNull<AppWindow> w) {
	auto it = _preservedDirectors.find(w->getId().str<Interface>());
	if (it != _preservedDirectors.end()) {
		return true;
	}
	return false;
}

Rc<Director> ServerAppThread::acquirePreservedDirector(NotNull<AppWindow> w) {
	auto it = _preservedDirectors.find(w->getId().str<Interface>());
	if (it != _preservedDirectors.end()) {
		auto d = sp::move(it->second);
		_preservedDirectors.erase(it);
		return d;
	}
	return nullptr;
}

Rc<Director> ServerAppThread::makeDirector(NotNull<AppWindow> w, const core::FrameConstraints &c) {
	if (hasPreservedDirector(w)) {
		auto d = acquirePreservedDirector(w);
		if (d) {
			d->setServer(w);
			return d;
		}
	}

	Rc<Scene> scene = makeScene(w, c);
	if (!scene) {
		return nullptr;
	}

	auto director = Rc<Director>::create(this, c, w);
	director->runScene(move(scene));
	return director;
}

Rc<Scene> ServerAppThread::makeScene(NotNull<AppWindow> w, const core::FrameConstraints &c) {
	Rc<Scene> scene;
	auto makeSceneSymbol = SharedModule::acquireTypedSymbol<Context::SymbolMakeSceneSignature>(
			buildconfig::MODULE_APPCOMMON_NAME, Context::SymbolMakeSceneName);
	if (makeSceneSymbol) {
		scene = makeSceneSymbol(this, w, c);
	}
	if (!scene) {
		log::source().error("AppThread", "Fail to create scene for the window '", w->getId(), "'");
		return nullptr;
	}
	return scene;
}

} // namespace stappler::xenolith
