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

// Incremental additions to already-existing sprt-backed STL headers, grouped:
//   - <algorithm> initializer_list min/max (with and without a comparator)
//   - <algorithm> reverse_copy
//   - <bitset> operator[] read + the writable proxy reference (assign / flip / ~)
//   - <tuple> operator=(pair) for 2-element tuples (copy and move)
//   - <string> operator=(string_view)
//   - <atomic> ATOMIC_FLAG_INIT
//   - <cmath> integer-argument overloads (pow / log2 / log10 promote to double)
//   - <chrono> chrono_literals (h / min / s / ms) visible via std::chrono_literals

#include <stdio.h>

#include <algorithm>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sprt::test {

namespace {

// initializer_list min/max are constexpr
static_assert(std::min({4, 2, 7, 1, 5}) == 1);
static_assert(std::max({4, 2, 7, 1, 5}) == 7);

} // namespace

void performStlAdditionsTest() {
	// --- <algorithm> min/max over an initializer_list ---
	printf("minmax_il: min=%d max=%d min_cmp=%d max_cmp=%d\n", std::min({4, 2, 7, 1, 5}),
			std::max({4, 2, 7, 1, 5}),
			// with a comparator that orders by magnitude away from 0 it still picks by <
			std::min({3, 1, 2}, [](int a, int b) { return a < b; }),
			std::max({3, 1, 2}, [](int a, int b) { return a < b; }));

	// --- <algorithm> reverse_copy ---
	{
		const int src[6] = {1, 2, 3, 4, 5, 6};
		int dst[6] = {0};
		std::reverse_copy(src, src + 6, dst);
		printf("reverse_copy: %d %d %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3], dst[4],
				dst[5]);
	}

	// --- <bitset> operator[] read + writable proxy ---
	{
		std::bitset<8> b;
		b[1] = true;
		b[3] = true;
		b[5] = b[3]; // proxy = proxy
		bool r0 = b[0];
		bool r1 = b[1];
		bool notr0 = ~b[0];
		b[1].flip();
		printf("bitset[]: r0=%d r1=%d not_r0=%d after_flip1=%d b3=%d b5=%d count=%zu\n", (int) r0,
				(int) r1, (int) notr0, (int) b[1], (int) b[3], (int) b[5], b.count());
	}

	// --- <tuple> operator=(pair) ---
	{
		std::tuple<int, long> t;
		t = std::pair<int, long>(5, 6L);
		printf("tuple=pair(copy): %d %ld\n", std::get<0>(t), std::get<1>(t));
		std::pair<int, long> src(11, 22L);
		t = std::move(src);
		printf("tuple=pair(move): %d %ld\n", std::get<0>(t), std::get<1>(t));
	}

	// --- <string> operator=(string_view) ---
	{
		std::string s = "initial";
		std::string_view sv = "from_view";
		s = sv;
		printf("string=sv: %s len=%zu\n", s.c_str(), s.size());
	}

	// --- <atomic> ATOMIC_FLAG_INIT ---
	{
		std::atomic_flag af = ATOMIC_FLAG_INIT;
		bool was = af.test_and_set();
		bool now = af.test_and_set();
		af.clear();
		printf("atomic_flag: initial_set=%d second_set=%d\n", (int) was, (int) now);
	}

	// --- <cmath> integer-argument overloads promote to double ---
	printf("cmath_int: pow(2,10)=%d log2(8)=%d log10(1000)=%d pow(2,3)=%.1f\n",
			(int) std::pow(2, 10), (int) std::log2(8), (int) std::log10(1000),
			std::pow(2.0, 3));

	// --- <chrono> chrono_literals ---
	{
		using namespace std::chrono_literals;
		auto mins = 90min;
		auto secs = std::chrono::duration_cast<std::chrono::seconds>(mins);
		auto ms = 1500ms;
		auto hrs = std::chrono::duration_cast<std::chrono::minutes>(2h);
		printf("chrono_lit: 90min=%llds 1500ms=%lldms 2h=%lldmin\n", (long long) secs.count(),
				(long long) ms.count(), (long long) hrs.count());
	}

	// --- <stdexcept> const string& constructors across the hierarchy ---
	// (constructed only, never thrown -- this suite is -fno-exceptions; what()
	// echoes the message). Exercises the new string-taking overloads.
	{
		std::string msg = "bad value";
		std::logic_error le(msg);
		std::runtime_error re(std::string("runtime failed"));
		std::domain_error de(std::string("domain"));
		std::invalid_argument ia(std::string("invalid"));
		std::length_error len(std::string("length"));
		std::out_of_range oor(std::string("out of range"));
		std::range_error rge(std::string("range"));
		std::overflow_error ofe(std::string("overflow"));
		std::underflow_error ufe(std::string("underflow"));
		printf("stdexcept: %s | %s | %s | %s | %s | %s | %s | %s | %s\n", le.what(), re.what(),
				de.what(), ia.what(), len.what(), oor.what(), rge.what(), ofe.what(), ufe.what());
	}

	// --- <new> sized + aligned operator delete ---
	// Deleting an over-aligned object of statically-known size selects the
	// sized-and-aligned deallocation overload added to <new>:
	// operator delete(void*, size_t, align_val_t) (which forwards to the aligned
	// operator delete). Check the storage honours the requested alignment and the
	// new/delete round-trip completes without corruption.
	{
		struct alignas(64) Over {
			int v;
		};
		Over *p = new Over{7};
		bool obj_aligned = (reinterpret_cast<uintptr_t>(p) % 64) == 0;
		int v = p->v;
		delete p; // -> operator delete(void*, size_t, align_val_t)
		printf("overaligned: obj=%d val=%d\n", (int) obj_aligned, v);
	}
}

} // namespace sprt::test
