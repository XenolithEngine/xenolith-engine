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
#include "XLRemoteProtocol.h"
#include "SPPlatform.h"

#include <openssl/ssl.h>
#include <openssl/quic.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include <sys/socket.h> // AF_INET, SOCK_DGRAM (via the sprt socket layer)

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// ALPN id (1-byte length prefix + "xlremote") -- must match the listener's.
static const unsigned char kAlpnConn[] = {8, 'x', 'l', 'r', 'e', 'm', 'o', 't', 'e'};

static void logConnErrors(StringView where) {
	unsigned long e;
	while ((e = ERR_get_error()) != 0) {
		char buf[256];
		ERR_error_string_n(e, buf, sizeof(buf));
		log::source().error("remote::Connector", where, ": ", buf);
	}
}

static void portToStrConn(uint16_t port, char out[6]) {
	char tmp[6];
	int n = 0;
	do {
		tmp[n++] = char('0' + port % 10);
		port = uint16_t(port / 10);
	} while (port);
	for (int i = 0; i < n; ++i) { out[i] = tmp[n - 1 - i]; }
	out[n] = '\0';
}

// --- Connector ---

Rc<ClientConnection> ClientConnection::connect(const Address &addr) {
	if (addr.isUnix()) {
		log::source().error("remote::Connector",
				"unix-domain QUIC connect is not supported yet (needs a custom datagram BIO): ",
				addr.description());
		return nullptr;
	}

	SSL_CTX *ctx = nullptr;
	SSL *ssl = nullptr;
	int fd = -1;
	BIO_ADDRINFO *res = nullptr;
	char service[6];

	ctx = SSL_CTX_new(OSSL_QUIC_client_method());
	if (!ctx) {
		logConnErrors("SSL_CTX_new");
		goto fail;
	}
	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr); // server uses a self-signed cert

	portToStrConn(addr.port, service);
	{
		const char *host = addr.host.empty() ? "127.0.0.1" : addr.host.data();
		if (!BIO_lookup_ex(host, service, BIO_LOOKUP_CLIENT, AF_INET, SOCK_DGRAM, 0, &res)
				|| !res) {
			logConnErrors("BIO_lookup_ex");
			goto fail;
		}
	}

	fd = BIO_socket(BIO_ADDRINFO_family(res), SOCK_DGRAM, 0, 0);
	if (fd < 0) {
		logConnErrors("BIO_socket");
		goto fail;
	}

	ssl = SSL_new(ctx);
	if (!ssl) {
		logConnErrors("SSL_new");
		goto fail;
	}
	if (!SSL_set_fd(ssl, fd)) {
		logConnErrors("SSL_set_fd");
		goto fail;
	}
	if (!SSL_set1_initial_peer_addr(ssl, BIO_ADDRINFO_address(res))) {
		logConnErrors("SSL_set1_initial_peer_addr");
		goto fail;
	}
	if (SSL_set_alpn_protos(ssl, kAlpnConn, sizeof(kAlpnConn)) != 0) { // 0 == success here
		logConnErrors("SSL_set_alpn_protos");
		goto fail;
	}
	SSL_set_tlsext_host_name(ssl, "localhost"); // SNI; matches the listener cert CN
	SSL_set_blocking_mode(ssl, 0); // non-blocking: bounded handshake with a wall-clock deadline

	// Drive the QUIC handshake with a 5s deadline so a missing/dead server fails fast instead of
	// retransmitting forever (UDP has no connection-refused).
	{
		uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + 5'000'000;
		for (;;) {
			int ret = SSL_connect(ssl);
			if (ret == 1) {
				break;
			}
			int err = SSL_get_error(ssl, ret);
			if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
				logConnErrors("SSL_connect");
				goto fail;
			}
			if (sp::platform::clock(ClockType::Monotonic) >= deadline) {
				log::source().error("remote::Connector", "connect timed out: ", addr.description());
				goto fail;
			}
			SSL_handle_events(ssl); // pump retransmit timers
			sp::platform::sleep(2'000); // 2ms, then retry
		}
	}

	// Handshake done: back to blocking mode so stream I/O and SSL_shutdown (graceful close) flush
	// synchronously -- otherwise the CONNECTION_CLOSE may never be sent before the client exits.
	SSL_set_blocking_mode(ssl, 1);

	BIO_ADDRINFO_free(res);
	res = nullptr;

	{
		auto conn = Rc<ClientConnection>::create((void *)ctx, (void *)ssl, fd);
		if (conn) {
			log::source().info("remote::Connector", "connected (QUIC) to ", addr.description());
			SSL_set_blocking_mode(ssl, 0);
			return conn;
		}
	}
	// ClientConnection::create failed -> clean up below

fail:
	if (ssl) {
		SSL_free(ssl);
	}
	if (fd >= 0) {
		BIO_closesocket(fd);
	}
	if (res) {
		BIO_ADDRINFO_free(res);
	}
	if (ctx) {
		SSL_CTX_free(ctx);
	}
	return nullptr;
}

// --- ClientConnection ---

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ClientConnection::~ClientConnection() {
	if (_ssl) {
		if (!_shutdown) {
			SSL_shutdown((SSL *)_ssl);
		}
		SSL_free((SSL *)_ssl);
		_ssl = nullptr;
	}
	if (_ctx) {
		SSL_CTX_free((SSL_CTX *)_ctx);
		_ctx = nullptr;
	}
	if (_fd >= 0) {
		BIO_closesocket(_fd); // SSL_set_fd uses BIO_NOCLOSE, so we close the socket ourselves
		_fd = -1;
	}
}

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool ClientConnection::init(void *ctx, void *ssl, int fd) {
	_ctx = ctx;
	_ssl = ssl;
	_fd = fd;
	return _ssl != nullptr;
}

GlobalError ClientConnection::handshake(BytesView key, BytesView suggestedDict) {
	if (!_ssl) {
		return GlobalError::NetworkBackend;
	}
	uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + 5'000'000; // 5s
	return clientHandshake(_ssl, key, suggestedDict, deadline, [&](const ServerHello &sh) {
		if (sh.status == toInt(GlobalError::Ok)) {
			_clientSerial = 1; // restart serial session
			if (sh.dictSource == toInt(DictSource::Server)) {
				_dict = sh.dict.bytes<Interface>();
			}
		}
	});
}

GlobalError ClientConnection::ping() { return sendPing(_ssl, Role::Client, _clientSerial++); }

GlobalError ClientConnection::pong(uint32_t serial) { return sendPong(_ssl, Role::Client, serial); }

GlobalError ClientConnection::sendCborMessage(Domain d, uint8_t message, const Value &val,
		uint32_t *outSerial) {
	Bytes bytes = data::write<Interface>(val, data::EncodeFormat::Cbor);
	return sendMessage(d, message, bytes, outSerial);
}

GlobalError ClientConnection::sendMessage(Domain d, uint8_t message, BytesView payload,
		uint32_t *outSerial) {
	auto serial = _clientSerial++;
	if (sendFrame(_ssl, sp::platform::clock(ClockType::Monotonic) + 500'000, _dict,
				MessageType::Client, d, message, serial, payload)) {
		if (outSerial) {
			*outSerial = serial;
		}
		return GlobalError::Ok;
	}
	return GlobalError::NetworkBackend;
}

GlobalError ClientConnection::sendCborReply(uint32_t serial, Domain d, uint8_t message,
		const Value &val) {
	Bytes bytes = data::write<Interface>(val, data::EncodeFormat::Cbor);
	return sendReply(serial, d, message, bytes);
}

GlobalError ClientConnection::sendReply(uint32_t serial, Domain d, uint8_t message,
		BytesView payload) {
	if (sendFrame(_ssl, sp::platform::clock(ClockType::Monotonic) + 500'000, _dict,
				MessageType::ClientReply, d, message, serial, payload)) {
		return GlobalError::Ok;
	}
	return GlobalError::NetworkBackend;
}

GlobalError ClientConnection::sendError(Domain d, GlobalError code, uint32_t failedMessageSerial) {
	if (sendFrame(_ssl, sp::platform::clock(ClockType::Monotonic) + 500'000, _dict,
				MessageType::ClientError, d, toInt(code), failedMessageSerial, BytesView())) {
		return GlobalError::Ok;
	}
	return GlobalError::NetworkBackend;
}

void ClientConnection::poll(const Callback<bool(const MessageHeader &, BytesView)> &dispatchCb) {
	if (!_ssl) {
		return;
	}
	auto ssl = (SSL *)_ssl;
	// Pump incoming datagrams, then drain the QUIC stream (non-blocking: read until WANT_READ) into the
	// reassembler. The reader holds any partial frame across pumps, so a message that spans datagrams
	// is handled correctly; complete frames are then dispatched (deferred ones stay queued).
	SSL_handle_events(ssl);
	uint8_t buf[4'096];
	BytesView dict(_dict.data(), _dict.size());
	for (;;) {
		size_t n = 0;
		if (SSL_read_ex(ssl, buf, sizeof(buf), &n) != 1 || n == 0) {
			break; // WANT_READ (drained) or closed/error
		}

		BytesViewNetwork nw(buf, n);

		// Fast path: only when the reassembler is fully idle -- no buffered partial AND nothing queued.
		// Then this chunk begins on a frame boundary and can be parsed (and dispatched inline) straight
		// out of it, no copy into _buffer. Requiring an empty buffer avoids desync: with a buffered
		// partial the fresh bytes are that frame's continuation, and parsing them as a new header would
		// scramble the stream. Requiring an empty pending queue preserves order: queued frames dispatch
		// at end-of-poll, so fast-path-dispatching newer frames ahead of them would reorder the stream
		// (e.g. a FrameInput after its FrameCommit). When either holds, fall through to append().
		if (!_reader.hasPartialMessage() && !_reader.hasPending()) {
			while (readMessagePayload(nw, _dict, [&](const MessageHeader &h, BytesView data) {
				if (!dispatchCb(h, data)) {
					_reader.addMessage(h, data);
				}
			})) { }
		}

		// Buffer the remainder: a trailing partial frame, or the whole chunk when a partial was already
		// buffered. The reassembler completes it on a later read.
		if (!nw.empty()) {
			if (!_reader.append(BytesView(nw), dict)) {
				log::source().error("remote::Connector",
						"framing violation; dropping connection buffer");
				_reader.clear();
				break;
			}
		}
	}

	// Retry any deferred messages (also covers a tick that read no new data).
	_reader.dispatch(dispatchCb);
}

/*bool ClientConnection::sendData(BytesView payload) {
	if (!_ssl) {
		return false;
	}
	return writeDataFrame(_ssl, BytesView(_dict.data(), _dict.size()), payload);
}

bool ClientConnection::recvData(Bytes &out, uint64_t timeoutUs) {
	if (!_ssl) {
		return false;
	}
	uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + timeoutUs;
	return readDataFrame(_ssl, BytesView(_dict.data(), _dict.size()), out, deadline);
}*/

void ClientConnection::close() {
	if (!_ssl || _shutdown) {
		return;
	}
	auto ssl = (SSL *)_ssl;
	// Drive the QUIC shutdown to completion so the CONNECTION_CLOSE is actually transmitted before
	// the socket is torn down -- a single SSL_shutdown only queues it. Bounded (1s) so we never hang
	// on an unresponsive peer.
	uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + 1'000'000;
	for (;;) {
		int ret = SSL_shutdown(ssl);
		if (ret != 0) {
			break; // 1 == fully shut down, <0 == error: either way we're done
		}
		SSL_handle_events(ssl); // flush the close datagram / pump the drain
		if (sp::platform::clock(ClockType::Monotonic) >= deadline) {
			break;
		}
		sp::platform::sleep(2'000); // 2ms
	}
	_shutdown = true;
}

} // namespace stappler::xenolith::remote
