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

#ifndef XENOLITH_REMOTE_XLREMOTEADDRESS_H_
#define XENOLITH_REMOTE_XLREMOTEADDRESS_H_

#include "XLCommon.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

// A listen/connect endpoint for the remote render session. The leading scheme selects WHICH
// transport carries the session; TransportRegistry resolves it to a registered implementation, so a
// build only understands the schemes it actually linked.
//
//   "quic://127.0.0.1:4480"       QUIC over UDP (the default when no scheme is given)
//   "127.0.0.1:4480" / ":4480"    same thing, kept working for every existing caller
//   "tcp://host:4480"             TLS 1.3 over TCP
//   "unix:/run/xenolith.sock"     AF_UNIX stream socket
//   "mem:name"                    in-process loopback, for tests
//
// An empty host means "all interfaces" on the listen side and loopback on the connect side.
enum class AddressScheme {
	Quic, // network host:port over QUIC; the default
	Tcp, // network host:port over TLS/TCP
	Unix, // filesystem path
	Mem, // in-process pair, named by `path`
};

SP_PUBLIC StringView getSchemeName(AddressScheme);

struct SP_PUBLIC Address {
	AddressScheme scheme = AddressScheme::Quic;
	String host; // network host; empty == all interfaces (listen) / loopback (connect)
	String path; // unix-domain socket path, or the mem: endpoint name
	uint16_t port = 0;

	static Address parse(StringView);

	// True for a scheme addressed by a path rather than host:port.
	bool isPathBased() const { return scheme == AddressScheme::Unix || scheme == AddressScheme::Mem; }

	// Kept for callers written before schemes existed.
	bool isUnix() const { return scheme == AddressScheme::Unix; }

	bool empty() const { return isPathBased() ? path.empty() : (port == 0 && host.empty()); }

	String description() const;

	auto operator<=>(const Address &) const = default;
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEADDRESS_H_ */
