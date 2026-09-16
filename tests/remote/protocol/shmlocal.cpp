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

// The `shm:` transport with the in-process provider (`shm:@name`): no files, no second process,
// so the same run proves the transport on Linux and in the Embox kernel, where a server and a
// client are two threads.

#include "SPCommon.h"

#include "XLRemoteTransport.h"
#include "XLRemoteProtocol.h"

#include "SPPlatform.h"

#include "../tests.h"

#include <sprt/cxx/thread>
#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

#if SPRT_LINUX || SPRT_EMBOX_ANY

namespace {

static uint64_t localNowUs() { return sp::platform::clock(ClockType::Monotonic); }

static TransportClientConfig localClientConfig() {
	TransportClientConfig cfg;
	cfg.shmControlCapacity = 4'096;
	cfg.shmBulkCapacity = 4'096;
	return cfg;
}

static Rc<TransportConnection> localAccept(TransportListener *listener, uint64_t timeoutUs) {
	Rc<TransportConnection> ret;
	auto deadline = localNowUs() + timeoutUs;
	while (!ret && localNowUs() < deadline) {
		listener->handleEvents([&](Rc<TransportConnection> &&c) { ret = sp::move(c); });
		if (!ret) {
			sp::platform::sleep(1'000);
		}
	}
	return ret;
}

static bool localWrite(TransportConnection *conn, StreamClass c, BytesView data,
		uint64_t timeoutUs) {
	auto stream = conn->getStream(c);
	auto deadline = localNowUs() + timeoutUs;
	size_t total = 0;
	while (total < data.size()) {
		size_t w = 0;
		if (stream->write(data.sub(total), w) != Status::Ok) {
			return false;
		}
		total += w;
		if (w == 0) {
			if (localNowUs() >= deadline) {
				return false;
			}
			sp::platform::sleep(1'000);
		}
	}
	return true;
}

static Bytes localRead(TransportConnection *conn, StreamClass c, size_t size, uint64_t timeoutUs) {
	auto stream = conn->getStream(c);
	auto deadline = localNowUs() + timeoutUs;
	Bytes out;
	uint8_t buf[512];
	while (out.size() < size && localNowUs() < deadline) {
		size_t got = 0;
		if (stream->read(buf, sprt::min(sizeof(buf), size - out.size()), got) != Status::Ok) {
			break;
		}
		if (got > 0) {
			out.insert(out.end(), buf, buf + got);
		} else {
			conn->handleEvents();
			sp::platform::sleep(1'000);
		}
	}
	return out;
}

static BytesView localBytes(StringView str) {
	return BytesView(reinterpret_cast<const uint8_t *>(str.data()), str.size());
}

static bool localEquals(const Bytes &b, StringView str) {
	return b.size() == str.size() && __sprt_memcmp(b.data(), str.data(), b.size()) == 0;
}

} // namespace

void performShmLocalTests() {
	sprt::cout << "--- remote transport (shm:@, in-process) ---\n";

	initializeTransports();
	check(TransportRegistry::has(AddressScheme::Shm), "shm local: registered");

	auto addr = Address::parse("shm:@remotetest");

	check(TransportRegistry::connect(Address::parse("shm:@missing"), localClientConfig())
					== nullptr,
			"shm local: connecting to an unbound name fails");

	auto listener = TransportRegistry::listen(addr);
	check(listener && listener->isOpen(), "shm local: listener binds");
	if (!listener) {
		return;
	}
	check(TransportRegistry::listen(addr) == nullptr, "shm local: a name can only be bound once");

	auto client = TransportRegistry::connect(addr, localClientConfig());
	auto server = localAccept(listener, 1'000'000);
	check(client && server, "shm local: connect and accept");
	if (!client || !server) {
		return;
	}

	check(client->hasCaps(TransportCaps::MultiStream | TransportCaps::PeerAuthenticated)
					&& server->hasCaps(
							TransportCaps::MultiStream | TransportCaps::PeerAuthenticated),
			"shm local: both halves are multi-stream and authenticated");
	check(localWrite(client, StreamClass::Control, localBytes("to server"), 100'000)
					&& localEquals(localRead(server, StreamClass::Control, 9, 100'000),
							"to server"),
			"shm local: client to server on Control");
	check(localWrite(server, StreamClass::Bulk, localBytes("to client"), 100'000)
					&& localEquals(localRead(client, StreamClass::Bulk, 9, 100'000), "to client"),
			"shm local: server to client on Bulk");

	{
		// What an AppThread registers; on Embox this is the reactor's spinWait watching the words.
		auto looper = sprt::dispatch::Looper::acquire();
		auto pump = [&](const Callback<bool()> &done) {
			auto deadline = localNowUs() + 2'000'000;
			while (!done() && localNowUs() < deadline) {
				looper->run(sprt::dispatch::TimeInterval::milliseconds(10));
			}
		};

		uint32_t listenWakes = 0;
		auto lw = listener->getWaitAddress();
		auto lh = lw.address ? looper->waitOnAddress(lw.address, lw.value,
									   [&](uint32_t) -> Status {
			++listenWakes;
			return Status::Ok;
		})
							 : nullptr;
		auto c = TransportRegistry::connect(addr, localClientConfig());
		pump([&] { return listenWakes > 0; });
		check(lh && listenWakes > 0, "shm local: a connect wakes the listener's looper");

		auto s = localAccept(listener, 1'000'000);
		uint32_t connWakes = 0;
		auto sw = s ? s->getWaitAddress() : TransportWaitAddress();
		auto sh = sw.address ? looper->waitOnAddress(sw.address, sw.value,
									   [&](uint32_t) -> Status {
			++connWakes;
			return Status::Ok;
		})
							 : nullptr;
		if (c) {
			localWrite(c, StreamClass::Control, localBytes("wake"), 100'000);
		}
		pump([&] { return connWakes > 0; });
		check(sh && connWakes > 0, "shm local: a peer write wakes the connection's looper");

		if (sh) {
			sh->cancel();
		}
		if (lh) {
			lh->cancel();
		}
	}

	{
		static Bytes s_key(kBearerKeySize, uint8_t(0x33));
		GlobalError serverResult = GlobalError::NetworkBackend;
		bool serverExchanged = false;
		sprt::thread serverThread([&] {
			auto s = localAccept(listener, 2'000'000);
			if (!s) {
				return;
			}
			Bytes dict;
			serverResult = serverHandshake(*s, s_key, BytesView(), dict, localNowUs() + 2'000'000,
					!s->hasCaps(TransportCaps::PeerAuthenticated));
			serverExchanged = localEquals(localRead(s, StreamClass::Bulk, 4, 2'000'000), "ping")
					&& localWrite(s, StreamClass::Bulk, localBytes("pong"), 1'000'000);
		});

		GlobalError clientResult = GlobalError::NetworkBackend;
		bool clientExchanged = false;
		if (auto c = TransportRegistry::connect(addr, localClientConfig())) {
			clientResult = clientHandshake(*c, s_key, BytesView(), localNowUs() + 2'000'000,
					[](const ServerHello &) { });
			clientExchanged = localWrite(c, StreamClass::Bulk, localBytes("ping"), 1'000'000)
					&& localEquals(localRead(c, StreamClass::Bulk, 4, 2'000'000), "pong");
		}
		serverThread.join();
		check(clientResult == GlobalError::Ok && serverResult == GlobalError::Ok,
				"shm local: handshake between two threads");
		check(clientExchanged && serverExchanged, "shm local: a message each way after it");
	}

	{
		static constexpr size_t total = 1'024 * 1'024;
		auto c = TransportRegistry::connect(addr, localClientConfig());
		auto s = localAccept(listener, 1'000'000);
		bool ordered = c && s;
		size_t received = 0;
		sprt::thread reader([&] {
			uint8_t buf[1'000];
			uint32_t expected = 0;
			auto deadline = localNowUs() + 20'000'000;
			while (ordered && received < total && localNowUs() < deadline) {
				size_t got = 0;
				if (s->getStream(StreamClass::Bulk)->read(buf, sizeof(buf), got) != Status::Ok) {
					break;
				}
				if (got == 0) {
					sp::platform::sleep(1'000);
					continue;
				}
				for (size_t i = 0; i < got; ++i) {
					if (buf[i] != uint8_t(expected++ * 7)) {
						ordered = false;
					}
				}
				received += got;
			}
		});
		Bytes chunk(3'000);
		for (size_t sent = 0; c && s && sent < total;) {
			auto n = sprt::min(chunk.size(), total - sent);
			for (size_t i = 0; i < n; ++i) { chunk[i] = uint8_t((sent + i) * 7); }
			if (!localWrite(c, StreamClass::Bulk, BytesView(chunk.data(), n), 20'000'000)) {
				break;
			}
			sent += n;
		}
		reader.join();
		checkEq(received, total, "shm local: 1 MiB through a 4 KiB ring between threads");
		check(ordered, "shm local: in order");
	}

	{
		auto c = TransportRegistry::connect(addr, localClientConfig());
		auto s = localAccept(listener, 1'000'000);
		if (c && s) {
			localWrite(c, StreamClass::Control, localBytes("tail"), 100'000);
			c->close();
			check(!s->isClosed()
							&& localEquals(localRead(s, StreamClass::Control, 4, 100'000), "tail")
							&& s->isClosed(),
					"shm local: bytes sent before close arrive, then the stream closes");
		} else {
			check(false, "shm local: connect for the close check");
		}
	}

	listener->close();
	check(TransportRegistry::connect(addr, localClientConfig()) == nullptr,
			"shm local: a closed listener releases its name");
}

#else

void performShmLocalTests() { sprt::cout << "--- remote transport (shm:@) --- skipped\n"; }

#endif

} // namespace stappler::xenolith::remote
