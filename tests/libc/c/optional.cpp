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

// std::optional conformance, via the <optional> STL wrapper (so it also builds
// against the system library; on the freestanding build std::optional ==
// sprt::optional). Standard semantics are pinned two ways:
//   * compile-time static_asserts on the type traits the standard mandates
//     (trivial-special-member propagation, conditional-explicit conversions);
//   * runtime printf of the observable results of each operation.
// Deterministic (no addresses, time, locale) so the Linux host (system libstdc++)
// and the x86_64-pc-windows-msvc freestanding (sprt) builds are byte-identical
// under compare.sh. sizeof/alignof are NOT printed: they are implementation-defined
// and legitimately differ between libstdc++ and the sprt (libc++-derived) layout.
//
// NB: value() on a disengaged optional aborts in this -fno-exceptions runtime,
// so this test never calls value() on an empty optional (it uses value_or for
// the empty cases).

#include <stdio.h>

#include <optional>
#include <type_traits>
#include <compare>
#include <initializer_list>
#include <utility>

namespace sprt::test {
namespace {

using std::optional;
using std::nullopt;
using std::make_optional;
using std::in_place;
using std::strong_ordering;
using std::initializer_list;
using std::is_trivially_destructible_v;
using std::is_trivially_copy_constructible_v;
using std::is_trivially_move_constructible_v;
using std::is_trivially_copy_assignable_v;
using std::is_trivially_move_assignable_v;
using std::is_trivially_copyable_v;
using std::is_convertible_v;

// Trivial element type: optional<TrivialPoint> must stay trivially copyable.
struct TrivialPoint {
	int x;
	int y;
};

// Non-trivial element type with user-provided special members. The move
// constructor records that the source was moved from and clobbers its value,
// which lets us observe that a moved-from optional stays *engaged* (standard
// behaviour) rather than being disengaged.
struct Tracked {
	int v;
	bool moved;

	Tracked(int value = 0) : v(value), moved(false) { }
	Tracked(const Tracked &o) : v(o.v), moved(false) { }
	Tracked(Tracked &&o) : v(o.v), moved(false) {
		o.moved = true;
		o.v = -1;
	}
	Tracked &operator=(const Tracked &) = default;
	Tracked &operator=(Tracked &&) = default;
	~Tracked() { } // user-provided -> non-trivial
};

// Constructible from an initializer_list, to exercise the in_place + braced
// constructor and emplace(initializer_list, ...).
struct Sum {
	int total;
	Sum(initializer_list<int> il) : total(0) {
		for (int x : il) { total += x; }
	}
};

static int ordsign(strong_ordering o) noexcept { return o < 0 ? -1 : (o > 0 ? 1 : 0); }

} // namespace

void performOptionalTest() {
	// --- 1. Trivial-special-member propagation (the reason for the libc++ port).
	static_assert(is_trivially_destructible_v<optional<int>>);
	static_assert(is_trivially_copy_constructible_v<optional<int>>);
	static_assert(is_trivially_move_constructible_v<optional<int>>);
	static_assert(is_trivially_copy_assignable_v<optional<int>>);
	static_assert(is_trivially_move_assignable_v<optional<int>>);
	static_assert(is_trivially_copyable_v<optional<int>>);
	static_assert(is_trivially_copyable_v<optional<TrivialPoint>>);
	static_assert(!is_trivially_destructible_v<optional<Tracked>>);
	static_assert(!is_trivially_copy_constructible_v<optional<Tracked>>);
	static_assert(!is_trivially_copyable_v<optional<Tracked>>);
	// Conditional-explicit converting constructor: int -> optional<int> is implicit.
	static_assert(is_convertible_v<int, optional<int>>);

	printf("trivial<int> dtor=%d cctor=%d mctor=%d cassign=%d massign=%d copyable=%d\n",
			(int) is_trivially_destructible_v<optional<int>>,
			(int) is_trivially_copy_constructible_v<optional<int>>,
			(int) is_trivially_move_constructible_v<optional<int>>,
			(int) is_trivially_copy_assignable_v<optional<int>>,
			(int) is_trivially_move_assignable_v<optional<int>>,
			(int) is_trivially_copyable_v<optional<int>>);
	printf("trivial<Tracked> dtor=%d copyable=%d\n",
			(int) is_trivially_destructible_v<optional<Tracked>>,
			(int) is_trivially_copyable_v<optional<Tracked>>);
	printf("convertible int->opt=%d opt<int>->opt<long>=%d\n",
			(int) is_convertible_v<int, optional<int>>,
			(int) is_convertible_v<optional<int>, optional<long>>);

	// --- 3. Engaged / disengaged state and observers.
	{
		optional<int> e;
		printf("empty has=%d bool=%d\n", (int) e.has_value(), (int) static_cast<bool>(e));

		optional<int> o(42);
		printf("engaged has=%d bool=%d star=%d value=%d\n", (int) o.has_value(),
				(int) static_cast<bool>(o), *o, o.value());

		optional<TrivialPoint> p(TrivialPoint {3, 4});
		printf("arrow x=%d y=%d\n", p->x, p->y);
	}

	// --- 4. Conditional-explicit / implicit construction.
	{
		optional<int> oi = 5; // implicit (non-explicit) conversion from int
		printf("implicit_from_int=%d\n", oi.value());
	}

	// --- 5. Moved-from optional stays engaged (key conformance fix).
	{
		optional<Tracked> a(Tracked {7});
		optional<Tracked> b(std::move(a));
		printf("move b.has=%d b.v=%d a.has=%d a.moved=%d a.v=%d\n", (int) b.has_value(), b->v,
				(int) a.has_value(), (int) a->moved, a->v);

		optional<Tracked> c(Tracked {1});
		optional<Tracked> d(Tracked {2});
		d = std::move(c);
		printf("moveassign d.v=%d c.has=%d c.moved=%d\n", d->v, (int) c.has_value(),
				(int) c->moved);
	}

	// --- 6. value_or.
	{
		optional<int> e;
		printf("value_or empty=%d engaged=%d\n", e.value_or(99), optional<int>(7).value_or(99));
	}

	// --- 7. emplace / reset / swap.
	{
		optional<int> em;
		em.emplace(123);
		printf("emplace=%d\n", em.value());
		em.reset();
		printf("reset has=%d\n", (int) em.has_value());

		optional<Sum> os;
		os.emplace({1, 2, 3, 4});
		printf("emplace_init_list=%d\n", os->total);

		optional<int> s1(1), s2(2);
		s1.swap(s2);
		printf("swap s1=%d s2=%d\n", s1.value(), s2.value());

		optional<int> s3, s4(5);
		s3.swap(s4);
		printf("swap_empty s3.has=%d s3=%d s4.has=%d\n", (int) s3.has_value(), s3.value(),
				(int) s4.has_value());
	}

	// --- 8. Comparisons (optional/optional, optional/nullopt, optional/T) incl. <=>.
	{
		optional<int> a(1), b(2), c(1), n;
		printf("cmp eq=%d ne=%d lt=%d gt=%d le=%d ge=%d\n", (int) (a == c), (int) (a != b),
				(int) (a < b), (int) (b > a), (int) (a <= c), (int) (a >= c));
		printf("cmp_empty e_eq_e=%d e_lt_v=%d v_gt_e=%d\n", (int) (n == optional<int>()),
				(int) (n < a), (int) (a > n));
		printf("nullopt e_eq_null=%d v_eq_null=%d\n", (int) (n == nullopt), (int) (a == nullopt));
		printf("withT opt_eq_v=%d opt_lt_v=%d v_eq_opt=%d v_lt_opt=%d\n", (int) (a == 1),
				(int) (a < 2), (int) (1 == a), (int) (0 < a));
		printf("3way opt=%d null_empty=%d null_engaged=%d withT=%d\n", ordsign(a <=> b),
				ordsign(n <=> nullopt), ordsign(a <=> nullopt), ordsign(a <=> 2));
	}

	// --- 9. make_optional / in_place / initializer_list construction.
	{
		auto mo = make_optional<int>(77);
		auto md = make_optional(88); // deduces optional<int>
		printf("make_optional typed=%d deduced=%d\n", mo.value(), md.value());

		optional<TrivialPoint> ip(in_place, 3, 4);
		printf("in_place x=%d y=%d\n", ip->x, ip->y);

		optional<Sum> os(in_place, {10, 20, 30});
		printf("in_place_init_list=%d\n", os->total);
	}

	// --- 10. Monadic operations (C++23). Guarded by standard version: sprt provides
	//     these unconditionally, but the C++20 system libstdc++ this is diffed against
	//     does not, so they are only compiled in C++23+ (where both have them).
#if __cplusplus >= 202302L
	{
		optional<int> m(10);
		optional<int> e;

		auto a1 = m.and_then([](int x) { return optional<int>(x * 2); });
		auto a2 = e.and_then([](int x) { return optional<int>(x * 2); });
		printf("and_then engaged=%d has=%d empty_has=%d\n", a1.value_or(-1), (int) a1.has_value(),
				(int) a2.has_value());

		auto t1 = m.transform([](int x) { return x + 1; });
		auto t2 = e.transform([](int x) { return x + 1; });
		printf("transform engaged=%d empty_has=%d\n", t1.value_or(-1), (int) t2.has_value());

		auto o1 = e.or_else([] { return optional<int>(5); });
		auto o2 = m.or_else([] { return optional<int>(5); });
		printf("or_else empty=%d engaged=%d\n", o1.value_or(-1), o2.value_or(-1));
	}
#endif

	// --- 11. Range support (optional as a range, C++26). Guarded past C++23 since the
	//     standard library only provides begin/end on optional from C++26.
#if __cplusplus > 202302L
	{
		optional<int> ro(42);
		int sum = 0;
		for (int x : ro) { sum += x; }

		optional<int> re;
		int count = 0;
		for (int x : re) {
			(void) x;
			++count;
		}
		printf("range engaged_sum=%d empty_count=%d\n", sum, count);
	}
#endif
}

} // namespace sprt::test
