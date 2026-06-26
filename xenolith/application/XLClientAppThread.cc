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
#include "XLCoreAttachment.h" // core::DependencyEvent id mask
#include "XLCoreInfo.h" // core::ImageInfoData / ImageFormat / Extent3
#include "XLRemoteBlockTransfer.h"

#if MODULE_XENOLITH_FONT
// Downstream module: reached only by SharedModule symbol + the font::FontController extension type.
#include "XLFontControllerRemote.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Top bit of the DependencyEvent id space reserved for the remote client so its ids never collide
// with server/local-minted ones (which use the low half).
static constexpr uint32_t kClientDependencyEventMask = 0x80000000u;

// Keepalive: disconnect if the server has not pinged us within this window (it pings ~1/s).
static constexpr uint64_t kKeepalivePingTimeoutUs = 5'000'000; // 5s

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ClientAppThread::~ClientAppThread() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool ClientAppThread::init(NotNull<ClientContext> ctx) {
	_clientContext = ctx;
	// This process is the remote client: mint DependencyEvent ids in the high half of the id space.
	core::DependencyEvent::SetIdGenerationMask(kClientDependencyEventMask);
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
		if (code != remote::GlobalError::Ok) {
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

		_sharedObjects = Rc<remote::ObjectFactory>::create();
	}

	// Run the looper (update timer + connection poll handle) until the thread stops. The connection
	// loop lives here now -- the client is no longer single-shot.
	return AppThread::worker();
}

const ContextInfo *ClientAppThread::getContextInfo() const { return _clientContext->getInfo(); }

bool ClientAppThread::sendMessageWithReply(remote::Domain d, uint8_t message, const Value &val,
		Function<void(const remote::MessageHeader &, BytesView payload)> &&cb, uint64_t timeoutUs) {
	if (!_connection || !_connection->isOpen()) {
		return false;
	}

	uint32_t serial = 0;
	if (_connection->sendCborMessage(d, message, val, &serial) == remote::GlobalError::Ok) {
		waitForReply(serial, sp::move(cb), timeoutUs);
		return true;
	}
	return false;
}

bool ClientAppThread::remoteSendCbor(remote::Domain d, uint8_t code, const Value &v,
		uint32_t *outSerial) {
	if (!_connection || !_connection->isOpen()) {
		return false;
	}
	return _connection->sendCborMessage(d, code, v, outSerial) == remote::GlobalError::Ok;
}

bool ClientAppThread::remoteSendRaw(remote::Domain d, uint8_t code, BytesView b,
		uint32_t *outSerial) {
	if (!_connection || !_connection->isOpen()) {
		return false;
	}
	return _connection->sendMessage(d, code, b, outSerial) == remote::GlobalError::Ok;
}

bool ClientAppThread::remoteSendCborReply(uint32_t serial, remote::Domain d, uint8_t code,
		const Value &v) {
	if (!_connection || !_connection->isOpen()) {
		return false;
	}
	return _connection->sendCborReply(serial, d, code, v) == remote::GlobalError::Ok;
}

bool ClientAppThread::remoteSendError(remote::Domain d, uint8_t code, uint32_t serial) {
	if (!_connection || !_connection->isOpen()) {
		return false;
	}
	// ClientConnection::sendError takes a typed GlobalError code (unlike the server's uint8_t).
	return _connection->sendError(d, remote::GlobalError(code), serial) == remote::GlobalError::Ok;
}

bool ClientAppThread::remoteSendCborWithReply(remote::Domain d, uint8_t code, const Value &v,
		Function<void(const remote::MessageHeader &, BytesView payload)> &&cb, uint64_t timeoutUs) {
	return sendMessageWithReply(d, code, v, sp::move(cb), timeoutUs);
}

void ClientAppThread::handleThreadInitialized() { _clientContext->handleAppThreadCreated(this); }

void ClientAppThread::handleThreadDisposed() { _clientContext->handleAppThreadDestroyed(this); }

void ClientAppThread::handleThreadUpdated(const UpdateTime &time) {
	_clientContext->handleAppThreadUpdate(this, time);
}

bool ClientAppThread::handleWindowConnected(NotNull<RemoteWindow> w) {
	return _clientContext->handleWindowConnected(this, w);
}

void ClientAppThread::handleWindowDisconnected(NotNull<RemoteWindow> w) {
	_clientContext->handleWindowDisconnected(this, w);
}

void ClientAppThread::loadExtensions() {
	AppThread::loadExtensions();

	// Accept incoming Screenshot block transfers (the server's response to our RequestScreenshot) and
	// route the assembled raw pixels to the RemoteWindow whose captureScreenshot() call triggered them,
	// matched by the announce reason's serial.
	if (_blockTransfer) {
		_blockTransfer->acceptPolicy = [](BlockTransferManager::DataType t, uint64_t, const Value &,
											   const Value &) {
			return t == remote::DataType::Screenshot;
		};
		_blockTransfer->onReceived = [this](uint64_t id, BlockTransferManager::DataType t,
											   const Value &meta, const Value &reason, BytesView data) {
			if (t != remote::DataType::Screenshot) {
				return;
			}
			core::ImageInfoData info;
			info.format = core::ImageFormat(meta.getInteger("fmt"));
			info.extent = Extent3(uint32_t(meta.getInteger("w")), uint32_t(meta.getInteger("h")),
					uint32_t(meta.getInteger("d")));
			auto serial = uint32_t(reason.getInteger("serial"));
			for (auto &it : _windows) {
				if (it.second->deliverScreenshot(serial, info, data)) {
					return;
				}
			}
			log::source().warn("ClientAppThread", "screenshot transfer ", id,
					" had no matching captureScreenshot() waiter (serial ", serial, ")");
		};
	}

#if MODULE_XENOLITH_FONT
	// Construct the headless client-side FontController (positioning + source announce + glyph requests
	// over remote::Domain::Font). Reached via SharedModule symbol because xenolith_font is downstream of
	// this module; registered under font::FontController so Labels' getExtension<FontController>() find it.
	auto createRemoteController = SharedModule::acquireTypedSymbol<
			decltype(&font::FontControllerRemote::createRemoteController)>(
			buildconfig::MODULE_XENOLITH_FONT_NAME, "FontControllerRemote::createRemoteController");
	if (createRemoteController) {
		if (auto controller = createRemoteController(this)) {
			addExtension(move(controller));
		}
	}
#endif
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

	bool disconnect = false;

	// Request watchdog (same cadence as keepalive): if the server left one of our requests unanswered
	// past that request's own reply deadline, the waiter was just failed with a local protocol error --
	// treat the server as gone and end the client.
	if (_connection && failTimedOutRequests()) {
		log::source().info("ClientAppThread",
				"request reply timeout; disconnecting from unresponsive server");
		disconnect = true;
	}

	// Keepalive: the server pings us ~1/s (resetting _lastPingTime). If it has gone silent past the
	// timeout, the server is gone -- disconnect and end the client (stop() unwinds the looper, worker()
	// returns, and the client process exits).
	//
	// Both checks are driven by AppThread's internal Looper timer (scheduleTimer, interval =
	// ContextInfo::appUpdateInterval, default 1s, count = Infinite -> performAppUpdate -> pumpConnection),
	// NOT by frame timing -- the client has no gapi loop / presentation cadence at all. So the timeouts
	// are evaluated at a steady ~1s regardless of whether the scene is animating or idle. Socket readiness
	// also calls pumpConnection, but only the timer guarantees this runs while no datagrams arrive.
	if (!disconnect && _connection
			&& sp::platform::clock(ClockType::Monotonic) - _lastPingTime
					>= kKeepalivePingTimeoutUs) {
		log::source().info("ClientAppThread",
				"server keepalive timeout (no ping for 5s); disconnecting");
		disconnect = true;
	}

	if (disconnect && _connection) {
		if (_blockTransfer) {
			_blockTransfer->reset();
		}
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
	if (AppThread::dispatchMessage(h, payload)) {
		return true;
	}

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
	} else if (remote::Domain(h.domain) == remote::Domain::Window) {
		switch (remote::WindowCode(h.code)) {
		case remote::WindowCode::UpdateMaterials:
			// server -> client push: apply the new MaterialSet to the mirror queue named in the blob
			if (!remote::QueueCodec::decodeMaterials(payload, *_sharedObjects)) {
				log::source().warn("ClientAppThread", "failed to apply materials update");
			}
			return true;
		case remote::WindowCode::AcquireFrame: {
			// server -> client: drive the window's scene graph to select a render queue; reply with
			// that queue's server id (per-frame attachment input is a later stage).
			auto val = data::read<Interface>(payload);
			auto frameId = uint64_t(val.getInteger(0));
			auto windowId = uint64_t(val.getInteger(1));
			auto constraints = remote::deserializeFrameConstraints(val.getValue(2));

			auto sendReply = [this, serial = h.serial, frameId](uint64_t queueId) {
				if (!_connection) {
					return;
				}
				Value reply;
				reply.addInteger(int64_t(frameId));
				reply.addInteger(int64_t(queueId));
				_connection->sendCborReply(serial, remote::Domain::Window,
						toInt(remote::WindowCode::AcquireFrame), reply);
			};

			auto wIt = _windows.find(windowId);
			if (wIt == _windows.end()) {
				log::source().warn("ClientAppThread", "AcquireFrame for unknown window ", windowId);
				sendReply(0);
				return true;
			}

			wIt->second->acquireFrame(frameId, constraints, sp::move(sendReply));
			return true;
		}
		case remote::WindowCode::InputEvents: {
			// server -> client: a raw blob [u64 windowId (network order)][InputEventData[] native layout].
			// Reconstruct the batch and replay it into the named window (its local Director -> scene).
			if (payload.size() < sizeof(uint64_t)) {
				return true;
			}
			uint64_t widN = 0;
			memcpy(&widN, payload.data(), sizeof(uint64_t));
			auto windowId = sprt::byteorder::NetworkToHost(widN);

			auto rest = payload.sub(sizeof(uint64_t));
			if (rest.size() % sizeof(core::InputEventData) != 0) {
				log::source().warn("ClientAppThread", "InputEvents: misaligned blob (", rest.size(),
						" bytes)");
				return true;
			}
			auto count = rest.size() / sizeof(core::InputEventData);
			Vector<core::InputEventData> events;
			events.resize(count);
			if (count) {
				memcpy(events.data(), rest.data(), rest.size());
			}

			auto wIt = _windows.find(windowId);
			if (wIt == _windows.end()) {
				log::source().warn("ClientAppThread", "InputEvents for unknown window ", windowId);
				return true;
			}
			log::source().info("ClientAppThread", "InputEvents: ", count, " event(s) for window ",
					windowId);
			wIt->second->handleInputEvents(sp::move(events));
			return true;
		}
		default:
			log::source().warn("ClientAppThread", "unhandled window message (code ",
					uint32_t(h.code), ")");
			return true; // consume unknown control messages (don't defer indefinitely)
		}
	} else if (remote::Domain(h.domain) == remote::Domain::Data) {
		return _blockTransfer ? _blockTransfer->dispatch(h, payload) : true;
	} else if (remote::Domain(h.domain) == remote::Domain::Font) {
#if MODULE_XENOLITH_FONT
		// Route to the client FontController (the SourcesReady reply is handled by the serial waiter, not
		// here; this path is for server->client notifications like AtlasReady).
		if (auto fc = getExtension<font::FontController>()) {
			return fc->dispatchFontMessage(h.code, h.serial, payload);
		}
#endif
		return true;
	}
	log::source().warn("ClientAppThread", "unhandled message domain (", uint32_t(h.domain), ")");
	return true;
}

void ClientAppThread::flushPendingFontGlyphs() {
#if MODULE_XENOLITH_FONT
	if (auto fc = getExtension<font::FontController>()) {
		fc->flushPendingGlyphs(this);
	}
#endif
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
				if (auto w = Rc<RemoteWindow>::create(this, it.second)) {
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
				if (handleWindowConnected(wIt->second)) {
					makeDirector(wIt->second, wIt->second->getConstraints());
				}
			}
		}
	}, this, true);
}

} // namespace stappler::xenolith
