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

/* Message classes over independent transport streams.
 *
 * Two things are asserted here, and they fail for different reasons.
 *
 * The first is the domain -> class mapping, which is a policy decision with a load-bearing
 * consequence: Font rides with Window because the client flushes a frame's glyph requests
 * immediately before its FrameInput, so separating them would ungate the glyphs under load and
 * nowhere else. That is not something a running test would catch, so the mapping itself is pinned.
 *
 * The second is that the separation is real end to end. Both ends of a session are built here in ONE
 * process over `mem:` -- Connection::init needs no handshake and poll() never blocks, so the two
 * halves can be pumped by hand in turn. That is a level of coverage the suite did not have: until
 * now `mem:` only exercised the raw transport, and everything above it (framing, the reassembler,
 * the send queue) was only ever driven over a real socket by remote-check.py.
 */

#include "SPCommon.h"

#include "XLRemoteConnection.h"
#include "XLRemoteProtocol.h"
#include "XLRemoteTransport.h"

#include "../tests.h"

#include <unistd.h> // getpid, for a per-run endpoint name

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

// One dispatched message, flattened so an assertion can name what it expects.
struct Received {
	Domain domain;
	uint8_t code;
	uint32_t serial;
	size_t size;
};

// Pump a connection once and record everything it dispatched, in dispatch order.
static Vector<Received> pump(Connection *conn) {
	Vector<Received> out;
	conn->poll([&](const MessageHeader &h, BytesView payload) -> bool {
		out.emplace_back(Received{Domain(h.domain), h.code, h.serial, payload.size()});
		return true; // consume everything; deferral is framing.cpp's subject
	});
	return out;
}

} // namespace

void performStreamTests() {
	sprt::cout << "--- remote streams ---\n";

	{
		// The mapping is policy, so it is pinned rather than derived. Font sitting with Window is the
		// assertion that matters: it is the one whose violation is invisible until a frame under load
		// reconciles against a glyph dependency that has not arrived.
		checkEq(uint64_t(toInt(streamClassForDomain(Domain::Global))),
				uint64_t(toInt(StreamClass::Control)), "streams: Global is control traffic");
		checkEq(uint64_t(toInt(streamClassForDomain(Domain::Window))),
				uint64_t(toInt(StreamClass::Control)), "streams: Window is control traffic");
		checkEq(uint64_t(toInt(streamClassForDomain(Domain::Font))),
				uint64_t(toInt(StreamClass::Control)), "streams: Font is control traffic");
		check(streamClassForDomain(Domain::Font) == streamClassForDomain(Domain::Window),
				"streams: Font and Window share a stream, so a glyph flush stays ordered before its "
				"FrameInput");
		checkEq(uint64_t(toInt(streamClassForDomain(Domain::Data))),
				uint64_t(toInt(StreamClass::Bulk)), "streams: Data is bulk traffic");
	}

	initializeTransports();

	auto name = toString("streams-", ::getpid());
	auto addr = Address::parse(toString("mem:", name));

	auto listener = TransportRegistry::listen(addr);
	check(listener != nullptr, "streams: the mem: endpoint binds");
	if (!listener) {
		return;
	}

	auto clientTransport = TransportRegistry::connect(addr);
	Rc<TransportConnection> serverTransport;
	listener->handleEvents([&](Rc<TransportConnection> &&c) { serverTransport = sp::move(c); });
	check(clientTransport != nullptr && serverTransport != nullptr,
			"streams: both halves of the session exist");
	if (!clientTransport || !serverTransport) {
		return;
	}

	// No handshake: init() only adopts the transport, and the dictionary stays empty. That is exactly
	// what makes a single-threaded loopback possible -- the handshake is the one blocking step.
	auto server = Rc<Connection>::create(sp::move(serverTransport), Role::Server);
	auto client = Rc<Connection>::create(sp::move(clientTransport), Role::Client);
	check(server != nullptr && client != nullptr, "streams: a Connection binds to a mem: transport");
	if (!server || !client) {
		return;
	}

	{
		Value small;
		small.addInteger(42);
		checkEq(uint64_t(toInt(server->sendCborMessage(Domain::Window, 7, small))),
				uint64_t(toInt(GlobalError::Ok)), "streams: a Window message is sent");

		Bytes blob;
		blob.resize(4'096);
		for (size_t i = 0; i < blob.size(); ++i) { blob[i] = uint8_t(i * 31 + 7); }
		checkEq(uint64_t(toInt(server->sendMessage(Domain::Data, 1, blob))),
				uint64_t(toInt(GlobalError::Ok)), "streams: a Data message is sent");

		auto got = pump(client);
		checkEq(uint64_t(got.size()), uint64_t(2), "streams: both messages arrive");
		if (got.size() == 2) {
			check(got[0].domain == Domain::Window && got[1].domain == Domain::Data,
					"streams: each message keeps its domain across its own reassembler");
			checkEq(uint64_t(got[1].size), uint64_t(blob.size()),
					"streams: a Data payload survives its own stream intact");
		}
	}

	{
		// The point of the whole exercise. A bulk blob is queued FIRST and a small control message
		// after it; on one stream the control message would sit behind every byte of the blob, which is
		// the head-of-line blocking an 8 MiB screenshot causes today. With Bulk separated, the control
		// message is dispatched first.
		Bytes blob;
		blob.resize(256u * 1'024);
		for (size_t i = 0; i < blob.size(); ++i) { blob[i] = uint8_t(i * 13 + 3); }
		server->sendMessage(Domain::Data, 1, blob);

		Value ping;
		ping.addInteger(1);
		server->sendCborMessage(Domain::Window, 9, ping);

		auto got = pump(client);
		check(got.size() >= 2, "streams: nothing is lost when a bulk blob is in flight");
		if (got.size() >= 2) {
			check(got[0].domain == Domain::Window,
					"streams: a control message queued AFTER a bulk blob is dispatched BEFORE it");
			check(got[1].domain == Domain::Data && got[1].size == blob.size(),
					"streams: the bulk blob still arrives whole, just later");
		}
	}

	{
		// The reverse direction is a separate pair of pipes, so it gets its own assertion rather than
		// an assumption of symmetry.
		Value v;
		v.addString("from the client");
		client->sendCborMessage(Domain::Data, 2, v);
		auto got = pump(server);
		checkEq(uint64_t(got.size()), uint64_t(1), "streams: the client can send on Bulk too");
		if (got.size() == 1) {
			check(got[0].domain == Domain::Data, "streams: and it arrives on the server's Bulk");
		}
	}

	{
		client->close();
		auto got = pump(server);
		check(got.empty(), "streams: a closed peer dispatches nothing further");
		check(server->isClosed(), "streams: the server sees the peer go away");
	}

	listener->close();
}

} // namespace stappler::xenolith::remote
