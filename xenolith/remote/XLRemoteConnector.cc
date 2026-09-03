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

#include "XLRemoteConnector.h"
#include "SPPlatform.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ClientConnection::~ClientConnection() { }

__SPRT_POP_ALLOW_CXXABI_ALLOC

Rc<ClientConnection> ClientConnection::connect(const Address &addr,
		BytesView expectedFingerprint) {
	initializeTransports();

	TransportClientConfig cfg;
	cfg.expectedFingerprint = expectedFingerprint.bytes<Interface>();

	auto transport = TransportRegistry::connect(addr, cfg);
	if (!transport) {
		return nullptr;
	}
	return Rc<ClientConnection>::create(sp::move(transport));
}

GlobalError ClientConnection::handshake(BytesView key, BytesView suggestedDict) {
	if (!_transport) {
		return GlobalError::BadProtocol;
	}
	uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + 500'000; // 500ms
	return clientHandshake(*_transport, key, suggestedDict, deadline, [&](const ServerHello &sh) {
		if (sh.dictSource == toInt(DictSource::Server)) {
			_dict = sh.dict.bytes<Interface>();
		} else if (sh.dictSource == toInt(DictSource::Client)) {
			_dict = suggestedDict.bytes<Interface>();
		}
		_serial = 1; // begin a new serial session
	});
}

} // namespace stappler::xenolith::remote
