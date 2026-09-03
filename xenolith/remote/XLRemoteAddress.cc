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

#include "XLRemoteAddress.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

StringView getSchemeName(AddressScheme s) {
	switch (s) {
	case AddressScheme::Quic: return StringView("quic"); break;
	case AddressScheme::Tcp: return StringView("tcp"); break;
	case AddressScheme::Unix: return StringView("unix"); break;
	case AddressScheme::Mem: return StringView("mem"); break;
	}
	return StringView();
}

Address Address::parse(StringView str) {
	Address addr;

	// Path-based schemes take the whole remainder verbatim -- a filesystem path may itself contain a
	// ':', so it must not reach the host:port split below.
	if (str.starts_with("unix:")) {
		addr.scheme = AddressScheme::Unix;
		addr.path = str.sub(5).str<Interface>();
		return addr;
	}
	if (str.starts_with("mem:")) {
		addr.scheme = AddressScheme::Mem;
		addr.path = str.sub(4).str<Interface>();
		return addr;
	}

	// Network schemes. No scheme at all means QUIC, which is what every caller written before
	// schemes existed passes ("127.0.0.1:4480").
	if (str.starts_with("quic://")) {
		addr.scheme = AddressScheme::Quic;
		str = str.sub(7);
	} else if (str.starts_with("tcp://")) {
		addr.scheme = AddressScheme::Tcp;
		str = str.sub(6);
	}

	// "host:port" or ":port" -- split on the last ':' so an IPv6 literal in brackets survives
	size_t colon = maxOf<size_t>();
	for (size_t i = str.size(); i > 0; --i) {
		if (str[i - 1] == ':') {
			colon = i - 1;
			break;
		}
	}

	if (colon != maxOf<size_t>()) {
		addr.host = str.sub(0, colon).str<Interface>();
		addr.port = uint16_t(str.sub(colon + 1).readInteger(10).get(0));
	} else {
		addr.port = uint16_t(str.readInteger(10).get(0));
	}

	// A bracketed IPv6 literal is stored bare: getaddrinfo wants "::1", not "[::1]".
	if (addr.host.size() >= 2 && addr.host.front() == '[' && addr.host.back() == ']') {
		addr.host = addr.host.substr(1, addr.host.size() - 2);
	}
	return addr;
}

String Address::description() const {
	StringStream s;
	s << getSchemeName(scheme) << ":";
	if (isPathBased()) {
		s << path;
	} else {
		// Re-emit an IPv6 literal in brackets so the description round-trips through parse().
		s << "//";
		if (host.find(':') != maxOf<size_t>()) {
			s << "[" << host << "]";
		} else {
			s << host;
		}
		s << ":" << port;
	}
	return s.str();
}

} // namespace stappler::xenolith::remote
