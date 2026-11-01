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

#include "lfs.h"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

// Time per call of every implementation on the same data, one row per
// (function, variant, size, implementation). A cell is the median, p90 and
// minimum over `reps` batches, each batch long enough to dwarf the clock step.

namespace lfs {

namespace {

using u8 = unsigned char;

constexpr uint64_t BatchNs = 200000;
constexpr unsigned MaxReps = 1000;

volatile uintptr_t s_sink;

uint64_t nowNs() {
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

// The smallest step the clock really makes, not what clock_getres claims.
uint64_t clockStep() {
	uint64_t best = ~uint64_t(0);
	for (int i = 0; i < 1000; ++i) {
		auto a = nowNs();
		uint64_t b;
		do { b = nowNs(); } while (b == a);
		if (b - a < best) {
			best = b - a;
		}
	}
	return best;
}

struct Stat {
	double median, p90, min;
	uint64_t batch;
	unsigned reps;
};

void sortDoubles(double *v, unsigned n) {
	for (unsigned i = 1; i < n; ++i) {
		auto x = v[i];
		unsigned j = i;
		for (; j > 0 && v[j - 1] > x; --j) { v[j] = v[j - 1]; }
		v[j] = x;
	}
}

template <typename Op>
Stat measure(const Options &opts, const Op &op) {
	// calibrate: how many calls make a batch of BatchNs
	uint64_t calls = 1;
	for (;;) {
		auto t0 = nowNs();
		for (uint64_t i = 0; i < calls; ++i) { op(); }
		auto dt = nowNs() - t0;
		if (dt >= BatchNs / 10 || calls >= (uint64_t(1) << 30)) {
			calls = dt ? calls * BatchNs / dt : calls * 10;
			break;
		}
		calls *= 10;
	}
	if (calls == 0) {
		calls = 1;
	}

	double times[MaxReps];
	auto reps = opts.reps < MaxReps ? opts.reps : MaxReps;
	for (unsigned r = 0; r < opts.warmup + reps; ++r) {
		auto t0 = nowNs();
		for (uint64_t i = 0; i < calls; ++i) { op(); }
		auto dt = nowNs() - t0;
		if (r >= opts.warmup) {
			times[r - opts.warmup] = double(dt) / double(calls);
		}
	}
	sortDoubles(times, reps);
	return Stat{times[reps / 2], times[(reps * 9) / 10 < reps ? (reps * 9) / 10 : reps - 1],
		times[0], calls, reps};
}

struct Variant {
	const char *name;
	size_t sa, da; // offsets of the source (or first operand) and destination
	long overlap;  // memmove only: 0 none, <0 forward (dst below src), >0 backward
};

const Variant s_plain[] = {{"al", 0, 0, 0}, {"un", 1, 3, 0}};
const Variant s_move[] = {{"al", 0, 0, 0}, {"un", 1, 3, 0}, {"fwd", 0, 0, -8}, {"bwd", 0, 0, 8}};

const size_t s_sizes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64, 128,
	256, 1 << 10, 4 << 10, 16 << 10, 64 << 10, 256 << 10, 1 << 20, 8 << 20};

u8 *mapBuffer(size_t size) {
	auto p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return p == MAP_FAILED ? nullptr : static_cast<u8 *>(p);
}

// Lays out the data a function scans in full: equal operands, no early match.
void prepare(Func f, u8 *a, u8 *b, size_t n) {
	switch (f) {
	case Func::Memcmp:
	case Func::Bcmp:
		for (size_t i = 0; i < n; ++i) { a[i] = b[i] = u8(i * 7 + 1); }
		break;
	case Func::Memchr:
		memset(a, 'a', n);
		if (n) {
			a[n - 1] = 'z';
		}
		break;
	case Func::Memrchr:
		memset(a, 'a', n);
		if (n) {
			a[0] = 'z';
		}
		break;
	case Func::Strlen:
	case Func::Strnlen:
	case Func::Strchr:
	case Func::Strrchr:
		memset(a, 'a', n);
		a[n] = 0;
		break;
	case Func::Strcmp:
	case Func::Strncmp:
		memset(a, 'a', n);
		memset(b, 'a', n);
		a[n] = b[n] = 0;
		break;
	default: memset(a, 0x5A, n + 64); break;
	}
}

Stat run(const Options &opts, const Impl &im, Func f, u8 *a, u8 *b, size_t n, const Variant &v) {
	auto src = a + v.sa;
	auto dst = b + v.da;
	switch (f) {
	case Func::Memcpy:
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fMemcpy(dst, src, n)); });
	case Func::Memmove:
		if (v.overlap) {
			auto base = a + 64;
			auto d = v.overlap < 0 ? base : base + v.overlap;
			auto s = v.overlap < 0 ? base - v.overlap : base;
			return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fMemmove(d, s, n)); });
		}
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fMemmove(dst, src, n)); });
	case Func::Memset:
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fMemset(dst, 0x5A, n)); });
	case Func::Memcmp:
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fMemcmp(src, dst, n)); });
	case Func::Bcmp:
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fBcmp(src, dst, n)); });
	case Func::Memchr:
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fMemchr(src, 'z', n)); });
	case Func::Memrchr:
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fMemrchr(src, 'z', n)); });
	case Func::Strlen: {
		auto s = reinterpret_cast<const char *>(src);
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fStrlen(s)); });
	}
	case Func::Strnlen: {
		auto s = reinterpret_cast<const char *>(src);
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fStrnlen(s, n + 1)); });
	}
	case Func::Strcmp: {
		auto l = reinterpret_cast<const char *>(src);
		auto r = reinterpret_cast<const char *>(dst);
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fStrcmp(l, r)); });
	}
	case Func::Strncmp: {
		auto l = reinterpret_cast<const char *>(src);
		auto r = reinterpret_cast<const char *>(dst);
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fStrncmp(l, r, n + 1)); });
	}
	case Func::Strchr: {
		auto s = reinterpret_cast<const char *>(src);
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fStrchr(s, 'z')); });
	}
	case Func::Strrchr: {
		auto s = reinterpret_cast<const char *>(src);
		return measure(opts, [&] { s_sink = s_sink + uintptr_t(im.fStrrchr(s, 'z')); });
	}
	case Func::Count: break;
	}
	return Stat{};
}

} // namespace

int runBench(const Options &opts) {
	Func only = Func::Count;
	if (opts.func && !findFunc(opts.func, only)) {
		printf("unknown function: %s\n", opts.func);
		return 1;
	}
	const Impl *onlyImpl = nullptr;
	if (opts.impl && !(onlyImpl = findImpl(opts.impl))) {
		printf("unknown implementation: %s\n", opts.impl);
		return 1;
	}

	auto bufSize = opts.maxSize + 4096;
	auto a = mapBuffer(bufSize);
	auto b = mapBuffer(bufSize);
	if (!a || !b) {
		printf("cannot map %zu bytes\n", bufSize);
		return 1;
	}
	memset(a, 1, bufSize);
	memset(b, 1, bufSize);

#ifndef NDEBUG
	printf("warning: a debug build; the times below are for scale only\n");
#endif
	printf("clock step %llu ns, batch %llu ns, %u reps after %u warmup\n",
			(unsigned long long)clockStep(), (unsigned long long)BatchNs, opts.reps, opts.warmup);
	printf("%-8s %-4s %8s %-9s %10s %10s %10s %8s %10s\n", "func", "var", "size", "impl", "median",
			"p90", "min", "GB/s", "calls");

	for (uint32_t fi = 0; fi < uint32_t(Func::Count); ++fi) {
		auto f = Func(fi);
		if (only != Func::Count && f != only) {
			continue;
		}
		auto variants = f == Func::Memmove ? s_move : s_plain;
		auto nVariants = f == Func::Memmove ? sizeof(s_move) / sizeof(Variant)
											: sizeof(s_plain) / sizeof(Variant);
		for (size_t vi = 0; vi < nVariants; ++vi) {
			auto &v = variants[vi];
			for (auto n : s_sizes) {
				if (n > opts.maxSize) {
					continue;
				}
				prepare(f, a + v.sa, b + v.da, n);
				for (size_t ii = 0; ii < implCount(); ++ii) {
					auto &im = impl(ii);
					if ((onlyImpl && &im != onlyImpl) || !im.has(f)) {
						continue;
					}
					auto st = run(opts, im, f, a, b, n, v);
					auto gbs = st.median > 0 ? double(n) / st.median : 0.0;
					printf("%-8s %-4s %8zu %-9s %10.2f %10.2f %10.2f %8.2f %10llu\n", funcName(f),
							v.name, n, im.name, st.median, st.p90, st.min, gbs,
							(unsigned long long)st.batch * st.reps);
				}
			}
		}
	}
	return 0;
}

} // namespace lfs
