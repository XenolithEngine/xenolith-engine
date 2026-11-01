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

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// Every implementation is held to the reference across lengths, alignments,
// overlaps and byte values. All buffers sit between PROT_NONE pages, and the
// page tests put the data against them: a read past the end of a string or
// below its start faults, and the SIGSEGV handler names the case.

namespace lfs {

namespace {

using u8 = unsigned char;

constexpr size_t Guard = 32;
constexpr size_t MaxLarge = 65536 + 1;

// Past the sizes where copies and fills change strategy (rep up to 1 MiB on x86_64).
constexpr size_t MaxHuge = (size_t(1) << 21) + 17;
const size_t s_huge[] = {(size_t(1) << 20) + 1, MaxHuge};

// Both sides of dst - src being 1..15 modulo 64.
const size_t s_hugeAligns[][2] = {{0, 0}, {1, 3}, {3, 1}, {0, 16}};
constexpr unsigned MaxFailuresShown = 10;

size_t s_pageSize = 4096;
char s_current[256] = "setup";

void handleFault(int sig) {
	char buf[320];
	auto len = snprintf(buf, sizeof(buf), "FAIL: signal %d in %s\n", sig, s_current);
	if (len > 0) {
		auto r = write(2, buf, size_t(len));
		(void)r;
	}
	_exit(2);
}

// [lo, hi) is readable and writable; the pages on both sides are not.
struct Region {
	u8 *lo = nullptr;
	u8 *hi = nullptr;

	bool init(size_t size) {
		size = (size + s_pageSize - 1) / s_pageSize * s_pageSize;
		auto total = size + 2 * s_pageSize;
		auto p = static_cast<u8 *>(
				mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		if (p == MAP_FAILED) {
			return false;
		}
		if (mprotect(p, s_pageSize, PROT_NONE) != 0
				|| mprotect(p + s_pageSize + size, s_pageSize, PROT_NONE) != 0) {
			return false;
		}
		lo = p + s_pageSize;
		hi = lo + size;
		return true;
	}
};

struct Buffers {
	Region a, b;	   // general data, two of them for the two-operand functions
	Region pa, pb;	   // one page each, for the tests against a protected page
	u8 *scratch = nullptr; // the reference's copy of `a`
	size_t size = 0;
};

Buffers s_buf;

// Bytes that are never zero and include the high half (0x80..0xFF).
inline u8 dataByte(size_t i, size_t seed = 0) { return u8((i * 29 + seed * 7 + 3) % 255 + 1); }

inline u8 fillByte(size_t i) { return u8(i * 7 + 0x5A); }

struct Ctx {
	const Options *opts;
	const Impl *impl;
	Func func;
	uint64_t checks = 0;
	uint64_t failures = 0;

	void setCase(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
		auto len = snprintf(s_current, sizeof(s_current), "%s %s: ", impl->name, funcName(func));
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(s_current + len, sizeof(s_current) - size_t(len), fmt, ap);
		va_end(ap);
	}

	bool expect(bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4))) {
		++checks;
		if (!ok) {
			if (failures < MaxFailuresShown) {
				printf("FAIL: %s %s: ", impl->name, funcName(func));
				va_list ap;
				va_start(ap, fmt);
				vprintf(fmt, ap);
				va_end(ap);
				printf("\n");
			}
			++failures;
		}
		return ok;
	}
};

inline int sgn(int v) { return (v > 0) - (v < 0); }

// Offset of a returned pointer from `base`, -1 for null; comparable across calls.
inline long off(const void *p, const void *base) {
	return p ? long(static_cast<const u8 *>(p) - static_cast<const u8 *>(base)) : -1;
}

struct Lengths {
	size_t list[512];
	size_t count = 0;

	Lengths() {
		for (size_t n = 0; n <= 256; ++n) { list[count++] = n; }
		for (size_t k = 9; k <= 16; ++k) {
			list[count++] = (size_t(1) << k) - 1;
			list[count++] = size_t(1) << k;
			list[count++] = (size_t(1) << k) + 1;
		}
	}
};

const Lengths s_lengths;

// How many start alignments a length gets: all the small sizes see every
// position of a 16 (or 64, --full) byte block, the large ones a sample.
size_t alignCount(const Ctx &ctx, size_t n) {
	if (n <= 64) {
		return ctx.opts->full ? 64 : 16;
	}
	if (n <= 256) {
		return ctx.opts->full ? 32 : 8;
	}
	return 0;
}

const size_t s_largeAligns[] = {0, 1, 7, 8, 15};

template <typename Fn>
void forEachAlign(const Ctx &ctx, size_t n, const Fn &fn) {
	if (auto c = alignCount(ctx, n)) {
		for (size_t a = 0; a < c; ++a) { fn(a); }
	} else {
		for (auto a : s_largeAligns) { fn(a); }
	}
}

// Positions of a difference or a match inside n bytes.
template <typename Fn>
void forEachPos(size_t n, const Fn &fn) {
	if (n <= 32) {
		for (size_t p = 0; p < n; ++p) { fn(p); }
		return;
	}
	const size_t pos[] = {0, 1, 7, 8, 15, 16, 31, n / 2, n - 17, n - 9, n - 8, n - 1};
	for (auto p : pos) { fn(p); }
}

// Start alignments of the two operands of a comparison.
template <typename Fn>
void forEachAlignPair(const Ctx &ctx, size_t n, const Fn &fn) {
	if (n <= 64) {
		auto c = ctx.opts->full ? size_t(16) : size_t(8);
		for (size_t sa = 0; sa < c; ++sa) {
			for (size_t sb = 0; sb < c; ++sb) { fn(sa, sb); }
		}
	} else if (n <= 256) {
		const size_t al[] = {0, 3, 1, 7};
		auto c = ctx.opts->full ? size_t(4) : size_t(2);
		for (size_t i = 0; i < c; ++i) {
			for (size_t j = 0; j < c; ++j) { fn(al[i], al[j]); }
		}
	} else {
		fn(0, 0);
		fn(1, 3);
		fn(7, 0);
	}
}

void fillRange(u8 *p, size_t n, size_t base) {
	for (size_t i = 0; i < n; ++i) { p[i] = fillByte(base + i); }
}

// ---- copying functions

void testMemcpy(Ctx &ctx) {
	auto &impl = *ctx.impl;
	u8 *src0 = s_buf.a.lo + Guard;
	u8 *dst0 = s_buf.b.lo + Guard;
	for (size_t i = 0; i < MaxHuge + 128; ++i) { src0[i] = dataByte(i); }
	fillRange(s_buf.b.lo, MaxHuge + 256, 0);

	for (size_t li = 0; li < s_lengths.count; ++li) {
		auto n = s_lengths.list[li];
		ctx.setCase("n=%zu", n);
		forEachAlign(ctx, n, [&](size_t sa) {
			forEachAlign(ctx, n, [&](size_t da) {
				auto dst = dst0 + da;
				auto ret = impl.fMemcpy(dst, src0 + sa, n);
				ctx.expect(ret == dst, "n=%zu sa=%zu da=%zu: returned %+ld", n, sa, da,
						off(ret, dst));
				bool ok = memcmp(dst, src0 + sa, n) == 0;
				for (size_t g = 1; g <= Guard; ++g) {
					ok = ok && dst[-long(g)] == fillByte(size_t(dst - s_buf.b.lo) - g)
							&& dst[n + g - 1] == fillByte(size_t(dst - s_buf.b.lo) + n + g - 1);
				}
				ctx.expect(ok, "n=%zu sa=%zu da=%zu: wrong bytes in or around dst", n, sa, da);
				fillRange(dst, n, size_t(dst - s_buf.b.lo));
			});
		});
	}
	for (auto n : s_huge) {
		ctx.setCase("n=%zu", n);
		for (auto &al : s_hugeAligns) {
			auto dst = dst0 + al[1];
			impl.fMemcpy(dst, src0 + al[0], n);
			bool ok = memcmp(dst, src0 + al[0], n) == 0 && dst[-1] == fillByte(size_t(dst - s_buf.b.lo) - 1)
					&& dst[n] == fillByte(size_t(dst - s_buf.b.lo) + n);
			ctx.expect(ok, "n=%zu sa=%zu da=%zu: wrong bytes in or around dst", n, al[0], al[1]);
			fillRange(dst, n, size_t(dst - s_buf.b.lo));
		}
	}
}

void testMemmove(Ctx &ctx) {
	auto &impl = *ctx.impl;
	auto &ref = refImpl();
	u8 *act = s_buf.a.lo;
	u8 *exp = s_buf.scratch;

	for (size_t li = 0; li < s_lengths.count; ++li) {
		auto n = s_lengths.list[li];
		ctx.setCase("n=%zu", n);
		long deltas[160];
		size_t nd = 0;
		if (n <= 256) {
			for (long d = -64; d <= 64; ++d) { deltas[nd++] = d; }
		} else {
			const long ds[] = {-64, -8, -7, -1, 1, 7, 8, 64};
			for (auto d : ds) { deltas[nd++] = d; }
			deltas[nd++] = long(n / 2);
			deltas[nd++] = -long(n / 2);
		}
		if (n > 0) {
			deltas[nd++] = long(n - 1);
			deltas[nd++] = -long(n - 1);
		}
		deltas[nd++] = long(n);
		deltas[nd++] = -long(n);
		deltas[nd++] = long(n + 1);
		deltas[nd++] = -long(n + 1);

		const size_t aligns[] = {0, 3};
		auto nAligns = n <= 256 ? size_t(2) : size_t(1);
		for (size_t di = 0; di < nd; ++di) {
			auto d = deltas[di];
			auto ad = size_t(d < 0 ? -d : d);
			for (size_t ai = 0; ai < nAligns; ++ai) {
				auto al = aligns[ai];
				auto span = n + ad + 2 * Guard + al;
				if (span > s_buf.size) {
					continue;
				}
				auto lo = Guard + al;
				auto srcOff = d >= 0 ? lo : lo + ad;
				auto dstOff = d >= 0 ? lo + ad : lo;
				fillRange(act, span, 0);
				fillRange(exp, span, 0);
				ref.fMemmove(exp + dstOff, exp + srcOff, n);
				auto ret = impl.fMemmove(act + dstOff, act + srcOff, n);
				ctx.expect(ret == act + dstOff, "n=%zu delta=%ld align=%zu: returned %+ld", n, d,
						al, off(ret, act + dstOff));
				ctx.expect(memcmp(act, exp, span) == 0, "n=%zu delta=%ld align=%zu: wrong bytes", n,
						d, al);
			}
		}
	}
	for (auto n : s_huge) {
		ctx.setCase("n=%zu", n);
		const long deltas[] = {-8, 8, -1, 3, -long(n / 2), long(n / 2)};
		for (auto d : deltas) {
			auto ad = size_t(d < 0 ? -d : d);
			auto span = n + ad + 2 * Guard;
			auto srcOff = d >= 0 ? Guard : Guard + ad;
			auto dstOff = d >= 0 ? Guard + ad : Guard;
			fillRange(act, span, 0);
			fillRange(exp, span, 0);
			ref.fMemmove(exp + dstOff, exp + srcOff, n);
			impl.fMemmove(act + dstOff, act + srcOff, n);
			ctx.expect(memcmp(act, exp, span) == 0, "n=%zu delta=%ld: wrong bytes", n, d);
		}
	}
}

void testMemset(Ctx &ctx) {
	auto &impl = *ctx.impl;
	u8 *dst0 = s_buf.b.lo + Guard;
	fillRange(s_buf.b.lo, MaxHuge + 256, 0);
	const int values[] = {0, 0xA5, 0x1A5, -1, 0x80};

	for (size_t li = 0; li < s_lengths.count; ++li) {
		auto n = s_lengths.list[li];
		ctx.setCase("n=%zu", n);
		forEachAlign(ctx, n, [&](size_t da) {
			for (auto c : values) {
				auto dst = dst0 + da;
				auto ret = impl.fMemset(dst, c, n);
				ctx.expect(ret == dst, "n=%zu da=%zu c=%d: returned %+ld", n, da, c, off(ret, dst));
				bool ok = true;
				for (size_t i = 0; i < n && ok; ++i) { ok = dst[i] == u8(c); }
				for (size_t g = 1; g <= Guard; ++g) {
					ok = ok && dst[-long(g)] == fillByte(size_t(dst - s_buf.b.lo) - g)
							&& dst[n + g - 1] == fillByte(size_t(dst - s_buf.b.lo) + n + g - 1);
				}
				ctx.expect(ok, "n=%zu da=%zu c=%d: wrong bytes in or around dst", n, da, c);
				fillRange(dst, n, size_t(dst - s_buf.b.lo));
			}
		});
	}
	for (auto n : s_huge) {
		ctx.setCase("n=%zu", n);
		for (auto &al : s_hugeAligns) {
			auto dst = dst0 + al[1];
			impl.fMemset(dst, 0xA5, n);
			bool ok = dst[-1] == fillByte(size_t(dst - s_buf.b.lo) - 1)
					&& dst[n] == fillByte(size_t(dst - s_buf.b.lo) + n);
			for (size_t i = 0; i < n && ok; ++i) { ok = dst[i] == 0xA5; }
			ctx.expect(ok, "n=%zu da=%zu: wrong bytes in or around dst", n, al[1]);
			fillRange(dst, n, size_t(dst - s_buf.b.lo));
		}
	}
}

// ---- comparisons

// Byte pairs at the first difference; the byte after it differs the other way, so
// an implementation that compares a word in the wrong byte order is caught.
const u8 s_diffPairs[][2] = {{0x7F, 0x80}, {0x80, 0x7F}, {0x10, 0x20}, {0x20, 0x10}, {0x01, 0xFF},
	{0xFF, 0x01}};

// Large lengths take the first two pairs only.
inline size_t diffPairCount(size_t n) { return n <= 64 ? 6 : 2; }

void testMemcmp(Ctx &ctx, bool bcmpMode) {
	auto &impl = *ctx.impl;
	auto fn = bcmpMode ? impl.fBcmp : impl.fMemcmp;
	u8 *a0 = s_buf.a.lo + Guard;
	u8 *b0 = s_buf.b.lo + Guard;

	for (size_t li = 0; li < s_lengths.count; ++li) {
		auto n = s_lengths.list[li];
		ctx.setCase("n=%zu", n);
		auto align = [&](size_t sa, size_t sb) {
			auto a = a0 + sa;
			auto b = b0 + sb;
			for (size_t i = 0; i < n + 1; ++i) { a[i] = b[i] = dataByte(i); }
			b[n] = u8(a[n] + 1); // past the end: must not be looked at
			ctx.expect(fn(a, b, n) == 0, "n=%zu sa=%zu sb=%zu: equal ranges differ", n, sa, sb);
			forEachPos(n, [&](size_t p) {
				for (size_t pi = 0; pi < diffPairCount(n); ++pi) {
					auto &pair = s_diffPairs[pi];
					a[p] = pair[0];
					b[p] = pair[1];
					if (p + 1 < n) {
						a[p + 1] = pair[1];
						b[p + 1] = pair[0];
					}
					auto expect = refImpl().fMemcmp(a, b, n);
					auto got = fn(a, b, n);
					if (bcmpMode) {
						ctx.expect((got != 0) == (expect != 0), "n=%zu sa=%zu sb=%zu p=%zu: %d", n,
								sa, sb, p, got);
					} else {
						ctx.expect(sgn(got) == sgn(expect),
								"n=%zu sa=%zu sb=%zu p=%zu %02x/%02x: %d, expected sign %d", n, sa,
								sb, p, pair[0], pair[1], got, sgn(expect));
					}
					a[p] = b[p] = dataByte(p);
					if (p + 1 < n) {
						a[p + 1] = b[p + 1] = dataByte(p + 1);
					}
				}
			});
		};
		forEachAlignPair(ctx, n, align);
	}
}

void testStrcmp(Ctx &ctx, bool bounded) {
	auto &impl = *ctx.impl;
	auto &ref = refImpl();
	char *a0 = reinterpret_cast<char *>(s_buf.a.lo + Guard);
	char *b0 = reinterpret_cast<char *>(s_buf.b.lo + Guard);

	auto call = [&](const char *a, const char *b, size_t lim, int &expect) {
		if (bounded) {
			expect = ref.fStrncmp(a, b, lim);
			return impl.fStrncmp(a, b, lim);
		}
		expect = ref.fStrcmp(a, b);
		return impl.fStrcmp(a, b);
	};

	for (size_t li = 0; li < s_lengths.count; ++li) {
		auto n = s_lengths.list[li];
		ctx.setCase("n=%zu", n);
		auto align = [&](size_t sa, size_t sb) {
			auto a = a0 + sa;
			auto b = b0 + sb;
			for (size_t i = 0; i < n; ++i) { a[i] = b[i] = char(dataByte(i)); }
			a[n] = b[n] = 0;
			a[n + 1] = 'x';
			b[n + 1] = 'y'; // past the terminator: must not matter
			const size_t lims[] = {n, n + 1, n + 100, SIZE_MAX, n / 2};
			for (auto lim : lims) {
				int e;
				auto got = call(a, b, lim, e);
				ctx.expect(sgn(got) == sgn(e), "n=%zu sa=%zu sb=%zu lim=%zu: equal strings: %d", n,
						sa, sb, lim, got);
				if (!bounded) {
					break;
				}
			}
			forEachPos(n, [&](size_t p) {
				for (size_t pi = 0; pi < diffPairCount(n); ++pi) {
					auto &pair = s_diffPairs[pi];
					a[p] = char(pair[0]);
					b[p] = char(pair[1]);
					if (p + 1 < n) {
						a[p + 1] = char(pair[1]);
						b[p + 1] = char(pair[0]);
					}
					const size_t lims[] = {SIZE_MAX, p, p + 1, n};
					for (auto lim : lims) {
						int e;
						auto got = call(a, b, lim, e);
						ctx.expect(sgn(got) == sgn(e),
								"n=%zu sa=%zu sb=%zu p=%zu lim=%zu %02x/%02x: %d, expected sign %d",
								n, sa, sb, p, lim, pair[0], pair[1], got, sgn(e));
						if (!bounded) {
							break;
						}
					}
					// a shorter string is a prefix of the other one
					a[p] = 0;
					int e;
					auto got = call(a, b, SIZE_MAX, e);
					ctx.expect(sgn(got) == sgn(e), "n=%zu sa=%zu sb=%zu p=%zu: prefix: %d", n, sa,
							sb, p, got);
					got = call(b, a, SIZE_MAX, e);
					ctx.expect(sgn(got) == sgn(e), "n=%zu sa=%zu sb=%zu p=%zu: prefix reversed: %d",
							n, sa, sb, p, got);
					a[p] = b[p] = char(dataByte(p));
					if (p + 1 < n) {
						a[p + 1] = b[p + 1] = char(dataByte(p + 1));
					}
				}
			});
			a[n + 1] = b[n + 1] = 0;
		};
		forEachAlignPair(ctx, n, align);
	}
}

// ---- searches

const u8 s_targets[] = {0x5A, 0x80, 0x00, 0xFF};

// Large lengths take the first two targets and only the plain `c`.
inline size_t targetCount(size_t n) { return n <= 256 ? 4 : 2; }
inline size_t charVariants(size_t n) { return n <= 256 ? 3 : 1; }

// A data byte other than `t`, never zero.
inline u8 dataByteWithout(size_t i, u8 t) {
	auto v = dataByte(i, t);
	return v != t ? v : u8(v == 0xFF ? 0xFE : v + 1);
}

// Fills n bytes from `p` with data that does not contain `t`.
void fillWithout(u8 *p, size_t n, u8 t) {
	for (size_t i = 0; i < n; ++i) { p[i] = dataByteWithout(i, t); }
}

void testMemchr(Ctx &ctx, bool reverse) {
	auto &impl = *ctx.impl;
	auto fn = reverse ? impl.fMemrchr : impl.fMemchr;
	auto rfn = reverse ? refImpl().fMemrchr : refImpl().fMemchr;
	u8 *s0 = s_buf.a.lo + Guard;

	for (size_t li = 0; li < s_lengths.count; ++li) {
		auto n = s_lengths.list[li];
		ctx.setCase("n=%zu", n);
		forEachAlign(ctx, n, [&](size_t al) {
			auto s = s0 + al;
			for (size_t ti = 0; ti < targetCount(n); ++ti) {
				auto t = s_targets[ti];
				fillWithout(s - Guard, n + 2 * Guard, t);
				s[-1] = s[n] = t; // just outside: must not be found
				auto got = fn(s, t, n);
				ctx.expect(got == nullptr, "n=%zu al=%zu c=%02x: absent, found at %ld", n, al, t,
						off(got, s));
				forEachPos(n, [&](size_t p) {
					s[p] = t;
					auto q = reverse ? p / 2 : p + (n - p) / 2; // a second match beyond the first
					auto hasSecond = reverse ? q < p : q > p;
					if (hasSecond) {
						s[q] = t;
					}
					const int cs[] = {int(t), int(t) + 256, int(t) - 256};
					for (size_t ci = 0; ci < charVariants(n); ++ci) {
						auto c = cs[ci];
						auto e = rfn(s, c, n);
						auto r = fn(s, c, n);
						ctx.expect(r == e, "n=%zu al=%zu c=%d p=%zu: found at %ld, expected %ld", n,
								al, c, p, off(r, s), off(e, s));
					}
					s[p] = dataByteWithout(p, t);
					if (hasSecond) {
						s[q] = dataByteWithout(q, t);
					}
				});
			}
		});
	}
}

void testStrlen(Ctx &ctx, bool bounded) {
	auto &impl = *ctx.impl;
	char *s0 = reinterpret_cast<char *>(s_buf.a.lo + Guard);

	for (size_t li = 0; li < s_lengths.count; ++li) {
		auto n = s_lengths.list[li];
		ctx.setCase("n=%zu", n);
		forEachAlign(ctx, n, [&](size_t al) {
			auto s = s0 + al;
			for (size_t i = 0; i < n; ++i) { s[i] = char(dataByte(i)); }
			s[n] = 0;
			s[n + 1] = 'x';
			if (!bounded) {
				auto r = impl.fStrlen(s);
				ctx.expect(r == n, "n=%zu al=%zu: %zu", n, al, r);
				return;
			}
			const size_t lims[] = {0, n / 2, n, n + 1, n + 100, SIZE_MAX};
			for (auto lim : lims) {
				auto r = impl.fStrnlen(s, lim);
				auto e = n < lim ? n : lim;
				ctx.expect(r == e, "n=%zu al=%zu max=%zu: %zu", n, al, lim, r);
			}
		});
	}
}

void testStrchr(Ctx &ctx, bool reverse) {
	auto &impl = *ctx.impl;
	auto fn = reverse ? impl.fStrrchr : impl.fStrchr;
	auto rfn = reverse ? refImpl().fStrrchr : refImpl().fStrchr;
	char *s0 = reinterpret_cast<char *>(s_buf.a.lo + Guard);
	const u8 targets[] = {0x5A, 0x80, 0xFF, 0x01};
	const int terminators[] = {0, 256};

	for (size_t li = 0; li < s_lengths.count; ++li) {
		auto n = s_lengths.list[li];
		ctx.setCase("n=%zu", n);
		forEachAlign(ctx, n, [&](size_t al) {
			auto s = s0 + al;
			for (size_t ti = 0; ti < targetCount(n); ++ti) {
				auto t = targets[ti];
				fillWithout(reinterpret_cast<u8 *>(s), n, t);
				s[n] = 0;
				s[n + 1] = char(t); // past the terminator: must not be found
				auto got = fn(s, t);
				ctx.expect(got == nullptr, "n=%zu al=%zu c=%02x: absent, found at %ld", n, al, t,
						off(got, s));
				for (auto c : terminators) {
					got = fn(s, c);
					ctx.expect(got == s + n, "n=%zu al=%zu c=%d: terminator at %ld", n, al, c,
							off(got, s));
				}
				forEachPos(n, [&](size_t p) {
					s[p] = char(t);
					auto q = reverse ? p / 2 : p + (n - p) / 2;
					auto hasSecond = reverse ? q < p : q > p;
					if (hasSecond) {
						s[q] = char(t);
					}
					const int cs[] = {int(t), int(t) + 256, int(t) - 256};
					for (size_t ci = 0; ci < charVariants(n); ++ci) {
						auto c = cs[ci];
						auto e = rfn(s, c);
						auto r = fn(s, c);
						ctx.expect(r == e, "n=%zu al=%zu c=%d p=%zu: found at %ld, expected %ld", n,
								al, c, p, off(r, s), off(e, s));
					}
					s[p] = char(dataByteWithout(p, t));
					if (hasSecond) {
						s[q] = char(dataByteWithout(q, t));
					}
				});
			}
		});
	}
}

// ---- against a protected page

// Data that ends at the last byte before a PROT_NONE page, or starts at the first
// byte after one: any read outside the object faults.
void testPageEdges(Ctx &ctx) {
	auto &impl = *ctx.impl;
	u8 *hi = s_buf.pa.hi;
	u8 *lo = s_buf.pa.lo;
	u8 *hi2 = s_buf.pb.hi;
	const size_t maxN = 160;

	for (size_t n = 0; n <= maxN; ++n) {
		ctx.setCase("page edge n=%zu", n);
		switch (ctx.func) {
		case Func::Memcpy:
			fillRange(s_buf.pb.lo, s_pageSize, 0);
			impl.fMemcpy(hi2 - n, hi - n, n);
			impl.fMemcpy(s_buf.pb.lo, lo, n);
			ctx.expect(memcmp(hi2 - n, hi - n, n) == 0 && memcmp(s_buf.pb.lo, lo, n) == 0,
					"page edge n=%zu: wrong bytes", n);
			break;
		case Func::Memmove:
			for (size_t i = 0; i < n + 8 && i < s_pageSize; ++i) { hi[-1 - long(i)] = dataByte(i); }
			if (n + 8 <= s_pageSize) {
				impl.fMemmove(hi - n, hi - n - 8, n);
				impl.fMemmove(hi - n - 8, hi - n, n);
				impl.fMemmove(lo, lo + 8, n);
				impl.fMemmove(lo + 8, lo, n);
			}
			ctx.expect(true, "page edge");
			break;
		case Func::Memset:
			impl.fMemset(hi - n, 0x11, n);
			impl.fMemset(lo, 0x22, n);
			{
				bool ok = true;
				for (size_t i = 0; i < n; ++i) { ok = ok && hi[-1 - long(i)] == 0x11 && lo[i] == 0x22; }
				ctx.expect(ok, "page edge n=%zu: wrong bytes", n);
			}
			break;
		case Func::Memcmp:
		case Func::Bcmp: {
			auto fn = ctx.func == Func::Memcmp ? impl.fMemcmp : impl.fBcmp;
			for (size_t i = 0; i < n; ++i) { hi[-1 - long(i)] = hi2[-1 - long(i)] = dataByte(i); }
			ctx.expect(fn(hi - n, hi2 - n, n) == 0, "page edge n=%zu: equal ranges differ", n);
			for (size_t d = 1; d < 16 && d <= n; ++d) {
				// the other operand is d bytes off the page end: different relative alignment
				memmove(hi2 - n - d, hi2 - n, n);
				ctx.expect(fn(hi - n, hi2 - n - d, n) == 0 && fn(hi2 - n - d, hi - n, n) == 0,
						"page edge n=%zu d=%zu: equal ranges differ", n, d);
				memmove(hi2 - n, hi2 - n - d, n);
			}
			break;
		}
		case Func::Memchr:
			fillWithout(hi - n, n, 0x5A);
			ctx.expect(impl.fMemchr(hi - n, 0x5A, n) == nullptr, "page edge n=%zu: absent found", n);
			// a range that ends k bytes before the page end, the match absent up to it: the
			// blocks that cover the range must not lead the scan on past its end
			for (size_t k = 1; k < 96 && n + k <= s_pageSize; ++k) {
				fillWithout(hi - n - k, n + k, 0x5A);
				ctx.expect(impl.fMemchr(hi - n - k, 0x5A, n) == nullptr,
						"page edge n=%zu k=%zu: absent found", n, k);
			}
			if (n > 0) {
				hi[-1] = 0x5A;
				// a bound past the page: the match at the last byte must stop the scan
				auto r = impl.fMemchr(hi - n, 0x5A, n + s_pageSize);
				ctx.expect(r == hi - 1, "page edge n=%zu: match at %ld", n, off(r, hi - n));
			}
			break;
		case Func::Memrchr:
			fillWithout(lo, n, 0x5A);
			ctx.expect(impl.fMemrchr(lo, 0x5A, n) == nullptr, "page edge n=%zu: absent found", n);
			fillWithout(hi - n, n, 0x5A);
			ctx.expect(impl.fMemrchr(hi - n, 0x5A, n) == nullptr, "page edge n=%zu: absent found",
					n);
			break;
		case Func::Strlen:
		case Func::Strnlen:
		case Func::Strchr:
		case Func::Strrchr: {
			// n bytes and the terminator end at the page end
			auto s = reinterpret_cast<char *>(hi - n - 1);
			fillWithout(reinterpret_cast<u8 *>(s), n, 0x5A);
			hi[-1] = 0;
			if (ctx.func == Func::Strlen) {
				ctx.expect(impl.fStrlen(s) == n, "page edge n=%zu", n);
				auto t = reinterpret_cast<char *>(lo);
				fillWithout(lo, n, 0x5A);
				lo[n] = 0;
				ctx.expect(impl.fStrlen(t) == n, "page start n=%zu", n);
			} else if (ctx.func == Func::Strnlen) {
				ctx.expect(impl.fStrnlen(s, n + s_pageSize) == n, "page edge n=%zu", n);
				// no terminator inside the bound, which ends at the page end
				auto t = reinterpret_cast<char *>(hi - n);
				fillWithout(hi - n, n, 0x5A);
				ctx.expect(impl.fStrnlen(t, n) == n, "page edge unterminated n=%zu", n);
			} else {
				auto fn = ctx.func == Func::Strchr ? impl.fStrchr : impl.fStrrchr;
				ctx.expect(fn(s, 0x5A) == nullptr, "page edge n=%zu: absent found", n);
				ctx.expect(fn(s, 0) == s + n, "page edge n=%zu: terminator", n);
			}
			break;
		}
		case Func::Strcmp:
		case Func::Strncmp: {
			auto s1 = reinterpret_cast<char *>(hi - n - 1);
			for (size_t d = 0; d < 16; ++d) {
				auto s2 = reinterpret_cast<char *>(hi2 - n - 1 - d);
				for (size_t i = 0; i < n; ++i) { s1[i] = s2[i] = char(dataByte(i)); }
				s1[n] = s2[n] = 0;
				int r1, r2;
				if (ctx.func == Func::Strcmp) {
					r1 = impl.fStrcmp(s1, s2);
					r2 = impl.fStrcmp(s2, s1);
				} else {
					r1 = impl.fStrncmp(s1, s2, n + s_pageSize);
					r2 = impl.fStrncmp(s2, s1, SIZE_MAX);
				}
				ctx.expect(r1 == 0 && r2 == 0, "page edge n=%zu d=%zu: equal strings differ", n,
						d);
				if (n > 0) {
					s2[n - 1] = char(s2[n - 1] ^ 0x40);
					auto e = refImpl().fStrcmp(s1, s2);
					r1 = ctx.func == Func::Strcmp ? impl.fStrcmp(s1, s2)
												  : impl.fStrncmp(s1, s2, n + s_pageSize);
					ctx.expect(sgn(r1) == sgn(e), "page edge n=%zu d=%zu: last byte differs: %d",
							n, d, r1);
				}
			}
			break;
		}
		case Func::Count: break;
		}
	}
}

// ---- random cases

struct Rng {
	uint64_t state = 0x9E3779B97F4A7C15ull;

	uint64_t next() {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		return state;
	}

	size_t below(size_t n) { return size_t(next() % n); }

	// Mostly small, sometimes up to `max`.
	size_t length(size_t max) {
		switch (below(4)) {
		case 0: return below(17);
		case 1: return below(129);
		case 2: return below(1025);
		default: return below(max + 1);
		}
	}
};

void testRandom(Ctx &ctx) {
	auto &impl = *ctx.impl;
	auto &ref = refImpl();
	Rng rng;
	auto iterations = ctx.opts->full ? 200000 : 5000;
	u8 *a = s_buf.a.lo + Guard;
	u8 *b = s_buf.b.lo + Guard;
	u8 *e = s_buf.scratch + Guard;

	for (int it = 0; it < iterations; ++it) {
		auto n = rng.length(4096);
		auto sa = rng.below(64);
		auto sb = rng.below(64);
		auto alphabet = 1 + rng.below(4); // few distinct bytes: frequent matches
		for (size_t i = 0; i < n + 128; i += 8) {
			auto x = rng.next(), y = rng.next();
			for (size_t k = 0; k < 8; ++k, x >>= 8, y >>= 8) {
				a[i + k] = u8(0x7E + (x & 0xFF) % alphabet);
				b[i + k] = u8(0x7E + (y & 0xFF) % alphabet);
			}
		}
		ctx.setCase("random #%d n=%zu sa=%zu sb=%zu", it, n, sa, sb);
		switch (ctx.func) {
		case Func::Memcpy:
		case Func::Memmove:
		case Func::Memset: {
			memcpy(e, b, n + 128);
			auto c = int(rng.next());
			if (ctx.func == Func::Memcpy) {
				ref.fMemcpy(e + sb, a + sa, n);
				impl.fMemcpy(b + sb, a + sa, n);
			} else if (ctx.func == Func::Memset) {
				ref.fMemset(e + sb, c, n);
				impl.fMemset(b + sb, c, n);
			} else {
				memcpy(e, a, n + 128);
				ref.fMemmove(e + sb, e + sa, n);
				impl.fMemmove(a + sb, a + sa, n);
				b = a;
			}
			ctx.expect(memcmp(b, e, n + 128) == 0, "random #%d n=%zu sa=%zu sb=%zu c=%d", it, n,
					sa, sb, c);
			b = s_buf.b.lo + Guard;
			break;
		}
		case Func::Memcmp:
		case Func::Bcmp: {
			if (rng.below(2)) {
				memcpy(b + sb, a + sa, n);
			}
			auto r = refImpl().fMemcmp(a + sa, b + sb, n);
			auto g = ctx.func == Func::Memcmp ? impl.fMemcmp(a + sa, b + sb, n)
											  : impl.fBcmp(a + sa, b + sb, n);
			ctx.expect(ctx.func == Func::Memcmp ? sgn(g) == sgn(r) : (g != 0) == (r != 0),
					"random #%d n=%zu sa=%zu sb=%zu: %d vs %d", it, n, sa, sb, g, r);
			break;
		}
		case Func::Memchr:
		case Func::Memrchr: {
			auto c = int(0x7E + rng.below(alphabet + 1));
			auto r = ctx.func == Func::Memchr ? ref.fMemchr(a + sa, c, n) : ref.fMemrchr(a + sa, c, n);
			auto g = ctx.func == Func::Memchr ? impl.fMemchr(a + sa, c, n)
											  : impl.fMemrchr(a + sa, c, n);
			ctx.expect(r == g, "random #%d n=%zu sa=%zu c=%d: %ld vs %ld", it, n, sa, c,
					off(g, a + sa), off(r, a + sa));
			break;
		}
		case Func::Strlen:
		case Func::Strnlen:
		case Func::Strchr:
		case Func::Strrchr:
		case Func::Strcmp:
		case Func::Strncmp: {
			auto s1 = reinterpret_cast<char *>(a + sa);
			auto s2 = reinterpret_cast<char *>(b + sb);
			s1[n] = 0;
			s2[rng.length(n)] = 0;
			if (rng.below(2)) {
				memcpy(s2, s1, n + 1);
			}
			auto lim = rng.length(n + 16);
			auto c = int(0x7E + rng.below(alphabet + 1)) - (rng.below(2) ? 256 : 0);
			bool ok = true;
			switch (ctx.func) {
			case Func::Strlen: ok = impl.fStrlen(s1) == ref.fStrlen(s1); break;
			case Func::Strnlen: ok = impl.fStrnlen(s1, lim) == ref.fStrnlen(s1, lim); break;
			case Func::Strchr: ok = impl.fStrchr(s1, c) == ref.fStrchr(s1, c); break;
			case Func::Strrchr: ok = impl.fStrrchr(s1, c) == ref.fStrrchr(s1, c); break;
			case Func::Strcmp: ok = sgn(impl.fStrcmp(s1, s2)) == sgn(ref.fStrcmp(s1, s2)); break;
			case Func::Strncmp:
				ok = sgn(impl.fStrncmp(s1, s2, lim)) == sgn(ref.fStrncmp(s1, s2, lim));
				break;
			default: break;
			}
			ctx.expect(ok, "random #%d n=%zu sa=%zu sb=%zu lim=%zu c=%d", it, n, sa, sb, lim, c);
			break;
		}
		case Func::Count: break;
		}
	}
}

void runFunc(Ctx &ctx) {
	switch (ctx.func) {
	case Func::Memcpy: testMemcpy(ctx); break;
	case Func::Memmove: testMemmove(ctx); break;
	case Func::Memset: testMemset(ctx); break;
	case Func::Memcmp: testMemcmp(ctx, false); break;
	case Func::Bcmp: testMemcmp(ctx, true); break;
	case Func::Memchr: testMemchr(ctx, false); break;
	case Func::Memrchr: testMemchr(ctx, true); break;
	case Func::Strlen: testStrlen(ctx, false); break;
	case Func::Strnlen: testStrlen(ctx, true); break;
	case Func::Strcmp: testStrcmp(ctx, false); break;
	case Func::Strncmp: testStrcmp(ctx, true); break;
	case Func::Strchr: testStrchr(ctx, false); break;
	case Func::Strrchr: testStrchr(ctx, true); break;
	case Func::Count: break;
	}
	testPageEdges(ctx);
	testRandom(ctx);
}

bool initBuffers() {
	auto ps = sysconf(_SC_PAGESIZE);
	if (ps > 0) {
		s_pageSize = size_t(ps);
	}
	s_buf.size = 2 * MaxHuge + 4 * Guard + 1024;
	if (!s_buf.a.init(s_buf.size) || !s_buf.b.init(s_buf.size) || !s_buf.pa.init(s_pageSize)
			|| !s_buf.pb.init(s_pageSize)) {
		return false;
	}
	Region scratch;
	if (!scratch.init(s_buf.size)) {
		return false;
	}
	s_buf.scratch = scratch.lo;
	return true;
}

} // namespace

int runConformance(const Options &opts) {
	Func only = Func::Count;
	bool fenvOnly = opts.func && strcmp(opts.func, "fenv") == 0;
	if (opts.func && !fenvOnly && !findFunc(opts.func, only)) {
		printf("unknown function: %s\n", opts.func);
		return 1;
	}
	const Impl *onlyImpl = nullptr;
	bool fenvImpl = opts.impl && (strcmp(opts.impl, "fs") == 0);
	if (opts.impl && !fenvImpl && !(onlyImpl = findImpl(opts.impl))) {
		printf("unknown implementation: %s\n", opts.impl);
		return 1;
	}
	if (!initBuffers()) {
		printf("FAIL: cannot map the test buffers\n");
		return 1;
	}

	signal(SIGSEGV, handleFault);
	signal(SIGBUS, handleFault);

	uint64_t checks = 0, failures = 0;
	for (size_t i = 0; i < implCount() && !fenvOnly && !fenvImpl; ++i) {
		auto &im = impl(i);
		// The reference is not tested against itself, and the host libc (which checks
		// the test rather than the module) only with --full, unless asked by name.
		if (onlyImpl ? &im != onlyImpl
					 : &im == &impl(0) || (!opts.full && strcmp(im.name, "host") == 0)) {
			continue;
		}
		for (uint32_t f = 0; f < uint32_t(Func::Count); ++f) {
			if ((only != Func::Count && Func(f) != only) || !im.has(Func(f))) {
				continue;
			}
			Ctx ctx{&opts, &im, Func(f)};
			runFunc(ctx);
			printf("%-9s %-8s %10llu checks, %llu failures\n", im.name, funcName(Func(f)),
					(unsigned long long)ctx.checks, (unsigned long long)ctx.failures);
			checks += ctx.checks;
			failures += ctx.failures;
		}
	}
	if (!opts.func || fenvOnly) {
		runFenvConformance(opts, checks, failures);
	}
	printf("%llu checks, %llu failures\n", (unsigned long long)checks,
			(unsigned long long)failures);
	return failures ? 1 : 0;
}

} // namespace lfs
