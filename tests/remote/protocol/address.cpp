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

#include "SPCommon.h"

#include "XLRemoteAddress.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;

void performAddressTests() {
	sprt::cout << "--- remote::Address ---\n";

	{
		auto a = Address::parse("127.0.0.1:4480");
		check(!a.isUnix() && a.host == "127.0.0.1" && a.port == 4'480, "address: host:port");
	}

	{
		// An empty host means "all interfaces" -- not "unset", so it must not read as empty().
		auto a = Address::parse(":4480");
		check(!a.isUnix() && a.host.empty() && a.port == 4'480 && !a.empty(),
				"address: :port is all interfaces");
	}

	{
		// No scheme means QUIC -- every caller written before schemes existed passes exactly this.
		auto a = Address::parse("127.0.0.1:4480");
		check(a.scheme == AddressScheme::Quic, "address: bare host:port defaults to quic");
	}

	{
		auto a = Address::parse("quic://127.0.0.1:4480");
		check(a.scheme == AddressScheme::Quic && a.host == "127.0.0.1" && a.port == 4'480,
				"address: quic:// scheme");
	}

	{
		auto a = Address::parse("tcp://example.org:9000");
		check(a.scheme == AddressScheme::Tcp && a.host == "example.org" && a.port == 9'000,
				"address: tcp:// scheme");
	}

	{
		// A path may contain a ':', so a path-based scheme must not reach the host:port split.
		auto a = Address::parse("unix:/run/xenolith.sock");
		check(a.isUnix() && a.scheme == AddressScheme::Unix && a.path == "/run/xenolith.sock",
				"address: unix-domain path");
		check(a.isPathBased() && a.port == 0, "address: unix has no port");
	}

	{
		auto a = Address::parse("mem:testpoint");
		check(a.scheme == AddressScheme::Mem && a.path == "testpoint" && a.isPathBased(),
				"address: mem: endpoint name");
	}

	{
		// An IPv6 literal is bracketed on the wire and bare in the struct: getaddrinfo wants "::1".
		auto a = Address::parse("quic://[::1]:4480");
		check(a.host == "::1" && a.port == 4'480, "address: bracketed IPv6 literal");
		auto round = Address::parse(a.description());
		check(round == a, "address: IPv6 description round-trips through parse");
	}

	{
		auto a = Address::parse("tcp://host:1234");
		check(Address::parse(a.description()) == a, "address: description round-trips");
	}

	{
		// Nothing parsed at all still has to be recognisable as nothing: startListening and the client
		// bootstrap both branch on empty().
		auto a = Address::parse("");
		check(a.empty(), "address: empty input is empty()");
	}

	{
		auto a = Address::parse("127.0.0.1:4480");
		auto b = Address::parse("127.0.0.1:4480");
		auto c = Address::parse("127.0.0.1:4481");
		check(a == b && !(a == c), "address: comparison");
	}
}

} // namespace stappler::xenolith::remote
