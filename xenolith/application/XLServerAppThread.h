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

#ifndef XENOLITH_APPLICATION_XLSERVERAPPTHREAD_H_
#define XENOLITH_APPLICATION_XLSERVERAPPTHREAD_H_

#include "XLAppThread.h"
#include "XLRemoteAddress.h"
#include "XLRemoteObject.h"
#include "XLRemoteBearerKeys.h"
#include "XLRemoteSession.h"
#include "XLWindowSceneInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace core {
class RenderClientChannel;
} // namespace core

namespace remote {
class Listener;
class ServerConnection;
struct MessageHeader;
} // namespace remote

namespace font {
class FontComponent;
} // namespace font

class Context;
class RemoteRenderClient;
class RemoteFontServer;

// Server (and local single-process) application thread: owns the Context, the native windows and
// their Directors, and the remote-connection listener. The default factory creates this thread.
class SP_PUBLIC ServerAppThread : public AppThread {
public:
	virtual ~ServerAppThread();

	virtual bool init(NotNull<Context>);

	Context *getContext() const { return _context; }

	remote::ObjectRegistry *getSharedObjects() const { return _sharedObjects; }

	// SHA-256 of the listener's DER SubjectPublicKeyInfo, handed to a client out-of-band to
	// authenticate this server (see remote::Listener::getCertificateFingerprint). Empty while not
	// listening.
	BytesView getListenerFingerprint() const;

	// True while any remote client is connected (it passed the handshake).
	bool hasRemoteClient() const;

	// How many clients may be connected at once; the next one is refused with Busy. 1 by default.
	void setMaxRemoteClients(uint32_t);
	uint32_t getMaxRemoteClients() const { return _maxRemoteClients; }

	const Vector<Rc<RemoteSession>> &getRemoteSessions() const { return _sessions; }
	RemoteSession *getRemoteSession(uint64_t id) const;

	/* What a window a CLIENT asked for actually is. Without a handler installed a server refuses
	every request and does not advertise the feature to its clients.

	Runs on the app thread before the window is created. `info` is the request as this server is
	prepared to honour it; the handler may change anything (clamp the size, force the type, retitle)
	-- the client asks, the server decides. `outScene` is the handle that defines the scene and the
	close behaviour, exactly as for a window the application opens itself.

	Anything but Status::Ok refuses the request, and that Status is what the client is told. */
	using ClientWindowHandler = Function<Status(NotNull<RemoteSession>,
			NotNull<sprt::window::WindowInfo>, Rc<WindowSceneInfo> &outScene)>;

	void setClientWindowHandler(ClientWindowHandler &&);
	bool hasClientWindowHandler() const { return _clientWindowHandler != nullptr; }

	// How many windows one session may hold at once; 4 by default.
	void setMaxClientWindows(uint32_t perSession);
	uint32_t getMaxClientWindows() const { return _maxClientWindows; }
	size_t getClientWindowCount(uint64_t session) const;

	/* Open a window because `session` asked for it: policy, then the application's handler, then
	Context::createWindow. `serial` is the request it answers (0 when driven locally), echoed to that
	session in the announce. `complete` runs on the app thread with the verdict, the final (possibly
	re-uniqued) window id and the request as the server granted it -- the window becomes drawable
	later, when its scene has shared a queue and the announce carries it. */
	using ClientWindowCallback =
			Function<void(Status, StringView id, const sprt::window::WindowInfo *granted)>;
	void createClientWindow(NotNull<RemoteSession>, Rc<sprt::window::WindowInfo> &&,
			uint32_t serial, ClientWindowCallback &&complete);

	// Start listening without sharing a window: the shape a window manager needs, where every window
	// belongs to a client.
	bool startSession(StringView address, BytesView key);

	/* A key issued to one client, typically a process this server launches with it: the session
	that presents it gets `label` (RemoteSession::getLabel). Accepted on every transport, including
	the ones that vouch for their peer and need no key otherwise. A single-use key stops matching
	once a session holds it. Can be added while listening. */
	void addBearerKey(BytesView key, StringView label, bool singleUse = true);
	bool removeBearerKey(StringView label);

	// Refuse every client that did not present a labelled key, even on a peer-authenticating
	// transport (AuthFailed).
	void setRequireLabelledKeys(bool);
	bool isRequireLabelledKeys() const { return _requireLabelledKeys; }

	/* Application messages from the clients (GlobalCode::AppRequest/AppNotify), on the app thread.
	`reply` is null for a notification; a request left unanswered is refused with NotImplemented
	when the last reference to its reply goes away. Installing a handler advertises
	PeerFeatures::AppMessages. */
	using AppMessageHandler =
			Function<void(NotNull<RemoteSession>, Value &&, Rc<AppReply> &&reply)>;
	void setAppMessageHandler(AppMessageHandler &&);

	// False when the session is closed or does not receive application messages. A request's
	// callback runs once with the answer, a timeout or a refusal (see getAppReplyStatus), but not
	// when the session closes first; an unanswered request never costs the session.
	bool sendAppNotification(NotNull<RemoteSession>, const Value &);
	bool sendAppRequest(NotNull<RemoteSession>, const Value &, Function<void(Status, Value &&)> &&,
			uint64_t timeoutUs);

	// Started: the session exchanged peer info and got its first announce. Closed: it is being torn
	// down, still with its id and label; it comes for every session, started or not.
	enum class SessionEvent {
		Started,
		Closed,
	};

	using SessionObserver = Function<void(NotNull<RemoteSession>, SessionEvent)>;
	void setSessionObserver(SessionObserver &&);

	/* Reserve the shared window with this name for one session: only that session sees it, and no
	other can take it. 0 lifts the reservation. Applies at once to a window already shared (other
	sessions see it vanish from their announce) and later to a window shared under the name. A
	reservation ends with its session; a window someone already serves stays with them. */
	void assignSharedWindow(StringView windowName, uint64_t session);

	// Drop every connected client; their windows go back to their own Directors.
	void resetRemoteSessions();

	virtual size_t cancelOutgoingTransfers() override;

	// This process's own identity: OS, window subsystem, gAPI, features. On a server the window
	// owner is itself, so local and remote scenes get the same kind of answer.
	virtual const remote::PeerInfo *getServerInfo() const override { return &_localInfo; }

	// AppThread platform-services interface (delegates to the OS via the Context).
	virtual void readFromClipboard(Function<void(Status, BytesView, StringView)> &&dataCallback,
			Function<StringView(SpanView<StringView>)> &&selectCallback,
			Ref *ref = nullptr) override;
	virtual void probeClipboard(Function<void(Status, SpanView<StringView>)> &&cb,
			Ref *ref = nullptr) override;
	virtual void writeToClipboard(BytesView data, StringView contentType = StringView("text/plain"),
			Ref *ref = nullptr, StringView label = StringView()) override;
	virtual void writeToClipboard(
			sprt::window::Function<sprt::window::Bytes(StringView)> &&dataCallback,
			SpanView<StringView> types, Ref *ref = nullptr,
			StringView label = StringView()) override;
	virtual void writeToClipboard(Rc<sprt::window::ClipboardData> &&data) override;
	virtual void acquireScreenInfo(Function<void(NotNull<ScreenInfo>)> &&,
			Ref * = nullptr) override;
	virtual void openUrl(StringView) override;

	virtual const ContextInfo *getContextInfo() const override;
	virtual core::Loop *getGlLoop() const override;

	virtual Rc<Director> handleAppWindowCreated(NotNull<AppWindow>,
			const core::FrameConstraints &c) override;
	virtual void handleAppWindowDestroyed(NotNull<AppWindow>, Rc<Director> &&) override;

	virtual bool isServerThread() const override;
	virtual bool isListening() const override;
	virtual bool setListenAddress(StringView) override;
	virtual bool setBearerKey(BytesView) override;
	virtual bool setCompressionDictionary(BytesView) override;

	// Starts listening (to make shared registry available) and share window
	virtual bool shareWindow(AppWindow *, SpanView<core::Queue *>,
			const HashMap<const core::MaterialAttachment *, Rc<core::MaterialSet>> & = {}) override;

protected:
	// A bearer key is set, or the listen address's transport authenticates peers without one.
	bool hasCredentials() const;

	// Also clears shared objects if not empty
	virtual bool startListening() override;

	// Stop listening, drop the clients and clear all shared objects
	virtual bool stopListening() override;

	virtual void handleThreadInitialized() override;
	virtual void handleThreadDisposed() override;
	virtual void handleThreadUpdated(const UpdateTime &) override;

	virtual void handleMatrialsUpdated(NotNull<core::MaterialSet>) override;

	virtual void performAppUpdate(const UpdateTime &, bool wakeup) override;

	virtual void loadExtensions() override;
	virtual void finalizeExtensions() override;

	// Pump the listener's QUIC events (poll-readable or update tick), accept new connections and
	// serve every session: messages, reply deadlines, keepalive.
	void pumpListener();
	void handleRemoteConnection(Rc<remote::ServerConnection> &&);

	// Recompute _localInfo. Called where the answer can change: a window appears, the listener
	// opens, the font extension loads.
	void updateServerInfo();

	// The peer answered (or refused) our ServerInfo request; on a compatible peer the session
	// starts here with the announce.
	void handleClientInfo(RemoteSession *, const remote::MessageHeader &, BytesView payload);

	// Re-send each session its view of the shared windows when that may have changed (the announce
	// is a snapshot the client reconciles). `except` is left out.
	void republishSharedObjects(RemoteSession *except = nullptr);

	// The session's WindowCode::AttachQueue: it takes the window, which is handed to its render
	// client. False when the window is unknown or can not be taken by this session.
	bool takeoverSharedWindow(uint64_t windowId, RemoteSession *);

	// Back to the window's own Director, killing in-flight remote frames.
	void revertSharedWindow(AppWindow *);

	// Tear a session down: revert its windows, free them for the others, close the connection and
	// free the slot. Used by the disconnect, request-timeout and keepalive-timeout paths.
	void resetSession(RemoteSession *);

	// Route one message of a session by (domain, code). Returns true if consumed, false to defer it
	// for a later poll.
	virtual bool dispatchSessionMessage(RemoteSession *, const remote::MessageHeader &,
			BytesView payload);

	// An idle font endpoint for a new session (created when none is idle); null without the font
	// module.
	Rc<RemoteFontServer> acquireFontServer();

	// Advance every accepted connection still in its setup handshake: answer each hello (the free
	// slot, or a refusal), drop the ones past their deadline and install the one that succeeded.
	// Never blocks, so a silent peer holds back neither other handshakes nor the session.
	void stepPendingHandshakes();

	// A connection whose handshake replied Ok becomes a session; `label` is its key's label.
	void installRemoteClient(Rc<remote::ServerConnection> &&, StringView label);

	// Close every connection still in its handshake.
	void dropPendingHandshakes();

	// Rate limit of failed handshakes, per peer where the transport knows who it is (uid), shared
	// by all peers otherwise.
	String getBackoffKey(const remote::ServerConnection &) const;
	bool isBackedOff(StringView key, uint64_t now) const;
	uint64_t recordHandshakeResult(StringView key, bool success, uint64_t now);

	virtual bool shouldPreserveDirector(NotNull<AppWindow>, NotNull<Director>);
	virtual void preserveDirector(NotNull<AppWindow>, Rc<Director> &&);

	virtual bool hasPreservedDirector(NotNull<AppWindow>);
	virtual Rc<Director> acquirePreservedDirector(NotNull<AppWindow>);

	virtual Rc<Director> makeDirector(NotNull<AppWindow>, const core::FrameConstraints &);
	virtual Rc<Scene> makeScene(NotNull<AppWindow>, const core::FrameConstraints &);

	Context *_context = nullptr;

	// Who we are, as told to a connecting client and as answered to local scene code.
	remote::PeerInfo _localInfo;

	Set<AppWindow *> _windows;
	HashMap<String, Rc<Director>> _preservedDirectors;

	// Server-side listener state (dormant unless a scene calls startListening).
	remote::Address _listenAddress;
	Rc<remote::Listener> _listener;
	Rc<sprt::dispatch::Handle> _listenPoll;

	// A window this server opened because a client asked for it. Identified by its handle, not by
	// its name: the runtime re-uniques a colliding id, so the name is only known once the window
	// exists.
	struct ClientWindow {
		uint64_t session = 0; // who asked; 0 once that session is gone
		uint32_t serial = 0; // the request it answers, echoed in the announce
		Rc<WindowSceneInfo> handle; // identity
		String id; // the final window id, filled in when the window is created
		bool orphaned = false; // the session left before the window arrived: close it on arrival
	};

	ClientWindow *findClientWindow(WindowSceneInfo *);
	void dropClientWindow(WindowSceneInfo *);

	ClientWindowHandler _clientWindowHandler;
	Vector<ClientWindow> _clientWindows;
	uint32_t _maxClientWindows = 4;

	// Connected clients, in the order they arrived.
	Vector<Rc<RemoteSession>> _sessions;
	uint64_t _nextSessionId = 1;
	uint32_t _maxRemoteClients = 1;

	// Window reservations by window name, applied as windows are shared.
	Map<String, uint64_t> _windowAssignments;

	// Font endpoints (Domain::Font), one per session. Kept idle with a compiled atlas between
	// sessions; all share one font store. The factory comes from xenolith_font.
	using FontServerFactory = Rc<RemoteFontServer> (*)(AppThread *, font::FontComponent *,
			Rc<Ref> &store);
	FontServerFactory _createFontServer = nullptr;
	font::FontComponent *_fontComponent = nullptr;
	Rc<Ref> _fontStore;
	Vector<Rc<RemoteFontServer>> _idleFontServers;

	// Remote auth + compression config.
	Bytes _expectedKey; // bearer key a client must present (empty ⇒ reject all)
	Bytes _dictionary; // server's LZ4 dictionary (priority over a client suggestion)
	remote::BearerKeyTable _labelledKeys;
	bool _requireLabelledKeys = false;

	AppMessageHandler _appMessageHandler;
	SessionObserver _sessionObserver;

	// Accepted connections still in their setup handshake, stepped from pumpListener.
	struct PendingHandshake {
		Rc<remote::ServerConnection> connection;
		Rc<sprt::dispatch::Handle> wake; // the connection's own readiness, when it has one
		uint64_t acceptedAt = 0;
		remote::GlobalError refusal = remote::GlobalError::Ok; // non-Ok: turn it away with this
		String backoffKey;
		String label; // of the labelled key the client presented
		bool holdsSlot = false; // replied Ok: later hellos are refused until this one settles
	};
	Vector<PendingHandshake> _pendingHandshakes;

	// Failed handshakes back off exponentially per key; a success clears the key.
	struct HandshakeBackoff {
		uint64_t until = 0;
		uint32_t failures = 0;
	};
	Map<String, HandshakeBackoff> _handshakeBackoff;
	Rc<remote::ObjectRegistry> _sharedObjects;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLSERVERAPPTHREAD_H_ */
