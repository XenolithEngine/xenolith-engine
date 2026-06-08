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

#include "XLClientAppThread.h"
#include "XLClientContext.h"
#include "XLRemoteConnector.h"
#include "XLRemoteProtocol.h"
#include "XLRemoteSerialize.h"
#include "XLContext.h"
#include "SPSharedModule.h"
#include "XLDirector.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Keepalive: disconnect if the server has not pinged us within this window (it pings ~1/s).
static constexpr uint64_t kKeepalivePingTimeoutUs = 5'000'000; // 5s

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ClientAppThread::~ClientAppThread() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool ClientAppThread::init(NotNull<ClientContext> ctx) {
	_clientContext = ctx;
	return true;
}

void ClientAppThread::run() { AppThread::wrap(); }

bool ClientAppThread::worker() {
	if (!_connection) {
		const auto &addr = _clientContext->getServerAddress();
		if (addr.empty()) {
			log::source().error("ClientAppThread", "no server address set; nothing to connect to");
			return false;
		}

		log::source().info("ClientAppThread", "connecting to ", addr.description());

		auto conn = remote::ClientConnection::connect(addr);
		if (!conn) {
			log::source().error("ClientAppThread", "failed to connect to ", addr.description());
			return false;
		}

		// Connected: run the X11-like setup handshake (auth + window info + dictionary negotiation).
		auto code = conn->handshake(_clientContext->getBearerKey(),
				_clientContext->getSuggestedDictionary());
		if (code != remote::ErrorCode::Ok) {
			log::source().error("ClientAppThread", "handshake failed (status ",
					uint32_t(toInt(code)), ")");
			conn->close();
			return false; // give up: the thread loop ends and the client process exits
		}

		log::source().info("ClientAppThread", "authenticated");
		_connection = conn;

		// Drive the async message-dispatch loop: socket readiness gives a prompt wakeup; QUIC timers
		// are pumped from performAppUpdate (same appUpdateInterval cadence).
		_listenPoll = _appLooper->listenPollableHandle(_connection->getPollFd(),
				sprt::dispatch::PollFlags::In,
				[this](sprt::dispatch::NativeHandle, sprt::dispatch::PollFlags) -> Status {
			pumpConnection();
			return Status::Ok;
		}, this);

		// Kick off the ping/pong exchange with one control ping.
		_connection->ping();

		// Start the keepalive clock: the server pings us ~1/s; if it goes silent we disconnect + exit.
		_lastPingTime = sp::platform::clock(ClockType::Monotonic);
	}

	// Run the looper (update timer + connection poll handle) until the thread stops. The connection
	// loop lives here now -- the client is no longer single-shot.
	return AppThread::worker();
}

const ContextInfo *ClientAppThread::getContextInfo() const { return _clientContext->getInfo(); }

void ClientAppThread::handleThreadInitialized() { _clientContext->handleAppThreadCreated(this); }

void ClientAppThread::handleThreadDisposed() { _clientContext->handleAppThreadDestroyed(this); }

void ClientAppThread::handleThreadUpdated(const UpdateTime &time) {
	_clientContext->handleAppThreadUpdate(this, time);
}

void ClientAppThread::handleWindowDisconnected(NotNull<RemoteWindow> w) {
	_clientContext->handleWindowDisconnected(this, w);
}

void ClientAppThread::handleWindowConnected(NotNull<RemoteWindow> w) {
	_clientContext->handleWindowConnected(this, w);
}

void ClientAppThread::loadExtensions() {
	AppThread::loadExtensions();

	// TODO(remote transport): construct the client-side FontController whose gAPI endpoints point at
	// proxy functions over the render-session channel, plus any other context-level extensions.
}

void ClientAppThread::pumpConnection() {
	if (!_connection) {
		return;
	}
	// Drain buffered frames and hand each (header, payload) to the dispatcher; the reader keeps any
	// message the dispatcher defers (returns false) for a later poll.
	_connection->poll([this](const remote::MessageHeader &h, BytesView payload) -> bool {
		return dispatchMessage(h, payload);
	});

	// Keepalive: the server pings us ~1/s (resetting _lastPingTime). If it has gone silent past the
	// timeout, the server is gone -- disconnect and end the client (stop() unwinds the looper, worker()
	// returns, and the client process exits).
	if (_connection
			&& sp::platform::clock(ClockType::Monotonic) - _lastPingTime
					>= kKeepalivePingTimeoutUs) {
		log::source().info("ClientAppThread",
				"server keepalive timeout (no ping for 5s); disconnecting");
		if (_listenPoll) {
			_listenPoll->cancel();
			_listenPoll = nullptr;
		}
		_connection->close();
		_connection = nullptr;
		stop();
	}
}

void ClientAppThread::readFromClipboard(Function<void(Status, BytesView, StringView)> &&cb,
		Function<StringView(SpanView<StringView>)> &&, Ref *ref) {
	// TODO(remote transport): route clipboard read to the remote server. Decline for now, honoring
	// the contract that the data callback runs in this thread.
	performOnAppThread([cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		cb(Status::Declined, BytesView(), StringView());
		ref = nullptr;
	}, this);
}

void ClientAppThread::probeClipboard(Function<void(Status, SpanView<StringView>)> &&cb, Ref *ref) {
	// TODO(remote transport): route clipboard probe to the remote server.
	performOnAppThread([cb = sp::move(cb), ref = Rc<Ref>(ref)]() mutable {
		cb(Status::Declined, SpanView<StringView>());
		ref = nullptr;
	}, this);
}

void ClientAppThread::writeToClipboard(BytesView, StringView, Ref *, StringView) {
	// TODO(remote transport): forward clipboard contents to the remote server.
}

void ClientAppThread::writeToClipboard(sprt::window::Function<sprt::window::Bytes(StringView)> &&,
		SpanView<StringView>, Ref *, StringView) {
	// TODO(remote transport): forward clipboard contents to the remote server.
}

void ClientAppThread::acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&, Ref *) {
	// TODO(remote transport): request screen info from the remote server.
}

void ClientAppThread::openUrl(StringView) {
	// TODO(remote transport): forward URL open request to the remote server.
}

void ClientAppThread::performAppUpdate(const UpdateTime &time, bool wakeup) {
	AppThread::performAppUpdate(time, wakeup);

	pumpConnection();
}

bool ClientAppThread::dispatchMessage(const remote::MessageHeader &h, BytesView payload) {
	if (remote::Domain(h.domain) == remote::Domain::Global) {
		switch (remote::GlobalCode(h.code)) {
		case remote::GlobalCode::Ping:
			log::source().info("ClientAppThread", "received ping (serial ", h.serial,
					"); replying pong");
			_lastPingTime = sp::platform::clock(ClockType::Monotonic);
			if (_connection) {
				_connection->pong(h.serial);
			}
			return true;
		case remote::GlobalCode::Pong:
			log::source().info("ClientAppThread", "received pong (serial ", h.serial, ")");
			return true;
		case remote::GlobalCode::SharedObjectsAnnounce:
			handleAnnounce(data::read<Interface>(payload));
			return true;
		default:
			log::source().warn("ClientAppThread", "unhandled global message (code ",
					uint32_t(h.code), ")");
			return true; // consume unknown control messages (don't defer indefinitely)
		}
	}
	log::source().warn("ClientAppThread", "unhandled message domain (", uint32_t(h.domain), ")");
	return true;
}

Rc<Director> ClientAppThread::makeDirector(NotNull<RemoteWindow> w,
		const core::FrameConstraints &c) {
	Rc<Scene> scene = makeScene(w, c);
	if (!scene) {
		return nullptr;
	}

	auto director = Rc<Director>::create(this, c, w);
	director->runScene(move(scene));
	return director;
}

Rc<Scene> ClientAppThread::makeScene(NotNull<RemoteWindow> w, const core::FrameConstraints &c) {
	Rc<Scene> scene;
	auto makeSceneSymbol = SharedModule::acquireTypedSymbol<Context::SymbolMakeSceneSignature>(
			buildconfig::MODULE_APPCOMMON_NAME, Context::SymbolMakeSceneName);
	if (makeSceneSymbol) {
		scene = makeSceneSymbol(this, w, c);
	}
	if (!scene) {
		log::source().error("AppThread", "Fail to create scene for the window");
		return nullptr;
	}
	return scene;
}

void ClientAppThread::handleAnnounce(const Value &data) {
	Map<uint64_t, AppQueueInfo> queues;
	Map<uint64_t, Value> windows;

	for (auto &it : data.asDict()) {
		if (it.first == "queues") {
			for (auto &qIt : it.second.asArray()) {
				auto id = static_cast<uint64_t>(qIt.getInteger(0));
				auto name = qIt.getString(1);

				queues.emplace(id, AppQueueInfo{id, sp::move(name)});
			}
		} else if (it.first == "windows") {
			for (auto &wIt : it.second.asArray()) {
				auto id = static_cast<uint64_t>(wIt.getInteger(0));
				windows.emplace(id, sp::move(wIt));
			}
		}
	}

	Vector<Rc<RemoteWindow>> connectedWindows;
	Vector<Rc<RemoteWindow>> disconnectedWindows;

	{
		for (auto &it : windows) {
			if (_windows.find(it.first) == _windows.end()) {
				if (auto w = Rc<RemoteWindow>::create(it.second)) {
					_windows.emplace(it.first, w);
					connectedWindows.emplace_back(w);
				}
			}
		}

		auto it = _windows.begin();
		while (it != _windows.end()) {
			if (windows.find(it->first) == windows.end()) {
				disconnectedWindows.emplace_back(it->second);
				it = _windows.erase(it);
			} else {
				++it;
			}
		}
	}

	{
		for (auto &it : queues) {
			if (_queues.find(it.first) == _queues.end()) {
				_queues.emplace(it.first, sp::move(it.second));
			}
		}

		auto it = _queues.begin();
		while (it != _queues.end()) {
			if (queues.find(it->first) == queues.end()) {
				it = _queues.erase(it);
			} else {
				++it;
			}
		}
	}

	performOnAppThread([this, connectedWindows, disconnectedWindows] {
		for (auto &it : disconnectedWindows) { handleWindowDisconnected(it); }

		for (auto &it : connectedWindows) {
			auto wIt = _windows.find(it->getServerId());
			if (wIt != _windows.end()) {
				handleWindowConnected(wIt->second);
			}
		}
	}, this, true);
}

} // namespace stappler::xenolith
