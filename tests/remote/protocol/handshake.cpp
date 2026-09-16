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

// The setup handshake with both ends stepped on one thread: the step-at-a-time machines make the
// loopback possible, and the clock is passed in, so deadlines need no real waiting.

#include "SPCommon.h"

#include "XLRemoteTransport.h"
#include "XLRemoteProtocol.h"

#include "../tests.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;

namespace {

struct HandshakePair {
	Rc<TransportListener> listener;
	Rc<TransportConnection> client;
	Rc<TransportConnection> server;

	bool open(StringView address) {
		listener = TransportRegistry::listen(Address::parse(address));
		if (!listener) {
			return false;
		}
		client = connect();
		server = accept();
		return client && server;
	}

	Rc<TransportConnection> connect() {
		TransportClientConfig cfg;
		cfg.shmControlCapacity = 4'096;
		cfg.shmBulkCapacity = 4'096;
		return TransportRegistry::connect(Address::parse(address()), cfg);
	}

	Rc<TransportConnection> accept() {
		Rc<TransportConnection> ret;
		listener->handleEvents([&](Rc<TransportConnection> &&c) { ret = sp::move(c); });
		return ret;
	}

	String address() const { return _address; }

	String _address;
};

static bool isTerminal(ClientHandshake::State s) {
	return s == ClientHandshake::State::Done || s == ClientHandshake::State::Failed;
}

static bool isTerminal(ServerHandshake::State s) {
	return s == ServerHandshake::State::Done || s == ServerHandshake::State::Failed;
}

// Step both ends until neither moves; `decide` answers the hello once it is in.
static void runPair(TransportConnection &clientConn, ClientHandshake &client,
		TransportConnection &serverConn, ServerHandshake &server,
		const Callback<void(ServerHandshake &)> &decide) {
	for (uint32_t i = 0; i < 64; ++i) {
		client.step(clientConn, 1);
		auto s = server.step(serverConn, 1);
		if (s == ServerHandshake::State::HelloReceived) {
			decide(server);
			server.step(serverConn, 1);
		}
		if (isTerminal(client.getState()) && isTerminal(server.getState())) {
			return;
		}
	}
}

static Bytes s_key(kBearerKeySize, uint8_t(0x42));

static BytesView bytesOf(StringView str) {
	return BytesView(reinterpret_cast<const uint8_t *>(str.data()), str.size());
}

static void runCases(StringView scheme, StringView name) {
	auto address = toString(scheme, name);
	auto tag = [&](StringView what) { return toString("handshake (", scheme, "): ", what); };

	HandshakePair pair;
	pair._address = address;
	if (!pair.open(address)) {
		check(false, tag("pair opened"));
		return;
	}

	{
		// The server's dictionary has priority and reaches the client.
		ClientHandshake client;
		ServerHandshake server;
		client.begin(s_key, BytesView(), 1'000);
		server.begin(1'000);
		runPair(*pair.client, client, *pair.server, server, [](ServerHandshake &hs) {
			hs.reply(hs.negotiate(s_key, bytesOf("server-dict"), true), bytesOf("server-dict"));
		});
		check(client.getState() == ClientHandshake::State::Done
						&& client.getResult() == GlobalError::Ok
						&& server.getState() == ServerHandshake::State::Done,
				tag("accepted with the right key"));
		check(client.getServerHello().dictSource == toInt(DictSource::Server)
						&& StringView(reinterpret_cast<const char *>(
											  client.getServerHello().dict.data()),
								   client.getServerHello().dict.size())
								== "server-dict",
				tag("the server dictionary reaches the client"));
	}

	{
		// With no dictionary of its own the server takes the client's suggestion.
		auto client = pair.connect();
		auto server = pair.accept();
		ClientHandshake ch;
		ServerHandshake sh;
		ch.begin(s_key, bytesOf("client-dict"), 1'000);
		sh.begin(1'000);
		runPair(*client, ch, *server, sh, [](ServerHandshake &hs) {
			hs.reply(hs.negotiate(s_key, BytesView(), true), BytesView());
		});
		check(ch.getResult() == GlobalError::Ok
						&& StringView(reinterpret_cast<const char *>(sh.getNegotiatedDict().data()),
								   sh.getNegotiatedDict().size())
								== "client-dict",
				tag("a client suggestion is adopted"));
	}

	{
		auto client = pair.connect();
		auto server = pair.accept();
		ClientHandshake ch;
		ServerHandshake sh;
		Bytes wrongKey(kBearerKeySize, uint8_t(0x13));
		ch.begin(wrongKey, BytesView(), 1'000);
		sh.begin(1'000);
		runPair(*client, ch, *server, sh, [](ServerHandshake &hs) {
			hs.reply(hs.negotiate(s_key, BytesView(), true), BytesView());
		});
		check(ch.getState() == ClientHandshake::State::Done
						&& ch.getResult() == GlobalError::AuthFailed,
				tag("a wrong key is refused with AuthFailed"));
	}

	{
		// Turned away before the hello is read, the way a busy server answers.
		auto client = pair.connect();
		auto server = pair.accept();
		ClientHandshake ch;
		ServerHandshake sh;
		sh.begin(1'000);
		sh.reply(GlobalError::Busy, BytesView());
		ch.begin(s_key, BytesView(), 1'000);
		runPair(*client, ch, *server, sh, [](ServerHandshake &) { });
		check(ch.getResult() == GlobalError::Busy && sh.getReplied() == GlobalError::Busy,
				tag("a refusal before the hello reaches the client"));
	}

	{
		auto client = pair.connect();
		auto server = pair.accept();
		ServerHandshake sh;
		sh.begin(100);
		auto early = sh.step(*server, 50);
		auto late = sh.step(*server, 100);
		check(early == ServerHandshake::State::ReadingHello
						&& late == ServerHandshake::State::Failed,
				tag("a silent peer fails at the deadline, not before"));
	}

	{
		// A frame that is not our protocol.
		auto client = pair.connect();
		auto server = pair.accept();
		Bytes junk;
		WireWriter w(junk);
		w.writeU8(uint8_t(toInt(MessageType::Client)));
		w.writeU8(0);
		w.writeU8(uint8_t(toInt(Domain::Global)));
		w.writeU8(uint8_t(toInt(GlobalCode::ClientHello)));
		w.writeU32(0);
		w.writeU32(12);
		w.writeZero(12);
		size_t written = 0;
		client->getStream(StreamClass::Control)->write(junk, written);
		ServerHandshake sh;
		sh.begin(1'000);
		auto state = sh.step(*server, 1);
		check(state == ServerHandshake::State::HelloReceived
						&& sh.negotiate(s_key, BytesView(), true) == GlobalError::BadProtocol,
				tag("a foreign hello negotiates to BadProtocol"));
	}

	{
		// Two at once: the silent one does not hold back the other.
		auto silentClient = pair.connect();
		auto silentServer = pair.accept();
		auto client = pair.connect();
		auto server = pair.accept();
		ServerHandshake silent;
		silent.begin(1'000);
		ClientHandshake ch;
		ServerHandshake sh;
		ch.begin(s_key, BytesView(), 1'000);
		sh.begin(1'000);
		for (uint32_t i = 0; i < 64 && !isTerminal(sh.getState()); ++i) {
			silent.step(*silentServer, 1);
			ch.step(*client, 1);
			if (sh.step(*server, 1) == ServerHandshake::State::HelloReceived) {
				sh.reply(sh.negotiate(s_key, BytesView(), true), BytesView());
			}
		}
		ch.step(*client, 1);
		check(silent.getState() == ServerHandshake::State::ReadingHello
						&& ch.getResult() == GlobalError::Ok,
				tag("a silent handshake does not hold back another"));
	}

	{
		// Bytes the client sends right after its hello belong to the connection.
		auto client = pair.connect();
		auto server = pair.accept();
		ClientHandshake ch;
		ServerHandshake sh;
		ch.begin(s_key, BytesView(), 1'000);
		ch.step(*client, 1);
		size_t written = 0;
		client->getStream(StreamClass::Control)->write(bytesOf("tail"), written);
		sh.begin(1'000);
		runPair(*client, ch, *server, sh, [](ServerHandshake &hs) {
			hs.reply(hs.negotiate(s_key, BytesView(), true), BytesView());
		});
		uint8_t buf[16];
		size_t got = 0;
		server->getStream(StreamClass::Control)->read(buf, sizeof(buf), got);
		check(ch.getResult() == GlobalError::Ok
						&& StringView(reinterpret_cast<const char *>(buf), got) == "tail",
				tag("the handshake leaves the bytes after the hello in the stream"));
	}

	pair.listener->close();
}

} // namespace

void performHandshakeTests() {
	sprt::cout << "--- remote setup handshake (one thread) ---\n";
	initializeTransports();
	runCases("mem:", "handshake-test");
#if SPRT_LINUX || SPRT_EMBOX_ANY
	runCases("shm:@", "handshake-test");
#endif
}

} // namespace stappler::xenolith::remote
