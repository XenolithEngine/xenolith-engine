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

#ifndef XENOLITH_APPLICATION_XLCLIENTAPPTHREAD_H_
#define XENOLITH_APPLICATION_XLCLIENTAPPTHREAD_H_

#include "XLAppThread.h"
#include "XLRemoteConnector.h"
#include "XLCoreRenderSession.h"
#include "XLRemoteObject.h"
#include "XLRemoteWindow.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

class ClientContext;

// Client-side application thread: runs the scene graph and produces draw data for a remote server.
// It owns a standalone ClientContext (not a Context) and has no native windows. Platform services
// (clipboard / screen-info / URL) and the GL loop are remote-only; until the wire protocol exists
// they are stubbed.
class SP_PUBLIC ClientAppThread : public AppThread {
public:
	struct AppQueueInfo {
		uint64_t id;
		String name;
	};

	virtual ~ClientAppThread();

	virtual bool init(NotNull<ClientContext>);

	virtual void run() override;

	// Single-shot connect flow: dial the server, (locate each other), gracefully close, then return
	// false so the thread loop ends and the client process exits.
	virtual bool worker() override;

	ClientContext *getClientContext() const { return _clientContext; }

	remote::ClientConnection *getConnection() const { return _connection; }

	remote::ObjectFactory *getSharedObjects() const { return _sharedObjects; }

	// Flush the font controller's pending glyph requests now (used by the remote frame-production path to
	// send GlyphRequests before the frame's FrameInput, so the server registers the gating dependency
	// before reconciling the frame against it). No-op if the font module / controller is absent.
	void flushPendingFontGlyphs();

	// AppThread platform-services interface (TODO: route to the remote server).
	// The remote transport carries no clipboard yet, and every call below is a stub. Said out loud
	// so that a caller refuses visibly instead of writing into a void.
	virtual bool hasClipboard() const override { return false; }

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

	// The SERVER's identity, not this process's: the scene runs here, the window and the GPU are
	// over there (M3.5). Null until the ServerInfo exchange completes, and against a version-1
	// server that has no such message it stays null for the whole session -- so a caller must have
	// a sensible answer for "not known".
	virtual const remote::PeerInfo *getServerInfo() const override {
		return _hasServerInfo ? &_serverInfo : nullptr;
	}

	// `timeoutUs` is this request's reply deadline (relative us; 0 == none) -- see waitForReply.
	bool sendMessageWithReply(remote::Domain, uint8_t message, const Value &,
			Function<void(const remote::MessageHeader &, BytesView payload)> &&, uint64_t timeoutUs);

protected:
	// Block-transfer send facade: route through the server connection.
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

	virtual void handleThreadInitialized() override;
	virtual void handleThreadDisposed() override;
	virtual void handleThreadUpdated(const UpdateTime &) override;

	// return true to use this window for output
	virtual bool handleWindowConnected(NotNull<RemoteWindow>);
	virtual void handleWindowDisconnected(NotNull<RemoteWindow>);

	virtual void loadExtensions() override;

	virtual void performAppUpdate(const UpdateTime &time, bool wakeup) override;

	void pumpConnection();

	// Parse a received message and route it by (domain, code). Returns true if the message was
	// consumed, false to defer it for a later poll (xcb-style out-of-order handling). Only the Global
	// ping/pong control messages are handled for now.
	virtual bool dispatchMessage(const remote::MessageHeader &, BytesView payload) override;

	virtual Rc<Director> makeDirector(NotNull<RemoteWindow>, const core::FrameConstraints &);
	virtual Rc<Scene> makeScene(NotNull<RemoteWindow>, const core::FrameConstraints &);

	void handleAnnounce(const Value &);

	// What this client tells the server about itself. Answers the ServerInfo request.
	remote::PeerInfo makeClientInfo() const;

	// Validate the server's PeerInfo and answer with ours -- or refuse with IncompatiblePeer and
	// end the session. Runs before anything has been announced.
	void handleServerInfo(const remote::MessageHeader &, BytesView payload);

	ClientContext *_clientContext = nullptr;

	Rc<sprt::dispatch::PollHandle> _listenPoll;
	Rc<remote::ClientConnection> _connection;

	Rc<remote::ObjectFactory> _sharedObjects;

	// The server's identity (GlobalCode::ServerInfo). Kept for the whole session; a reconnect
	// builds a new thread.
	remote::PeerInfo _serverInfo;
	bool _hasServerInfo = false;

	// Set by a dispatcher that has decided the session is over; acted on in pumpConnection, which
	// is the only place allowed to drop the connection (a dispatcher runs inside its poll).
	bool _disconnectRequested = false;

	// Keepalive (monotonic us): the server pings us periodically; if no ping arrives within the
	// timeout the server is presumed gone and the client disconnects. Reset on connect and each ping.
	uint64_t _lastPingTime = 0;

	Map<uint64_t, Rc<RemoteWindow>> _windows;
	Map<uint64_t, AppQueueInfo> _queues;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLCLIENTAPPTHREAD_H_ */
