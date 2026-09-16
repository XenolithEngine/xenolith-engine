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

#ifndef XENOLITH_APPLICATION_XLREMOTESESSION_H_
#define XENOLITH_APPLICATION_XLREMOTESESSION_H_

#include "XLRemotePeer.h"
#include "XLRemotePeerInfo.h"
#include "XLRemoteReplyTable.h"

#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith {

namespace remote {
class ServerConnection;
} // namespace remote

class ServerAppThread;
class RemoteRenderClient;
class RemoteFontServer;
class BlockTransferManager;

// One connected client on a server: its connection, the render client its windows are handed to,
// and everything counted per connection -- reply serials, block transfers, font dependencies,
// keepalive. App thread only.
class SP_PUBLIC RemoteSession : public Ref, public RemotePeer {
public:
	virtual ~RemoteSession();

	// `id` is the server's name for the session, never reused while the server runs.
	bool init(NotNull<ServerAppThread>, uint64_t id, Rc<remote::ServerConnection> &&);

	uint64_t getId() const { return _id; }
	ServerAppThread *getHost() const { return _host; }

	// Null once the session is closed.
	remote::ServerConnection *getConnection() const { return _connection; }
	RemoteRenderClient *getRenderClient() const { return _renderClient; }
	BlockTransferManager *getBlockTransfer() const { return _blockTransfer; }
	RemoteFontServer *getFontServer() const { return _fontServer; }

	// The client's own description; empty until it answers ServerInfo (or when it predates it).
	const remote::PeerInfo &getPeerInfo() const { return _peerInfo; }
	void setPeerInfo(remote::PeerInfo &&info) { _peerInfo = sp::move(info); }

	// What the transport knows about the process on the other end (uid/pid on local transports).
	int64_t getPeerPid() const;

	// True once the connection has begun terminating.
	bool isClosed();

	// The connection's own readiness, cancelled when the session closes.
	void setWake(Rc<sprt::dispatch::Handle> &&);

	// Bind a font endpoint to this session; close() hands it back.
	void setFontServer(Rc<RemoteFontServer> &&);

	// Replies and errors to this session's requests.
	bool dispatchReply(const remote::MessageHeader &, BytesView payload);

	// Fail waiters past their deadline; true when any expired (the session is then dropped).
	bool failExpiredRequests(uint64_t now);

	// A dispatcher ended the session; the host acts on it outside the connection's poll.
	void requestReset() { _resetRequested = true; }
	bool isResetRequested() const { return _resetRequested; }

	void handlePong(uint64_t now) { _lastPongTime = now; }

	// Ping when due. False once the client has not answered a ping for `pongTimeoutUs`.
	bool updateKeepalive(uint64_t now, uint64_t pingIntervalUs, uint64_t pongTimeoutUs);

	bool sendMessageWithReply(remote::Domain, uint8_t code, const Value &, ReplyCallback &&,
			uint64_t timeoutUs);

	// Drop waiters and transfers, close the connection. Returns the font endpoint, unbound, for
	// the host to reuse. Idempotent.
	Rc<RemoteFontServer> close();

	virtual AppThread *getPeerThread() const override;

	virtual bool remoteSendCbor(remote::Domain, uint8_t code, const Value &,
			uint32_t *outSerial = nullptr) override;
	virtual bool remoteSendRaw(remote::Domain, uint8_t code, BytesView,
			uint32_t *outSerial = nullptr) override;
	virtual bool remoteSendCborReply(uint32_t serial, remote::Domain, uint8_t code,
			const Value &) override;
	virtual bool remoteSendError(remote::Domain, uint8_t code, uint32_t serial) override;
	virtual bool remoteSendCborWithReply(remote::Domain, uint8_t code, const Value &,
			ReplyCallback &&, uint64_t timeoutUs) override;

protected:
	ServerAppThread *_host = nullptr;
	uint64_t _id = 0;
	Rc<remote::ServerConnection> _connection;
	Rc<RemoteRenderClient> _renderClient;
	Rc<BlockTransferManager> _blockTransfer;
	Rc<RemoteFontServer> _fontServer;
	Rc<sprt::dispatch::Handle> _wake;
	remote::ReplyTable _replies;
	remote::PeerInfo _peerInfo;

	// Monotonic us, restarted when the session is created.
	uint64_t _lastPingTime = 0;
	uint64_t _lastPongTime = 0;

	bool _resetRequested = false;
};

} // namespace stappler::xenolith

#endif /* XENOLITH_APPLICATION_XLREMOTESESSION_H_ */
