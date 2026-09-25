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
#include "XLRemoteSession.h"
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

// Keepalive cadence: ping the connected client this often, and terminate it if it has not answered
// a pong within the timeout.
static constexpr uint64_t kKeepalivePingIntervalUs = 1'000'000; // 1s
static constexpr uint64_t kKeepalivePongTimeoutUs = 5'000'000; // 5s

// Rate limit on setup handshakes: every failure doubles a cool-off window during which connections
// are refused, so key guessing costs wall-clock time. A successful handshake clears it.
static constexpr uint64_t kHandshakeBackoffBaseUs = 250'000; // after the 1st failure
static constexpr uint64_t kHandshakeBackoffMaxUs = 8'000'000; // ceiling

// Connections allowed in their setup handshake at once, how long one may take, and how long a
// refused one is given to send its hello before the refusal is written anyway (see
// remote::serverHandshakeReject for why it waits at all).
static constexpr size_t kMaxPendingHandshakes = 8;
static constexpr uint64_t kServerHandshakeDeadlineUs = 2'000'000; // 2s
static constexpr uint64_t kRefusalHelloWaitUs = 100'000; // 100ms

// A client must answer ServerInfo within this budget (from its first app-thread tick); nothing is
// announced until it arrives.
static constexpr uint64_t kPeerInfoReplyTimeoutUs = 5'000'000; // 5s

/* Screenshot transfers sit below the default priority, so bulk data a frame waits on (fonts)
 * goes first; negative so a caller with the default priority still outranks it. */
static constexpr int32_t kScreenshotTransferPriority = -1;

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ServerAppThread::~ServerAppThread() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool ServerAppThread::init(NotNull<Context> ctx) {
	_context = ctx;
	return true;
}

BytesView ServerAppThread::getListenerFingerprint() const {
	return _listener ? _listener->getCertificateFingerprint() : BytesView();
}

bool ServerAppThread::hasRemoteClient() const {
	// isClosed() is non-const (it pumps events), so only check whether a slot is taken.
	return !_sessions.empty();
}

void ServerAppThread::setMaxRemoteClients(uint32_t count) {
	_maxRemoteClients = sprt::max(count, uint32_t(1));
}

RemoteSession *ServerAppThread::getRemoteSession(uint64_t id) const {
	for (auto &it : _sessions) {
		if (it->getId() == id) {
			return it;
		}
	}
	return nullptr;
}

void ServerAppThread::assignSharedWindow(StringView windowName, uint64_t session) {
	if (session) {
		_windowAssignments.insert_or_assign(windowName.str<Interface>(), session);
	} else {
		_windowAssignments.erase(windowName.str<Interface>());
	}

	if (!_sharedObjects) {
		return;
	}
	bool changed = false;
	for (auto &it : _sharedObjects->getWindows()) {
		if (it.second.window && it.second.window->getId() == windowName) {
			changed = _sharedObjects->assignWindow(it.first, session) || changed;
		}
	}
	if (changed) {
		republishSharedObjects();
	}
}

void ServerAppThread::setClientWindowHandler(ClientWindowHandler &&handler) {
	_clientWindowHandler = sp::move(handler);
	if (_clientWindowHandler
			&& !hasFlag(_context->getInfo()->flags,
					sprt::window::ContextFlags::KeepRunningWithoutWindows)) {
		// A server whose windows all belong to clients has none of its own between them, and would
		// exit as the last one closes. --keep-running is what a window manager wants here.
		log::source()
				.warn("AppThread",
						"serving client window requests without KeepRunningWithoutWindows: the "
						"server " "exits when the last window closes");
	}
	updateServerInfo(); // the feature bit follows the handler
}

void ServerAppThread::setMaxClientWindows(uint32_t perSession) { _maxClientWindows = perSession; }

void ServerAppThread::addBearerKey(BytesView key, StringView label, bool singleUse) {
	_labelledKeys.add(key, label, singleUse);
}

bool ServerAppThread::removeBearerKey(StringView label) { return _labelledKeys.remove(label); }

void ServerAppThread::setRequireLabelledKeys(bool value) { _requireLabelledKeys = value; }

void ServerAppThread::setAppMessageHandler(AppMessageHandler &&handler) {
	_appMessageHandler = sp::move(handler);
	updateServerInfo(); // the feature bit follows the handler
}

bool ServerAppThread::sendAppNotification(NotNull<RemoteSession> session, const Value &val) {
	if (session->isClosed()
			|| !session->getPeerInfo().supports(remote::Domain::Global,
					toInt(remote::GlobalCode::AppNotify))) {
		return false;
	}
	return session->remoteSendCbor(remote::Domain::Global, toInt(remote::GlobalCode::AppNotify),
			val);
}

bool ServerAppThread::sendAppRequest(NotNull<RemoteSession> session, const Value &val,
		Function<void(Status, Value &&)> &&cb, uint64_t timeoutUs) {
	if (session->isClosed()
			|| !session->getPeerInfo().supports(remote::Domain::Global,
					toInt(remote::GlobalCode::AppRequest))) {
		return false;
	}
	return session->sendMessageWithReply(remote::Domain::Global,
			toInt(remote::GlobalCode::AppRequest), val,
			[cb = sp::move(cb)](const remote::MessageHeader &h, BytesView payload) {
		auto st = getAppReplyStatus(h);
		cb(st, sprt::status::isSuccessful(st) ? data::read<Interface>(payload) : Value());
	}, timeoutUs, false);
}

void ServerAppThread::setSessionObserver(SessionObserver &&observer) {
	_sessionObserver = sp::move(observer);
}

size_t ServerAppThread::getClientWindowCount(uint64_t session) const {
	size_t ret = 0;
	for (auto &it : _clientWindows) {
		if (it.session == session) {
			++ret;
		}
	}
	return ret;
}

ServerAppThread::ClientWindow *ServerAppThread::findClientWindow(WindowSceneInfo *handle) {
	if (!handle) {
		return nullptr;
	}
	for (auto &it : _clientWindows) {
		if (it.handle == handle) {
			return &it;
		}
	}
	return nullptr;
}

void ServerAppThread::dropClientWindow(WindowSceneInfo *handle) {
	for (auto it = _clientWindows.begin(); it != _clientWindows.end(); ++it) {
		if (it->handle == handle) {
			_clientWindows.erase(it);
			return;
		}
	}
}

void ServerAppThread::createClientWindow(NotNull<RemoteSession> session,
		Rc<sprt::window::WindowInfo> &&info, uint32_t serial, ClientWindowCallback &&complete) {
	auto refuse = [&](Status st) {
		if (complete) {
			complete(st, StringView(), nullptr);
		}
	};

	if (!_clientWindowHandler) {
		refuse(Status::ErrorNotImplemented);
		return;
	}
	if (!info) {
		refuse(Status::ErrorInvalidArguemnt);
		return;
	}

	/* Engine policy first, so the handler is never asked for a window the platform can not make.

	Only Root windows for now. A subwindow needs a parent, and a parent named by a client would be
	the one field through which it could reach into another session's windows -- so the check that
	belongs with it (the parent is a window THIS session owns) is written when a transport carries
	a parent at all. */
	if (info->type != sprt::window::WindowType::Root) {
		refuse(Status::ErrorNotSupported);
		return;
	}
	if (getClientWindowCount(session->getId()) >= _maxClientWindows) {
		refuse(Status::ErrorBusy);
		return;
	}

	// The client names its window, but not in the server's own namespace, and none of the fields
	// the window system answers are its to set.
	info->id = toString("client", session->getId(), ".",
			info->id.empty() ? StringView("window") : StringView(info->id));
	info->capabilities = sprt::window::WindowCapabilities::None;
	info->state = core::WindowState::None;
	// Whether the window is a plane of this server's compositor is the server's decision alone; the
	// handler below may set it.
	info->flags &= ~sprt::window::WindowCreationFlags::Virtual;
	info->icon = nullptr;
	info->appData = nullptr;

	Rc<WindowSceneInfo> handle;
	auto status = _clientWindowHandler(session, info, handle);
	if (status != Status::Ok || !handle) {
		refuse(status != Status::Ok ? status : Status::Declined);
		return;
	}

	_clientWindows.emplace_back(ClientWindow{session->getId(), serial, handle});
	info->appData = handle;

	// The completion runs on this thread with the final id; a failed creation has already handed
	// the handle back to its close callback (see Context::createWindow).
	// Kept for the answer: the caller is told what it was granted, not what it asked for.
	auto granted = info;
	_context->createWindow(sp::move(info),
			[this, handle, granted, complete = sp::move(complete)](Status st,
					StringView id) mutable {
		if (auto rec = findClientWindow(handle)) {
			if (st == Status::Ok) {
				rec->id = id.str<Interface>();
			} else {
				dropClientWindow(handle);
			}
		}
		if (complete) {
			complete(st, id, granted.get());
		}
	});
}

bool ServerAppThread::startSession(StringView address, BytesView key) {
	if (isListening()) {
		return true;
	}
	if (!setListenAddress(address) || !setBearerKey(key)) {
		return false;
	}
	return startListening();
}

void ServerAppThread::resetRemoteSessions() {
	auto sessions = _sessions;
	for (auto &it : sessions) { resetSession(it); }
}

size_t ServerAppThread::cancelOutgoingTransfers() {
	size_t ret = 0;
	for (auto &it : _sessions) {
		if (auto transfer = it->getBlockTransfer()) {
			ret += transfer->cancelAllTransfers();
		}
	}
	return ret;
}

const ContextInfo *ServerAppThread::getContextInfo() const { return _context->getInfo(); }

core::Loop *ServerAppThread::getGlLoop() const {
	return static_cast<core::Loop *>(_context->getGlLoop());
}

void ServerAppThread::handleThreadInitialized() {
	updateServerInfo();
	_context->handleAppThreadCreated(this);
}

void ServerAppThread::updateServerInfo() {
	auto info = remote::PeerInfo::makeLocal();

	if (auto loop = getGlLoop()) {
		if (auto instance = loop->getInstance()) {
			info.api = instance->getApi();
		}
	}

	// The window subsystem comes from the windows (Unknown with none). All windows share one
	// subsystem, so the first is taken; capabilities are per window, so they are a union.
	// Per-window values travel in the announce.
	bool first = true;
	for (auto w : _windows) {
		// A virtual window is a plane of this server's compositor: it says nothing about the window
		// system, and has no subwindows.
		if (w->isVirtual()) {
			continue;
		}
		if (first) {
			info.wm = remote::toWindowSubsystem(w->getSurfaceBackend());
			first = false;
		}
		if (hasFlag(w->getCapabilities(), sprt::window::WindowCapabilities::Subwindows)) {
			info.features |= remote::PeerFeatures::Subwindows;
		}
	}

	// The server owns the GPU, so it can always hand a frame back (WindowCode::RequestScreenshot).
	info.features |= remote::PeerFeatures::FrameCapture;
	if (_createFontServer) {
		info.features |= remote::PeerFeatures::FontServer;
	}
	if (_clientWindowHandler) {
		info.features |= remote::PeerFeatures::ClientWindows;
	}
	if (_appMessageHandler) {
		info.features |= remote::PeerFeatures::AppMessages;
	}
	if (hasClipboard()) {
		info.features |= remote::PeerFeatures::Clipboard;
	}

	if (!_listenAddress.empty()) {
		info.transportScheme = remote::getSchemeName(_listenAddress.scheme).str<Interface>();
		// Caps belong to the transport, not the listener, so they are valid without a bound socket.
		if (auto t = remote::TransportRegistry::get(_listenAddress.scheme)) {
			info.transportCaps = t->getCaps();
		}
	}

	_localInfo = sp::move(info);
}

void ServerAppThread::handleThreadDisposed() { _context->handleAppThreadDestroyed(this); }

void ServerAppThread::handleThreadUpdated(const UpdateTime &time) {
	_context->handleAppThreadUpdate(this, time);
}

void ServerAppThread::handleMatrialsUpdated(NotNull<core::MaterialSet> set) {
	AppThread::handleMatrialsUpdated(set);

	if (!_sharedObjects || _sessions.empty()) {
		return;
	}
	auto v = _sharedObjects->attachMaterials(set);
	if (!v) {
		return;
	}
	// Only to the sessions that can see a window drawn with this queue.
	auto sessions = _sessions;
	for (auto &it : sessions) {
		if (it->isClosed() || !_sharedObjects->isQueueVisible(v, it->getId())) {
			continue;
		}
		// Keep the font atlas image's wire id constant across its (per-update-replaced)
		// ImageObjects, so a dynamic font material's encoded image identity stays stable for the
		// client's mirror.
		if (auto fontServer = it->getFontServer()) {
			fontServer->pinAtlasImage();
		}
		it->getRenderClient()->handleMaterialsUpdated(v, set, _sharedObjects);
	}
}

/* Delivers a clipboard callback exactly once: the backend may both call it and return a failure
(base ContextController), or return a failure without calling it (Wayland). The first answer wins;
`take()` is app-thread-only, so no atomic is needed. */
template <typename Callback>
struct ClipboardAnswer : public Ref {
	Callback callback;
	Rc<Ref> target;
	bool claimed = false;

	// Null on every call after the first. Also drops the caller's Ref.
	Callback take() {
		if (claimed) {
			return nullptr;
		}
		claimed = true;
		target = nullptr;
		return sp::move(callback);
	}
};

void ServerAppThread::readFromClipboard(Function<void(Status, BytesView, StringView)> &&cb,
		Function<StringView(SpanView<StringView>)> &&tcb, Ref *ref) {
	auto answer = Rc<ClipboardAnswer<Function<void(Status, BytesView, StringView)>>>::alloc();
	answer->callback = sp::move(cb);
	answer->target = ref;

	_context->performOnThread([this, answer, tcb = sp::move(tcb)]() mutable {
		// Both ways of answering funnel through here, so the bytes are copied and the hop is
		// written once
		auto deliver = [this, answer](Status st, BytesView data, StringView type) {
			performOnAppThread(
					[answer, st, data = data.bytes<Interface>(),
							type = type.str<Interface>()]() mutable {
				if (auto cb = answer->take()) {
					cb(st, data, type);
				}
			},
					this);
		};

		auto st = _context->readFromClipboard(
				[deliver](Status st, BytesView data, StringView type) mutable {
			deliver(st, data, type);
		}, sp::move(tcb), this);

		// The read never started (on Wayland this is the only signal); if the backend answered
		// anyway, the answer is already claimed.
		if (st != Status::Ok) {
			deliver(st, BytesView(), StringView());
		}
	}, this);
}

void ServerAppThread::probeClipboard(Function<void(Status, SpanView<StringView>)> &&cb, Ref *ref) {
	// Claimed once by the probe's answer or the failure fallback (platforms without a probe).
	auto answer = Rc<ClipboardAnswer<Function<void(Status, SpanView<StringView>)>>>::alloc();
	answer->callback = sp::move(cb);
	answer->target = ref;

	_context->performOnThread([this, answer]() mutable {
		auto st = _context->probeClipboard(
				[this, answer](Status st, SpanView<StringView> types) mutable {
			Vector<String> typesData;
			typesData.reserve(types.size());
			for (auto it : types) { typesData.emplace_back(it.str<Interface>()); }
			performOnAppThread([answer, st, types = sp::move(typesData)]() mutable {
				if (auto cb = answer->take()) {
					Vector<StringView> typesView;
					typesView.reserve(types.size());
					for (auto &it : types) { typesView.emplace_back(it); }
					cb(st, typesView);
				}
			}, this);
		}, this);

		if (st != Status::Ok) {
			performOnAppThread([answer, st]() mutable {
				if (auto cb = answer->take()) {
					cb(st, SpanView<StringView>());
				}
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

void ServerAppThread::writeToClipboard(Rc<sprt::window::ClipboardData> &&data) {
	// The object is already whole and its members are malloc-backed, so it crosses to the context
	// thread as-is - nothing to copy apart from the Rc
	_context->performOnThread([this, data = sp::move(data)]() mutable {
		_context->writeToClipboard(sp::move(data));
	}, this);
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

	const bool isVirtual = w->isVirtual();
	addListener(w, [w, isVirtual](const UpdateTime &, bool wakeup) {
		if (wakeup) {
			w->setReadyForNextFrame();

			// force display link to update views. Not on a virtual window: once a compositor claims
			// it, a DisplayLink from anywhere else is a frame the compositor did not ask for.
			if (!isVirtual) {
				w->update(core::PresentationUpdateFlags::DisplayLink);
			}
		}
	});

	auto dir = makeDirector(w, c);
	if (dir) {
		_windows.emplace(w.get());
		// A window is what names the window subsystem (and its subwindow support).
		updateServerInfo();
	}

	// The final id is known only here; a window whose session left while it was on its way has
	// nobody to serve and goes straight back out. The close drops the record, so nothing of it is
	// read afterwards.
	bool orphaned = false;
	if (auto rec = findClientWindow(w->getSceneInfo())) {
		rec->id = w->getId().str<Interface>();
		orphaned = rec->orphaned;
	}
	if (orphaned) {
		log::source().info("AppThread", "client window '", w->getId(),
				"' outlived the session that asked for it; closing");
		w->close(false);
	}
	return dir;
}

void ServerAppThread::handleAppWindowDestroyed(NotNull<AppWindow> w, Rc<Director> &&d) {
	log::source().info("AppThread", "handleAppWindowDestroyed");

	dropClientWindow(w->getSceneInfo());

	if (_sharedObjects) {
		_sharedObjects->drop(w);
		// Withdraw it from a connected client too: the announce is a snapshot, so a window missing
		// from the new one is how the client learns it went away (handleWindowDisconnected).
		republishSharedObjects();
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
	updateServerInfo();

	if (_windows.empty()
			&& !hasFlag(_context->getInfo()->flags,
					sprt::window::ContextFlags::KeepRunningWithoutWindows)) {
		/* Listening is started by a window's loader scene; with no windows left it must stop, or the
		app can not close. A new window's loader scene can restart it.

		Connected clients do not hold it open: with no windows there is nothing left to serve. A
		server whose windows all belong to clients -- a window manager -- wants
		KeepRunningWithoutWindows, which setClientWindowHandler warns about. */
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
		if (_listenAddress.empty() || !hasCredentials()) {
			log::error("ServerAppThread",
					"Listen address and credentials should be set before sharing anything");
			return false;
		}

		if (!startListening()) {
			log::error("ServerAppThread", "Fail to start listener for shareWindow");
			return false;
		}
	}

	if (q.empty()) {
		// A window with no queue can not be rendered by a client (it would not even build a mirror
		// for it), and announcing one would strand whoever asked for it.
		log::source().error("ServerAppThread", "refusing to share a window with no render queue");
		return false;
	}

	_sharedObjects->shareWindow(w, q, materials);

	auto windowId = _sharedObjects->get(w);
	if (auto rec = findClientWindow(w->getSceneInfo())) {
		// A window exists because a client asked for it: it belongs to that session from its very
		// first announce, so there is no moment in which another session could see or take it.
		_sharedObjects->assignWindow(windowId, rec->session);
		_sharedObjects->setWindowCreator(windowId, rec->session, rec->serial);
	} else {
		auto assignment = _windowAssignments.find(w->getId().str<Interface>());
		if (assignment != _windowAssignments.end()) {
			_sharedObjects->assignWindow(windowId, assignment->second);
		}
	}

	// A connected client must be told: the announce is a full snapshot the client reconciles, so
	// re-sending it is the whole update mechanism.
	republishSharedObjects();
	return true;
}

void ServerAppThread::republishSharedObjects(RemoteSession *except) {
	if (!_sharedObjects) {
		return;
	}
	auto sessions = _sessions;
	for (auto &it : sessions) {
		if (it != except && !it->isClosed()) {
			it->getRenderClient()->announce(_sharedObjects);
		}
	}
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

bool ServerAppThread::hasCredentials() const {
	if (!_expectedKey.empty() || !_labelledKeys.empty()) {
		return true;
	}
	// A transport that establishes who the peer is does not consult the key.
	remote::initializeTransports();
	auto transport = remote::TransportRegistry::get(_listenAddress.scheme);
	return transport && hasFlag(transport->getCaps(), remote::TransportCaps::PeerAuthenticated);
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
	if (!hasCredentials()) {
		log::source().error("AppThread",
				"startListening: no bearer key for a transport that does not authenticate peers");
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

	// Socket readiness or the rendezvous doorbell wakes accept promptly; QUIC timers are pumped from
	// performAppUpdate(), so no separate listen timer is needed.
	_listenPoll = watchTransport(_listener->getPollHandle(), _listener->getWaitAddress(),
			[this] { pumpListener(); });
	updateServerInfo(); // the transport is now known
	pumpListener();
	return true;
}

bool ServerAppThread::stopListening() {
	if (_listenPoll) {
		_listenPoll->cancel();
		_listenPoll = nullptr;
	}
	dropPendingHandshakes();
	resetRemoteSessions();

	if (_sharedObjects) {
		_sharedObjects.clear();
		_sharedObjects = nullptr;
	}

	if (_listener) {
		_listener->close();
		_listener = nullptr;
	}
	updateServerInfo();
	return true;
}

void ServerAppThread::pumpListener() {
	if (!_listener) {
		return;
	}
	// Pump the listener first: read the socket, route datagrams to the connections, and accept any
	// new ones.
	_listener->handleEvents([this](Rc<remote::ServerConnection> &&conn) {
		handleRemoteConnection(sp::move(conn));
	});

	stepPendingHandshakes();

	// A copy: any step below may drop a session, and a dispatcher may drop all of them.
	auto sessions = _sessions;
	for (auto &session : sessions) {
		if (session->isClosed()) {
			log::source().info("AppThread", "remote client ", session->getId(),
					" disconnected; its windows revert to their fallback");
			resetSession(session);
			continue;
		}

		// Drain and dispatch messages; deferred ones (cb returns false) stay queued for a later
		// poll. The connection is held: a dispatcher may close the session under its own poll.
		if (auto conn = Rc<remote::ServerConnection>(session->getConnection())) {
			conn->poll([&](const remote::MessageHeader &h, BytesView payload) -> bool {
				return dispatchSessionMessage(session, h, payload);
			});
		}

		// A dispatcher may end the session, but it runs inside the reader's poll loop above, so
		// dropping the connection there would free the reader under it; the reset is carried out
		// here.
		if (session->isResetRequested()) {
			resetSession(session);
			continue;
		}

		auto now = sp::platform::clock(ClockType::Monotonic);

		/* Frame watchdog: a client that answered too late, or never committed the input it
		promised, loses that frame -- not its session. Checked here, so the cancel and the window's
		nudge happen outside the connection's own poll. */
		if (auto client = session->getRenderClient()) {
			if (client->checkFrameDeadlines(now)) {
				log::source().info("AppThread", "client ", session->getId(), " was late with ",
						client->getConsecutiveLateFrames(),
						" frames in a row; terminating the session");
				resetSession(session);
				continue;
			}
		}

		// Request watchdog: if the client left a request unanswered past its reply deadline, the
		// waiters were already failed locally, so drop the connection. A late frame is not one of
		// those requests (see ReplyTable::wait).
		if (session->failExpiredRequests(now)) {
			log::source().info("AppThread",
					"request reply timeout; terminating unresponsive client ", session->getId());
			resetSession(session);
			continue;
		}

		// Keepalive: a pong restarts the timeout (see dispatchSessionMessage). Driven by the
		// AppThread update timer (appUpdateInterval), not frame timing, so it ticks while idle.
		if (!session->updateKeepalive(now, kKeepalivePingIntervalUs, kKeepalivePongTimeoutUs)) {
			log::source().info("AppThread", "client ", session->getId(),
					" keepalive timeout (no pong for 5s); terminating connection");
			resetSession(session);
		}
	}
}

void ServerAppThread::resetSession(RemoteSession *session) {
	auto it = _sessions.begin();
	while (it != _sessions.end() && it->get() != session) { ++it; }
	if (it == _sessions.end()) {
		return;
	}
	auto keep = sp::move(*it);
	_sessions.erase(it);

	if (_sessionObserver) {
		_sessionObserver(keep.get(), SessionEvent::Closed);
	}

	/* Windows this session asked us to open exist for it alone, so they go with it. One that has
	not arrived yet is marked instead; handleAppWindowCreated closes it on arrival.

	They are collected before anything is closed: closing one drops its record (see
	handleAppWindowDestroyed), which would cut this loop from under itself. */
	Set<AppWindow *> closing;
	Vector<Rc<WindowSceneInfo>> handles;
	for (auto &rec : _clientWindows) {
		if (rec.session != session->getId()) {
			continue;
		}
		rec.session = 0;
		if (auto w = rec.handle ? rec.handle->getWindow() : nullptr) {
			closing.emplace(w);
			handles.emplace_back(rec.handle);
		} else {
			rec.orphaned = true;
		}
	}

	/* Revert the rest of its windows to their local Directors (killing in-flight remote frames),
	whether or not the registry still knows them.

	A window that is about to close is left out: reverting it hands work (releasing text input,
	restarting presentation) to the window system for a window that will not exist by the time that
	work runs. */
	auto client = session->getRenderClient();
	for (auto w : _windows) {
		if (client && w->getRenderClient() == client && closing.find(w) == closing.end()) {
			revertSharedWindow(w);
		}
	}

	for (auto &handle : handles) {
		if (auto w = handle->getWindow()) {
			w->close(false);
		}
	}

	bool released = false;
	if (_sharedObjects) {
		released = !_sharedObjects->releaseSession(session->getId()).empty();
	}

	/* A font endpoint serves one session and is not handed to the next: its FontLibrary adopted
	this client's FaceIds, and a kept face would answer the next client's CharIds with its glyphs
	keyed under another id - blank text. A fresh endpoint waits for the next client instead. */
	if (auto fontServer = session->close()) {
		fontServer->invalidate();
	}
	if (_createFontServer && _idleFontServers.empty()) {
		_idleFontServers.emplace_back(_createFontServer(this, _fontComponent, _fontStore));
	}

	for (auto assignment = _windowAssignments.begin(); assignment != _windowAssignments.end();) {
		if (assignment->second == session->getId()) {
			assignment = _windowAssignments.erase(assignment);
		} else {
			++assignment;
		}
	}

	// The windows it held or had reserved are open to the others again.
	if (released || !_sessions.empty()) {
		republishSharedObjects();
	}
}

void ServerAppThread::revertSharedWindow(AppWindow *w) {
	w->invalidateRemoteFrames();

	/* Release text input the departed client may have acquired, otherwise the native window keeps
	claiming keys (and the OS keyboard stays up on mobile). Releasing unacquired input is a no-op. */
	w->releaseTextInput();

	w->setRenderClient(static_cast<core::RenderClientChannel *>(w->getDirector()));
	// Restart presentation for the new client (clears a stale display-link barrier, pumps a frame).
	w->resetForRenderClientChange();
}

bool ServerAppThread::dispatchSessionMessage(RemoteSession *session, const remote::MessageHeader &h,
		BytesView payload) {
	if (session->dispatchReply(h, payload)) {
		return true;
	}

	// Null once the session is closed (the message may still sit in the deferred queue).
	auto conn = session->getConnection();
	auto client = session->getRenderClient();
	auto sessionId = session->getId();

	// A window this session may address; any other is answered as an unknown handle.
	auto resolveWindow = [&](uint64_t windowId) -> AppWindow * {
		if (!_sharedObjects || !_sharedObjects->isWindowVisible(windowId, sessionId)) {
			return nullptr;
		}
		return static_cast<AppWindow *>(_sharedObjects->resolveWindow(windowId));
	};

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
			session->handlePong(sp::platform::clock(ClockType::Monotonic));
			return true;
		case remote::GlobalCode::AppRequest:
		case remote::GlobalCode::AppNotify: {
			// A reply that came after its waiter expired is not a new request.
			if (remote::isReplyOrError(h)) {
				return true;
			}
			auto isRequest = remote::GlobalCode(h.code) == remote::GlobalCode::AppRequest;
			if (!_appMessageHandler) {
				if (isRequest && conn) {
					conn->sendError(remote::Domain::Global,
							toInt(remote::GlobalError::NotImplemented), h.serial);
				}
				return true;
			}
			Rc<AppReply> reply;
			if (isRequest) {
				reply = Rc<AppReply>::create(session, session, h.serial);
			}
			_appMessageHandler(session, data::read<Interface>(payload), sp::move(reply));
			return true;
		}
		default:
			if (conn && !remote::isReplyOrError(h)) {
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
			// Replies go through `conn`, which is null once the client is gone (the message may
			// still sit in the deferred queue).
			if (!conn || !_sharedObjects) {
				return true;
			}
			auto val = data::read<Interface>(payload);
			auto queueId = uint64_t(val.getInteger());
			auto q = _sharedObjects->isQueueVisible(queueId, sessionId)
					? _sharedObjects->resolveQueue(queueId)
					: nullptr;
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
		case remote::WindowCode::AcquireFrame: {
			// The client's answer to a frame we already gave up on (see checkFrameDeadlines): the
			// waiter is gone, so the reply falls through to here. Consumed in silence -- a frame
			// that arrived too late is not a protocol error.
			return true;
		};
		case remote::WindowCode::FrameInput: {
			// client -> server: one streamed input for one or more attachments [frameId, keys[],
			// bytes]
			if (client) {
				auto val = data::read<Interface>(payload);
				Vector<StringView> keys;
				for (auto &k : val.getValue(1).asArray()) { keys.emplace_back(k.getString()); }
				client->handleFrameInput(uint64_t(val.getInteger(0)), keys, val.getBytes(2));
			}
			return true;
		};
		case remote::WindowCode::FrameCommit: {
			// client -> server: all inputs for a frame were submitted
			if (client) {
				auto val = data::read<Interface>(payload);
				client->handleFrameCommit(uint64_t(val.getInteger(0)));
			}
			return true;
		};
		case remote::WindowCode::CompileMaterials: {
			// client -> server: compile a runtime (font atlas) material the headless client can't
			// compile.
			// Compiled into the window's queue, so only for a window this session can see.
			if (client
					&& resolveWindow(
							uint64_t(data::read<Interface>(payload).getInteger("window")))) {
				client->handleCompileMaterials(payload);
			}
			return true;
		};
		case remote::WindowCode::AttachQueue: {
			// client -> server: the client attached the compiled shared queue to its Director. Hand
			// the window's frame production over to the remote client and acknowledge with an empty
			// reply.
			if (!conn) {
				return true;
			}
			auto windowId = uint64_t(data::read<Interface>(payload).getInteger());
			if (!takeoverSharedWindow(windowId, session)) {
				conn->sendError(remote::Domain::Window,
						toInt(remote::WindowError::InvalidObjecthandle), h.serial);
				return true;
			}
			conn->sendReply(h.serial, remote::Domain(h.domain), h.code, BytesView());
			// Taken, the window is no longer offered to the others.
			republishSharedObjects(session);
			return true;
		};
		case remote::WindowCode::CreateWindow: {
			// client -> server request: open a window for this session. A refusal is a Status in
			// the reply, not an error: policy must not cost the session.
			if (!conn) {
				return true;
			}
			if (!_clientWindowHandler) {
				conn->sendError(remote::Domain::Window, toInt(remote::WindowError::NotImplemented),
						h.serial);
				return true;
			}
			auto val = data::read<Interface>(payload);
			if (!val.isDictionary()) {
				conn->sendError(remote::Domain::Window,
						toInt(remote::WindowError::SerializationFailed), h.serial);
				return true;
			}

			createClientWindow(session, remote::deserializeWindowRequest(val), h.serial,
					[session = Rc<RemoteSession>(session), serial = h.serial, code = h.code](
							Status st, StringView id, const sprt::window::WindowInfo *granted) {
				Value reply;
				reply.setInteger(int64_t(toInt(st)), "st");
				reply.setString(id, "id");
				if (granted) {
					reply.setValue(remote::serializeWindowRequest(*granted), "win");
				}
				session->remoteSendCborReply(serial, remote::Domain::Window, code, reply);
			});
			return true;
		};
		case remote::WindowCode::ReadyForNextFrame: {
			// client -> server: the client's scene wants the next frame (active actions/input);
			// schedule it on the window's PresentationEngine. Notification only, no reply.
			auto windowId = uint64_t(data::read<Interface>(payload).getInteger());
			if (auto w = resolveWindow(windowId)) {
				w->setReadyForNextFrame();
			}
			return true;
		};
		case remote::WindowCode::RequestScreenshot: {
			// client -> server: capture the window's current contents and send them back over
			// Domain::Data as a Screenshot transfer; `reason` references this request so the client
			// can match it. No reply.
			if (_sharedObjects) {
				auto windowId = uint64_t(data::read<Interface>(payload).getInteger());
				auto reqSerial = h.serial;
				if (auto w = resolveWindow(windowId)) {
					w->captureScreenshot(
							[this, session = Rc<RemoteSession>(session), reqSerial,
									windowId](const core::ImageInfoData &info, BytesView pixels) {
						// GL loop thread: the pixels view is transient, so copy it, then hop to the
						// app thread (the connection and block transfer live there) to offer the
						// blob.
						auto pixelsCopy = pixels.bytes<Interface>();
						auto infoCopy = info;
						performOnAppThread(
								[session, reqSerial, windowId, infoCopy,
										pixelsCopy = sp::move(pixelsCopy)]() mutable {
							// The client may be gone by now; its transfers went with it.
							auto transfer =
									session->isClosed() ? nullptr : session->getBlockTransfer();
							if (!transfer) {
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

							auto id = transfer->startTransfer(remote::DataType::Screenshot,
									BytesView(pixelsCopy.data(), pixelsCopy.size()), sp::move(meta),
									sp::move(reason),
									[transfer = Rc<BlockTransferManager>(transfer)](uint64_t tid,
											bool ok) {
								log::source().info("AppThread", "screenshot transfer ",
										ok ? "completed" : "failed");
								// One-shot push: release it once done to free the client's retained
								// copy.
								if (ok) {
									transfer->releaseObject(tid);
								}
							},
									kScreenshotTransferPriority);
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
			// client -> server: the window's interaction layers (hit/cursor/drag regions) in the
			// typed wire format (see serializeWindowLayers), applied to the real window for OS
			// hit-testing.
			if (!_sharedObjects) {
				return true;
			}
			uint64_t windowId = 0;
			sprt::window::Vector<sprt::window::WindowLayer> layers;
			if (!remote::deserializeWindowLayers(payload, windowId, layers)) {
				log::source().warn("AppThread", "UpdateLayers: malformed batch (", payload.size(),
						" bytes)");
				return true;
			}
			for (size_t i = 0; i < layers.size(); ++i) {
				auto &layer = layers[i];
				log::source().debug("AppThread", "UpdateLayers: layer[", i, "] rect{",
						layer.rect.origin.x, ",", layer.rect.origin.y, " ", layer.rect.size.width,
						"x", layer.rect.size.height, "} cursor=", uint32_t(toInt(layer.cursor)),
						" flags=", uint32_t(toInt(layer.flags)));
			}

			if (auto w = resolveWindow(windowId)) {
				log::source().info("AppThread", "UpdateLayers: applying ", layers.size(),
						" layer(s) to window ", windowId);
				w->updateLayers(sp::move(layers));
			} else {
				log::source().warn("AppThread", "UpdateLayers for unknown shared window ",
						windowId);
			}
			return true;
		};
		case remote::WindowCode::TextInputControl: {
			// Fire-and-forget: the answer travels back as a TextInputState echo, not as a reply.
			if (!_sharedObjects) {
				return true;
			}
			auto val = data::read<Interface>(payload);
			auto w = resolveWindow(uint64_t(val.getInteger("w")));
			if (!w) {
				return true;
			}
			switch (remote::TextInputOp(val.getInteger("op"))) {
			case remote::TextInputOp::Acquire:
				w->acquireTextInput(remote::deserializeTextInputRequest(val.getValue("req")));
				break;
			case remote::TextInputOp::Release: w->releaseTextInput(); break;
			case remote::TextInputOp::Perform:
				w->performTextInput(remote::deserializeTextInputCommand(val.getValue("cmd")));
				break;
			}
			return true;
		}
		case remote::WindowCode::WindowControl: {
			// Handles every window op a remote scene can request; the reply is always a Status.
			if (!conn || !_sharedObjects) {
				return true;
			}
			auto val = data::read<Interface>(payload);
			auto windowId = uint64_t(val.getInteger("w"));
			auto w = resolveWindow(windowId);
			if (!w) {
				conn->sendError(remote::Domain::Window,
						toInt(remote::WindowError::InvalidObjecthandle), h.serial);
				return true;
			}

			// Some answers come later, when the session may be gone.
			auto reply = [session = Rc<RemoteSession>(session), serial = h.serial, code = h.code](
								 Status st) {
				Value r;
				r.addInteger(int64_t(toInt(st)));
				session->remoteSendCborReply(serial, remote::Domain::Window, code, r);
			};

			switch (remote::WindowControlOp(val.getInteger("op"))) {
			case remote::WindowControlOp::Close:
				// Reply before closing: the close tears down the connection the reply travels over.
				reply(Status::Ok);
				w->close(val.getBool("graceful"));
				break;
			case remote::WindowControlOp::EnableState:
				// The client already refuses impossible states; this re-checks a peer that sent one
				// anyway.
				reply(w->enableState(core::WindowState(uint64_t(val.getInteger("state"))))
								? Status::Ok
								: Status::Declined);
				break;
			case remote::WindowControlOp::DisableState:
				reply(w->disableState(core::WindowState(uint64_t(val.getInteger("state"))))
								? Status::Ok
								: Status::Declined);
				break;
			case remote::WindowControlOp::SetFullscreen: {
				// Where a virtual window is and how big is its window manager's decision (this server),
				// never the client's.
				if (w->isVirtual()) {
					reply(Status::Declined);
					break;
				}
				auto info = remote::deserializeFullscreenInfo(val.getValue("fs"));
				if (!w->setFullscreen(sp::move(info), [reply](Status st) { reply(st); }, this)) {
					reply(Status::Declined);
				}
				break;
			}
			case remote::WindowControlOp::SetPreferredFrameRate:
				if (!w->setPreferredFrameRate(float(val.getDouble("rate")),
							[reply](Status st) { reply(st); })) {
					reply(Status::Declined);
				}
				break;
			case remote::WindowControlOp::SetPreferredFrameInterval:
				w->setPreferredFrameInterval(uint64_t(val.getInteger("iv")));
				reply(Status::Ok);
				break;
			case remote::WindowControlOp::SetWindowExtent: {
				if (w->isVirtual()) {
					reply(Status::Declined);
					break;
				}
				auto &ext = val.getValue("ext");
				w->setWindowExtent(
						Extent2(uint32_t(ext.getInteger(0)), uint32_t(ext.getInteger(1))),
						[reply](Status st) { reply(st); }, this);
				break;
			}
			case remote::WindowControlOp::OpenWindowMenu: {
				auto &p = val.getValue("pos");
				reply(w->openWindowMenu(Vec2(float(p.getDouble(0)), float(p.getDouble(1))))
								? Status::Ok
								: Status::Declined);
				break;
			}
			case remote::WindowControlOp::BackButton:
				w->handleBackButton();
				reply(Status::Ok);
				break;
			default:
				conn->sendError(remote::Domain::Window, toInt(remote::WindowError::NotImplemented),
						h.serial);
				break;
			}
			return true;
		}
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
		auto transfer = session->getBlockTransfer();
		return transfer ? transfer->dispatch(h, payload) : true;
	} else if (remote::Domain(h.domain) == remote::Domain::Font) {
		if (auto fontServer = session->getFontServer()) {
			return fontServer->dispatch(h.code, h.serial, payload);
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
	auto now = sp::platform::clock(ClockType::Monotonic);
	if (_pendingHandshakes.size() >= kMaxPendingHandshakes) {
		log::source().warn("AppThread", "too many connections in their setup handshake; dropping ",
				"a new one");
		conn->close();
		return;
	}

	PendingHandshake pending;
	pending.acceptedAt = now;
	pending.backoffKey = getBackoffKey(*conn);
	if (isBackedOff(pending.backoffKey, now)) {
		log::source().warn("AppThread", "handshake rate limit in force; refusing new connection");
		pending.refusal = remote::GlobalError::Busy;
	} else if (_sessions.size() >= _maxRemoteClients) {
		log::source().warn("AppThread", "all ", _maxRemoteClients,
				" remote client slot(s) taken; refusing new connection");
		pending.refusal = remote::GlobalError::Busy;
	}

	conn->getHandshake().begin(now + kServerHandshakeDeadlineUs);

	// Step the handshake as soon as the peer writes. QUIC's accepted connections share the
	// listener's socket, which _listenPoll already watches.
	auto handle = conn->getPollHandle();
	if (handle.fd >= 0) {
		if (handle.fd != _listener->getPollHandle().fd) {
			pending.wake = watchTransport(handle, remote::TransportWaitAddress(),
					[this] { pumpListener(); });
		}
	} else {
		pending.wake = watchTransport(handle, conn->getWaitAddress(), [this] { pumpListener(); });
	}

	pending.connection = sp::move(conn);
	_pendingHandshakes.emplace_back(sp::move(pending));
}

void ServerAppThread::stepPendingHandshakes() {
	using State = remote::ServerHandshake::State;

	if (_pendingHandshakes.empty()) {
		return;
	}

	auto now = sp::platform::clock(ClockType::Monotonic);
	size_t slotsReserved = 0;
	for (auto &it : _pendingHandshakes) { slotsReserved += it.holdsSlot ? 1 : 0; }

	for (size_t i = 0; i < _pendingHandshakes.size();) {
		auto &pending = _pendingHandshakes[i];
		auto transport = pending.connection->getTransport();
		auto &handshake = pending.connection->getHandshake();
		auto state = transport ? handshake.step(*transport, now) : State::Failed;
		bool negotiationFailed = false;

		if (state == State::HelloReceived) {
			if (pending.refusal != remote::GlobalError::Ok) {
				handshake.reply(pending.refusal, BytesView());
			} else if (_sessions.size() + slotsReserved >= _maxRemoteClients) {
				// Another handshake took the last slot while this one was on its way.
				pending.refusal = remote::GlobalError::Busy;
				handshake.reply(pending.refusal, BytesView());
			} else {
				// A labelled key identifies its client, so it counts on every transport; without
				// one the transport's own authentication stands in for the key.
				auto labelled = _labelledKeys.match(handshake.getPresentedKey(), pending.label);
				auto requireKey =
						!labelled && !transport->hasCaps(remote::TransportCaps::PeerAuthenticated);
				auto status = handshake.negotiate(_expectedKey, _dictionary, requireKey);
				if (status == remote::GlobalError::Ok && _requireLabelledKeys && !labelled) {
					status = remote::GlobalError::AuthFailed;
				}
				handshake.reply(status, _dictionary);
				if (status == remote::GlobalError::Ok) {
					pending.holdsSlot = true;
					++slotsReserved;
				} else {
					pending.refusal = status;
					negotiationFailed = true;
				}
			}
			state = handshake.step(*transport, now);
		} else if (state == State::ReadingHello && pending.refusal != remote::GlobalError::Ok
				&& now >= pending.acceptedAt + kRefusalHelloWaitUs) {
			handshake.reply(pending.refusal, BytesView());
			state = handshake.step(*transport, now);
		}

		if (negotiationFailed) {
			auto backoff = recordHandshakeResult(pending.backoffKey, false, now);
			log::source().error("AppThread", "client handshake failed (status ",
					uint32_t(toInt(pending.refusal)), "); refusing further ones for ",
					backoff / 1'000, "ms");
		}

		if (state != State::Done && state != State::Failed) {
			++i;
			continue;
		}

		auto entry = sp::move(pending);
		_pendingHandshakes.erase(_pendingHandshakes.begin() + i);
		if (entry.wake) {
			entry.wake->cancel();
		}

		if (entry.holdsSlot) {
			--slotsReserved;
			if (state == State::Done) {
				recordHandshakeResult(entry.backoffKey, true, now);
				entry.connection->adoptHandshake();
				log::source().info("AppThread", "client authenticated");
				installRemoteClient(sp::move(entry.connection), entry.label);
				continue;
			}
			log::source().error("AppThread", "client handshake failed while replying; dropping");
		}
		entry.connection->close();
	}
}

void ServerAppThread::installRemoteClient(Rc<remote::ServerConnection> &&conn, StringView label) {
	auto session = Rc<RemoteSession>::create(this, _nextSessionId++, sp::move(conn));
	if (!session) {
		return;
	}
	if (!label.empty()) {
		session->setLabel(label);
		_labelledKeys.consume(label);
	}

	// Wake on the connection's own readiness; otherwise the session advances only on the 1s app
	// update tick and the frame protocol can not keep up.
	auto c = session->getConnection();
	auto handle = c->getPollHandle();
	// native_handle is a union with no comparison, so compare the fd. A transport whose accept
	// returns the listening socket itself (QUIC) is already covered by _listenPoll.
	if (handle.fd >= 0) {
		if (handle.fd != _listener->getPollHandle().fd) {
			session->setWake(watchTransport(handle, remote::TransportWaitAddress(),
					[this] { pumpListener(); }));
		}
	} else {
		session->setWake(watchTransport(handle, c->getWaitAddress(), [this] { pumpListener(); }));
	}

	session->setFontServer(acquireFontServer());
	_sessions.emplace_back(session);

	// Have an endpoint ready for the next client, so its atlas is compiled before it asks.
	if (_createFontServer && _idleFontServers.empty() && _sessions.size() < _maxRemoteClients) {
		_idleFontServers.emplace_back(_createFontServer(this, _fontComponent, _fontStore));
	}

	// Exchange peer info before anything is announced; handleClientInfo starts the session. The
	// waiter lives in the session's own reply table, so it can not outlive the session.
	updateServerInfo();
	auto s = session.get();
	if (!session->sendMessageWithReply(remote::Domain::Global,
				toInt(remote::GlobalCode::ServerInfo), remote::serializePeerInfo(_localInfo),
				[this, s](const remote::MessageHeader &h, BytesView payload) {
		handleClientInfo(s, h, payload);
	}, kPeerInfoReplyTimeoutUs)) {
		log::source().error("AppThread", "failed to send ServerInfo; dropping connection");
		resetSession(session);
	}
}

Rc<RemoteFontServer> ServerAppThread::acquireFontServer() {
	if (!_idleFontServers.empty()) {
		auto ret = sp::move(_idleFontServers.back());
		_idleFontServers.pop_back();
		return ret;
	}
	if (_createFontServer) {
		return _createFontServer(this, _fontComponent, _fontStore);
	}
	return nullptr;
}

void ServerAppThread::dropPendingHandshakes() {
	auto pending = sp::move(_pendingHandshakes);
	_pendingHandshakes.clear();
	for (auto &it : pending) {
		if (it.wake) {
			it.wake->cancel();
		}
		it.connection->close();
	}
}

String ServerAppThread::getBackoffKey(const remote::ServerConnection &conn) const {
	auto transport = conn.getTransport();
	if (transport) {
		auto &peer = transport->getPeerIdentity();
		if (peer.authenticated && peer.uid >= 0) {
			return toString("uid:", peer.uid);
		}
	}
	return String();
}

bool ServerAppThread::isBackedOff(StringView key, uint64_t now) const {
	auto it = _handshakeBackoff.find(key.str<Interface>());
	return it != _handshakeBackoff.end() && now < it->second.until;
}

uint64_t ServerAppThread::recordHandshakeResult(StringView key, bool success, uint64_t now) {
	auto name = key.str<Interface>();
	if (success) {
		_handshakeBackoff.erase(name);
		return 0;
	}
	auto it = _handshakeBackoff.find(name);
	if (it == _handshakeBackoff.end()) {
		it = _handshakeBackoff.emplace(name, HandshakeBackoff()).first;
	}
	// Each consecutive failure doubles the cool-off, up to the ceiling.
	auto backoff = kHandshakeBackoffBaseUs << sprt::min(it->second.failures, uint32_t(8));
	backoff = sprt::min(backoff, kHandshakeBackoffMaxUs);
	++it->second.failures;
	it->second.until = now + backoff;
	return backoff;
}

void ServerAppThread::handleClientInfo(RemoteSession *session, const remote::MessageHeader &h,
		BytesView payload) {
	if (session->isClosed()) {
		return; // the connection went away while the request was outstanding
	}

	if (remote::isError(h)) {
		if (remote::GlobalError(h.code) == remote::GlobalError::NotImplemented) {
			// A version-1 client does not know this message; the session proceeds without peer
			// info.
			log::source().info("AppThread",
					"client does not implement ServerInfo; continuing as a version-1 peer");
		} else {
			log::source().error("AppThread", "client refused ServerInfo (code ", uint32_t(h.code),
					"); dropping connection");
			session->requestReset();
			return;
		}
	} else {
		auto info = remote::deserializePeerInfo(data::read<Interface>(payload));
		// A compliant client refuses a mismatch itself (error branch above); this catches a peer
		// that answered anyway.
		if (!_localInfo.isWireCompatible(info)) {
			// Not fatal: the wire format is field-by-field, so a differing tag only means the
			// builds disagree about some enum range. Logged for diagnostics.
			StringStream clientDesc;
			StringStream localDesc;
			info.description([&](StringView str) { clientDesc << str; });
			_localInfo.description([&](StringView str) { localDesc << str; });
			log::source().warn("AppThread",
					"client was built against a different wire contract; continuing\n  client: ",
					clientDesc.str(), "\n  server: ", localDesc.str());
		}

		StringStream desc;
		info.description([&](StringView str) { desc << str; });
		log::source().info("AppThread", "client: ", desc.str());

		// Log the codes the client's build does not implement.
		StringStream missing;
		_localInfo.describeMissingCodes(info, [&](StringView str) { missing << str; });
		if (!missing.empty()) {
			log::source().warn("AppThread", "client does not implement: ", missing.str());
		}

		session->setPeerInfo(sp::move(info));
	}

	// Do not take the windows over yet: each is handed over when the client sends
	// WindowCode::AttachQueue, so AcquireFrame never reaches a client not ready to serve it.
	if (_sharedObjects) {
		session->getRenderClient()->announce(_sharedObjects);
	}
	if (_sessionObserver) {
		_sessionObserver(session, SessionEvent::Started);
	}
}

bool ServerAppThread::takeoverSharedWindow(uint64_t windowId, RemoteSession *session) {
	if (!_sharedObjects) {
		return false;
	}
	auto w = static_cast<AppWindow *>(_sharedObjects->resolveWindow(windowId));
	if (!w) {
		log::source().warn("AppThread", "AttachQueue for unknown shared window ", windowId);
		return false;
	}
	if (!_sharedObjects->claimWindow(windowId, session->getId())) {
		log::source().warn("AppThread", "client ", session->getId(), " can not take window ",
				windowId, ": it is reserved or served by another client");
		return false;
	}
	w->setRenderClient(session->getRenderClient());

	/* The window's state as it is now, as the one event a local window gets when it maps. The
	announce carries a state too, but it is the one the window had when it was first shared - usually
	before the map event reached this thread - and a state change that happened before the client
	attached was forwarded to nobody. Without this the client's mirror stays empty until the next
	change. */
	auto state = w->getWindowState();
	if (state != core::WindowState::None) {
		core::InputEventData event{
			0,
			core::InputEventName::WindowState,
			{.input = {core::InputMouseButton::None, core::InputModifier::None, nan(), nan()}},
			{.window = {state, state}},
		};
		Vector<core::InputEventData> events;
		events.emplace_back(event);
		session->getRenderClient()->handleInputEvents(windowId, sp::move(events));
	}

	// Restart presentation for the new client (clears a stale display-link barrier, pumps a frame).
	w->resetForRenderClientChange();
	return true;
}

void ServerAppThread::performAppUpdate(const UpdateTime &time, bool wakeup) {
	AppThread::performAppUpdate(time, wakeup);

	// Service the listener's QUIC timers on the app-update cadence (no-op unless listening).
	// Socket readiness is handled via _listenPoll; keepalive runs from pumpListener.
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

		// Network font endpoints (remote::Domain::Font), one per session: each a separate
		// controller with its own FontLibrary and atlas, so client FaceIds never collide with the
		// local controller's or each other's - nor with an earlier client's, since an endpoint is
		// dropped with its session. One is created now, so the first client finds its atlas
		// compiled.
		_createFontServer = SharedModule::acquireTypedSymbol<
				decltype(&font::RemoteFontServerEndpoint::createServerFontEndpoint)>(
				buildconfig::MODULE_XENOLITH_FONT_NAME,
				"RemoteFontServerEndpoint::createServerFontEndpoint");
		if (_createFontServer) {
			_fontComponent = comp;
			if (auto server = _createFontServer(this, comp, _fontStore)) {
				_idleFontServers.emplace_back(sp::move(server));
			}
		}
	}
#endif

	updateServerInfo(); // PeerFeatures::FontServer depends on what just loaded
}

void ServerAppThread::finalizeExtensions() {
	AppThread::finalizeExtensions();

	// Font endpoints are not registered extensions, so release them here while the gapi device is
	// still up: their atlases are device images.
	for (auto &it : _sessions) {
		if (auto server = it->close()) {
			server->invalidate();
		}
	}
	_sessions.clear();
	for (auto &it : _idleFontServers) { it->invalidate(); }
	_idleFontServers.clear();
	_createFontServer = nullptr;
	_fontComponent = nullptr;
	_fontStore = nullptr;
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

	// A window created with a WindowSceneInfo defines its own scene.
	if (auto sceneInfo = w->getSceneInfo()) {
		scene = sceneInfo->makeScene(this, w, c);
		if (scene) {
			return scene;
		}
	}

	// Fallback for windows the application did not create itself — above all the root window,
	// whose WindowInfo is built from the command line before the app thread exists.
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
