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

// The `shm:` transport: rendezvous, slots, identity, both stream classes, the handshake over the
// rings, close, and a second process that is killed while connected.

#include "SPCommon.h"

#include "XLRemoteTransport.h"
#include "XLRemoteProtocol.h"

#include "SPPlatform.h"

#include "../tests.h"

#include <sprt/cxx/thread>
#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

#if SPRT_LINUX
#include <sprt/c/cross/__sprt_syscall.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

__SPRT_C_FUNC long int syscall(long int __sysno, ...);
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

#if SPRT_LINUX

namespace {

static constexpr StringView s_peerEnv = "XL_SHM_PEER";

static uint64_t shmNowUs() { return sp::platform::clock(ClockType::Monotonic); }

static TransportClientConfig smallClientConfig() {
	TransportClientConfig cfg;
	cfg.shmControlCapacity = 4'096;
	cfg.shmBulkCapacity = 4'096;
	return cfg;
}

// Entries in the rendezvous directory that belong to `path`: the listener file and any
// connection block next to it.
static uint32_t countBlockFiles(StringView path) {
	auto slash = path.rfind('/');
	auto dir = path.sub(0, slash).str<Interface>();
	auto prefix = path.sub(slash + 1);
	uint32_t count = 0;
	if (auto d = ::opendir(dir.data())) {
		while (auto ent = ::readdir(d)) {
			auto name = StringView(ent->d_name);
			if (name.starts_with(prefix) && name.size() > prefix.size()
					&& name[prefix.size()] == '.') {
				++count;
			}
		}
		::closedir(d);
	}
	return count;
}

static Rc<TransportConnection> acceptOne(TransportListener *listener, uint64_t timeoutUs) {
	Rc<TransportConnection> ret;
	auto deadline = shmNowUs() + timeoutUs;
	while (!ret && shmNowUs() < deadline) {
		listener->handleEvents([&](Rc<TransportConnection> &&c) { ret = sp::move(c); });
		if (!ret) {
			sp::platform::sleep(1'000);
		}
	}
	return ret;
}

static bool writeAllUntil(TransportConnection *conn, StreamClass c, BytesView data,
		uint64_t timeoutUs) {
	auto stream = conn->getStream(c);
	auto deadline = shmNowUs() + timeoutUs;
	size_t total = 0;
	while (total < data.size()) {
		size_t w = 0;
		if (stream->write(data.sub(total), w) != Status::Ok) {
			return false;
		}
		total += w;
		if (w == 0) {
			if (shmNowUs() >= deadline) {
				return false;
			}
			conn->handleEvents();
			sp::platform::sleep(1'000);
		}
	}
	return true;
}

static Bytes readUntil(TransportConnection *conn, StreamClass c, size_t size, uint64_t timeoutUs) {
	auto stream = conn->getStream(c);
	auto deadline = shmNowUs() + timeoutUs;
	Bytes out;
	uint8_t buf[512];
	while (out.size() < size && shmNowUs() < deadline) {
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

static BytesView bytesOf(StringView str) {
	return BytesView(reinterpret_cast<const uint8_t *>(str.data()), str.size());
}

static bool equals(const Bytes &b, StringView str) {
	return b.size() == str.size() && __sprt_memcmp(b.data(), str.data(), b.size()) == 0;
}

static Bytes s_key(kBearerKeySize, uint8_t(0x5A));

} // namespace

void performShmTransportTests() {
	sprt::cout << "--- remote transport (shm:) ---\n";

	initializeTransports();
	check(TransportRegistry::has(AddressScheme::Shm), "shm transport: registered");

	auto path = toString("/dev/shm/xl-remotetest-", ::getpid());
	auto addr = Address::parse(toString("shm:", path));

	{
		::unlink(path.data());
		auto conn = TransportRegistry::connect(addr, smallClientConfig());
		check(conn == nullptr, "shm transport: connecting without a listener fails");
	}

	{
		// A rendezvous file left behind by a crashed server must not block a new one.
		int fd = ::open(path.data(), O_CREAT | O_WRONLY, 0600);
		if (fd >= 0) {
			::write(fd, "junk", 4);
			::close(fd);
		}
	}

	auto listener = TransportRegistry::listen(addr);
	check(listener && listener->isOpen(), "shm transport: listener replaces a stale file");
	if (!listener) {
		return;
	}

	auto client = TransportRegistry::connect(addr, smallClientConfig());
	check(client != nullptr, "shm transport: connect posts a request");
	auto server = acceptOne(listener, 1'000'000);
	check(server != nullptr, "shm transport: listener accepts it");
	if (!client || !server) {
		return;
	}

	check(client->hasCaps(TransportCaps::MultiStream | TransportCaps::PeerAuthenticated)
					&& server->hasCaps(
							TransportCaps::MultiStream | TransportCaps::PeerAuthenticated),
			"shm transport: both halves are multi-stream and authenticated");
	check(server->getPeerIdentity().uid == int64_t(::getuid())
					&& server->getPeerIdentity().pid == int64_t(::getpid()),
			"shm transport: the server sees the client's uid and pid");
	check(client->getPeerIdentity().pid == int64_t(::getpid()),
			"shm transport: the client sees the server's pid");
	checkEq(countBlockFiles(path), 0, "shm transport: the connection block is unlinked on accept");

	check(writeAllUntil(client, StreamClass::Control, bytesOf("hello control"), 100'000)
					&& equals(readUntil(server, StreamClass::Control, 13, 100'000),
							"hello control"),
			"shm transport: client to server on Control");
	check(writeAllUntil(server, StreamClass::Bulk, bytesOf("hello bulk"), 100'000)
					&& equals(readUntil(client, StreamClass::Bulk, 10, 100'000), "hello bulk"),
			"shm transport: server to client on Bulk");
	check(writeAllUntil(client, StreamClass::Bulk, bytesOf("b"), 100'000)
					&& readUntil(server, StreamClass::Control, 1, 20'000).empty()
					&& equals(readUntil(server, StreamClass::Bulk, 1, 100'000), "b"),
			"shm transport: Bulk does not leak into Control");

	{
		// Bytes written before a close are still delivered, then the stream reads as closed.
		writeAllUntil(client, StreamClass::Control, bytesOf("tail"), 100'000);
		client->close();
		check(!server->isClosed(), "shm transport: not closed while bytes remain");
		check(equals(readUntil(server, StreamClass::Control, 4, 100'000), "tail"),
				"shm transport: bytes sent before close arrive");
		check(server->isClosed(), "shm transport: closed once drained");
		server->close();
	}

	{
		// Many 64 KiB messages: the ring wraps and the writer rides out backpressure.
		auto c = TransportRegistry::connect(addr, TransportClientConfig());
		auto s = acceptOne(listener, 1'000'000);
		Bytes payload(64 * 1'024);
		for (size_t i = 0; i < payload.size(); ++i) { payload[i] = uint8_t(i * 7); }
		bool ok = c && s;
		sprt::thread reader([&] {
			for (uint32_t i = 0; ok && i < 64; ++i) {
				auto got = readUntil(s, StreamClass::Bulk, payload.size(), 2'000'000);
				ok = got == payload;
			}
		});
		for (uint32_t i = 0; c && s && i < 64; ++i) {
			if (!writeAllUntil(c, StreamClass::Bulk, payload, 2'000'000)) {
				ok = false;
				break;
			}
		}
		reader.join();
		check(ok, "shm transport: 4 MiB across a 16 MiB bulk ring with a concurrent reader");
	}

	{
		TransportServerConfig small;
		small.shmMaxBlockSize = 8'192;
		auto smallPath = toString(path, "-small");
		auto smallAddr = Address::parse(toString("shm:", smallPath));
		auto smallListener = TransportRegistry::listen(smallAddr, small);
		auto c = TransportRegistry::connect(smallAddr, smallClientConfig());
		auto s = smallListener ? acceptOne(smallListener, 50'000) : nullptr;
		check(c != nullptr && s == nullptr,
				"shm transport: a block above the server's limit is refused");
		checkEq(countBlockFiles(smallPath), 0, "shm transport: a refused block is unlinked");
	}

	{
		Vector<Rc<TransportConnection>> pending;
		for (uint32_t i = 0; i < 16; ++i) {
			if (auto c = TransportRegistry::connect(addr, smallClientConfig())) {
				pending.emplace_back(sp::move(c));
			}
		}
		checkEq(pending.size(), 16, "shm transport: sixteen requests fit the slots");
		auto extra = TransportRegistry::connect(addr, smallClientConfig());
		check(extra == nullptr, "shm transport: the seventeenth is refused");
		pending.clear();
		checkEq(countBlockFiles(path), 0, "shm transport: withdrawn requests leave no files");
		auto s = acceptOne(listener, 50'000);
		check(s == nullptr, "shm transport: withdrawn requests are not accepted");
		auto c = TransportRegistry::connect(addr, smallClientConfig());
		check(c != nullptr, "shm transport: slots are free again");
		s = acceptOne(listener, 1'000'000);
		check(s != nullptr, "shm transport: and accept normally");
	}

	{
		GlobalError serverResult = GlobalError::NetworkBackend;
		sprt::thread serverThread([&] {
			auto s = acceptOne(listener, 2'000'000);
			if (s) {
				Bytes dict;
				serverResult = serverHandshake(*s, s_key, BytesView(), dict, shmNowUs() + 2'000'000,
						!s->hasCaps(TransportCaps::PeerAuthenticated));
			}
		});
		GlobalError clientResult = GlobalError::NetworkBackend;
		if (auto c = TransportRegistry::connect(addr, smallClientConfig())) {
			clientResult = clientHandshake(*c, s_key, BytesView(), shmNowUs() + 2'000'000,
					[](const ServerHello &) { });
		}
		serverThread.join();
		check(clientResult == GlobalError::Ok && serverResult == GlobalError::Ok,
				"shm transport: handshake over the rings");
	}

	{
		// What AppThread registers: the listener's and the connection's doorbells on a Looper.
		auto looper = sprt::dispatch::Looper::acquire();
		auto pump = [&](const Callback<bool()> &done) {
			auto deadline = shmNowUs() + 1'000'000;
			while (!done() && shmNowUs() < deadline) {
				looper->run(sprt::dispatch::TimeInterval::milliseconds(10));
			}
		};

		auto lw = listener->getWaitAddress();
		check(lw.address != nullptr, "shm transport: the listener offers a wait address");
		uint32_t listenWakes = 0;
		auto lh = looper->waitOnAddress(lw.address, lw.value, [&](uint32_t) -> Status {
			++listenWakes;
			return Status::Ok;
		});
		auto c = TransportRegistry::connect(addr, smallClientConfig());
		pump([&] { return listenWakes > 0; });
		check(listenWakes > 0, "shm transport: a connect wakes the listener's waiter");

		auto s = acceptOne(listener, 1'000'000);
		auto sw = s ? s->getWaitAddress() : TransportWaitAddress();
		check(sw.address != nullptr, "shm transport: a connection offers a wait address");
		uint32_t connWakes = 0;
		Rc<sprt::dispatch::AddressWaitHandle> sh;
		if (sw.address) {
			sh = looper->waitOnAddress(sw.address, sw.value, [&](uint32_t) -> Status {
				++connWakes;
				return Status::Ok;
			});
		}
		if (c) {
			writeAllUntil(c, StreamClass::Control, bytesOf("wake"), 100'000);
		}
		pump([&] { return connWakes > 0; });
		check(connWakes > 0, "shm transport: a peer write wakes the connection's waiter");

		// A connection closed under an armed wait keeps its memory until the handle is gone.
		if (s && sw.address) {
			s->close();
			looper->run(sprt::dispatch::TimeInterval::milliseconds(20));
			check(*sw.address >= sw.value, "shm transport: the doorbell stays mapped after close");
		}
		if (sh) {
			sh->cancel();
		}
		if (lh) {
			lh->cancel();
		}
	}

	{
		// A second process: handshake, one message each way, then SIGKILL.
		::fflush(nullptr);
		auto env = toString(s_peerEnv, "=", path);
		auto pid = ::fork();
		if (pid == 0) {
			char *argv[] = {const_cast<char *>("remotetest"), const_cast<char *>("shm-peer"),
				nullptr};
			char *envp[] = {env.data(), nullptr};
			::execve("/proc/self/exe", argv, envp);
			::_exit(127);
		}

		auto s = acceptOne(listener, 5'000'000);
		check(s != nullptr, "shm transport: a second process connects");
		GlobalError result = GlobalError::NetworkBackend;
		bool exchanged = false;
		bool detected = false;
		if (s) {
			check(s->getPeerIdentity().pid == int64_t(pid),
					"shm transport: the peer pid is the child's");
			Bytes dict;
			result = serverHandshake(*s, s_key, BytesView(), dict, shmNowUs() + 2'000'000,
					!s->hasCaps(TransportCaps::PeerAuthenticated));
			exchanged = equals(readUntil(s, StreamClass::Bulk, 10, 2'000'000), "from child")
					&& writeAllUntil(s, StreamClass::Bulk, bytesOf("from parent"), 1'000'000);
			sp::platform::sleep(200'000);
			::kill(pid, SIGKILL);
			auto deadline = shmNowUs() + 2'000'000;
			while (!(detected = s->isClosed()) && shmNowUs() < deadline) {
				s->handleEvents();
				sp::platform::sleep(1'000);
			}
		} else if (pid > 0) {
			::kill(pid, SIGKILL);
		}
		if (pid > 0) {
			int status = 0;
			::syscall(__SPRT_SYSCALL_wait4, pid, &status, 0, nullptr);
		}
		check(result == GlobalError::Ok && exchanged,
				"shm transport: handshake and exchange with another process");
		check(detected, "shm transport: a killed peer reads as closed");
	}

	listener->close();
	checkEq(countBlockFiles(path), 0, "shm transport: no connection blocks left behind");
	check(::access(path.data(), F_OK) != 0, "shm transport: the listener removes its file");
}

void performShmPeerProcess() {
	auto path = ::getenv(s_peerEnv.data());
	if (!path) {
		return;
	}

	initializeTransports();
	auto conn =
			TransportRegistry::connect(Address::parse(toString("shm:", path)), smallClientConfig());
	if (!conn) {
		::_exit(2);
	}
	auto st = clientHandshake(*conn, s_key, BytesView(), shmNowUs() + 2'000'000,
			[](const ServerHello &) { });
	if (st != GlobalError::Ok) {
		::_exit(3);
	}
	writeAllUntil(conn, StreamClass::Bulk, bytesOf("from child"), 1'000'000);
	readUntil(conn, StreamClass::Bulk, 11, 2'000'000);
	// Stay connected until the parent kills us.
	for (uint32_t i = 0; i < 100; ++i) { sp::platform::sleep(100'000); }
	::_exit(0);
}

#else

void performShmTransportTests() {
	sprt::cout << "--- remote transport (shm:) --- skipped: Linux only\n";
}

void performShmPeerProcess() { }

#endif

} // namespace stappler::xenolith::remote
