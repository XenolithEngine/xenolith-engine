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

// Tests for dispatch::Looper::listenSocket / connectSocket: SocketAddress
// parsing, a loopback-TCP echo round-trip (listen on an ephemeral port,
// connect on the same looper, exchange + EOF), a large transfer that forces
// write backpressure, AF_UNIX path sockets with stale-file rebinding, and the
// error paths (connect refused, double bind). Both endpoints run on ONE looper
// thread - the API's target usage shape (the scene inspector).

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

#include <sprt/cxx/thread>
#include <sprt/c/__sprt_unistd.h>

namespace sprt {

// The whole suite drives real sockets, which the wasm sandbox does not provide;
// performSocketTests() reports a SKIP there instead, so none of the machinery below
// is built.
#if !SPRT_WASM

namespace {

namespace dispatch = sprt::dispatch;

using dispatch::SocketAddress;
using dispatch::StreamHandle;

static bool report(bool ok, StringView name, int &failed) {
	sprt::cout << (ok ? "PASS  " : "FAIL  ") << name << "\n";
	if (!ok) {
		++failed;
	}
	return ok;
}

// Drive the loop until `done` or a wall-clock budget expires (never hangs a CI
// run on a lost event).
static bool drive(dispatch::Looper *looper, const bool &done, uint32_t maxMs = 5'000) {
	uint32_t waited = 0;
	while (!done && waited < maxMs) {
		looper->wait(dispatch::TimeInterval::milliseconds(25));
		waited += 25;
	}
	return done;
}

static int runSocketSuite(dispatch::Looper *looper) {
	int failed = 0;

	// 1. SocketAddress::parse forms
	{
		auto u = SocketAddress::parse("unix:/tmp/test.sock");
		auto a = SocketAddress::parse("unix:@abstract-name");
		auto hp = SocketAddress::parse("127.0.0.1:4490");
		auto p = SocketAddress::parse(":8080");
		auto v6 = SocketAddress::parse("[::1]:4490");
		auto v6full = SocketAddress::parse("[2001:db8::ff00:42:8329]:80");
		auto v6mapped = SocketAddress::parse("[::ffff:127.0.0.1]:1");
		auto bad1 = SocketAddress::parse("no-colon");
		auto bad2 = SocketAddress::parse("host.example:80"); // DNS names not supported yet
		auto bad3 = SocketAddress::parse("1.2.3.4:99999");
		auto bad4 = SocketAddress::parse("unix:");
		auto bad5 = SocketAddress::parse("[::1]"); // v6 literal requires a port
		auto bad6 = SocketAddress::parse("[:z:]:1");
		auto bad7 = SocketAddress::parse("[1::2::3]:1"); // second '::'
		report(u.isValid() && u.family == SocketAddress::Family::Unix
						&& u.path == "/tmp/test.sock" //
						&& a.isValid() && a.path == "@abstract-name" //
						&& hp.isValid() && hp.family == SocketAddress::Family::IPv4
						&& hp.host == "127.0.0.1" && hp.port == 4'490 //
						&& p.isValid() && p.host.empty() && p.port == 8'080 //
						&& v6.isValid() && v6.family == SocketAddress::Family::IPv6
						&& v6.host == "::1" && v6.port == 4'490 //
						&& v6full.isValid() && v6mapped.isValid() //
						&& !bad1.isValid() && !bad2.isValid() && !bad3.isValid() && !bad4.isValid()
						&& !bad5.isValid() && !bad6.isValid() && !bad7.isValid()
						&& u.description() == "unix:/tmp/test.sock"
						&& hp.description() == "127.0.0.1:4490" && p.description() == "127.0.0.1:8080"
						&& v6.description() == "[::1]:4490",
				"SocketAddress::parse / description", failed);
	}

	// 2. loopback TCP echo round-trip with EOF propagation
	{
		Rc<StreamHandle> serverStream;
		bool clientEof = false;
		bool serverClosed = false, clientClosed = false;
		Status connectSt = Status::Pending;
		char echoBuf[64] = {0};
		size_t echoLen = 0;

		auto listener = looper->listenSocket(SocketAddress::parse(":0"),
				[&](Rc<StreamHandle> &&stream) {
			serverStream = sprt::move(stream);
			serverStream->setCloseCallback([&](Status) { serverClosed = true; });
			auto s = serverStream.get();
			serverStream->read([&, s](BytesView d) {
				if (d.empty()) {
					return Status::Ok;
				}
				s->write(d); // echo
				s->shutdownWrite(); // reply complete -> peer sees EOF
				return Status::Done; // stop reading
			});
		});

		bool ok = listener != nullptr;
		uint16_t port = ok ? listener->getAddress().port : 0;
		ok = ok && port != 0;

		Rc<StreamHandle> client;
		if (ok) {
			client = looper->connectSocket(
					SocketAddress::parse(dispatch::toString("127.0.0.1:", port)),
					[&](StreamHandle *, Status s) { connectSt = s; });
			ok = client != nullptr;
		}
		if (ok) {
			client->setCloseCallback([&](Status) { clientClosed = true; });
			client->read([&](BytesView d) {
				if (d.empty()) {
					clientEof = true;
				} else if (echoLen + d.size() <= sizeof(echoBuf)) {
					sprt::memcpy(echoBuf + echoLen, d.data(), d.size());
					echoLen += d.size();
				}
				return Status::Ok;
			});
			client->write(BytesView(reinterpret_cast<const uint8_t *>("ping"), 4));
			client->shutdownWrite();

			drive(looper, clientEof);
			drive(looper, serverClosed);
			drive(looper, clientClosed);
		}

		report(ok && connectSt == Status::Ok && clientEof && echoLen == 4
						&& sprt::memcmp(echoBuf, "ping", 4) == 0 && serverClosed && clientClosed,
				"TCP loopback echo + EOF round-trip", failed);

		if (listener) {
			listener->cancel();
		}
	}

	// 3. large transfer (1 MiB server -> client) forces partial writes / Out interest
	{
		static constexpr size_t TotalSize = 1'024 * 1'024;
		static uint8_t s_pattern[TotalSize];
		for (size_t i = 0; i < TotalSize; ++i) { s_pattern[i] = uint8_t(i * 131 + 17); }

		Rc<StreamHandle> serverStream;
		bool clientEof = false;
		size_t received = 0;
		bool corrupt = false;

		auto listener = looper->listenSocket(SocketAddress::parse(":0"),
				[&](Rc<StreamHandle> &&stream) {
			serverStream = sprt::move(stream);
			// one bulk write: exceeds any socket buffer -> exercises outBuf + Out
			serverStream->write(BytesView(s_pattern, TotalSize));
			serverStream->shutdownWrite();
		});

		bool ok = listener != nullptr && listener->getAddress().port != 0;
		Rc<StreamHandle> client;
		if (ok) {
			client = looper->connectSocket(SocketAddress::parse(
												   dispatch::toString("127.0.0.1:",
														   listener->getAddress().port)),
					[&](StreamHandle *, Status) { });
			ok = client != nullptr;
		}
		if (ok) {
			client->read([&](BytesView d) {
				if (d.empty()) {
					clientEof = true;
					return Status::Ok;
				}
				for (size_t i = 0; i < d.size(); ++i) {
					if (d[i] != uint8_t((received + i) * 131 + 17)) {
						corrupt = true;
						break;
					}
				}
				received += d.size();
				return Status::Ok;
			});
			client->shutdownWrite(); // nothing to send
			drive(looper, clientEof, 15'000);
		}

		report(ok && clientEof && received == TotalSize && !corrupt,
				"large transfer (1 MiB) with backpressure", failed);

		if (listener) {
			listener->cancel();
		}
	}

	// 4. AF_UNIX path echo + rebind over a stale socket file
	{
		const char *path = "sprt_socket_test.sock";
		::__sprt_unlink(path);

		auto addr = SocketAddress::parse(dispatch::toString("unix:", path));

		bool ok = true;
		bool supported = true;
		for (int round = 0; ok && supported && round < 2; ++round) {
			// round 0 binds fresh; round 1 rebinds over the file the cancelled
			// listener may have left behind (stale-file handling)
			Rc<StreamHandle> serverStream;
			bool clientEof = false;
			char buf[8] = {0};
			size_t len = 0;

			auto listener = looper->listenSocket(addr, [&](Rc<StreamHandle> &&stream) {
				serverStream = sprt::move(stream);
				auto s = serverStream.get();
				serverStream->read([&, s](BytesView d) {
					if (d.empty()) {
						return Status::Ok;
					}
					s->write(d);
					s->shutdownWrite();
					return Status::Done;
				});
			});
#if SPRT_WINDOWS
			// AF_UNIX exists on Windows 10 1803+ (afunix.h) but not under wine -
			// treat a failed first bind as "no AF_UNIX here", not as a test failure
			if (!listener && round == 0) {
				supported = false;
				break;
			}
#endif
			ok = listener != nullptr;

			Rc<StreamHandle> client;
			if (ok) {
				client = looper->connectSocket(addr, [&](StreamHandle *, Status) { });
				ok = client != nullptr;
			}
			if (ok) {
				client->read([&](BytesView d) {
					if (d.empty()) {
						clientEof = true;
					} else if (len + d.size() <= sizeof(buf)) {
						sprt::memcpy(buf + len, d.data(), d.size());
						len += d.size();
					}
					return Status::Ok;
				});
				client->write(BytesView(reinterpret_cast<const uint8_t *>("unix"), 4));
				client->shutdownWrite();
				ok = drive(looper, clientEof) && len == 4 && sprt::memcmp(buf, "unix", 4) == 0;
			}
			if (listener) {
				listener->cancel();
				// let the cancel run so round 1 does not race the teardown
				looper->wait(dispatch::TimeInterval::milliseconds(25));
			}
		}
		if (!supported) {
			sprt::cout << "SKIP  AF_UNIX path echo + stale-file rebind (AF_UNIX unsupported)\n";
			sprt::cout << "SKIP  AF_UNIX cancel unlinks the socket path (AF_UNIX unsupported)\n";
		} else {
			report(ok, "AF_UNIX path echo + stale-file rebind", failed);
			// cancel() owns the bound path: a cancelled listener must not leave the socket file
			// behind (the loop above already drove the cancel to completion)
			report(::__sprt_access(path, __SPRT_F_OK) != 0,
					"AF_UNIX cancel unlinks the socket path", failed);
		}
		::__sprt_unlink(path);
	}

	// 4b. IPv6 loopback echo ([::1]:0 -> ephemeral port)
	{
		Rc<StreamHandle> serverStream;
		bool clientEof = false;
		char buf[8] = {0};
		size_t len = 0;
		bool supported = true;

		auto listener = looper->listenSocket(SocketAddress::parse("[::1]:0"),
				[&](Rc<StreamHandle> &&stream) {
			serverStream = sprt::move(stream);
			auto s = serverStream.get();
			serverStream->read([&, s](BytesView d) {
				if (d.empty()) {
					return Status::Ok;
				}
				s->write(d);
				s->shutdownWrite();
				return Status::Done;
			});
		});
		if (!listener) {
			supported = false; // IPv6 disabled on this host
		}

		bool ok = supported;
		Rc<StreamHandle> client;
		if (ok) {
			ok = listener->getAddress().port != 0;
			client = looper->connectSocket(
					SocketAddress::parse(
							dispatch::toString("[::1]:", listener->getAddress().port)),
					[&](StreamHandle *, Status) { });
			ok = ok && client != nullptr;
		}
		if (ok) {
			client->read([&](BytesView d) {
				if (d.empty()) {
					clientEof = true;
				} else if (len + d.size() <= sizeof(buf)) {
					sprt::memcpy(buf + len, d.data(), d.size());
					len += d.size();
				}
				return Status::Ok;
			});
			client->write(BytesView(reinterpret_cast<const uint8_t *>("six6"), 4));
			client->shutdownWrite();
			ok = drive(looper, clientEof) && len == 4 && sprt::memcmp(buf, "six6", 4) == 0;
		}
		if (!supported) {
			sprt::cout << "SKIP  IPv6 loopback echo (IPv6 unsupported)\n";
		} else {
			report(ok, "IPv6 loopback echo", failed);
		}
		if (listener) {
			listener->cancel();
		}
	}

	// 5. connect refused reports an error exactly once
	{
		// grab an ephemeral port, then close the listener - connecting to it
		// afterwards must be refused
		auto probe = looper->listenSocket(SocketAddress::parse(":0"),
				[](Rc<StreamHandle> &&stream) { stream->cancel(); });
		bool ok = probe != nullptr;
		uint16_t port = ok ? probe->getAddress().port : 0;
		if (probe) {
			probe->cancel();
			looper->wait(dispatch::TimeInterval::milliseconds(25));
		}

		if (ok) {
			int fired = 0;
			Status connectSt = Status::Pending;
			bool done = false;
			auto client = looper->connectSocket(
					SocketAddress::parse(dispatch::toString("127.0.0.1:", port)),
					[&](StreamHandle *, Status s) {
				++fired;
				connectSt = s;
				done = true;
			});
			// the failure may be reported synchronously (client == nullptr) or
			// through the completion once the loop runs
			if (client) {
				drive(looper, done);
			}
			ok = (fired == 1) && !isSuccessful(connectSt) && connectSt != Status::Pending;
		}
		report(ok, "connect refused -> single error completion", failed);
	}

	// 6. double bind on an active port fails cleanly
	{
		auto first = looper->listenSocket(SocketAddress::parse(":0"),
				[](Rc<StreamHandle> &&stream) { stream->cancel(); });
		bool ok = first != nullptr && first->getAddress().port != 0;
		if (ok) {
			auto second = looper->listenSocket(
					SocketAddress::parse(dispatch::toString("127.0.0.1:",
							first->getAddress().port)),
					[](Rc<StreamHandle> &&stream) { stream->cancel(); });
			ok = second == nullptr;
		}
		if (first) {
			first->cancel();
		}
		report(ok, "double bind on active port fails (nullptr)", failed);
	}

	return failed;
}

} // namespace

#endif // !SPRT_WASM

void performSocketTests() {
	sprt::cout << "\n== runtime socket tests ==\n";

#if SPRT_WASM
	// There are no sockets inside the wasm sandbox: the module has no fds and no
	// syscall to bind or connect one, so the wasm queue leaves its listen/connect
	// hooks null (SPEvent-wasm.cc) and every handle below would be nullptr. Reaching
	// the network from wasm means a JS-side transport (WebSocket/WebTransport over
	// memory BIOs), which is a different API than the one under test here.
	sprt::cout << "SKIP  socket tests (no socket backend on wasm)\n";
#else
	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		sprt::cout << "FAIL  could not acquire looper\n";
		return;
	}

	sprt::cout << "[default engine = " << uint32_t(toInt(looper->getQueue()->getEngine())) << "]\n";
	int failed = runSocketSuite(looper);

#if SPRT_LINUX
	// Re-run against a forced-epoll looper on a worker thread, since only one
	// looper may exist per thread (the default on Linux is io_uring).
	{
		int epollFailed = -1;
		sprt::thread th([&] {
			auto l = dispatch::Looper::acquire(dispatch::LooperInfo{
				.name = StringView("SocketEpoll"),
				.workersCount = 0,
				.engineMask = dispatch::QueueEngine::EPoll,
			});
			if (!l) {
				epollFailed = 1'000;
				return;
			}
			sprt::cout << "[epoll engine = " << uint32_t(toInt(l->getQueue()->getEngine()))
					   << "]\n";
			epollFailed = runSocketSuite(l);
		});
		th.join();
		if (epollFailed > 0) {
			failed += epollFailed;
		}
	}
#endif

	sprt::cout << "socket tests: " << (failed == 0 ? "ALL PASS" : "FAILURES") << " (" << failed
			   << " failed)\n";
#endif
}

} // namespace sprt
