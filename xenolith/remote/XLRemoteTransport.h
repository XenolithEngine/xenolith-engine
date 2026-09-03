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

#ifndef XENOLITH_REMOTE_XLREMOTETRANSPORT_H_
#define XENOLITH_REMOTE_XLREMOTETRANSPORT_H_

#include "XLRemoteAddress.h"

#include <sprt/runtime/dispatch/event.h> // sprt::dispatch::NativeHandle

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// What carries the session, separated from what the session SAYS.
//
// The protocol layer used to be sewn to OpenSSL: an `SSL *` travelled through every entry point as
// a `void *`, and both connection classes held SSL_CTX/EVP_PKEY/X509/fd in their own fields. There
// was no seam to put another transport at, which made unix-domain sockets, a plain TLS fallback, a
// browser (WebTransport) client and an in-process loopback for tests all equally impossible.
//
// The seam is here. Above it the protocol only ever moves bytes over an ordered stream; below it a
// transport decides how those bytes travel, who the peer is, and how the looper learns there is
// work. Each implementation registers itself under an address scheme, so a build understands
// exactly the schemes it managed to link.

// What a transport can do. The protocol adapts rather than assuming; anything not declared here is
// emulated above (or simply not used).
enum class TransportCaps : uint32_t {
	None = 0,
	Encrypted = 1 << 0, // confidentiality is provided by the transport itself
	PeerAuthenticated = 1 << 1, // the transport established WHO the peer is (SO_PEERCRED, mTLS),
	// so the protocol's own bearer key may be treated as optional
	MultiStream = 1 << 2, // independent ordered streams; StreamClass maps onto real streams
	Datagrams = 1 << 3, // unreliable datagrams are available
	MessageFramed = 1 << 4, // message boundaries survive (WebSocket, in-process)
	Pollable = 1 << 5, // exposes a NativeHandle the Looper can wait on
};

SP_DEFINE_ENUM_AS_MASK(TransportCaps)

// Which logical channel a message belongs to. On a transport without MultiStream all three resolve
// to the same stream and the protocol interleaves exactly as it does today; the distinction only
// starts paying off when independent streams exist (a bulk screenshot must not delay input).
enum class StreamClass {
	Control, // handshake, ping/pong, window control
	Frame, // frame acquisition and per-frame input
	Bulk, // Domain::Data blocks, font payloads
};

// Who is on the other end, as far as the transport can tell. `authenticated` is the load-bearing
// field: when it is true the server may accept a connection without a bearer key, because something
// stronger than a shared secret already established the peer's identity.
struct SP_PUBLIC PeerIdentity {
	bool authenticated = false;
	Bytes spki; // TLS/QUIC: SHA-256 of the peer's DER SubjectPublicKeyInfo
	int64_t uid = -1; // unix-domain: SO_PEERCRED
	int64_t gid = -1;
	int64_t pid = -1;
	String description; // for logs; never parsed

	String getDescription() const;
};

// One ordered byte channel. Every operation is non-blocking: a transport never parks the caller's
// thread, because that thread is usually the app thread building a frame.
class SP_PUBLIC TransportStream : public Ref {
public:
	virtual ~TransportStream();

	// Write what the transport accepts right now. `written` may be 0 (backpressure) or less than
	// `data.size()`; neither is an error. Anything but Status::Ok means the stream is unusable.
	virtual Status write(BytesView data, size_t &written) = 0;

	// Read what has already arrived. `got` == 0 means nothing is available yet, not end-of-stream --
	// ask isClosed() for that.
	virtual Status read(uint8_t *buf, size_t len, size_t &got) = 0;

	virtual bool isClosed() const = 0;
};

// One established session with a peer.
class SP_PUBLIC TransportConnection : public Ref {
public:
	virtual ~TransportConnection();

	virtual TransportCaps getCaps() const = 0;
	bool hasCaps(TransportCaps c) const { return (getCaps() & c) == c; }

	// The channel for a class of messages. Without MultiStream every class returns the same stream.
	// Never null on a live connection.
	virtual TransportStream *getStream(StreamClass) = 0;

	virtual const PeerIdentity &getPeerIdentity() const = 0;

	// --- integration with the host looper ---

	// The handle to wait on, when the transport declares Pollable. A transport that does not (a
	// browser one, driven by callbacks) returns an invalid handle and calls the readable callback
	// instead.
	virtual sprt::dispatch::NativeHandle getPollHandle() const { return sprt::dispatch::NativeHandle(-1); }

	// Microseconds until this transport next needs servicing regardless of IO (retransmit timers);
	// maxOf<uint64_t>() means "only when readable".
	virtual uint64_t getEventTimeout() const { return maxOf<uint64_t>(); }

	// Service the transport: read datagrams, run timers, complete writes. Called from the looper on
	// readiness and on every update tick.
	virtual Status handleEvents() = 0;

	// For a transport with no pollable handle: invoked when bytes have arrived. Ignored by the
	// pollable ones.
	virtual void setOnReadable(Function<void()> &&) { }

	virtual bool isClosed() = 0;
	virtual void close(bool graceful = true) = 0;
};

// Server-side configuration handed to a transport when it opens a listener. A transport ignores
// what does not apply to it (a unix socket has no certificate).
struct SP_PUBLIC TransportServerConfig {
	// Filesystem permissions for a path-based transport; ignored elsewhere.
	uint32_t socketMode = 0600;
};

struct SP_PUBLIC TransportClientConfig {
	// Expected SHA-256 of the server's DER SubjectPublicKeyInfo. When non-empty a TLS-based
	// transport refuses to connect unless the server presents exactly that key; without it the
	// server is not authenticated at all and anything the session then sends (the bearer key
	// included) goes to whoever answered. Ignored by transports that authenticate by other means.
	Bytes expectedFingerprint;
};

// A bound endpoint accepting connections.
class SP_PUBLIC TransportListener : public Ref {
public:
	virtual ~TransportListener();

	virtual Status open(const Address &, const TransportServerConfig &) = 0;
	virtual bool isOpen() const = 0;
	virtual void close() = 0;

	virtual sprt::dispatch::NativeHandle getPollHandle() const = 0;
	virtual uint64_t getEventTimeout() const = 0;

	// Accept whatever is ready. Implementations bound how many they take per call so a burst cannot
	// monopolise the caller's thread inside one looper iteration.
	virtual void handleEvents(const Callback<void(Rc<TransportConnection> &&)> &) = 0;

	// This listener's identity, for handing to a client out-of-band: the SHA-256 of the DER
	// SubjectPublicKeyInfo for a TLS-based transport, empty for one that has no such notion.
	virtual BytesView getIdentity() const { return BytesView(); }
};

// The implementation registered for one address scheme.
class SP_PUBLIC Transport : public Ref {
public:
	virtual ~Transport();

	virtual AddressScheme getScheme() const = 0;

	// Capabilities a connection from this transport will declare. Available before connecting, so a
	// caller can pick an endpoint by what it can do.
	virtual TransportCaps getCaps() const = 0;

	virtual Rc<TransportConnection> connect(const Address &, const TransportClientConfig &) = 0;
	virtual Rc<TransportListener> listen(const Address &, const TransportServerConfig &) = 0;
};

// Scheme -> implementation. Each transport registers itself from its own translation unit, which is
// compiled only where it can be linked, so the set of schemes a build understands IS the set it can
// actually carry -- asking for one that is not there fails with a message instead of a link error.
class SP_PUBLIC TransportRegistry {
public:
	static void registerTransport(Rc<Transport> &&);
	static Transport *get(AddressScheme);
	static bool has(AddressScheme);

	// Every registered scheme, for diagnostics and for the server-info handshake.
	static Vector<AddressScheme> getSchemes();

	// Resolve the address's scheme and hand off. Null (with a logged reason) when the scheme is not
	// registered in this build.
	static Rc<TransportConnection> connect(const Address &, const TransportClientConfig & = {});
	static Rc<TransportListener> listen(const Address &, const TransportServerConfig & = {});
};

// Register every transport this build linked, once. Called from the app-thread setup and from the
// tests; each registerXxxTransport below is compiled only where its dependencies exist, so this is
// where "what schemes does this build understand" is actually decided.
SP_PUBLIC void initializeTransports();

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTETRANSPORT_H_ */
