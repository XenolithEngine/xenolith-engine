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
#include "XLTextInputManager.h" // cancel text input when the session ends
#include "XLCoreAttachment.h" // core::DependencyEvent id mask
#include "XLCoreInfo.h" // core::ImageInfoData / ImageFormat / Extent3
#include "XLRemoteBlockTransfer.h"

#if MODULE_XENOLITH_FONT
// Downstream module: reached only via SharedModule symbol and the font::FontController type.
#include "XLFontControllerRemote.h"
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith {

// Top bit of the DependencyEvent id space is reserved for the remote client, so its ids never
// collide with server/local ones (low half).
static constexpr uint32_t kClientDependencyEventMask = 0x8000'0000u;

// Keepalive: disconnect if the server has not pinged us within this window (it pings ~1/s).
static constexpr uint64_t kKeepalivePingTimeoutUs = 5'000'000; // 5s

/* Creating a window is a hop to the server's context thread plus the window system's own work; it
is NOT the window becoming drawable (that needs a presented frame and a compiled queue, which is why
the reply does not wait for it -- see the announce's creator serial). Generous, because an
unanswered request past its deadline is treated as a dead server. */
static constexpr uint64_t kCreateWindowReplyTimeoutUs = 10'000'000; // 10s

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

		if (_clientContext->getServerFingerprint().empty()) {
			log::source().warn("ClientAppThread",
					"no server fingerprint configured: the server is NOT authenticated and the "
					"bearer key is exposed to a man in the middle");
		}

		auto conn = remote::ClientConnection::connect(addr, _clientContext->getServerFingerprint());
		if (!conn) {
			log::source().error("ClientAppThread", "failed to connect to ", addr.description());
			return false;
		}

#if DEBUG
		// XL_REMOTE_HANDSHAKE_DELAY_MS=<n>: hold the connection open without a hello, the way a
		// stalled peer would, so a test can check that the server does not wait on it.
		if (auto env = ::getenv("XL_REMOTE_HANDSHAKE_DELAY_MS")) {
			auto delayMs = StringView(env).readInteger(10).get(0);
			log::source().warn("ClientAppThread", "XL_REMOTE_HANDSHAKE_DELAY_MS: waiting ", delayMs,
					"ms before the handshake");
			sp::platform::sleep(uint64_t(delayMs) * 1'000);
		}
#endif

		// Connected: run the X11-like setup handshake (auth + window info + dictionary negotiation).
		auto code = conn->handshake(_clientContext->getBearerKey(),
				_clientContext->getSuggestedDictionary());
		if (code != remote::GlobalError::Ok) {
			log::source().error("ClientAppThread", "handshake refused: ",
					remote::getGlobalErrorName(code), " (status ", uint32_t(toInt(code)), ")");
			conn->close();
			return false; // give up: the thread loop ends and the client process exits
		}

		log::source().info("ClientAppThread", "authenticated");
		_connection = conn;

		// Drive the async message-dispatch loop: socket readiness or the shared-memory doorbell gives
		// a prompt wakeup; QUIC timers are pumped from performAppUpdate (same appUpdateInterval
		// cadence).
		auto handle = _connection->getPollHandle();
		_listenPoll = watchTransport(handle,
				handle.fd >= 0 ? remote::TransportWaitAddress() : _connection->getWaitAddress(),
				[this] { pumpConnection(); });

		// Kick off the ping/pong exchange with one control ping.
		_connection->ping();

		// Start the keepalive clock: the server pings ~1/s; if it goes silent we disconnect and
		// exit.
		_lastPingTime = sp::platform::clock(ClockType::Monotonic);

		_sharedObjects = Rc<remote::ObjectFactory>::create();
	}

	// Run the looper (update timer + connection poll handle) until the thread stops.
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

void ClientAppThread::setFrameDelay(uint64_t delayUs, uint32_t frames) {
	_frameDelayUs = delayUs;
	_frameDelayFrames = frames;
	log::source().warn("ClientAppThread", "holding back the next ", frames, " frame(s) by ",
			delayUs / 1'000, "ms");
}

void ClientAppThread::setSilentFrames(uint32_t frames) {
	_silentFrames = frames;
	log::source().warn("ClientAppThread", "abandoning the next ", frames,
			" frame(s) after answering for them");
}

bool ClientAppThread::takeSilentFrame() {
	if (_silentFrames == 0) {
		return false;
	}
	--_silentFrames;
	return true;
}

bool ClientAppThread::isWindowCreationSupported() const {
	auto info = getServerInfo();
	if (!info) {
		return false;
	}
	// Two questions: the mask says this build knows the message, the feature bit says this server
	// will actually serve it (a silent peer's mask reads as "supports everything").
	return info->supports(remote::Domain::Window, toInt(remote::WindowCode::CreateWindow))
			&& hasFlag(info->features, remote::PeerFeatures::ClientWindows);
}

void ClientAppThread::createWindow(Rc<sprt::window::WindowInfo> &&info,
		Function<void(Status, StringView id)> &&complete) {
	Rc<WindowSceneInfo> handle;
	if (info) {
		if (auto payload = info->takeAppData()) {
			handle = dynamic_cast<WindowSceneInfo *>(payload.get());
			if (!handle) {
				log::source().error("ClientAppThread",
						"createWindow: WindowInfo::appData is not a WindowSceneInfo");
			}
		}
	}

	auto refuse = [&](Status st) {
		if (complete) {
			complete(st, StringView());
		}
		if (handle) {
			handle->fireClose();
		}
	};

	if (!info) {
		refuse(Status::ErrorInvalidArguemnt);
		return;
	}
	if (!isWindowCreationSupported()) {
		// Answered here: a server that does not serve window requests must not be sent one, and a
		// refusal must not cost the session.
		log::source().warn("ClientAppThread",
				"createWindow: the server does not open windows on " "request");
		refuse(Status::ErrorNotSupported);
		return;
	}

	uint32_t serial = 0;
	if (_connection
			&& _connection->sendCborMessage(remote::Domain::Window,
					   toInt(remote::WindowCode::CreateWindow),
					   remote::serializeWindowRequest(*info), &serial)
					== remote::GlobalError::Ok) {
		waitForReply(serial, [this, serial](const remote::MessageHeader &h, BytesView payload) {
			auto rec = findPendingWindow(serial, StringView());
			if (!rec) {
				return;
			}
			auto complete = sp::move(rec->complete);
			rec->complete = nullptr;

			Status st = Status::ErrorNotImplemented;
			String id;
			if (!remote::isError(h)) {
				auto val = data::read<Interface>(payload);
				st = Status(int32_t(val.getInteger("st")));
				id = val.getString("id");
				rec->grantedId = id;
			}

			if (complete) {
				complete(st, id);
			}
			// A refused request has no window coming: settle the handle now.
			if (st != Status::Ok) {
				if (auto again = findPendingWindow(serial, StringView())) {
					auto handle = again->handle;
					dropPendingWindow(serial);
					if (handle) {
						handle->fireClose();
					}
				}
			}
		}, kCreateWindowReplyTimeoutUs);

		_pendingWindows.emplace_back(
				PendingWindow{serial, sp::move(handle), sp::move(complete), String(), false});
		return;
	}

	refuse(Status::ErrorNotSupported);
}

ClientAppThread::PendingWindow *ClientAppThread::findPendingWindow(uint32_t serial, StringView id) {
	for (auto &it : _pendingWindows) {
		if (serial != 0 && it.serial == serial) {
			return &it;
		}
		// A server that does not echo the serial leaves the granted id as the only handle on it.
		if (!id.empty() && !it.grantedId.empty() && StringView(it.grantedId) == id) {
			return &it;
		}
	}
	return nullptr;
}

void ClientAppThread::dropPendingWindow(uint32_t serial) {
	for (auto it = _pendingWindows.begin(); it != _pendingWindows.end(); ++it) {
		if (it->serial == serial) {
			_pendingWindows.erase(it);
			return;
		}
	}
}

void ClientAppThread::failPendingWindows(Status st) {
	auto pending = sp::move(_pendingWindows);
	_pendingWindows.clear();
	for (auto &it : pending) {
		if (it.complete) {
			it.complete(st, StringView(it.grantedId));
		}
		if (it.handle) {
			it.handle->fireClose();
		}
	}
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
	return _connection->sendError(d, code, serial) == remote::GlobalError::Ok;
}

bool ClientAppThread::remoteSendCborWithReply(remote::Domain d, uint8_t code, const Value &v,
		Function<void(const remote::MessageHeader &, BytesView payload)> &&cb, uint64_t timeoutUs) {
	return sendMessageWithReply(d, code, v, sp::move(cb), timeoutUs);
}

void ClientAppThread::handleThreadInitialized() { _clientContext->handleAppThreadCreated(this); }

void ClientAppThread::handleThreadDisposed() {
	// Backstop for a path that did not go through the disconnect above: a handle destroyed without
	// its callback firing warns in debug builds, and that is the diagnostic worth keeping honest.
	failPendingWindows(Status::ErrorCancelled);
	_clientContext->handleAppThreadDestroyed(this);
}

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

	// Accept Screenshot block transfers (replies to RequestScreenshot) and route the raw pixels to
	// the RemoteWindow whose captureScreenshot() requested them, matched by the announce reason's
	// serial.
	if (_blockTransfer) {
		_blockTransfer->acceptPolicy = [](BlockTransferManager::DataType t, uint64_t, const Value &,
											   const Value &) {
			return t == remote::DataType::Screenshot;
		};
		_blockTransfer->onReceived = [this](uint64_t id, BlockTransferManager::DataType t,
											 const Value &meta, const Value &reason,
											 BytesView data) {
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

		// A cancelled screenshot (server cancel or dropped link) answers the waiting
		// captureScreenshot() with empty info and pixels, the same as a failed send.
		_blockTransfer->onCancelled = [this](uint64_t id, BlockTransferManager::DataType t,
											  const Value &, const Value &reason) {
			if (t != remote::DataType::Screenshot) {
				return;
			}
			auto serial = uint32_t(reason.getInteger("serial"));
			for (auto &it : _windows) {
				if (it.second->deliverScreenshot(serial, core::ImageInfoData(), BytesView())) {
					log::source().info("ClientAppThread", "screenshot transfer ", id,
							" was cancelled; its waiter was told");
					return;
				}
			}
		};
	}

#if MODULE_XENOLITH_FONT
	// Create the headless client FontController (positioning, source announce, glyph requests over
	// remote::Domain::Font) via SharedModule symbol, since xenolith_font is downstream; registered
	// as font::FontController so getExtension<FontController>() finds it.
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
	// Dispatch buffered messages; the reader keeps messages the dispatcher defers (returns false).
	_connection->poll([this](const remote::MessageHeader &h, BytesView payload) -> bool {
		return dispatchMessage(h, payload);
	});

	// A dispatcher can end the session, but must not tear the connection down inside the poll.
	bool disconnect = _disconnectRequested;

	// Request watchdog: if the server left a request unanswered past its reply deadline, the waiter
	// was already failed locally; treat the server as gone and end the client.
	if (_connection && failTimedOutRequests()) {
		log::source().info("ClientAppThread",
				"request reply timeout; disconnecting from unresponsive server");
		disconnect = true;
	}

	// Keepalive: the server pings ~1/s (resetting _lastPingTime); past the timeout, disconnect and
	// end the client. Both checks run from the AppThread update timer (appUpdateInterval), not
	// frame timing, so they are evaluated steadily while idle.
	if (!disconnect && _connection
			&& sp::platform::clock(ClockType::Monotonic) - _lastPingTime
					>= kKeepalivePingTimeoutUs) {
		log::source().info("ClientAppThread",
				"server keepalive timeout (no ping for 5s); disconnecting");
		disconnect = true;
	}

	if (disconnect && _connection) {
		/* Tell focused widgets their text input is gone: no echo can arrive after disconnect, and
		an enabled field would wait forever. cancel() delivers enabled=false, then
		releaseTextInput() no-ops on the dead connection. */
		for (auto &it : _windows) {
			if (auto dir = dynamic_cast<Director *>(it.second->getRenderClient())) {
				if (auto tm = dir->getTextInputManager()) {
					tm->cancel();
				}
			}
		}

		/* Tear the windows down as the announce would if they had simply gone away: end the
		Directors, answer the handles, and drop the maps -- a RemoteWindow references this thread,
		so leaving them here leaks both. */
		auto windows = sp::move(_windows);
		_windows.clear();
		_queues.clear();
		for (auto &it : windows) {
			if (auto dir = dynamic_cast<Director *>(it.second->getRenderClient())) {
				dir->end();
			}
			if (auto sceneInfo = it.second->getSceneInfo()) {
				sceneInfo->fireClose();
			}
			handleWindowDisconnected(it.second);
		}
		failPendingWindows(Status::ErrorCancelled);

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

void ClientAppThread::writeToClipboard(Rc<sprt::window::ClipboardData> &&) {
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
			//log::source().info("ClientAppThread", "received ping (serial ", h.serial,
			//		"); replying pong");
			_lastPingTime = sp::platform::clock(ClockType::Monotonic);
			if (_connection) {
				_connection->pong(h.serial);
			}
			return true;
		case remote::GlobalCode::Pong:
			//log::source().info("ClientAppThread", "received pong (serial ", h.serial, ")");
			return true;
		case remote::GlobalCode::SharedObjectsAnnounce:
			handleAnnounce(data::read<Interface>(payload));
			return true;
		case remote::GlobalCode::ServerInfo: handleServerInfo(h, payload); return true;
		default:
			log::source().warn("ClientAppThread", "unhandled global message (code ",
					uint32_t(h.code), ")");
			// An unknown request must be answered, not dropped: the peer holds a waiter with a
			// deadline, and an answer lets a newer server probe an older client (as ServerInfo
			// does).
			if (!remote::isReplyOrError(h) && _connection) {
				_connection->sendError(remote::Domain::Global,
						toInt(remote::GlobalError::NotImplemented), h.serial);
			}
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
			// that queue's server id.

			/* A scene that takes too long is what setFrameDelay imitates, and the answer is held
			back rather than slept through: the looper keeps polling, so pings are still answered
			and what the server sees is a late FRAME, not a peer that went quiet. */
			if (_frameDelayFrames > 0 && _frameDelayUs > 0) {
				--_frameDelayFrames;
				auto header = h;
				auto bytes = payload.bytes<Interface>();
				_appLooper->schedule(sprt::dispatch::TimeInterval::microseconds(_frameDelayUs),
						[this, header, bytes = sp::move(bytes)](sprt::dispatch::Handle *,
								bool success) mutable {
					if (success) {
						dispatchMessage(header, BytesView(bytes.data(), bytes.size()));
					}
					return true;
				},
						this);
				return true;
			}

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

			// Indices 3/4 are the server's frame telemetry; check the type: absent means no update.
			core::FrameTimingInfo timing;
			core::DrawStat stat{};
			const core::FrameTimingInfo *timingPtr = nullptr;
			const core::DrawStat *statPtr = nullptr;
			if (val.getValue(3).isArray()) {
				timing = remote::deserializeFrameTiming(val.getValue(3));
				timingPtr = &timing;
			}
			if (val.getValue(4).isArray()) {
				stat = remote::deserializeDrawStat(val.getValue(4));
				statPtr = &stat;
			}

			wIt->second->acquireFrame(frameId, constraints, timingPtr, statPtr,
					sp::move(sendReply));
			return true;
		}
		case remote::WindowCode::TextInputState: {
			// server -> client: [windowId, TextInputState]. The processor's echo, on its way to the
			// focused widget through the Director's TextInputManager.
			auto val = data::read<Interface>(payload);
			auto windowId = uint64_t(val.getInteger(0));
			auto wIt = _windows.find(windowId);
			if (wIt == _windows.end()) {
				log::source().warn("ClientAppThread", "TextInputState for unknown window ",
						windowId);
				return true;
			}
			wIt->second->handleTextInput(remote::deserializeTextInputState(val.getValue(1)));
			return true;
		}
		case remote::WindowCode::WindowGeometryChanged: {
			// server -> client: [windowId, WindowGeometry]. Updates the window's mirror and lets the
			// scene hear about the move.
			auto val = data::read<Interface>(payload);
			auto windowId = uint64_t(val.getInteger(0));
			auto wIt = _windows.find(windowId);
			if (wIt == _windows.end()) {
				log::source().warn("ClientAppThread", "WindowGeometryChanged for unknown window ",
						windowId);
				return true;
			}
			wIt->second->handleWindowGeometryChanged(
					remote::deserializeWindowGeometry(val.getValue(1)));
			return true;
		}
		case remote::WindowCode::InputEvents: {
			// server -> client: the typed input batch (see serializeInputEvents). Reconstruct it and
			// replay it into the named window (its local Director -> scene).
			uint64_t windowId = 0;
			Vector<core::InputEventData> events;
			if (!remote::deserializeInputEvents(payload, windowId, events)) {
				log::source().warn("ClientAppThread", "InputEvents: malformed batch (",
						payload.size(), " bytes)");
				return true;
			}

			auto wIt = _windows.find(windowId);
			if (wIt == _windows.end()) {
				log::source().warn("ClientAppThread", "InputEvents for unknown window ", windowId);
				return true;
			}
			//log::source().info("ClientAppThread", "InputEvents: ", count, " event(s) for window ",
			//		windowId);
			wIt->second->handleInputEvents(sp::move(events));
			return true;
		}
		default:
			log::source().warn("ClientAppThread", "unhandled window message (code ",
					uint32_t(h.code), ")");
			if (!remote::isReplyOrError(h) && _connection) {
				_connection->sendError(remote::Domain::Window,
						toInt(remote::WindowError::NotImplemented), h.serial);
			}
			return true; // consume unknown control messages (don't defer indefinitely)
		}
	} else if (remote::Domain(h.domain) == remote::Domain::Data) {
		return _blockTransfer ? _blockTransfer->dispatch(h, payload) : true;
	} else if (remote::Domain(h.domain) == remote::Domain::Font) {
#if MODULE_XENOLITH_FONT
		// Route to the client FontController (SourcesReady replies are handled by the serial
		// waiter; this path is for notifications like AtlasReady).
		if (auto fc = getExtension<font::FontController>()) {
			return fc->dispatchFontMessage(h.code, h.serial, payload);
		}
#endif
		return true;
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

	// Same order as on the server: the window's own data first (see ServerAppThread::makeScene).
	if (auto sceneInfo = w->getSceneInfo()) {
		scene = sceneInfo->makeScene(this, w, c);
		if (scene) {
			return scene;
		}
	}

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

remote::PeerInfo ClientAppThread::makeClientInfo() const {
	auto ret = remote::PeerInfo::makeLocal();
	// A client renders nothing itself: `api` stays None and no window subsystem is claimed, since
	// the window belongs to the server.
	if (_connection) {
		if (auto t = _connection->getTransport()) {
			ret.transportCaps = t->getCaps();
		}
	}
	ret.transportScheme =
			remote::getSchemeName(_clientContext->getServerAddress().scheme).str<Interface>();

#if DEBUG
	// XL_REMOTE_FAKE_ABI=<hex>: report a different ABI tag, to test the mismatch path with binaries
	// from one tree. Debug-only.
	if (auto env = ::getenv("XL_REMOTE_FAKE_ABI")) {
		auto str = StringView(env);
		auto forced = uint64_t(str.readInteger(16).get(0));
		log::source().warn("ClientAppThread", "XL_REMOTE_FAKE_ABI: reporting abi ", forced,
				" instead of ", ret.abi);
		ret.abi = forced;
	}
#endif

	return ret;
}

void ClientAppThread::handleServerInfo(const remote::MessageHeader &h, BytesView payload) {
	auto info = remote::deserializePeerInfo(data::read<Interface>(payload));
	auto local = makeClientInfo();

	if (!local.isWireCompatible(info)) {
		// Reported, not refused: messages are typed field-by-field, so a differing tag only means
		// the builds disagree about some enum range.
		StringStream serverDesc;
		StringStream localDesc;
		info.description([&](StringView str) { serverDesc << str; });
		local.description([&](StringView str) { localDesc << str; });
		log::source().warn("ClientAppThread",
				"server was built against a different wire contract; continuing\n  server: ",
				serverDesc.str(), "\n  client: ", localDesc.str());
	}

	StringStream missing;
	local.describeMissingCodes(info, [&](StringView str) { missing << str; });
	if (!missing.empty()) {
		log::source().warn("ClientAppThread", "server does not implement: ", missing.str());
	}

	_serverInfo = sp::move(info);
	_hasServerInfo = true;

	StringStream desc;
	_serverInfo.description([&](StringView str) { desc << str; });
	log::source().info("ClientAppThread", "server: ", desc.str());

	if (_connection) {
		_connection->sendCborReply(h.serial, remote::Domain::Global,
				toInt(remote::GlobalCode::ServerInfo), remote::serializePeerInfo(local));
	}

	// The earliest point at which a client knows what this server can do -- and therefore the
	// earliest at which it may ask for a window of its own.
	_clientContext->handleServerInfo(this, _serverInfo);
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
		for (auto &it : disconnectedWindows) {
			// End the window's Director before announcing the window is gone: dropping the
			// reference does not exit the scene, so whatever it registered (e.g. its inspector)
			// would stay registered. The server does the same in handleAppWindowDestroyed.
			if (auto dir = dynamic_cast<Director *>(it->getRenderClient())) {
				dir->end();
			}
			// The opener asked for this window and is owed an answer however it went away.
			if (auto sceneInfo = it->getSceneInfo()) {
				dropPendingWindow(it->getCreatorSerial());
				sceneInfo->fireClose();
			}
			handleWindowDisconnected(it);
		}

		for (auto &it : connectedWindows) {
			// A window this client asked for carries its request's serial, so the scene it was
			// asked with is bound before the Director is built (makeScene consults it).
			if (auto rec = findPendingWindow(it->getCreatorSerial(), it->getId())) {
				rec->bound = true;
				it->setSceneInfo(Rc<WindowSceneInfo>(rec->handle));
			}

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
