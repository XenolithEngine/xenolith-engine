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

/* The `quic://` transport: OpenSSL QUIC over UDP.
 *
 * This is the code that used to BE the protocol layer -- SSL_CTX, the ephemeral certificate, the
 * bound socket and the read/write loops all lived inside remote::Listener and remote::Connector,
 * with an `SSL *` handed upward as a `void *`. Nothing has changed about how it talks to OpenSSL;
 * what changed is that it now says so through TransportConnection, so it is one implementation
 * among several rather than the only thing the protocol can speak to.
 */

#include "XLRemoteTransport.h"

#include "SPCoreCrypto.h" // crypto::isEqualConstantTime for the SPKI pin
#include "SPPlatform.h" // sp::platform::clock / sleep

#include <openssl/ssl.h>
#include <openssl/quic.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <sys/socket.h> // AF_UNSPEC, SOCK_DGRAM (via the sprt socket layer)
#include <sys/time.h> // struct timeval (SSL_get_event_timeout)

// Needs a kernel socket layer -- wasm has none (every entry point in the runtime socket
// backend answers ENOSYS), so the scheme is simply absent there rather than failing at run time.
#if !SPRT_WASM

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

namespace {

// ALPN id (1-byte length prefix + "xlremote"); QUIC mandates ALPN on both ends.
static const unsigned char kAlpn[] = {8, 'x', 'l', 'r', 'e', 'm', 'o', 't', 'e'};

// Largest number of connections taken in one handleEvents pass, so a burst of QUIC handshakes cannot
// monopolise the caller's thread inside a single looper iteration.
static constexpr uint32_t kMaxAcceptsPerPump = 8;

static void logSslErrors(StringView where) {
	unsigned long e;
	while ((e = ERR_get_error()) != 0) {
		char buf[256];
		ERR_error_string_n(e, buf, sizeof(buf));
		log::source().error("remote::quic", where, ": ", buf);
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

// SHA-256 over the DER SubjectPublicKeyInfo -- the conventional "SPKI pin". The whole certificate is
// deliberately not hashed: the pin then survives a re-issued certificate carrying the same key.
static Bytes spkiFingerprint(X509 *cert) {
	Bytes out;
	if (!cert) {
		return out;
	}
	unsigned char *der = nullptr;
	int len = i2d_X509_PUBKEY(X509_get_X509_PUBKEY(cert), &der);
	if (len > 0 && der) {
		out.resize(SHA256_DIGEST_LENGTH);
		SHA256(der, size_t(len), out.data());
		OPENSSL_free(der);
	}
	return out;
}

static Bytes peerSpkiFingerprint(SSL *ssl) {
	X509 *cert = SSL_get1_peer_certificate(ssl);
	auto out = spkiFingerprint(cert);
	if (cert) {
		X509_free(cert);
	}
	return out;
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

// Bind a UDP socket for the listener. AF_UNSPEC, so an IPv6 host (or an IPv6-capable "all
// interfaces") resolves and binds like any other -- the old AF_INET made every address IPv4-only.
static int makeBoundUdpSocket(const Address &addr) {
	char service[6];
	portToStr(addr.port, service);
	const char *host = addr.host.empty() ? nullptr : addr.host.data();

	BIO_ADDRINFO *res = nullptr;
	if (!BIO_lookup_ex(host, service, BIO_LOOKUP_SERVER, AF_UNSPEC, SOCK_DGRAM, 0, &res) || !res) {
		logSslErrors("BIO_lookup_ex");
		return -1;
	}

	int fd = -1;
	// Walk the candidates: an AF_UNSPEC lookup can answer with a family this host cannot open.
	for (const BIO_ADDRINFO *ai = res; ai; ai = BIO_ADDRINFO_next(ai)) {
		fd = BIO_socket(BIO_ADDRINFO_family(ai), SOCK_DGRAM, 0, 0);
		if (fd < 0) {
			continue;
		}
		if (BIO_bind(fd, BIO_ADDRINFO_address(ai), BIO_SOCK_REUSEADDR)) {
			break;
		}
		BIO_closesocket(fd);
		fd = -1;
	}
	if (fd < 0) {
		logSslErrors("BIO_bind");
	}
	BIO_ADDRINFO_free(res);
	return fd;
}

// --- stream ---

class QuicStream : public TransportStream {
public:
	virtual ~QuicStream() = default;

	bool init(void *ssl) {
		_ssl = ssl;
		return _ssl != nullptr;
	}

	// Detach when the connection tears the SSL down, so a stream outliving its connection answers
	// "closed" rather than touching freed memory.
	void invalidate() { _ssl = nullptr; }

	virtual Status write(BytesView data, size_t &written) override {
		written = 0;
		if (!_ssl || data.empty()) {
			return _ssl ? Status::Ok : Status::ErrorNotPermitted;
		}
		auto ssl = (SSL *)_ssl;
		size_t w = 0;
		if (SSL_write_ex(ssl, data.data(), data.size(), &w) == 1) {
			written = w;
			return Status::Ok;
		}
		int err = SSL_get_error(ssl, 0);
		if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
			// Backpressure: the send buffer or the peer's flow-control window is full. Not an error;
			// the caller keeps the remainder and retries on the next pump.
			SSL_handle_events(ssl);
			return Status::Ok;
		}
		return Status::ErrorNotPermitted;
	}

	virtual Status read(uint8_t *buf, size_t len, size_t &got) override {
		got = 0;
		if (!_ssl || len == 0) {
			return _ssl ? Status::Ok : Status::ErrorNotPermitted;
		}
		auto ssl = (SSL *)_ssl;
		size_t n = 0;
		if (SSL_read_ex(ssl, buf, len, &n) == 1) {
			got = n;
			return Status::Ok;
		}
		int err = SSL_get_error(ssl, 0);
		if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
			return Status::Ok; // drained, not closed
		}
		return Status::ErrorNotPermitted;
	}

	virtual bool isClosed() const override {
		if (!_ssl) {
			return true;
		}
		SSL_CONN_CLOSE_INFO info;
		return SSL_get_conn_close_info((SSL *)_ssl, &info, sizeof(info)) != 0;
	}

protected:
	void *_ssl = nullptr; // borrowed from the owning QuicConnection
};

// --- connection ---

class QuicConnection : public TransportConnection {
public:
	virtual ~QuicConnection();

	// `ctx` and `fd` are owned only on the client side; an accepted server connection gets the SSL
	// alone (the listener owns the context and the socket).
	bool init(void *ctx, void *ssl, int fd);

	virtual TransportCaps getCaps() const override {
		// QUIC gives independent streams and datagrams; neither is used yet (the protocol still
		// interleaves everything on the default stream), but declaring them is what lets the message
		// classes start mapping onto real streams without another transport change.
		return TransportCaps::Encrypted | TransportCaps::MultiStream | TransportCaps::Datagrams
				| TransportCaps::Pollable;
	}

	virtual TransportStream *getStream(StreamClass) override { return _stream; }

	virtual const PeerIdentity &getPeerIdentity() const override { return _peer; }

	virtual sprt::dispatch::NativeHandle getPollHandle() const override {
		return sprt::dispatch::NativeHandle(_fd);
	}

	virtual uint64_t getEventTimeout() const override;
	virtual Status handleEvents() override;

	virtual bool isClosed() override;
	virtual void close(bool graceful) override;

protected:
	void *_ctx = nullptr; // SSL_CTX*, client side only
	void *_ssl = nullptr; // SSL* (the connection)
	int _fd = -1; // bound UDP socket, client side only (SSL_set_fd uses BIO_NOCLOSE)
	bool _shutdown = false;
	Rc<QuicStream> _stream;
	PeerIdentity _peer;
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

QuicConnection::~QuicConnection() {
	if (_stream) {
		_stream->invalidate();
	}
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
		BIO_closesocket(_fd);
		_fd = -1;
	}
}

__SPRT_POP_ALLOW_CXXABI_ALLOC

bool QuicConnection::init(void *ctx, void *ssl, int fd) {
	_ctx = ctx;
	_ssl = ssl;
	_fd = fd;
	if (!_ssl) {
		return false;
	}
	// Non-blocking throughout: the host looper drives both the deadline-bounded setup handshake and
	// the drain in poll(); a blocking read would stall the app thread.
	SSL_set_blocking_mode((SSL *)_ssl, 0);

	_stream = Rc<QuicStream>::create(_ssl);
	if (!_stream) {
		return false;
	}

	_peer.spki = peerSpkiFingerprint((SSL *)_ssl);
	// The certificate is ephemeral and self-signed, so holding its key proves nothing about WHO the
	// peer is -- only that it is the same party across a session. Identity comes from the protocol's
	// bearer key, which is why this stays false.
	_peer.authenticated = false;
	_peer.description = toString("quic:",
			_peer.spki.empty()
					? String("<no certificate>")
					: base16::encode<Interface>(BytesView(_peer.spki.data(), _peer.spki.size())));
	return true;
}

uint64_t QuicConnection::getEventTimeout() const {
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

Status QuicConnection::handleEvents() {
	if (!_ssl) {
		return Status::ErrorNotPermitted;
	}
	SSL_handle_events((SSL *)_ssl);
	return Status::Ok;
}

bool QuicConnection::isClosed() {
	if (!_ssl) {
		return true;
	}
	auto ssl = (SSL *)_ssl;
	SSL_handle_events(ssl);
	SSL_CONN_CLOSE_INFO info;
	return SSL_get_conn_close_info(ssl, &info, sizeof(info)) != 0;
}

void QuicConnection::close(bool graceful) {
	if (!_ssl || _shutdown) {
		return;
	}
	auto ssl = (SSL *)_ssl;
	if (graceful) {
		// Drive the shutdown to completion so the CONNECTION_CLOSE is actually transmitted -- a single
		// SSL_shutdown only queues it. Bounded so an unresponsive peer cannot hang us.
		uint64_t deadline = sp::platform::clock(ClockType::Monotonic) + 1'000'000;
		for (;;) {
			int ret = SSL_shutdown(ssl);
			if (ret != 0) {
				break; // 1 == fully shut down, <0 == error: either way we are done
			}
			SSL_handle_events(ssl);
			if (sp::platform::clock(ClockType::Monotonic) >= deadline) {
				break;
			}
			sp::platform::sleep(2'000);
		}
	} else {
		SSL_shutdown(ssl);
	}
	_shutdown = true;
}

// --- listener ---

class QuicListener : public TransportListener {
public:
	virtual ~QuicListener();

	virtual Status open(const Address &, const TransportServerConfig &) override;
	virtual bool isOpen() const override { return _ssl != nullptr; }
	virtual void close() override;

	virtual sprt::dispatch::NativeHandle getPollHandle() const override {
		return sprt::dispatch::NativeHandle(_fd);
	}
	virtual uint64_t getEventTimeout() const override;

	virtual void handleEvents(const Callback<void(Rc<TransportConnection> &&)> &) override;

	virtual BytesView getIdentity() const override {
		return BytesView(_certFingerprint.data(), _certFingerprint.size());
	}

protected:
	void *_ctx = nullptr; // SSL_CTX*
	void *_ssl = nullptr; // SSL* (the listener)
	void *_pkey = nullptr; // EVP_PKEY* (self-signed key)
	void *_cert = nullptr; // X509* (self-signed cert)
	int _fd = -1;
	Bytes _certFingerprint;
};

__SPRT_PUSH_ALLOW_CXXABI_ALLOC

QuicListener::~QuicListener() { close(); }

__SPRT_POP_ALLOW_CXXABI_ALLOC

Status QuicListener::open(const Address &addr, const TransportServerConfig &) {
	int fd = makeBoundUdpSocket(addr);
	if (fd < 0) {
		return Status::ErrorNotPermitted;
	}

	EVP_PKEY *pkey = nullptr;
	X509 *cert = nullptr;
	if (!makeSelfSignedCert(&pkey, &cert)) {
		logSslErrors("makeSelfSignedCert");
		BIO_closesocket(fd);
		return Status::ErrorNotPermitted;
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
	SSL_set_blocking_mode(listener, 0); // driven by the host looper
	if (!SSL_listen(listener)) {
		logSslErrors("SSL_listen");
		goto fail;
	}

	_ctx = ctx;
	_ssl = listener;
	_pkey = pkey;
	_cert = cert;
	_fd = fd;
	_certFingerprint = spkiFingerprint(cert);
	log::source().info("remote::quic", "listening on ", addr.description(), ", SPKI ",
			base16::encode<Interface>(getIdentity()));
	return Status::Ok;

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
	return Status::ErrorNotPermitted;
}

void QuicListener::close() {
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
	_certFingerprint.clear();
}

uint64_t QuicListener::getEventTimeout() const {
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

void QuicListener::handleEvents(const Callback<void(Rc<TransportConnection> &&)> &onAccept) {
	if (!_ssl) {
		return;
	}
	SSL_handle_events((SSL *)_ssl);
	for (uint32_t n = 0; n < kMaxAcceptsPerPump; ++n) {
		SSL *conn = SSL_accept_connection((SSL *)_ssl, 0);
		if (!conn) {
			break;
		}
		// The accepted connection owns neither the context nor the socket -- both belong to this
		// listener and outlive it.
		auto c = Rc<QuicConnection>::create(nullptr, (void *)conn, -1);
		if (c) {
			log::source().info("remote::quic", "accepted a client connection");
			onAccept(Rc<TransportConnection>(c.get()));
		} else {
			SSL_free(conn);
		}
	}
}

// --- transport ---

class QuicTransport : public Transport {
public:
	virtual ~QuicTransport() = default;

	virtual AddressScheme getScheme() const override { return AddressScheme::Quic; }

	virtual TransportCaps getCaps() const override {
		return TransportCaps::Encrypted | TransportCaps::MultiStream | TransportCaps::Datagrams
				| TransportCaps::Pollable;
	}

	virtual Rc<TransportConnection> connect(const Address &, const TransportClientConfig &) override;

	virtual Rc<TransportListener> listen(const Address &addr,
			const TransportServerConfig &cfg) override {
		auto l = Rc<QuicListener>::create();
		if (!l || l->open(addr, cfg) != Status::Ok) {
			return nullptr;
		}
		return Rc<TransportListener>(l.get());
	}
};

Rc<TransportConnection> QuicTransport::connect(const Address &addr,
		const TransportClientConfig &cfg) {
	SSL_CTX *ctx = nullptr;
	SSL *ssl = nullptr;
	int fd = -1;
	BIO_ADDRINFO *res = nullptr;
	const BIO_ADDRINFO *ai = nullptr;
	char service[6];

	ctx = SSL_CTX_new(OSSL_QUIC_client_method());
	if (!ctx) {
		logSslErrors("SSL_CTX_new");
		goto fail;
	}
	// Chain validation is meaningless against an ephemeral self-signed certificate; the server is
	// authenticated by pinning its key below instead (cfg.expectedFingerprint).
	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);

	portToStr(addr.port, service);
	{
		const char *host = addr.host.empty() ? "127.0.0.1" : addr.host.data();
		if (!BIO_lookup_ex(host, service, BIO_LOOKUP_CLIENT, AF_UNSPEC, SOCK_DGRAM, 0, &res)
				|| !res) {
			logSslErrors("BIO_lookup_ex");
			goto fail;
		}
	}

	// Try each candidate: AF_UNSPEC may answer with a family this host cannot open.
	for (ai = res; ai; ai = BIO_ADDRINFO_next(ai)) {
		fd = BIO_socket(BIO_ADDRINFO_family(ai), SOCK_DGRAM, 0, 0);
		if (fd >= 0) {
			break;
		}
	}
	if (fd < 0 || !ai) {
		logSslErrors("BIO_socket");
		goto fail;
	}

	ssl = SSL_new(ctx);
	if (!ssl) {
		logSslErrors("SSL_new");
		goto fail;
	}
	if (!SSL_set_fd(ssl, fd)) {
		logSslErrors("SSL_set_fd");
		goto fail;
	}
	if (!SSL_set1_initial_peer_addr(ssl, BIO_ADDRINFO_address(ai))) {
		logSslErrors("SSL_set1_initial_peer_addr");
		goto fail;
	}
	if (SSL_set_alpn_protos(ssl, kAlpn, sizeof(kAlpn)) != 0) { // 0 == success here
		logSslErrors("SSL_set_alpn_protos");
		goto fail;
	}
	SSL_set_tlsext_host_name(ssl, "localhost"); // SNI; matches the listener cert CN
	SSL_set_blocking_mode(ssl, 0);

	// Drive the QUIC handshake with a deadline so a missing or dead server fails fast instead of
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
				logSslErrors("SSL_connect");
				goto fail;
			}
			if (sp::platform::clock(ClockType::Monotonic) >= deadline) {
				log::source().error("remote::quic", "connect timed out: ", addr.description());
				goto fail;
			}
			SSL_handle_events(ssl); // pump retransmit timers
			sp::platform::sleep(2'000);
		}
	}

	// Pin the server's key BEFORE the session sends anything: the bearer key goes out in the very
	// next step, so a man in the middle that gets past this point has it.
	if (!cfg.expectedFingerprint.empty()) {
		auto actual = peerSpkiFingerprint(ssl);
		if (actual.empty()
				|| !crypto::isEqualConstantTime(BytesView(actual.data(), actual.size()),
						BytesView(cfg.expectedFingerprint.data(), cfg.expectedFingerprint.size()))) {
			log::source().error("remote::quic", "server key mismatch for ", addr.description(),
					": expected SPKI ",
					base16::encode<Interface>(BytesView(cfg.expectedFingerprint.data(),
							cfg.expectedFingerprint.size())),
					", got ",
					actual.empty() ? String("<none>")
								   : base16::encode<Interface>(
											 BytesView(actual.data(), actual.size())));
			goto fail;
		}
	}

	BIO_ADDRINFO_free(res);
	res = nullptr;

	{
		auto conn = Rc<QuicConnection>::create((void *)ctx, (void *)ssl, fd);
		if (conn) {
			log::source().info("remote::quic", "connected to ", addr.description());
			return Rc<TransportConnection>(conn.get());
		}
	}

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

} // namespace

void registerQuicTransport() { TransportRegistry::registerTransport(Rc<QuicTransport>::create()); }

} // namespace stappler::xenolith::remote

#else

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// Not available in this build: wasm has none (every entry point in the runtime socket
// backend answers ENOSYS), so the scheme is simply absent there rather than failing at run time.
void registerQuicTransport() { }

} // namespace stappler::xenolith::remote

#endif
