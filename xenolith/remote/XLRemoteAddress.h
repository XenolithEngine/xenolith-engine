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

// A listen/connect endpoint for the remote render session: either a network host:port or a
// Unix-domain socket path. Parsed from a string:
//   "unix:/run/xenolith.sock"  -> unix-domain
//   "127.0.0.1:4480" / ":4480" -> network (empty host == all interfaces)
struct SP_PUBLIC Address {
	bool unixDomain = false;
	String host; // network host; empty == all interfaces
	String path; // unix-domain socket path
	uint16_t port = 0;

	static Address parse(StringView);

	bool isUnix() const { return unixDomain; }
	bool empty() const { return !unixDomain && port == 0 && host.empty(); }

	String description() const;

	auto operator<=>(const Address &) const = default;
};

} // namespace stappler::xenolith::remote

#endif /* XENOLITH_REMOTE_XLREMOTEADDRESS_H_ */
