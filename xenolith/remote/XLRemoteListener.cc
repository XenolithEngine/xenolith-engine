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
#include "XLRemoteProtocol.h"
#include "SPPlatform.h"

#include <openssl/ssl.h>
#include <openssl/quic.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

#include <arpa/inet.h> // AF_INET, SOCK_DGRAM (via the sprt socket layer)
#include <sys/time.h> // struct timeval (SSL_get_event_timeout)

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// ALPN id (1-byte length prefix + "xlremote"); QUIC mandates ALPN on both ends.
static const unsigned char kAlpn[] = {8, 'x', 'l', 'r', 'e', 'm', 'o', 't', 'e'};

static void logSslErrors(StringView where) {
	unsigned long e;
	while ((e = ERR_get_error()) != 0) {
		char buf[256];
		ERR_error_string_n(e, buf, sizeof(buf));
		log::source().error("remote::Listener", where, ": ", buf);
	}
}

static void portToStr(uint16_t port, char out[6]) {
	char tmp[6];
	int n = 0;
	do {
		tmp[n++] = char('0' + port % 10);
		port = uint16_t(port / 10);
	} while (port);
	for (int i = 0; i < n; ++i) { out[i] = tmp[n - 1 - i]; }
	out[n] = '\0';
}

// Ephemeral in-memory self-signed P-256 certificate for the QUIC server.
static bool makeSelfSignedCert(EVP_PKEY **pkeyOut, X509 **certOut) {
	EVP_PKEY *pkey = EVP_EC_gen("P-256");
	if (!pkey) {
		return false;
	}
	X509 *x = X509_new();
	if (!x) {
		EVP_PKEY_free(pkey);
		return false;
	}
	X509_set_version(x, 2);
	ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
	X509_gmtime_adj(X509_getm_notBefore(x), 0);
	X509_gmtime_adj(X509_getm_notAfter(x), long(60 * 60 * 24 * 365));
	X509_set_pubkey(x, pkey);
	X509_NAME *name = X509_get_subject_name(x);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
			reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0);
	X509_set_issuer_name(x, name);
	if (!X509_sign(x, pkey, EVP_sha256())) {
		X509_free(x);
		EVP_PKEY_free(pkey);
		return false;
	}
	*pkeyOut = pkey;
	*certOut = x;
	return true;
}

static int alpnSelectCb(SSL *, const unsigned char **out, unsigned char *outlen,
		const unsigned char *in, unsigned int inlen, void *) {
	if (SSL_select_next_proto(const_cast<unsigned char **>(out), outlen, kAlpn, sizeof(kAlpn), in,
				inlen)
			== OPENSSL_NPN_NEGOTIATED) {
		return SSL_TLSEXT_ERR_OK;
	}
	return SSL_TLSEXT_ERR_ALERT_FATAL;
}

// Bind a UDP socket for the QUIC listener (network address). Returns -1 on failure.
static int makeBoundUdpSocket(const Address &addr) {
	char service[6];
	portToStr(addr.port, service);
	const char *host = addr.host.empty() ? "0.0.0.0" : addr.host.data();

	BIO_ADDRINFO *res = nullptr;
	if (!BIO_lookup_ex(host, service, BIO_LOOKUP_SERVER, AF_INET, SOCK_DGRAM, 0, &res) || !res) {
		logSslErrors("BIO_lookup_ex");
		return -1;
	}
	int fd = BIO_socket(BIO_ADDRINFO_family(res), SOCK_DGRAM, 0, 0);
	if (fd < 0) {
		logSslErrors("BIO_socket");
		BIO_ADDRINFO_free(res);
		return -1;
	}
	if (!BIO_bind(fd, BIO_ADDRINFO_address(res), BIO_SOCK_REUSEADDR)) {
		logSslErrors("BIO_bind");
		BIO_closesocket(fd);
		BIO_ADDRINFO_free(res);
		return -1;
	}
	BIO_ADDRINFO_free(res);
	return fd;
}

// --- ServerConnection ---

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

ServerConnection::~ServerConnection() {
	if (_ssl) {
		SSL_free((SSL *)_ssl);
		_ssl = nullptr;
	}
}

Listener::~Listener() { close(); }

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool ServerConnection::init(void *ssl) {
	_ssl = ssl;
	if (_ssl) {
		// Non-blocking: the host looper drives the synchronous handshake (deadline-bounded) and then
		// the SSL_read_ex drain in poll(); a blocking read would stall the app thread.
		SSL_set_blocking_mode((SSL *)_ssl, 0);
	}
	return _ssl != nullptr;
}

bool ServerConnection::isClosed() {
	if (!_ssl) {
		return true;
	}
	auto ssl = (SSL *)_ssl;
	SSL_handle_events(ssl);

	SSL_CONN_CLOSE_INFO info;
	return SSL_get_conn_close_info(ssl, &info, sizeof(info)) != 0;
}

ErrorCode ServerConnection::handshake(BytesView expectedKey, BytesView serverDict) {
	if (!_ssl) {
		return ErrorCode::BadProtocol;
	}
	uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + 500'000; // 500ms
	auto ret = serverHandshake(_ssl, expectedKey, serverDict, _dict, deadline);
	if (ret == ErrorCode::Ok) {
		// begin new serial session
		_serial = 1;
	}
	return ret;
}

ErrorCode ServerConnection::ping() { return sendPing(_ssl, Role::Server, _serial++); }

ErrorCode ServerConnection::pong(uint32_t serial) { return sendPong(_ssl, Role::Server, serial); }

ErrorCode ServerConnection::sendCborMessage(Domain d, uint8_t message, const Value &val) {
	Bytes bytes = data::write<Interface>(val, data::EncodeFormat::Cbor);
	return sendMessage(d, message, bytes);
}

ErrorCode ServerConnection::sendMessage(Domain d, uint8_t message, BytesView payload) {
	if (sendFrame(_ssl, sp::platform::clock(ClockType::Monotonic) + 500'000, _dict,
				MessageType::Server, d, message, _serial++, payload)) {
		return ErrorCode::Ok;
	}
	return ErrorCode::NetworkBackend;
};

void ServerConnection::poll(const Callback<bool(const MessageHeader &, BytesView)> &dispatchCb) {
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

		// Read full messages immediately
		while (readMessagePayload(nw, _dict, [&](const MessageHeader &h, BytesView data) {
			if (!dispatchCb(h, data)) {
				_reader.addMessage(h, data);
			}
		})) { }

		if (!_reader.append(BytesView(nw), dict)) {
			log::source().error("remote::Connector",
					"framing violation; dropping connection buffer");
			_reader.clear();
			break;
		}
	}
	// Retry any deferred messages (also covers a tick that read no new data).
	_reader.dispatch(dispatchCb);
}

void ServerConnection::close() {
	if (!_ssl || _shutdown) {
		return;
	}
	auto ssl = (SSL *)_ssl;
	// Drive the QUIC shutdown to completion so the CONNECTION_CLOSE is actually transmitted before the
	// SSL is freed -- a single SSL_shutdown only queues it. Bounded (1s) so we never hang on an
	// unresponsive peer.
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

/*bool ServerConnection::sendData(BytesView payload) {
	if (!_ssl) {
		return false;
	}
	return writeDataFrame(_ssl, BytesView(_dict.data(), _dict.size()), payload);
}

bool ServerConnection::recvData(Bytes &out, uint64_t timeoutUs) {
	if (!_ssl) {
		return false;
	}
	uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + timeoutUs;
	return readDataFrame(_ssl, BytesView(_dict.data(), _dict.size()), out, deadline);
}*/

// --- Listener ---

bool Listener::open(const Address &addr) {
	if (addr.isUnix()) {
		log::source().error("remote::Listener",
				"unix-domain QUIC listener is not supported yet (needs a custom datagram BIO): ",
				addr.description());
		return false;
	}

	int fd = makeBoundUdpSocket(addr);
	if (fd < 0) {
		return false;
	}

	EVP_PKEY *pkey = nullptr;
	X509 *cert = nullptr;
	if (!makeSelfSignedCert(&pkey, &cert)) {
		logSslErrors("makeSelfSignedCert");
		BIO_closesocket(fd);
		return false;
	}

	SSL_CTX *ctx = SSL_CTX_new(OSSL_QUIC_server_method());
	SSL *listener = nullptr;
	if (!ctx) {
		logSslErrors("SSL_CTX_new");
		goto fail;
	}
	if (SSL_CTX_use_certificate(ctx, cert) <= 0 || SSL_CTX_use_PrivateKey(ctx, pkey) <= 0) {
		logSslErrors("SSL_CTX_use_certificate/PrivateKey");
		goto fail;
	}
	SSL_CTX_set_alpn_select_cb(ctx, alpnSelectCb, nullptr);

	listener = SSL_new_listener(ctx, 0);
	if (!listener) {
		logSslErrors("SSL_new_listener");
		goto fail;
	}
	if (!SSL_set_fd(listener, fd)) {
		logSslErrors("SSL_set_fd");
		goto fail;
	}
	SSL_set_blocking_mode(listener, 0); // non-blocking: driven by the host looper
	if (!SSL_listen(listener)) {
		logSslErrors("SSL_listen");
		goto fail;
	}

	_ctx = ctx;
	_ssl = listener;
	_pkey = pkey;
	_cert = cert;
	_fd = fd;
	log::source().info("remote::Listener", "listening (QUIC) on ", addr.description());
	return true;

fail:
	if (listener) {
		SSL_free(listener);
	}
	if (ctx) {
		SSL_CTX_free(ctx);
	}
	X509_free(cert);
	EVP_PKEY_free(pkey);
	BIO_closesocket(fd);
	return false;
}

void Listener::close() {
	if (_ssl) {
		SSL_free((SSL *)_ssl);
		_ssl = nullptr;
	}
	if (_ctx) {
		SSL_CTX_free((SSL_CTX *)_ctx);
		_ctx = nullptr;
	}
	if (_cert) {
		X509_free((X509 *)_cert);
		_cert = nullptr;
	}
	if (_pkey) {
		EVP_PKEY_free((EVP_PKEY *)_pkey);
		_pkey = nullptr;
	}
	if (_fd >= 0) {
		BIO_closesocket(_fd);
		_fd = -1;
	}
}

void Listener::handleEvents(const AcceptCallback &onAccept) {
	if (!_ssl) {
		return;
	}
	// Pump incoming packets (handshakes) then drain ready connections (non-blocking listener).
	SSL_handle_events((SSL *)_ssl);
	for (;;) {
		SSL *conn = SSL_accept_connection((SSL *)_ssl, 0);
		if (!conn) {
			break;
		}
		auto sc = Rc<ServerConnection>::create((void *)conn);
		if (sc) {
			log::source().info("remote::Listener", "accepted a client connection");
			onAccept(sp::move(sc));
		} else {
			SSL_free(conn);
		}
	}
}

uint64_t Listener::getEventTimeout() const {
	if (!_ssl) {
		return maxOf<uint64_t>();
	}
	struct timeval tv;
	int infinite = 0;
	if (SSL_get_event_timeout((SSL *)_ssl, &tv, &infinite)) {
		if (infinite) {
			return maxOf<uint64_t>();
		}
		return uint64_t(tv.tv_sec) * 1'000'000ull + uint64_t(tv.tv_usec);
	}
	return maxOf<uint64_t>();
}

} // namespace stappler::xenolith::remote
