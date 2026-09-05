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

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace core {
class RenderClientChannel;
} // namespace core

namespace remote {
class Listener;
class ServerConnection;
struct MessageHeader;
} // namespace remote

class Context;
class RemoteRenderClient;
class RemoteFontServer;

// Server (and local/single-process) application thread: owns the Context, the native windows and
// their Directors, and the remote-connection listener. This is the thread the default factory
// creates, so local apps behave exactly as before the client/server split.
class SP_PUBLIC ServerAppThread : public AppThread {
public:
	virtual ~ServerAppThread();

	virtual bool init(NotNull<Context>);

	Context *getContext() const { return _context; }

	remote::ObjectRegistry *getSharedObjects() const { return _sharedObjects; }

	// SHA-256 of the listener's DER SubjectPublicKeyInfo, for handing to a client out-of-band so it
	// can authenticate this server (see remote::Listener::getCertificateFingerprint). Empty while not
	// listening -- the certificate is generated when the listener opens.
	BytesView getListenerFingerprint() const;

	// True while a remote client holds the single connection slot and its connection is live. The
	// end-to-end check polls this to know the client actually got through the handshake, rather than
	// inferring it from a screenshot.
	bool hasRemoteClient() const;

	// This process's own identity: OS, window subsystem, gAPI, features (M3.5). On a server the
	// answer to "who owns the window" is itself, so local scene code asks the same question a
	// remote scene does and gets the same shape of answer.
	virtual const remote::PeerInfo *getServerInfo() const override { return &_localInfo; }

	// The server-side font endpoint (remote::Domain::Font). Persists across client reconnects; used by
	// RemoteRenderClient to reconcile a frame's font dependency ids. Null if xenolith_font is absent.
	RemoteFontServer *getFontServer() const { return _fontServer.get(); }

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

	// `timeoutUs` is this request's reply deadline (relative us; 0 == none) -- see waitForReply.
	bool sendMessageWithReply(remote::Domain, uint8_t message, const Value &,
			Function<void(const remote::MessageHeader &, BytesView payload)> &&, uint64_t timeoutUs);

protected:
	// Block-transfer send facade: route through the connected remote client's connection.
	virtual bool remoteSendCbor(remote::Domain, uint8_t code, const Value &,
			uint32_t *outSerial) override;
	virtual bool remoteSendRaw(remote::Domain, uint8_t code, BytesView,
			uint32_t *outSerial) override;
	virtual bool remoteSendCborReply(uint32_t serial, remote::Domain, uint8_t code,
			const Value &) override;
	virtual bool remoteSendError(remote::Domain, uint8_t code, uint32_t serial) override;
	virtual bool remoteSendCborWithReply(remote::Domain, uint8_t code, const Value &,
			Function<void(const remote::MessageHeader &, BytesView payload)> &&,
			uint64_t timeoutUs) override;

	// Also clears shared objects if not empty
	virtual bool startListening() override;

	// Stop listening and clear all shared objects
	virtual bool stopListening() override;

	virtual void handleThreadInitialized() override;
	virtual void handleThreadDisposed() override;
	virtual void handleThreadUpdated(const UpdateTime &) override;

	virtual void handleMatrialsUpdated(NotNull<core::MaterialSet>) override;

	virtual void performAppUpdate(const UpdateTime &, bool wakeup) override;

	virtual void loadExtensions() override;
	virtual void finalizeExtensions() override;

	// Pump the listener's QUIC events (poll-readable or update tick) and accept new connections.
	void pumpListener();
	void handleRemoteConnection(Rc<remote::ServerConnection> &&);

	// Recompute _localInfo. Cheap; called from the few points where an answer can change (a window
	// appears and names the window subsystem, the listener opens and names the transport, the font
	// extension loads). Deliberately not lazy-on-read: the read is on a const getter used from
	// scene code, and a mutable cache there would buy nothing at this rate.
	void updateServerInfo();

	// The peer answered our ServerInfo request (or refused it). On a compatible peer this is where
	// the session actually begins -- the announce happens here and not before.
	void handleClientInfo(const remote::MessageHeader &, BytesView payload);

	// Swap every shared window's render client: `client` (the connected remote client) takes over on
	// connect; pass nullptr to revert each window to its own local Director on disconnect.
	void takeoverSharedWindows(core::RenderClientChannel *client);

	// Swap a single shared window's render client (resolved by its server-assigned id). Driven by the
	// client's WindowCode::AttachQueue once it is ready to render that window; pass nullptr to revert
	// just that window to its own local Director.
	void takeoverSharedWindow(uint64_t windowId, core::RenderClientChannel *client);

	// Tear down the current remote client: revert all shared windows to their local Directors (killing
	// in-flight remote frames), drop outstanding reply waiters, close the connection and free the slot.
	// Shared by the disconnect, request-timeout and keepalive-timeout paths.
	void resetRemoteClient();

	// Parse a received message and route it by (domain, code). Returns true if the message was
	// consumed, false to defer it for a later poll (xcb-style out-of-order handling). Only the Global
	// ping/pong control messages are handled for now.
	virtual bool dispatchMessage(const remote::MessageHeader &, BytesView payload) override;

	// Drive the (bounded, synchronous) setup handshake for a freshly accepted connection: validate
	// auth, negotiate the dictionary, reply with the window info, and on success attach the client.
	void completePendingHandshake();

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
	Rc<sprt::dispatch::PollHandle> _listenPoll;

	// Readiness on the ACCEPTED connection, which is a different fd from the listener's on every
	// transport whose accept yields a new socket (unix, tcp). QUIC hides this: one UDP socket carries
	// both accepts and connection data, so listener readiness happened to cover the connection too --
	// and a transport where it does not was serviced only by the 1s update tick.
	Rc<sprt::dispatch::PollHandle> _clientPoll;
	Rc<RemoteRenderClient> _remoteClient;

	// Set by a dispatcher that decided the session is over; acted on in pumpListener, which is the
	// only place allowed to drop the connection (a dispatcher runs inside its poll).
	bool _resetClientRequested = false;

	// Server font endpoint (Domain::Font). Created once in loadExtensions, persists across reconnects
	// (only its per-connection dependency registry is reset on disconnect).
	Rc<RemoteFontServer> _fontServer;

	// Remote auth + compression config.
	Bytes _expectedKey; // bearer key a client must present (empty ⇒ reject all)
	Bytes _dictionary; // server's LZ4 dictionary (priority over a client suggestion)

	// A just-accepted connection awaiting its setup handshake (driven from pumpListener).
	Rc<remote::ServerConnection> _pendingConnection;

	// Connections refused in the accept callback (the single-client slot was taken, or the handshake
	// rate limit is in force). pumpListener answers each with GlobalError::Busy and drops it, so the
	// peer is told rather than left to time its own handshake out.
	Vector<Rc<remote::ServerConnection>> _refusedConnections;

	// Handshake rate limit (monotonic us). Failures back off exponentially; a success resets both.
	// The window is global rather than per-address because the QUIC listener multiplexes every peer
	// over one UDP socket and exposes no per-connection address -- that lands with PeerIdentity in
	// the transport abstraction.
	uint64_t _handshakeBackoffUntil = 0;
	uint32_t _handshakeFailures = 0;
	Rc<remote::ObjectRegistry> _sharedObjects;

	// Keepalive (monotonic us): ping the client every kKeepalivePingIntervalUs and terminate the
	// connection if no pong arrives within kKeepalivePongTimeoutUs. Reset on a fresh connection.
	uint64_t _lastPingTime = 0;
	uint64_t _lastPongTime = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLSERVERAPPTHREAD_H_ */
