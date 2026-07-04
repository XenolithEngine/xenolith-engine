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

// <chrono> conformance. On the freestanding x86_64-pc-windows-msvc build <chrono> is
// the sprt-backed STL header; on the Linux host it is the system header. compare.sh
// diffs the two, so the sprt implementation is checked against the system reference.
// Only deterministic operations are printed (durations / time_point arithmetic /
// time_point_cast / floor / ceil / round / <=>); clock now() values are wall-clock
// and therefore only checked structurally (is_steady, monotonicity) without printing.

#include <stdio.h>

#include <chrono>
#include <compare>

namespace sprt::test {

namespace {

namespace ch = std::chrono;

// count() rep differs (signed on libstdc++, unsigned in sprt); print through a fixed
// width so positive magnitudes diff identically.
template <typename _Dur>
long long cnt(const _Dur &__d) {
	return static_cast<long long>(__d.count());
}

} // namespace

void performChronoTest() {
	// duration_cast + count
	printf("dur: s_from_ms=%lld ms_from_s=%lld\n", cnt(ch::duration_cast<ch::seconds>(ch::milliseconds(2500))),
			cnt(ch::duration_cast<ch::milliseconds>(ch::seconds(3))));

	// duration arithmetic
	auto sum = ch::seconds(2) + ch::milliseconds(500);
	printf("arith: sum_ms=%lld diff_ms=%lld\n", cnt(ch::duration_cast<ch::milliseconds>(sum)),
			cnt(ch::duration_cast<ch::milliseconds>(ch::seconds(1) - ch::milliseconds(250))));

	// three-way comparison + rewritten relationals
	printf("ship: lt=%d eq=%d gt=%d rw_lt=%d rw_ge=%d\n", (int) ((ch::seconds(1) <=> ch::seconds(2)) < 0),
			(int) ((ch::seconds(2) <=> ch::seconds(2)) == 0), (int) ((ch::seconds(3) <=> ch::seconds(2)) > 0),
			(int) (ch::milliseconds(999) < ch::seconds(1)), (int) (ch::seconds(1) >= ch::milliseconds(1000)));

	// floor / ceil / round (round ties to even)
	printf("floor: %lld %lld\n", cnt(ch::floor<ch::seconds>(ch::milliseconds(1500))),
			cnt(ch::floor<ch::seconds>(ch::milliseconds(1999))));
	printf("ceil: %lld %lld\n", cnt(ch::ceil<ch::seconds>(ch::milliseconds(1001))),
			cnt(ch::ceil<ch::seconds>(ch::milliseconds(2000))));
	printf("round: r05=%lld r15=%lld r25=%lld r21=%lld\n", cnt(ch::round<ch::seconds>(ch::milliseconds(500))),
			cnt(ch::round<ch::seconds>(ch::milliseconds(1500))), cnt(ch::round<ch::seconds>(ch::milliseconds(2500))),
			cnt(ch::round<ch::seconds>(ch::milliseconds(2100))));

	// time_point: construction / time_since_epoch / arithmetic / difference / cast / compare
	using tp = ch::time_point<ch::system_clock, ch::seconds>;
	tp a(ch::seconds(100));
	auto b = a + ch::seconds(25);
	auto c = ch::seconds(5) + a;
	printf("tp: tse=%lld add=%lld radd=%lld diff=%lld\n", cnt(a.time_since_epoch()), cnt(b.time_since_epoch()),
			cnt(c.time_since_epoch()), cnt(b - a));
	printf("tp_cmp: lt=%d eq=%d ship=%d\n", (int) (a < b), (int) (a == a), (int) ((a <=> b) < 0));
	auto ms_tp = ch::time_point<ch::system_clock, ch::milliseconds>(ch::milliseconds(7800));
	printf("tp_cast: s=%lld\n", cnt(ch::time_point_cast<ch::seconds>(ms_tp).time_since_epoch()));

	// clocks: structural only (no wall-clock values printed)
	int steady_is_steady = (int) ch::steady_clock::is_steady;
	int system_is_steady = (int) ch::system_clock::is_steady;
	auto t1 = ch::steady_clock::now();
	auto t2 = ch::steady_clock::now();
	int monotonic = (int) (t2 >= t1);
	auto sys = ch::system_clock::now();
	auto tt = ch::system_clock::to_time_t(sys);
	auto sys2 = ch::system_clock::from_time_t(tt);
	int roundtrip_ok = (int) (ch::duration_cast<ch::seconds>(sys2.time_since_epoch()).count()
			== ch::duration_cast<ch::seconds>(sys.time_since_epoch()).count());
	(void) ch::high_resolution_clock::now();
	printf("clocks: steady=%d system=%d monotonic=%d to_from_t=%d\n", steady_is_steady, system_is_steady,
			monotonic, roundtrip_ok);
}

} // namespace sprt::test
