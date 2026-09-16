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

#include "XLRemoteListener.h"
#include "SPPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ServerConnection::~ServerConnection() { }

Listener::~Listener() { close(); }

__SPRT_POP_ALLOW_CXXABI_ALLOC

GlobalError ServerConnection::handshake(BytesView expectedKey, BytesView serverDict) {
	if (!_transport) {
		return GlobalError::BadProtocol;
	}
	uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + 500'000; // 500ms
	// A transport that already established the peer's identity (unix-domain credentials) makes the
	// bearer key redundant -- see serverHandshake's `requireBearerKey`.
	auto requireKey = !_transport->hasCaps(TransportCaps::PeerAuthenticated);
	auto ret = serverHandshake(*_transport, expectedKey, serverDict, _dict, deadline, requireKey);
	if (ret == GlobalError::Ok) {
		_serial = 1; // begin a new serial session
	}
	return ret;
}

GlobalError ServerConnection::reject(GlobalError status) {
	if (!_transport) {
		return GlobalError::BadProtocol;
	}
	uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + 500'000; // 500ms
	return serverHandshakeReject(*_transport, status, deadline);
}

// --- Listener ---

bool Listener::open(const Address &addr) {
	initializeTransports();
	_listener = TransportRegistry::listen(addr);
	return _listener != nullptr;
}

void Listener::close() {
	if (_listener) {
		_listener->close();
		_listener = nullptr;
	}
}

bool Listener::isOpen() const { return _listener && _listener->isOpen(); }

sprt::dispatch::NativeHandle Listener::getPollHandle() const {
	return _listener ? _listener->getPollHandle() : sprt::dispatch::NativeHandle(-1);
}

uint64_t Listener::getEventTimeout() const {
	return _listener ? _listener->getEventTimeout() : maxOf<uint64_t>();
}

BytesView Listener::getCertificateFingerprint() const {
	return _listener ? _listener->getIdentity() : BytesView();
}

String Listener::encodeFingerprint(BytesView fp) { return base16::encode<Interface>(fp); }

void Listener::handleEvents(const AcceptCallback &onAccept) {
	if (!_listener) {
		return;
	}
	_listener->handleEvents([&](Rc<TransportConnection> &&conn) {
		auto sc = Rc<ServerConnection>::create(sp::move(conn));
		if (sc) {
			onAccept(sp::move(sc));
		}
	});
}

} // namespace stappler::xenolith::remote
