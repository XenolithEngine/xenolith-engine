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

// Tests for dispatch::Looper::readFile / writeFile: streamed chunked reads,
// byte-span writes (incl. Append and CreateExclusive), the path / fd / chained
// handle input forms, and the empty-file / large-file edge cases. On Linux the
// default looper uses the io_uring native path; the inline path is exercised by
// re-running with a forced epoll engine on a worker thread.

#include <sprt/runtime/dispatch/looper.h>
#include <sprt/runtime/dispatch/handle.h>

#include <sprt/cxx/thread>
#include <sprt/c/__sprt_unistd.h>
#include <sprt/c/__sprt_fcntl.h>

namespace sprt {

namespace {

namespace dispatch = sprt::dispatch;

using OpenFlags = dispatch::OpenFlags;
static constexpr OpenFlags kOverride = OpenFlags::Write | OpenFlags::Create | OpenFlags::Truncate;

// Fixed-capacity accumulator for read chunks (callbacks run within a transient
// notify pool, so there is no heap/pool dependency here).
struct ReadBuffer {
	static constexpr size_t Cap = 512 * 1024;
	uint8_t *buf = nullptr;
	size_t len = 0;
	void append(BytesView d) {
		size_t n = d.size();
		if (n > Cap - len) {
			n = Cap - len;
		}
		if (n) {
			sprt::memcpy(buf + len, d.data(), n);
			len += n;
		}
	}
};

static void fillPattern(uint8_t *p, size_t n) {
	for (size_t i = 0; i < n; ++i) { p[i] = uint8_t(i * 31 + 7); }
}

static bool checkPattern(const uint8_t *p, size_t n) {
	for (size_t i = 0; i < n; ++i) {
		if (p[i] != uint8_t(i * 31 + 7)) {
			return false;
		}
	}
	return true;
}

static void drive(dispatch::Looper *looper, bool &done) {
	while (!done) { looper->wait(dispatch::TimeInterval::Infinite); }
}

static Status writeFileSync(dispatch::Looper *looper, StringView path, BytesView data,
		OpenFlags flags) {
	bool done = false;
	Status st = Status::Pending;
	looper->writeFile(path, data, flags, [&](Status s) {
		st = s;
		done = true;
	});
	drive(looper, done);
	return st;
}

static Status readFileSync(dispatch::Looper *looper, StringView path, ReadBuffer &out) {
	bool done = false;
	Status st = Status::Pending;
	looper->readFile(path, [&](BytesView d) { out.append(d); }, [&](Status s) {
		st = s;
		done = true;
	});
	drive(looper, done);
	return st;
}

static bool report(bool ok, StringView name, int &failed) {
	sprt::cout << (ok ? "PASS  " : "FAIL  ") << name << "\n";
	if (!ok) {
		++failed;
	}
	return ok;
}

// Runs the whole suite against `looper` (whichever engine it was created with).
static int runFileSuite(dispatch::Looper *looper) {
	int failed = 0;

	static uint8_t s_readStore[ReadBuffer::Cap];
	static uint8_t s_src[100 * 1024];

	StringView path("sprt_file_test.tmp");
	StringView pathExcl("sprt_file_excl.tmp");

	// 1. small write+read round-trip
	{
		fillPattern(s_src, 37);
		auto ws = writeFileSync(looper, path, BytesView(s_src, 37), kOverride);
		ReadBuffer rb{s_readStore, 0};
		auto rs = readFileSync(looper, path, rb);
		report(isSuccessful(ws) && isSuccessful(rs) && rb.len == 37 && checkPattern(rb.buf, rb.len),
				"write+read round-trip (small)", failed);
	}

	// 2. large multi-chunk round-trip (> FileChunkSize == 32 KiB)
	{
		fillPattern(s_src, sizeof(s_src));
		auto ws = writeFileSync(looper, path, BytesView(s_src, sizeof(s_src)), kOverride);
		ReadBuffer rb{s_readStore, 0};
		auto rs = readFileSync(looper, path, rb);
		report(isSuccessful(ws) && isSuccessful(rs) && rb.len == sizeof(s_src)
						&& checkPattern(rb.buf, rb.len),
				"write+read round-trip (100 KiB, multi-chunk)", failed);
	}

	// 3. Append extends the file
	{
		fillPattern(s_src, 15);
		writeFileSync(looper, path, BytesView(s_src, 10), kOverride);
		auto ws = writeFileSync(looper, path, BytesView(s_src, 5),
				OpenFlags::Write | OpenFlags::Append);
		ReadBuffer rb{s_readStore, 0};
		readFileSync(looper, path, rb);
		report(isSuccessful(ws) && rb.len == 15, "writeFile Append extends the file", failed);
	}

	// 4. empty-file read: completion Ok, 0 bytes, reader never called
	{
		writeFileSync(looper, path, BytesView(), kOverride); // truncate to empty
		ReadBuffer rb{s_readStore, 0};
		bool readerCalled = false;
		bool done = false;
		Status st = Status::Pending;
		looper->readFile(path, [&](BytesView d) {
			readerCalled = true;
			rb.append(d);
		}, [&](Status s) {
			st = s;
			done = true;
		});
		drive(looper, done);
		report(isSuccessful(st) && rb.len == 0 && !readerCalled,
				"empty-file read (no reader call, Ok)", failed);
	}

	// 5. CreateExclusive: succeeds on a fresh path, fails when the file exists
	{
		::__sprt_unlink("sprt_file_excl.tmp");
		fillPattern(s_src, 8);
		auto first = writeFileSync(looper, pathExcl, BytesView(s_src, 8),
				OpenFlags::Write | OpenFlags::CreateExclusive);
		auto second = writeFileSync(looper, pathExcl, BytesView(s_src, 8),
				OpenFlags::Write | OpenFlags::CreateExclusive);
		report(isSuccessful(first) && !isSuccessful(second),
				"CreateExclusive: fresh ok, existing fails", failed);
		::__sprt_unlink("sprt_file_excl.tmp");
	}

	// 6. fd form (Info API): the handle reads a caller-owned fd and never closes it
	{
		fillPattern(s_src, 64);
		writeFileSync(looper, path, BytesView(s_src, 64), kOverride);

		int fd = ::__sprt_open("sprt_file_test.tmp", __SPRT_O_RDONLY, 0);
		bool ok = fd >= 0;
		if (ok) {
			ReadBuffer rb{s_readStore, 0};
			bool done = false;
			Status st = Status::Pending;

			dispatch::FileReadInfo info;
			info.fd = dispatch::NativeHandle(fd);
			info.flags = OpenFlags::Read;
			info.reader = [&](BytesView d) { rb.append(d); };

			struct Ctx : public Ref {
				bool *done;
				Status *st;
			};
			auto ctx = Rc<Ctx>::alloc();
			ctx->done = &done;
			ctx->st = &st;
			info.completion = dispatch::FileReadInfo::Completion::create<Ctx>(ctx.get(),
					[](Ctx *c, dispatch::FileHandle *, uint32_t, Status s) {
				*c->st = s;
				*c->done = true;
			});

			looper->readFile(sprt::move(info), ctx.get());
			drive(looper, done);

			// the borrowed fd must still be open and usable
			::__sprt_lseek(fd, 0, 0 /*SEEK_SET*/);
			uint8_t tmp[1] = {0};
			auto n = ::__sprt_read(fd, tmp, 1);
			ok = isSuccessful(st) && rb.len == 64 && checkPattern(rb.buf, rb.len) && n == 1
					&& tmp[0] == s_src[0];
			::__sprt_close(fd);
		}
		report(ok, "fd form: reads borrowed fd, leaves it open", failed);
	}

	// 7. chaining: two sequential writes on the SAME handle reconstruct the file
	{
		fillPattern(s_src, 48);
		bool w1Done = false, w2Done = false;
		Status w1St = Status::Pending, w2St = Status::Pending;

		auto h = looper->writeFile(path, BytesView(s_src, 30), kOverride, [&](Status s) {
			w1St = s;
			w1Done = true;
		});
		bool appended = false;
		if (h) {
			appended = h->appendWrite(BytesView(s_src + 30, 18), [&](Status s) {
				w2St = s;
				w2Done = true;
			}) == Status::Ok;
		}
		if (appended) {
			while (!(w1Done && w2Done)) { looper->wait(dispatch::TimeInterval::Infinite); }
		}
		ReadBuffer rb{s_readStore, 0};
		auto rs = readFileSync(looper, path, rb);
		report(h != nullptr && appended && isSuccessful(w1St) && isSuccessful(w2St)
						&& isSuccessful(rs) && rb.len == 48 && checkPattern(rb.buf, rb.len),
				"chaining: appendWrite after writeFile reconstructs file", failed);
	}

	::__sprt_unlink("sprt_file_test.tmp");
	return failed;
}

} // namespace

void performFileTests() {
	sprt::cout << "\n== runtime file tests ==\n";

	auto looper = dispatch::Looper::acquire();
	if (!looper) {
		sprt::cout << "FAIL  could not acquire looper\n";
		return;
	}

	sprt::cout << "[default engine = " << uint32_t(toInt(looper->getQueue()->getEngine())) << "]\n";
	int failed = runFileSuite(looper);

#if SPRT_LINUX
	// Re-run against a forced-epoll looper (the inline strategy) on a worker
	// thread, since only one looper may exist per thread.
	{
		int epollFailed = -1;
		sprt::thread th([&] {
			auto l = dispatch::Looper::acquire(dispatch::LooperInfo{
				.name = StringView("FileEpoll"),
				.workersCount = 0,
				.engineMask = dispatch::QueueEngine::EPoll,
			});
			if (!l) {
				epollFailed = 1000;
				return;
			}
			sprt::cout << "[epoll engine = " << uint32_t(toInt(l->getQueue()->getEngine()))
					   << "]\n";
			epollFailed = runFileSuite(l);
		});
		th.join();
		if (epollFailed > 0) {
			failed += epollFailed;
		}
	}
#endif

	sprt::cout << "file tests: " << (failed == 0 ? "ALL PASS" : "FAILURES")
			   << " (failures=" << failed << ")\n";
}

} // namespace sprt
