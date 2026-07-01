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

// std::variant conformance, via the <variant> STL wrapper (so it also builds against
// the system library; on the freestanding build std::variant == sprt::variant).
//
// Deterministic, tests/libc style: prints only stable values, so the Linux host
// (system libstdc++) and x86_64-pc-windows-msvc freestanding (sprt) builds are
// byte-identical under compare.sh. sizeof is NOT printed (implementation-defined
// layout). The sprt-specific valueless-on-move / variant_empty visit behaviour is
// NOT tested here — it is a non-standard extension (std::variant only becomes
// valueless via a throwing type-changing operation, impossible with -fno-exceptions).
//
// NB: get<>() on the wrong alternative aborts in this -fno-exceptions runtime,
// so this test only get<>()s alternatives it knows are active (and uses get_if
// otherwise).

#include <stdio.h>

#include <variant>
#include <type_traits>
#include <compare>
#include <initializer_list>
#include <utility>

namespace sprt::test {
namespace {

using std::variant;
using std::monostate;
using std::in_place_index;
using std::in_place_type;
using std::holds_alternative;
using std::get;
using std::get_if;
using std::visit;
using std::variant_size_v;
using std::variant_alternative_t;
using std::is_trivially_destructible_v;
using std::is_trivially_copy_constructible_v;
using std::is_trivially_move_constructible_v;
using std::is_trivially_copyable_v;
using std::is_same_v;
using std::decay_t;
using std::initializer_list;

// Non-trivial alternative (user-provided move/dtor), so variant<...,Tracked> is not
// trivially copyable and uses the non-trivial move path.
struct Tracked {
	int v;
	Tracked(int value = 0) : v(value) { }
	Tracked(const Tracked &o) : v(o.v) { }
	Tracked(Tracked &&o) : v(o.v) { o.v = -1; }
	Tracked &operator=(const Tracked &) = default;
	Tracked &operator=(Tracked &&) = default;
	~Tracked() { }
};

// Constructible from initializer_list, for the in_place + braced constructor.
struct Sum {
	int total;
	Sum(initializer_list<int> il) : total(0) {
		for (int x : il) { total += x; }
	}
};

} // namespace

void performVariantTest() {
	using V3 = variant<int, float, char>;

	// --- 1. Trivial-special-member propagation.
	static_assert(is_trivially_destructible_v<variant<int, float>>);
	static_assert(is_trivially_copy_constructible_v<variant<int, float>>);
	static_assert(is_trivially_move_constructible_v<variant<int, float>>);
	static_assert(is_trivially_copyable_v<variant<int, float>>);
	static_assert(!is_trivially_destructible_v<variant<int, Tracked>>);
	static_assert(!is_trivially_move_constructible_v<variant<int, Tracked>>);
	static_assert(!is_trivially_copyable_v<variant<int, Tracked>>);

	printf("trivial<int,float> dtor=%d cctor=%d mctor=%d copyable=%d\n",
			(int) is_trivially_destructible_v<variant<int, float>>,
			(int) is_trivially_copy_constructible_v<variant<int, float>>,
			(int) is_trivially_move_constructible_v<variant<int, float>>,
			(int) is_trivially_copyable_v<variant<int, float>>);
	printf("trivial<int,Tracked> dtor=%d mctor=%d copyable=%d\n",
			(int) is_trivially_destructible_v<variant<int, Tracked>>,
			(int) is_trivially_move_constructible_v<variant<int, Tracked>>,
			(int) is_trivially_copyable_v<variant<int, Tracked>>);

	// --- 2. Helper traits (sizeof is implementation-defined and not printed).
	static_assert(variant_size_v<V3> == 3);
	static_assert(is_same_v<variant_alternative_t<0, V3>, int>);
	static_assert(is_same_v<variant_alternative_t<1, V3>, float>);
	static_assert(is_same_v<variant_alternative_t<2, V3>, char>);
	printf("variant_size<V3>=%zu\n", variant_size_v<V3>);

	// --- 3. Construction, index, holds_alternative, get / get_if.
	{
		variant<monostate, int, const char *> v;
		printf("default index=%zu valueless=%d\n", v.index(), (int) v.valueless_by_exception());

		v = 42;
		printf("assign int index=%zu holds_int=%d get_if=%d\n", v.index(),
				(int) holds_alternative<int>(v), *get_if<int>(&v));

		v = "hello";
		printf("assign str index=%zu holds_cstr=%d\n", v.index(),
				(int) holds_alternative<const char *>(v));

		variant<int, float> vi(in_place_index<1>, 2.5f);
		printf("in_place_index index=%zu val=%g\n", vi.index(), (double) get<1>(vi));

		variant<int, float> vt(in_place_type<int>, 9);
		printf("in_place_type index=%zu val=%d\n", vt.index(), get<int>(vt));

		variant<int, Sum> vs(in_place_index<1>, {1, 2, 3, 4});
		printf("in_place_init_list total=%d\n", get<Sum>(vs).total);

		variant<int, float> g(5);
		printf("get_if int=%d float_null=%d\n", *get_if<int>(&g), (int) (get_if<float>(&g) == nullptr));
	}

	// --- 4. Converting construction picks the best match.
	{
		variant<int, const char *> v1(true); // bool -> int
		variant<int, const char *> v2("str");
		printf("converting: index_from_bool=%zu index_from_cstr=%zu\n", v1.index(), v2.index());
	}

	// --- 5. Copy / move. Trivial alternatives: moved-from stays engaged.
	{
		variant<int, float> a(3.5f);
		variant<int, float> b(a);
		printf("copy: a.index=%zu b.index=%zu b.val=%g\n", a.index(), b.index(),
				(double) get<float>(b));

		variant<int, float> c(7);
		variant<int, float> d(std::move(c));
		printf("trivial move: src_valueless=%d src_holds_int=%d dst=%d\n",
				(int) c.valueless_by_exception(), (int) holds_alternative<int>(c), get<int>(d));
	}

	// --- 6. Non-trivial alternative: move populates the destination and visit
	//     dispatches on the active (moved-in) alternative. The moved-from source's
	//     state is intentionally NOT observed: std::variant keeps it engaged, unlike
	//     the sprt valueless-on-move extension, so observing it is not portable.
	{
		variant<int, Tracked> nt(in_place_type<Tracked>, 7);
		printf("nt before move: index=%zu holds_tracked=%d\n", nt.index(),
				(int) holds_alternative<Tracked>(nt));

		variant<int, Tracked> nt2(std::move(nt));
		int tag = visit(
				[](auto &&arg) -> int {
					using T = decay_t<decltype(arg)>;
					if constexpr (is_same_v<T, Tracked>) {
						return arg.v;
					} else {
						return 0;
					}
				},
				nt2);
		printf("nt move dst: holds_tracked=%d v=%d visit_tag=%d\n",
				(int) holds_alternative<Tracked>(nt2), get<Tracked>(nt2).v, tag);
	}

	// --- 7. visit (single) and multi-variant visit.
	{
		variant<int, float, const char *> v(2.5f);
		const char *kind = visit(
				[](auto &&arg) -> const char * {
					using T = decay_t<decltype(arg)>;
					if constexpr (is_same_v<T, int>) {
						return "int";
					} else if constexpr (is_same_v<T, float>) {
						return "float";
					} else {
						return "cstr";
					}
				},
				v);
		printf("visit single kind=%s\n", kind);

		variant<int, float> m1(2);
		variant<int, float> m2(3);
		int sum = visit([](auto a, auto b) -> int { return (int) (a + b); }, m1, m2);
		printf("visit multi sum=%d\n", sum);
	}

	// --- 8. emplace / assignment.
	{
		variant<int, Tracked> e;
		e.emplace<Tracked>(11);
		printf("emplace Tracked index=%zu v=%d\n", e.index(), get<Tracked>(e).v);
		e.emplace<0>(22);
		printf("emplace int index=%zu v=%d\n", e.index(), get<0>(e));

		variant<int, float> as(1);
		as = 4.5f;
		printf("converting assign index=%zu val=%g\n", as.index(), (double) get<float>(as));
	}

	// --- 9. swap.
	{
		variant<int, float> s1(1);
		variant<int, float> s2(2.0f);
		swap(s1, s2);
		printf("swap s1.index=%zu s2.index=%zu s1=%g s2=%d\n", s1.index(), s2.index(),
				(double) get<float>(s1), get<int>(s2));
	}

	// --- 10. Comparisons (same and differing alternatives).
	{
		variant<int, float> a(1), b(2), c(1);
		printf("cmp eq=%d ne=%d lt=%d gt=%d le=%d ge=%d\n", (int) (a == c), (int) (a != b),
				(int) (a < b), (int) (b > a), (int) (a <= c), (int) (a >= c));

		variant<int, float> ai(1), af(1.0f); // index 0 vs 1
		printf("cmp by index: ai_lt_af=%d af_gt_ai=%d\n", (int) (ai < af), (int) (af > ai));

		auto r1 = (a <=> b); // same index (0), 1 <=> 2
		auto r2 = (a <=> c); // same index (0), 1 <=> 1
		auto r3 = (ai <=> af); // by index: 0 <=> 1
		printf("3way: a<b=%d a==c=%d ai<af=%d\n", (int) (r1 < 0 ? -1 : (r1 > 0 ? 1 : 0)),
				(int) (r2 < 0 ? -1 : (r2 > 0 ? 1 : 0)), (int) (r3 < 0 ? -1 : (r3 > 0 ? 1 : 0)));
	}

	// --- 11. monostate.
	{
		printf("monostate eq=%d\n", (int) (monostate {} == monostate {}));
	}
}

} // namespace sprt::test
