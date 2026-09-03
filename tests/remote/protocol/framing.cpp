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

/* The message codec and the stream reassembler.
 *
 * A QUIC stream is an ordered byte stream with no message boundaries, so the reassembler is what
 * turns socket wakeups back into messages. Every failure mode it has is invisible in normal running
 * -- a frame that is split across two reads still arrives, just later; a frame that is mis-split
 * corrupts every message after it. So the split is driven explicitly here, one byte at a time.
 */

#include "SPCommon.h"

#include "XLRemoteProtocol.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

struct Received {
	MessageHeader header;
	Bytes payload;
};

// Drain everything the reader currently holds, consuming every message.
static Vector<Received> drain(MessageReader &r) {
	Vector<Received> out;
	r.dispatch([&](const MessageHeader &h, BytesView p) {
		out.emplace_back(Received{h, p.bytes<Interface>()});
		return true;
	});
	return out;
}

static Bytes makePayload(size_t n, uint8_t seed) {
	Bytes b;
	b.resize(n);
	for (size_t i = 0; i < n; ++i) { b[i] = uint8_t(seed + i * 31); }
	return b;
}

} // namespace

void performFramingTests() {
	sprt::cout << "--- remote framing ---\n";

	// A payload that does not compress (pseudo-random bytes) and one that does (a long run), so both
	// branches of encodeFrame -- stored and compressed -- are exercised.
	auto incompressible = makePayload(300, 7);
	Bytes compressible;
	compressible.resize(4'096);

	{
		Bytes wire;
		encodeFrame(wire, BytesView(), MessageType::Server, Domain::Window,
				toInt(WindowCode::AcquireFrame), 42, BytesView(incompressible));

		MessageReader r;
		check(r.append(BytesView(wire), BytesView()), "framing: append accepts a whole frame");
		auto got = drain(r);
		check(got.size() == 1, "framing: one frame in, one message out");
		if (got.size() == 1) {
			checkEq(got[0].header.serial, 42, "framing: serial survives");
			checkEq(got[0].header.domain, toInt(Domain::Window), "framing: domain survives");
			checkEq(got[0].header.code, toInt(WindowCode::AcquireFrame), "framing: code survives");
			check(BytesView(got[0].payload) == BytesView(incompressible),
					"framing: payload round-trips");
		}
	}

	{
		// Compressible payload: the encoder takes the LZ4 branch, so this asserts the decoder's
		// rawSize path (the one that was reading a byte-swapped length before).
		Bytes wire;
		encodeFrame(wire, BytesView(), MessageType::Client, Domain::Data,
				toInt(DataCode::Packet), 7, BytesView(compressible));
		check(wire.size() < compressible.size(), "framing: a run of zeros is compressed");

		MessageReader r;
		check(r.append(BytesView(wire), BytesView()), "framing: append accepts a compressed frame");
		auto got = drain(r);
		check(got.size() == 1 && BytesView(got[0].payload) == BytesView(compressible),
				"framing: compressed payload round-trips");
	}

	{
		// One byte at a time: nothing may surface until the frame is complete, and then exactly one
		// message must. This is the case a socket produces whenever a frame spans two datagrams.
		Bytes wire;
		encodeFrame(wire, BytesView(), MessageType::Server, Domain::Global,
				toInt(GlobalCode::Ping), 1, BytesView(incompressible));

		MessageReader r;
		bool earlyMessage = false;
		for (size_t i = 0; i + 1 < wire.size(); ++i) {
			r.append(BytesView(wire.data() + i, 1), BytesView());
			if (r.hasPending()) {
				earlyMessage = true;
			}
		}
		check(!earlyMessage, "framing: a partial frame produces nothing");
		check(r.hasPartialMessage(), "framing: the partial frame is held");
		r.append(BytesView(wire.data() + wire.size() - 1, 1), BytesView());
		auto got = drain(r);
		check(got.size() == 1 && BytesView(got[0].payload) == BytesView(incompressible),
				"framing: the frame surfaces on its last byte");
		check(!r.hasPartialMessage(), "framing: nothing is left buffered");
	}

	{
		// Several frames arriving in one read, which is what a busy connection actually looks like.
		Bytes wire;
		encodeFrame(wire, BytesView(), MessageType::Server, Domain::Window, 1, 100,
				BytesView(incompressible));
		encodeFrame(wire, BytesView(), MessageType::Server, Domain::Window, 2, 101, BytesView());
		encodeFrame(wire, BytesView(), MessageType::Server, Domain::Window, 3, 102,
				BytesView(compressible));

		MessageReader r;
		check(r.append(BytesView(wire), BytesView()), "framing: append accepts three coalesced");
		auto got = drain(r);
		check(got.size() == 3, "framing: three coalesced frames, three messages");
		if (got.size() == 3) {
			check(got[0].header.serial == 100 && got[1].header.serial == 101
							&& got[2].header.serial == 102,
					"framing: coalesced frames keep their order");
			check(got[1].payload.empty(), "framing: an empty payload stays empty");
		}
	}

	{
		// Deferred dispatch: a handler that cannot consume a message yet must get it again later,
		// and must not lose the ones after it. This is what lets a reply arrive before its requester
		// is ready.
		Bytes wire;
		encodeFrame(wire, BytesView(), MessageType::ServerReply, Domain::Window, 1, 200,
				BytesView());
		encodeFrame(wire, BytesView(), MessageType::ServerReply, Domain::Window, 1, 201,
				BytesView());

		MessageReader r;
		r.append(BytesView(wire), BytesView());

		// Refuse serial 200, take 201.
		Vector<uint32_t> firstPass;
		r.dispatch([&](const MessageHeader &h, BytesView) {
			firstPass.emplace_back(h.serial);
			return h.serial != 200;
		});
		bool offered200 = false, offered201 = false;
		for (auto serial : firstPass) {
			offered200 = offered200 || serial == 200;
			offered201 = offered201 || serial == 201;
		}
		check(offered200 && offered201, "framing: both messages are offered on the first pass");
		// dispatch() keeps retrying deferred messages inside one call until a pass consumes nothing,
		// so the refused one is offered a second time before it returns -- that retry is what lets a
		// reply and the message it depends on resolve in a single pump whatever order they arrived in.
		check(firstPass.size() == 3, "framing: the refused message is retried within the dispatch");
		check(r.pendingCount() == 1, "framing: the refused message stays queued");

		auto second = drain(r);
		check(second.size() == 1 && second[0].header.serial == 200,
				"framing: the refused message comes back");
		check(!r.hasPending(), "framing: nothing is left pending");
	}

	{
		// Dictionary: encoded with one, it decodes only with the same one. Without it the frame is a
		// framing violation, not a silent empty message.
		Bytes dict = makePayload(1'024, 3);
		Bytes wire;
		encodeFrame(wire, BytesView(dict), MessageType::Server, Domain::Window, 4, 300,
				BytesView(compressible));

		MessageReader ok;
		check(ok.append(BytesView(wire), BytesView(dict)), "framing: dict frame decodes with dict");
		auto got = drain(ok);
		check(got.size() == 1 && BytesView(got[0].payload) == BytesView(compressible),
				"framing: dict payload round-trips");

		MessageReader missing;
		check(!missing.append(BytesView(wire), BytesView()),
				"framing: dict frame without the dict is rejected");
	}

	{
		// A header claiming more than the frame cap must be refused before anything is allocated --
		// this is the whole defence against a hostile or garbled peer.
		Bytes wire;
		wire.resize(sizeof(MessageHeader));
		auto h = (MessageHeader *)wire.data();
		h->msgtype = toInt(MessageType::Server);
		h->msgflags = 0;
		h->domain = toInt(Domain::Window);
		h->code = 1;
		h->serial = sprt::byteorder::HostToNetwork(uint32_t(1));
		h->size = sprt::byteorder::HostToNetwork(uint32_t(512u * 1'024 * 1'024)); // 512 MiB

		MessageReader r;
		check(!r.append(BytesView(wire), BytesView()), "framing: an oversized frame is refused");
	}
}

} // namespace stappler::xenolith::remote
