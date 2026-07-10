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
#include "XLRemoteSerialize.h"
#include "XLRemoteBlockTransfer.h"
#include "XLRemoteFontServer.h"

#include <sprt/runtime/dispatch/handle.h>

#if MODULE_XENOLITH_FONT

#include "XLFontComponent.h"
#include "XLRemoteFontServerEndpoint.h"

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

void ServerAppThread::handleMatrialsUpdated(NotNull<core::MaterialSet> set) {
	AppThread::handleMatrialsUpdated(set);

	if (_remoteClient && !_remoteClient->isClosed()) {
		// Keep the font atlas image's wire id constant across its (per-update-replaced) ImageObjects, so a
		// dynamic font material's encoded image identity stays stable for the client's mirror.
		if (_fontServer) {
			_fontServer->pinAtlasImage();
		}
		if (auto v = _sharedObjects->attachMaterials(set)) {
			_remoteClient->handleMaterialsUpdated(v, set, _sharedObjects);
		}
	}
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

	if (_sharedObjects) {
		_sharedObjects->drop(w);
	}

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

bool ServerAppThread::shareWindow(AppWindow *w, SpanView<core::Queue *> q,
		const HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> &materials) {
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

	_sharedObjects->shareWindow(w, q, materials);
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

bool ServerAppThread::sendMessageWithReply(remote::Domain d, uint8_t message, const Value &val,
		Function<void(const remote::MessageHeader &, BytesView payload)> &&cb, uint64_t timeoutUs) {
	if (!_remoteClient || _remoteClient->isClosed()) {
		return false;
	}

	uint32_t serial = 0;
	if (_remoteClient->getConnection()->sendCborMessage(d, message, val, &serial)
			== remote::GlobalError::Ok) {
		waitForReply(serial, sp::move(cb), timeoutUs);
		return true;
	}
	return false;
}

bool ServerAppThread::remoteSendCbor(remote::Domain d, uint8_t code, const Value &v,
		uint32_t *outSerial) {
	if (!_remoteClient || _remoteClient->isClosed()) {
		return false;
	}
	return _remoteClient->getConnection()->sendCborMessage(d, code, v, outSerial)
			== remote::GlobalError::Ok;
}

bool ServerAppThread::remoteSendRaw(remote::Domain d, uint8_t code, BytesView b,
		uint32_t *outSerial) {
	if (!_remoteClient || _remoteClient->isClosed()) {
		return false;
	}
	return _remoteClient->getConnection()->sendMessage(d, code, b, outSerial)
			== remote::GlobalError::Ok;
}

bool ServerAppThread::remoteSendCborReply(uint32_t serial, remote::Domain d, uint8_t code,
		const Value &v) {
	if (!_remoteClient || _remoteClient->isClosed()) {
		return false;
	}
	return _remoteClient->getConnection()->sendCborReply(serial, d, code, v)
			== remote::GlobalError::Ok;
}

bool ServerAppThread::remoteSendError(remote::Domain d, uint8_t code, uint32_t serial) {
	if (!_remoteClient || _remoteClient->isClosed()) {
		return false;
	}
	return _remoteClient->getConnection()->sendError(d, code, serial) == remote::GlobalError::Ok;
}

bool ServerAppThread::remoteSendCborWithReply(remote::Domain d, uint8_t code, const Value &v,
		Function<void(const remote::MessageHeader &, BytesView payload)> &&cb, uint64_t timeoutUs) {
	return sendMessageWithReply(d, code, v, sp::move(cb), timeoutUs);
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
		resetRemoteClient();
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

	// Request watchdog (same Looper cadence as keepalive below): if the client left one of our requests
	// unanswered past that request's own reply deadline -- e.g. it received AcquireFrame but never
	// replied -- the waiters were just failed with a local protocol error, so drop the connection.
	if (_remoteClient && !_remoteClient->isClosed()) {
		if (failTimedOutRequests()) {
			log::source().info("AppThread",
					"request reply timeout; terminating unresponsive client connection");
			resetRemoteClient();
		}
	}

	// Keepalive: a pong resets _lastPongTime (see dispatchMessage). If the client has not answered for
	// kKeepalivePongTimeoutUs, terminate it (drops the connection, freeing the slot for a new client);
	// otherwise send a ping at most every kKeepalivePingIntervalUs.
	//
	// The cadence is driven by AppThread's internal Looper timer (scheduleTimer, interval =
	// ContextInfo::appUpdateInterval, default 1s, count = Infinite -> performAppUpdate -> pumpListener),
	// NOT by frame/presentation timing. So keepalive keeps ticking at ~1s even when the window is idle
	// and producing no frames (the display-link/PresentationEngine cadence is a separate path). Socket
	// readiness also calls pumpListener, but only the timer guarantees progress while idle.
	if (_remoteClient && !_remoteClient->isClosed()) {
		auto now = sp::platform::clock(ClockType::Monotonic);
		if (now - _lastPongTime >= kKeepalivePongTimeoutUs) {
			log::source().info("AppThread",
					"client keepalive timeout (no pong for 5s); terminating connection");
			resetRemoteClient();
		} else if (now - _lastPingTime >= kKeepalivePingIntervalUs) {
			if (auto conn = _remoteClient->getConnection()) {
				conn->ping();
			}
			_lastPingTime = now;
		}
	}
}

void ServerAppThread::resetRemoteClient() {
	// Revert every shared window to its local Director (this also kills the windows' in-flight remote
	// frames), drop any still-outstanding reply waiters for the dead connection (their frames were just
	// invalidated), then close the connection and free the single-connection slot for a new client.
	takeoverSharedWindows(nullptr);
	_requests.clear();
	if (_blockTransfer) {
		_blockTransfer->reset();
	}
	if (_fontServer) {
		// Drop per-connection gating state; the persistent font store + network atlas survive.
		_fontServer->reset();
	}
	if (_remoteClient) {
		_remoteClient->closeConnection();
		_remoteClient = nullptr;
	}
}

bool ServerAppThread::dispatchMessage(const remote::MessageHeader &h, BytesView payload) {
	if (AppThread::dispatchMessage(h, payload)) {
		return true;
	}

	auto conn = _remoteClient ? _remoteClient->getConnection() : nullptr;
	if (remote::Domain(h.domain) == remote::Domain::Global) {
		switch (remote::GlobalCode(h.code)) {
		case remote::GlobalCode::Ping: {
			//log::source().info("AppThread", "received ping (serial ", h.serial, "); replying pong");
			if (conn) {
				conn->pong(h.serial);
			}
			return true;
		}
		case remote::GlobalCode::Pong:
			//log::source().info("AppThread", "received pong (serial ", h.serial, ")");
			_lastPongTime = sp::platform::clock(ClockType::Monotonic);
			return true;
		default:
			if (conn) {
				conn->sendError(remote::Domain::Global, toInt(remote::GlobalError::NotImplemented),
						h.serial);
			}
			log::source().warn("AppThread", "unhandled global message (code ", uint32_t(h.code),
					")");
			return true; // consume unknown control messages (don't defer indefinitely)
		}
	} else if (remote::Domain(h.domain) == remote::Domain::Window) {
		switch (remote::WindowCode(h.code)) {
		case remote::WindowCode::CompileQueue: {
			auto val = data::read<Interface>(payload);
			auto q = _sharedObjects->resolveQueue(val.getInteger());
			if (!q) {
				conn->sendError(remote::Domain::Window,
						toInt(remote::WindowError::InvalidObjecthandle), h.serial);
				return true;
			}

			auto data = remote::QueueCodec::encodeQueue(*q->queue, q->materials, *_sharedObjects);
			if (data.empty()) {
				conn->sendError(remote::Domain::Window,
						toInt(remote::WindowError::SerializationFailed), h.serial);
				return true;
			}

			conn->sendReply(h.serial, remote::Domain(h.domain), h.code, data);
			return true;
		};
		case remote::WindowCode::FrameInput: {
			// client -> server: one streamed input addressed to one or more attachments [frameId,
			// keys[], bytes]
			if (_remoteClient) {
				auto val = data::read<Interface>(payload);
				Vector<StringView> keys;
				for (auto &k : val.getValue(1).asArray()) { keys.emplace_back(k.getString()); }
				_remoteClient->handleFrameInput(uint64_t(val.getInteger(0)), keys, val.getBytes(2));
			}
			return true;
		};
		case remote::WindowCode::FrameCommit: {
			// client -> server: all inputs for a frame were submitted
			if (_remoteClient) {
				auto val = data::read<Interface>(payload);
				_remoteClient->handleFrameCommit(uint64_t(val.getInteger(0)));
			}
			return true;
		};
		case remote::WindowCode::CompileMaterials: {
			// client -> server: compile a runtime (font atlas) material the headless client can't compile.
			if (_remoteClient) {
				_remoteClient->handleCompileMaterials(payload);
			}
			return true;
		};
		case remote::WindowCode::AttachQueue: {
			// client -> server: the client compiled the shared queue and attached it to its Director, so
			// it is now ready to serve frames. Hand the named window's frame production over to the
			// remote client (its PresentationEngine starts pulling through RemoteRenderClient::acquireFrame
			// instead of the local Director) and acknowledge with an empty atomic reply.
			if (_remoteClient) {
				auto windowId = uint64_t(data::read<Interface>(payload).getInteger());
				takeoverSharedWindow(windowId, _remoteClient);
			}
			if (conn) {
				conn->sendReply(h.serial, remote::Domain(h.domain), h.code, BytesView());
			}
			return true;
		};
		case remote::WindowCode::ReadyForNextFrame: {
			// client -> server: the client's scene has active actions/input and wants the next frame
			// produced. Schedule it on the window's PresentationEngine so animation keeps progressing.
			// Notification only: no reply.
			if (_sharedObjects) {
				auto windowId = uint64_t(data::read<Interface>(payload).getInteger());
				if (auto w = static_cast<AppWindow *>(_sharedObjects->resolveWindow(windowId))) {
					w->setReadyForNextFrame();
				}
			}
			return true;
		};
		case remote::WindowCode::RequestScreenshot: {
			// client -> server: capture the named window's current contents (which, while the client is
			// attached, is the client's own remote-rendered output) and hand them back over Domain::Data
			// as a Screenshot transfer. The announce `reason` points back at this request so the client
			// can match the asynchronously-arriving pixels to its captureScreenshot() call. No reply here.
			if (_sharedObjects) {
				auto windowId = uint64_t(data::read<Interface>(payload).getInteger());
				auto reqSerial = h.serial;
				if (auto w = static_cast<AppWindow *>(_sharedObjects->resolveWindow(windowId))) {
					w->captureScreenshot(
							[this, reqSerial, windowId](const core::ImageInfoData &info,
									BytesView pixels) {
						// On the GL loop thread: the pixels view is transient, so copy it (and the image
						// info), then hop to the app thread -- the connection / block-transfer must be
						// touched there -- and offer the blob.
						auto pixelsCopy = pixels.bytes<Interface>();
						auto infoCopy = info;
						performOnAppThread(
								[this, reqSerial, windowId, infoCopy,
										pixelsCopy = sp::move(pixelsCopy)]() mutable {
							if (!_blockTransfer) {
								return;
							}
							Value meta;
							meta.setInteger(int64_t(toInt(infoCopy.format)), "fmt");
							meta.setInteger(int64_t(infoCopy.extent.width), "w");
							meta.setInteger(int64_t(infoCopy.extent.height), "h");
							meta.setInteger(int64_t(infoCopy.extent.depth), "d");

							Value reason;
							reason.setInteger(int64_t(toInt(remote::Domain::Window)), "domain");
							reason.setInteger(int64_t(toInt(remote::WindowCode::RequestScreenshot)),
									"code");
							reason.setInteger(int64_t(toInt(remote::MessageType::Client)), "mtype");
							reason.setInteger(int64_t(reqSerial), "serial");

							auto id = _blockTransfer->startTransfer(remote::DataType::Screenshot,
									BytesView(pixelsCopy.data(), pixelsCopy.size()), sp::move(meta),
									sp::move(reason), [this](uint64_t tid, bool ok) {
								log::source().info("AppThread", "screenshot transfer ",
										ok ? "completed" : "failed");
								// One-shot push: once the client has it (or it failed) we will never
								// reference it again, so release it to free the client's retained copy.
								if (ok && _blockTransfer) {
									_blockTransfer->releaseObject(tid);
								}
							});
							log::source().info("AppThread", "captured window ", windowId,
									" -> screenshot transfer ", id);
						},
								this);
					});
				} else {
					log::source().warn("AppThread", "RequestScreenshot for unknown shared window ",
							windowId);
				}
			}
			return true;
		};
		case remote::WindowCode::UpdateLayers: {
			// client -> server: the window's interaction layers (hit/cursor/drag regions) computed by the
			// client's scene graph. Raw blob [u64 windowId (network order)][WindowLayer[] native layout];
			// reconstruct and apply to the real window so the OS does cursor/hit-testing/decorations.
			if (!_sharedObjects || payload.size() < sizeof(uint64_t)) {
				return true;
			}
			uint64_t widN = 0;
			__sprt_memcpy(&widN, payload.data(), sizeof(uint64_t));
			auto windowId = sprt::byteorder::NetworkToHost(widN);

			auto rest = payload.sub(sizeof(uint64_t));
			if (rest.size() % sizeof(sprt::window::WindowLayer) != 0) {
				log::source().warn("AppThread", "UpdateLayers: misaligned blob (", rest.size(),
						" bytes)");
				return true;
			}
			auto count = rest.size() / sizeof(sprt::window::WindowLayer);
			sprt::window::Vector<sprt::window::WindowLayer> layers;
			for (size_t i = 0; i < count; ++i) {
				sprt::window::WindowLayer layer;
				__sprt_memcpy(&layer, rest.data() + i * sizeof(sprt::window::WindowLayer),
						sizeof(sprt::window::WindowLayer));
				log::source().info("AppThread", "UpdateLayers: layer[", i, "] rect{",
						layer.rect.origin.x, ",", layer.rect.origin.y, " ", layer.rect.size.width,
						"x", layer.rect.size.height, "} cursor=", uint32_t(toInt(layer.cursor)),
						" flags=", uint32_t(toInt(layer.flags)));
				layers.emplace_back(layer);
			}

			if (auto w = static_cast<AppWindow *>(_sharedObjects->resolveWindow(windowId))) {
				log::source().info("AppThread", "UpdateLayers: applying ", count,
						" layer(s) to window ", windowId);
				w->updateLayers(sp::move(layers));
			} else {
				log::source().warn("AppThread", "UpdateLayers for unknown shared window ",
						windowId);
			}
			return true;
		};
		default:
			if (conn) {
				conn->sendError(remote::Domain::Window, toInt(remote::GlobalError::NotImplemented),
						h.serial);
			}
			log::source().warn("AppThread", "unhandled window message (code ", uint32_t(h.code),
					")");
			return true; // consume unknown control messages (don't defer indefinitely)
		}
	} else if (remote::Domain(h.domain) == remote::Domain::Data) {
		return _blockTransfer ? _blockTransfer->dispatch(h, payload) : true;
	} else if (remote::Domain(h.domain) == remote::Domain::Font) {
		if (_fontServer) {
			return _fontServer->dispatch(h.code, h.serial, payload);
		}
		if (conn) {
			conn->sendError(remote::Domain::Font, toInt(remote::FontError::NotImplemented),
					h.serial);
		}
		return true;
	} else {
		if (conn) {
			conn->sendError(remote::Domain(h.domain), toInt(remote::GlobalError::NotImplemented),
					h.serial);
		}
		log::source().warn("AppThread", "unhandled message domain (", uint32_t(h.domain), ")");
	}
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
	if (status != remote::GlobalError::Ok) {
		log::source().error("AppThread", "client handshake failed (status ",
				uint32_t(toInt(status)), "); dropping connection");
		return; // conn dropped, slot stays free
	}

	log::source().info("AppThread", "client authenticated");

	_remoteClient = Rc<RemoteRenderClient>::create(this, sp::move(conn));
	if (_remoteClient) {
		_remoteClient->announce(_sharedObjects);
		// Do NOT take the windows over yet: the client must first compile each shared queue and attach
		// it to its Director. The per-window handover happens when the client sends WindowCode::AttachQueue
		// (handled in dispatchMessage); until then the server keeps rendering through the local Directors,
		// so AcquireFrame requests never reach a client that isn't ready to serve them.
		// Start the keepalive clock fresh so the timeout is measured from connection establishment.
		_lastPingTime = _lastPongTime = sp::platform::clock(ClockType::Monotonic);
	}
}

void ServerAppThread::takeoverSharedWindows(core::RenderClientChannel *client) {
	if (!_sharedObjects) {
		return;
	}
	for (auto &it : _sharedObjects->getWindows()) {
		auto w = static_cast<AppWindow *>(it.second.window);
		if (!w) {
			continue;
		}
		if (!client) {
			// Reverting to the local Director: kill any in-flight frames the (now-gone) remote client was
			// producing so a frame stuck on it cannot wedge presentation before the local scene resumes.
			w->invalidateRemoteFrames();
		}
		// On revert (client == nullptr) restore the window's own local Director.
		w->setRenderClient(
				client ? client : static_cast<core::RenderClientChannel *>(w->getDirector()));
		// Restart presentation for the new client (clears a stale display-link barrier + pumps a frame).
		w->resetForRenderClientChange();
	}
}

void ServerAppThread::takeoverSharedWindow(uint64_t windowId, core::RenderClientChannel *client) {
	if (!_sharedObjects) {
		return;
	}
	auto w = static_cast<AppWindow *>(_sharedObjects->resolveWindow(windowId));
	if (!w) {
		log::source().warn("AppThread", "AttachQueue for unknown shared window ", windowId);
		return;
	}
	// On revert (client == nullptr) restore the window's own local Director.
	w->setRenderClient(
			client ? client : static_cast<core::RenderClientChannel *>(w->getDirector()));
	// Restart presentation for the new client (clears a stale display-link barrier + pumps a frame).
	w->resetForRenderClientChange();
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
	if (auto comp = _context->getComponent<font::FontComponent>()) {
		// Local-scene controller: drives the server's own windows/Directors (registered extension).
		auto createFontController = SharedModule::acquireTypedSymbol<
				decltype(&font::FontComponent::createDefaultController)>(
				buildconfig::MODULE_XENOLITH_FONT_NAME, "FontComponent::createDefaultController");
		if (createFontController) {
			if (auto controller =
							createFontController(comp, _appLooper, "ApplicationFontController")) {
				addExtension(move(controller));
			}
		}

		// Network-serving font endpoint (remote::Domain::Font): a *separate* controller with its own
		// FontLibrary + atlas, so the FaceIds a client forces never collide with the local-scene
		// controller's. Owned here (not a registered extension); persists across client reconnects.
		auto createServerFontEndpoint = SharedModule::acquireTypedSymbol<
				decltype(&font::RemoteFontServerEndpoint::createServerFontEndpoint)>(
				buildconfig::MODULE_XENOLITH_FONT_NAME,
				"RemoteFontServerEndpoint::createServerFontEndpoint");
		if (createServerFontEndpoint) {
			_fontServer = createServerFontEndpoint(this, comp);
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
