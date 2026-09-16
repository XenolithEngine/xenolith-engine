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

// The shared-memory block of the `shm:` transport: layout validation, ring semantics, the doorbell,
// a two-thread SPSC run and a hostile peer rewriting the block under a live endpoint.

#include "SPCommon.h"

#include "XLRemoteShmBlock.h"

#include "SPPlatform.h"

#include "../tests.h"

#include <sprt/cxx/thread>
#include <sprt/c/sys/__sprt_sprt.h>

#if SPRT_LINUX || SPRT_ANDROID
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace STAPPLER_VERSIONIZED stappler::xenolith::remote {

using stappler::test::check;
using stappler::test::checkEq;

namespace {

// A block with inaccessible pages on both sides where the platform allows it, so a read or write
// past the block faults instead of passing silently.
struct TestBlock {
	uint8_t *mem = nullptr;
	size_t size = 0;

#if SPRT_LINUX || SPRT_ANDROID
	void *mapping = nullptr;
	size_t mappingSize = 0;

	bool init(size_t required) {
		auto page = size_t(::sysconf(_SC_PAGESIZE));
		size = (required + page - 1) & ~(page - 1);
		mappingSize = size + page * 2;
		mapping = ::mmap(nullptr, mappingSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (mapping == MAP_FAILED) {
			mapping = nullptr;
			return false;
		}
		mem = static_cast<uint8_t *>(mapping) + page;
		return ::mprotect(mem, size, PROT_READ | PROT_WRITE) == 0;
	}

	~TestBlock() {
		if (mapping) {
			::munmap(mapping, mappingSize);
		}
	}
#else
	Bytes storage;

	bool init(size_t required) {
		size = (required + 4'095) & ~size_t(4'095);
		storage.resize(size);
		mem = storage.data();
		return true;
	}
#endif
};

struct XorShift {
	uint32_t state;

	uint32_t next() {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}

	uint32_t below(uint32_t bound) { return next() % bound; }
};

static constexpr ShmBlockConfig s_smallConfig{4'096, 8'192};

static bool writeString(ShmEndpoint &ep, StreamClass c, StringView str) {
	size_t written = 0;
	return ep.write(c, BytesView(reinterpret_cast<const uint8_t *>(str.data()), str.size()),
				   written)
			== Status::Ok
			&& written == str.size();
}

static String readString(ShmEndpoint &ep, StreamClass c) {
	String out;
	uint8_t buf[256];
	for (;;) {
		size_t got = 0;
		if (ep.read(c, buf, sizeof(buf), got) != Status::Ok || got == 0) {
			break;
		}
		out.append(reinterpret_cast<const char *>(buf), got);
	}
	return out;
}

static void testLayout() {
	check(ShmBlock::computeSize(ShmBlockConfig{5'000, 8'192}) == 0,
			"shm: capacity that is not a power of two is rejected");
	check(ShmBlock::computeSize(ShmBlockConfig{1'024, 8'192}) == 0,
			"shm: capacity below the minimum is rejected");

	auto required = ShmBlock::computeSize(s_smallConfig);
	check(required >= sizeof(ShmBlockHeader) + 2 * 4'096 + 2 * 8'192,
			"shm: size covers every ring");

	TestBlock block;
	if (!block.init(required)) {
		check(false, "shm: test block allocated");
		return;
	}

	check(ShmBlock::format(block.mem, required - 1, s_smallConfig) != Status::Ok,
			"shm: format refuses a block that is too small");
	check(ShmBlock::format(block.mem, block.size, s_smallConfig) == Status::Ok, "shm: format");

	auto header = reinterpret_cast<ShmBlockHeader *>(block.mem);
	checkEq(header->blockSize, block.size, "shm: header records the block size");

	ShmEndpoint server;
	ShmEndpoint client;
	check(server.attach(block.mem, block.size, ShmSide::Server) == Status::Ok,
			"shm: server attach");
	check(client.attach(block.mem, block.size, ShmSide::Client) == Status::Ok,
			"shm: client attach");
	check(!server.hasPending() && !server.isClosed(), "shm: fresh endpoint is open and empty");

	struct Mutation {
		StringView name;
		void (*apply)(ShmBlockHeader *, size_t size);
	};

	static constexpr Mutation s_mutations[] = {
		{"magic", [](ShmBlockHeader *h, size_t) { h->magic ^= 1; }},
		{"version", [](ShmBlockHeader *h, size_t) { h->version = kShmBlockVersion + 1; }},
		{"ring count", [](ShmBlockHeader *h, size_t) { h->ringCount = 3; }},
		{"block size", [](ShmBlockHeader *h, size_t) { h->blockSize -= 4'096; }},
		{"capacity", [](ShmBlockHeader *h, size_t) { h->rings[0].capacity = 5'000; }},
		{"offset into the header", [](ShmBlockHeader *h, size_t) { h->rings[1].offset = 16; }},
		{"misaligned offset", [](ShmBlockHeader *h, size_t) { h->rings[2].offset += 1; }},
		{"ring past the end",
			[](ShmBlockHeader *h, size_t size) { h->rings[3].offset = uint32_t(size - 64); }},
		{"overlapping rings",
			[](ShmBlockHeader *h, size_t) { h->rings[1].offset = h->rings[0].offset + 64; }},
		{"inconsistent indices",
			[](ShmBlockHeader *h, size_t) {
		auto ring = reinterpret_cast<ShmRingHeader *>(
				reinterpret_cast<uint8_t *>(h) + h->rings[0].offset);
		ring->head = 1'000'000;
	}},
	};

	for (auto &it : s_mutations) {
		ShmBlock::format(block.mem, block.size, s_smallConfig);
		it.apply(header, block.size);
		ShmEndpoint ep;
		check(ep.attach(block.mem, block.size, ShmSide::Client) != Status::Ok,
				toString("shm: attach rejects bad ", it.name));
	}

	ShmBlock::format(block.mem, block.size, s_smallConfig);
	ShmEndpoint ep;
	check(ep.attach(block.mem, block.size - 4'096, ShmSide::Client) != Status::Ok,
			"shm: attach rejects a size that differs from the header");
}

static void testExchange() {
	TestBlock block;
	if (!block.init(ShmBlock::computeSize(s_smallConfig))) {
		check(false, "shm: test block allocated");
		return;
	}
	ShmBlock::format(block.mem, block.size, s_smallConfig);

	ShmEndpoint server;
	ShmEndpoint client;
	server.attach(block.mem, block.size, ShmSide::Server);
	client.attach(block.mem, block.size, ShmSide::Client);

	check(writeString(server, StreamClass::Control, "hello"), "shm: server writes control");
	check(writeString(client, StreamClass::Bulk, "bulk data"), "shm: client writes bulk");
	check(client.hasPending(), "shm: client sees pending bytes");
	checkEq(readString(client, StreamClass::Bulk), "", "shm: classes are separate rings");
	checkEq(readString(client, StreamClass::Control), "hello", "shm: client reads control");
	checkEq(readString(server, StreamClass::Bulk), "bulk data", "shm: server reads bulk");
	check(writeString(server, StreamClass::Frame, "frame"), "shm: frame writes");
	checkEq(readString(client, StreamClass::Control), "frame",
			"shm: frame shares the control ring");

	// Many passes over the end of a 4 KiB ring with a chunk that does not divide it.
	bool wrapOk = true;
	Bytes chunk(1'500);
	uint8_t readBuf[1'500];
	for (uint32_t i = 0; i < 1'000 && wrapOk; ++i) {
		for (size_t j = 0; j < chunk.size(); ++j) { chunk[j] = uint8_t(i * 31 + j); }
		size_t written = 0;
		size_t got = 0;
		wrapOk = server.write(StreamClass::Control, chunk, written) == Status::Ok
				&& written == chunk.size()
				&& client.read(StreamClass::Control, readBuf, sizeof(readBuf), got) == Status::Ok
				&& got == chunk.size() && __sprt_memcmp(readBuf, chunk.data(), got) == 0;
	}
	check(wrapOk, "shm: data survives wrapping around the ring");

	Bytes fill(4'096 - 10);
	size_t written = 0;
	server.write(StreamClass::Control, fill, written);
	checkEq(written, fill.size(), "shm: ring accepts up to its capacity");

	Bytes more(100);
	check(server.write(StreamClass::Control, more, written) == Status::Ok && written == 10,
			"shm: partial write takes what fits");
	check(server.write(StreamClass::Control, more, written) == Status::Ok && written == 0,
			"shm: full ring accepts nothing and is not an error");

	auto header = reinterpret_cast<ShmBlockHeader *>(block.mem);
	auto serverBell = header->doorbell[uint32_t(ShmSide::Server)];
	size_t got = 0;
	client.read(StreamClass::Control, readBuf, 50, got);
	check(header->doorbell[uint32_t(ShmSide::Server)] == serverBell + 1,
			"shm: reading from a full ring rings the blocked writer");
	client.read(StreamClass::Control, readBuf, 50, got);
	check(header->doorbell[uint32_t(ShmSide::Server)] == serverBell + 1,
			"shm: the writer is rung once per blocked write");
}

static void testClose() {
	TestBlock block;
	if (!block.init(ShmBlock::computeSize(s_smallConfig))) {
		check(false, "shm: test block allocated");
		return;
	}
	ShmBlock::format(block.mem, block.size, s_smallConfig);

	ShmEndpoint server;
	ShmEndpoint client;
	server.attach(block.mem, block.size, ShmSide::Server);
	client.attach(block.mem, block.size, ShmSide::Client);

	writeString(server, StreamClass::Control, "bye");
	server.close();

	check(client.isPeerClosed(), "shm: peer close is visible");
	check(!client.isClosed(), "shm: not closed while bytes remain");
	checkEq(readString(client, StreamClass::Control), "bye", "shm: bytes sent before close arrive");
	check(client.isClosed(), "shm: closed once drained");

	uint8_t buf[8];
	size_t got = 0;
	check(client.read(StreamClass::Control, buf, sizeof(buf), got) == Status::ErrorNotPermitted,
			"shm: read after drain reports the close");
	check(!writeString(client, StreamClass::Control, "x"), "shm: write to a closed peer fails");
	check(!writeString(server, StreamClass::Control, "x"), "shm: write after own close fails");
}

static void testDoorbell() {
	TestBlock block;
	if (!block.init(ShmBlock::computeSize(s_smallConfig))) {
		check(false, "shm: test block allocated");
		return;
	}
	ShmBlock::format(block.mem, block.size, s_smallConfig);

	ShmEndpoint server;
	server.attach(block.mem, block.size, ShmSide::Server);

	auto header = reinterpret_cast<ShmBlockHeader *>(block.mem);
	auto before = header->doorbell[uint32_t(ShmSide::Client)];
	writeString(server, StreamClass::Control, "ping");
	checkEq(header->doorbell[uint32_t(ShmSide::Client)], before + 1,
			"shm: a write increments the peer doorbell");

	ShmBlock::format(block.mem, block.size, s_smallConfig);
	server.attach(block.mem, block.size, ShmSide::Server);

	struct WaitResult {
		String data;
		uint64_t waitedUs = 0;
	} result;

	sprt::thread waiter([&] {
		ShmEndpoint client;
		client.attach(block.mem, block.size, ShmSide::Client);
		auto start = sp::platform::clock(ClockType::Monotonic);
		while (!client.hasPending()) {
			auto expected = client.prepareWait();
			if (!client.hasPending() && !client.isClosed()) {
				__sprt_sprt_qlock_wait(client.getDoorbell(), expected, 5'000'000'000ULL,
						__SPRT_SPRT_LOCK_FLAG_SHARED);
			}
			client.finishWait();
			if (sp::platform::clock(ClockType::Monotonic) - start > 5'000'000) {
				break;
			}
		}
		result.waitedUs = sp::platform::clock(ClockType::Monotonic) - start;
		result.data = readString(client, StreamClass::Control);
	});

	sprt::this_thread::sleep_for(50'000'000);
	writeString(server, StreamClass::Control, "wake");
	waiter.join();

	checkEq(result.data, "wake", "shm: sleeping reader receives the write");
	check(result.waitedUs < 2'000'000, "shm: the doorbell wakes the reader without a timeout");
}

static void testSpsc() {
	static constexpr ShmBlockConfig config{65'536, 4'096};
	static constexpr size_t totalBytes = 32u * 1'024 * 1'024;

	TestBlock block;
	if (!block.init(ShmBlock::computeSize(config))) {
		check(false, "shm: test block allocated");
		return;
	}
	ShmBlock::format(block.mem, block.size, config);

	static constexpr uint64_t waitSliceNs = 100'000'000;

	sprt::thread writer([&] {
		ShmEndpoint ep;
		ep.attach(block.mem, block.size, ShmSide::Server);
		XorShift sizes{7};
		XorShift bytes{12'345};
		Bytes chunk;
		size_t sent = 0;
		while (sent < totalBytes) {
			chunk.resize(sprt::min(size_t(1 + sizes.below(9'000)), totalBytes - sent));
			for (auto &b : chunk) { b = uint8_t(bytes.next()); }
			size_t offset = 0;
			while (offset < chunk.size()) {
				size_t written = 0;
				auto expected = ep.prepareWait();
				if (ep.write(StreamClass::Control, BytesView(chunk).sub(offset), written)
						!= Status::Ok) {
					return;
				}
				offset += written;
				if (written == 0) {
					__sprt_sprt_qlock_wait(ep.getDoorbell(), expected, waitSliceNs,
							__SPRT_SPRT_LOCK_FLAG_SHARED);
				}
				ep.finishWait();
			}
			sent += chunk.size();
		}
		ep.close();
	});

	size_t received = 0;
	bool ordered = true;
	sprt::thread reader([&] {
		ShmEndpoint ep;
		ep.attach(block.mem, block.size, ShmSide::Client);
		XorShift bytes{12'345};
		uint8_t buf[7'000];
		for (;;) {
			size_t got = 0;
			if (ep.read(StreamClass::Control, buf, sizeof(buf), got) != Status::Ok) {
				break;
			}
			if (got == 0) {
				auto expected = ep.prepareWait();
				if (!ep.hasPending() && !ep.isClosed()) {
					__sprt_sprt_qlock_wait(ep.getDoorbell(), expected, waitSliceNs,
							__SPRT_SPRT_LOCK_FLAG_SHARED);
				}
				ep.finishWait();
				continue;
			}
			for (size_t i = 0; i < got; ++i) {
				if (buf[i] != uint8_t(bytes.next())) {
					ordered = false;
				}
			}
			received += got;
		}
	});

	writer.join();
	reader.join();

	checkEq(received, totalBytes, "shm: SPSC delivers every byte");
	check(ordered, "shm: SPSC preserves order and content");
}

static void testHostilePeer() {
	static constexpr uint32_t iterations = 10'000;

	TestBlock block;
	if (!block.init(ShmBlock::computeSize(s_smallConfig))) {
		check(false, "shm: test block allocated");
		return;
	}

	XorShift rnd{0xC0'FFEE};
	ShmEndpoint ep;
	uint32_t corruptions = 0;
	uint32_t refusedAttach = 0;
	bool contractHeld = true;
	Bytes out(9'000);
	uint8_t in[9'000];

	auto reset = [&] {
		ShmBlock::format(block.mem, block.size, s_smallConfig);
		ep.attach(block.mem, block.size, ShmSide(rnd.below(2)));
	};
	reset();

	// Ring offsets of the valid layout: the header itself is among the things being rewritten.
	auto header = reinterpret_cast<ShmBlockHeader *>(block.mem);
	uint32_t ringOffsets[kShmBlockRingCount];
	for (uint32_t idx = 0; idx < kShmBlockRingCount; ++idx) {
		ringOffsets[idx] = header->rings[idx].offset;
	}
	auto ringHeader = [&](uint32_t idx) {
		return reinterpret_cast<ShmRingHeader *>(block.mem + ringOffsets[idx]);
	};

	for (uint32_t i = 0; i < iterations; ++i) {
		switch (rnd.below(10)) {
		case 0: ringHeader(rnd.below(4))->head = rnd.next(); break;
		case 1: ringHeader(rnd.below(4))->tail = rnd.next(); break;
		case 2: ringHeader(rnd.below(4))->head += rnd.below(64); break;
		case 3: ringHeader(rnd.below(4))->writerBlocked = rnd.next(); break;
		case 4: header->closed = rnd.below(4); break;
		case 5: header->waiting[rnd.below(2)] = rnd.next(); break;
		case 6: {
			// Rewrite one byte of the layout and attach again: attach either refuses or yields an
			// endpoint that stays inside the block.
			ShmBlock::format(block.mem, block.size, s_smallConfig);
			block.mem[rnd.below(sizeof(ShmBlockHeader))] = uint8_t(rnd.next());
			if (ep.attach(block.mem, block.size, ShmSide(rnd.below(2))) != Status::Ok) {
				++refusedAttach;
				reset();
			}
			break;
		}
		default: break;
		}

		auto c = rnd.below(2) ? StreamClass::Bulk : StreamClass::Control;
		size_t n = 0;
		Status st;
		if (rnd.below(2)) {
			st = ep.write(c, BytesView(out.data(), rnd.below(out.size())), n);
		} else {
			st = ep.read(c, in, rnd.below(sizeof(in)), n);
		}
		ep.hasPending();
		ep.isClosed();

		if (ep.isCorrupted()) {
			++corruptions;
			if (st != Status::ErrorInvalidArguemnt || n != 0 || !ep.isClosed()) {
				contractHeld = false;
			}
			reset();
		}
	}

	check(true, toString("shm: ", iterations, " hostile rewrites without a fault"));
	check(corruptions > 0, toString("shm: corruption detected (", corruptions, " times)"));
	check(refusedAttach > 0,
			toString("shm: attach refused a bad layout (", refusedAttach, " times)"));
	check(contractHeld, "shm: a corrupted endpoint fails every operation and reads as closed");
}

} // namespace

void performShmTests() {
	sprt::cout << "--- remote shared-memory block ---\n";
	testLayout();
	testExchange();
	testClose();
	testDoorbell();
	testSpsc();
	testHostilePeer();
}

} // namespace stappler::xenolith::remote
