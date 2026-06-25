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

// Server (and local/single-process) application thread: owns the Context, the native windows and
// their Directors, and the remote-connection listener. This is the thread the default factory
// creates, so local apps behave exactly as before the client/server split.
class SP_PUBLIC ServerAppThread : public AppThread {
public:
	virtual ~ServerAppThread();

	virtual bool init(NotNull<Context>);

	Context *getContext() const { return _context; }

	remote::ObjectRegistry *getSharedObjects() const { return _sharedObjects; }

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

	// Pump the listener's QUIC events (poll-readable or update tick) and accept new connections.
	void pumpListener();
	void handleRemoteConnection(Rc<remote::ServerConnection> &&);

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

	Set<AppWindow *> _windows;
	HashMap<String, Rc<Director>> _preservedDirectors;

	// Server-side listener state (dormant unless a scene calls startListening).
	remote::Address _listenAddress;
	Rc<remote::Listener> _listener;
	Rc<sprt::dispatch::PollHandle> _listenPoll;
	Rc<RemoteRenderClient> _remoteClient;

	// Remote auth + compression config.
	Bytes _expectedKey; // bearer key a client must present (empty ⇒ reject all)
	Bytes _dictionary; // server's LZ4 dictionary (priority over a client suggestion)

	// A just-accepted connection awaiting its setup handshake (driven from pumpListener).
	Rc<remote::ServerConnection> _pendingConnection;
	Rc<remote::ObjectRegistry> _sharedObjects;

	// Keepalive (monotonic us): ping the client every kKeepalivePingIntervalUs and terminate the
	// connection if no pong arrives within kKeepalivePongTimeoutUs. Reset on a fresh connection.
	uint64_t _lastPingTime = 0;
	uint64_t _lastPongTime = 0;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLSERVERAPPTHREAD_H_ */
