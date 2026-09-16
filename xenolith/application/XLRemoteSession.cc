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

#include "XLRemoteSession.h"
#include "XLServerAppThread.h"
#include "XLRemoteListener.h"
#include "XLRemoteRenderClient.h"
#include "XLRemoteBlockTransfer.h"
#include "XLRemoteFontServer.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

RemoteSession::~RemoteSession() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool RemoteSession::init(NotNull<ServerAppThread> host, uint64_t id,
		Rc<remote::ServerConnection> &&conn) {
	if (!conn) {
		return false;
	}
	_host = host;
	_id = id;
	_connection = sp::move(conn);
	_renderClient = Rc<RemoteRenderClient>::create(host, this);
	_blockTransfer = Rc<BlockTransferManager>::create(this);
	_lastPingTime = _lastPongTime = sp::platform::clock(ClockType::Monotonic);
	return _renderClient && _blockTransfer;
}

int64_t RemoteSession::getPeerPid() const {
	if (_connection) {
		if (auto transport = _connection->getTransport()) {
			return transport->getPeerIdentity().pid;
		}
	}
	return -1;
}

bool RemoteSession::isClosed() { return !_connection || _connection->isClosed(); }

void RemoteSession::setWake(Rc<sprt::dispatch::Handle> &&wake) {
	if (_wake) {
		_wake->cancel();
	}
	_wake = sp::move(wake);
}

void RemoteSession::setFontServer(Rc<RemoteFontServer> &&server) {
	if (_fontServer) {
		_fontServer->reset();
	}
	_fontServer = sp::move(server);
	if (_fontServer) {
		_fontServer->setPeer(this);
	}
}

bool RemoteSession::dispatchReply(const remote::MessageHeader &h, BytesView payload) {
	return _replies.dispatch(h, payload);
}

bool RemoteSession::failExpiredRequests(uint64_t now) {
	return _replies.failExpired(now, remote::MessageType::ClientError);
}

bool RemoteSession::updateKeepalive(uint64_t now, uint64_t pingIntervalUs, uint64_t pongTimeoutUs) {
	if (now - _lastPongTime >= pongTimeoutUs) {
		return false;
	}
	if (now - _lastPingTime >= pingIntervalUs) {
		if (_connection) {
			_connection->ping();
		}
		_lastPingTime = now;
	}
	return true;
}

bool RemoteSession::sendMessageWithReply(remote::Domain d, uint8_t code, const Value &val,
		ReplyCallback &&cb, uint64_t timeoutUs, bool fatal) {
	if (isClosed()) {
		return false;
	}

	uint32_t serial = 0;
	if (_connection->sendCborMessage(d, code, val, &serial) != remote::GlobalError::Ok) {
		return false;
	}
	auto deadline = timeoutUs ? sp::platform::clock(ClockType::Monotonic) + timeoutUs : 0;
	_replies.wait(serial, sp::move(cb), deadline, fatal);
	return true;
}

Rc<RemoteFontServer> RemoteSession::close() {
	_resetRequested = false;
	_replies.clear();
	if (_blockTransfer) {
		_blockTransfer->invalidate();
	}
	if (_wake) {
		_wake->cancel();
		_wake = nullptr;
	}
	auto fontServer = sp::move(_fontServer);
	_fontServer = nullptr;
	if (fontServer) {
		fontServer->reset();
	}
	if (_renderClient) {
		_renderClient->detach();
	}
	if (_connection) {
		_connection->close();
		_connection = nullptr;
	}
	return fontServer;
}

AppThread *RemoteSession::getPeerThread() const { return _host; }

bool RemoteSession::remoteSendCbor(remote::Domain d, uint8_t code, const Value &v,
		uint32_t *outSerial) {
	if (isClosed()) {
		return false;
	}
	return _connection->sendCborMessage(d, code, v, outSerial) == remote::GlobalError::Ok;
}

bool RemoteSession::remoteSendRaw(remote::Domain d, uint8_t code, BytesView b,
		uint32_t *outSerial) {
	if (isClosed()) {
		return false;
	}
	return _connection->sendMessage(d, code, b, outSerial) == remote::GlobalError::Ok;
}

bool RemoteSession::remoteSendCborReply(uint32_t serial, remote::Domain d, uint8_t code,
		const Value &v) {
	if (isClosed()) {
		return false;
	}
	return _connection->sendCborReply(serial, d, code, v) == remote::GlobalError::Ok;
}

bool RemoteSession::remoteSendError(remote::Domain d, uint8_t code, uint32_t serial) {
	if (isClosed()) {
		return false;
	}
	return _connection->sendError(d, code, serial) == remote::GlobalError::Ok;
}

bool RemoteSession::remoteSendCborWithReply(remote::Domain d, uint8_t code, const Value &v,
		ReplyCallback &&cb, uint64_t timeoutUs) {
	return sendMessageWithReply(d, code, v, sp::move(cb), timeoutUs);
}

} // namespace stappler::xenolith
