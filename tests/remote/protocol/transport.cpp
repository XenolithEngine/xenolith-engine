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

/* The transport seam, exercised through the in-process `mem:` implementation.
 *
 * These assertions are about the CONTRACT, not about mem: itself -- a socket transport has to
 * behave the same way at this interface, and the same cases are what a new one has to satisfy to be
 * usable by the protocol above.
 */

#include "SPCommon.h"

#include "XLRemoteTransport.h"

#include "SPPlatform.h"

#include "../tests.h"

#include <unistd.h> // getpid, for a per-run socket path

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

// Read everything currently available on a stream.
static Bytes readAll(TransportStream *s) {
	Bytes out;
	uint8_t buf[512];
	for (;;) {
		size_t got = 0;
		if (s->read(buf, sizeof(buf), got) != Status::Ok || got == 0) {
			break;
		}
		out.insert(out.end(), buf, buf + got);
	}
	return out;
}

static size_t writeAll(TransportStream *s, BytesView data) {
	size_t total = 0;
	while (total < data.size()) {
		size_t w = 0;
		if (s->write(data.sub(total), w) != Status::Ok || w == 0) {
			break;
		}
		total += w;
	}
	return total;
}

} // namespace

void performTransportTests() {
	sprt::cout << "--- remote transport (mem:) ---\n";

	initializeTransports();

	check(TransportRegistry::has(AddressScheme::Mem), "transport: mem: is registered");

	{
		// A scheme nothing registered must fail with a diagnosis, not a crash and not a link error.
		// This is the whole point of the registry: what a build can carry is a runtime fact.
		auto conn = TransportRegistry::connect(Address::parse("tcp://127.0.0.1:1"));
		check(conn == nullptr, "transport: an unregistered scheme refuses to connect");
	}

	{
		auto conn = TransportRegistry::connect(Address::parse("mem:nobody-is-listening"));
		check(conn == nullptr, "transport: connecting to an unbound endpoint fails");
	}

	auto listener = TransportRegistry::listen(Address::parse("mem:testpoint"));
	check(listener != nullptr && listener->isOpen(), "transport: listener binds");
	if (!listener) {
		return;
	}

	{
		auto second = TransportRegistry::listen(Address::parse("mem:testpoint"));
		check(second == nullptr, "transport: a name can only be bound once");
	}

	auto client = TransportRegistry::connect(Address::parse("mem:testpoint"));
	check(client != nullptr, "transport: connect finds the bound endpoint");

	Rc<TransportConnection> server;
	listener->handleEvents([&](Rc<TransportConnection> &&c) { server = sp::move(c); });
	check(server != nullptr, "transport: the listener yields the server half");
	if (!client || !server) {
		return;
	}

	{
		check(server->hasCaps(TransportCaps::PeerAuthenticated),
				"transport: an in-process peer is authenticated");
		check(server->getPeerIdentity().authenticated && server->getPeerIdentity().uid >= 0,
				"transport: PeerIdentity carries the credentials");
		// Deliberately NOT MessageFramed: the pipe is a byte stream, so the reassembler above keeps
		// running the same path a socket exercises.
		check(!server->hasCaps(TransportCaps::MessageFramed),
				"transport: mem: is a byte stream, not message-framed");
	}

	auto cs = client->getStream(StreamClass::Control);
	auto ss = server->getStream(StreamClass::Control);
	check(cs != nullptr && ss != nullptr, "transport: both halves expose a stream");
	if (!cs || !ss) {
		return;
	}

	{
		// Without MultiStream every class is the same channel -- the protocol may ask for any of them
		// and must still get one connected stream.
		check(client->getStream(StreamClass::Frame) == cs
						&& client->getStream(StreamClass::Bulk) == cs,
				"transport: single-stream transport folds every class onto one");
	}

	{
		Bytes payload;
		payload.resize(1'000);
		for (size_t i = 0; i < payload.size(); ++i) { payload[i] = uint8_t(i * 17 + 5); }

		checkEq(writeAll(cs, BytesView(payload)), payload.size(), "transport: client writes");
		auto got = readAll(ss);
		check(BytesView(got) == BytesView(payload), "transport: server reads what was written");

		// And the other direction, on the same pair.
		checkEq(writeAll(ss, BytesView(payload)), payload.size(), "transport: server writes back");
		check(BytesView(readAll(cs)) == BytesView(payload), "transport: the pair is bidirectional");
	}

	{
		// A read with nothing available is not an error and not end-of-stream: the protocol's poll
		// path calls this on every tick and must be able to tell "nothing yet" from "gone".
		uint8_t buf[16];
		size_t got = 1;
		check(ss->read(buf, sizeof(buf), got) == Status::Ok && got == 0,
				"transport: an empty read yields nothing, not an error");
		check(!ss->isClosed(), "transport: an empty stream is not a closed one");
	}

	{
		// The same contract, over a real socket. A transport is only usable by the protocol if it
		// behaves identically at this interface, so the unix one is driven through the same steps --
		// and its distinguishing property, a peer the KERNEL identifies, is asserted here.
		auto path = toString("/tmp/xl-remote-transport-test-", ::getpid(), ".sock");
		auto ua = Address::parse(toString("unix:", path));

		auto ul = TransportRegistry::listen(ua);
		check(ul != nullptr && ul->isOpen(), "unix: listener binds");
		if (ul) {
			auto uc = TransportRegistry::connect(ua);
			check(uc != nullptr, "unix: connect reaches the bound path");

			Rc<TransportConnection> us;
			ul->handleEvents([&](Rc<TransportConnection> &&c) { us = sp::move(c); });
			check(us != nullptr, "unix: the listener yields the server half");

			if (uc && us) {
				check(us->hasCaps(TransportCaps::PeerAuthenticated),
						"unix: the peer is authenticated by the kernel");
				check(us->getPeerIdentity().authenticated && us->getPeerIdentity().uid >= 0,
						"unix: SO_PEERCRED yields the peer's uid");
				// NOT Encrypted: the bytes never leave the kernel, and claiming otherwise would be a
				// lie the policy layer might act on.
				check(!us->hasCaps(TransportCaps::Encrypted),
						"unix: does not claim encryption it does not provide");
				check(us->hasCaps(TransportCaps::Pollable), "unix: exposes a pollable handle");

				Bytes msg{'u', 'n', 'i', 'x'};
				checkEq(writeAll(uc->getStream(StreamClass::Control), BytesView(msg)), msg.size(),
						"unix: client writes");
				// A socket is asynchronous: the bytes are in the kernel, but the reader may need a
				// moment before recv sees them.
				Bytes got;
				for (int i = 0; i < 200 && got.size() < msg.size(); ++i) {
					auto part = readAll(us->getStream(StreamClass::Control));
					got.insert(got.end(), part.begin(), part.end());
					if (got.size() < msg.size()) {
						sp::platform::sleep(1'000);
					}
				}
				check(BytesView(got) == BytesView(msg), "unix: server reads what was written");
			}
			if (uc) {
				uc->close(true);
			}
			ul->close();
		}
	}

	{
		// Bytes already sent must survive the sender closing: the peer has to be able to drain them,
		// which is what lets a graceful shutdown deliver its last messages.
		Bytes tail{'b', 'y', 'e'};
		writeAll(cs, BytesView(tail));
		client->close(true);

		check(BytesView(readAll(ss)) == BytesView(tail), "transport: buffered bytes survive close");
		check(ss->isClosed(), "transport: the stream reads closed once drained");
		check(server->isClosed(), "transport: the peer observes the close");
	}
}

} // namespace stappler::xenolith::remote
