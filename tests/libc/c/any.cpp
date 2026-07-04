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

// std::any conformance, via the <any> STL wrapper (so it also builds against the
// system library; on the freestanding build std::any == sprt::any).
//
// Deterministic, tests/libc style: it prints only stable values, so the Linux host
// (system libstdc++) and x86_64-pc-windows-msvc freestanding (sprt) builds are
// byte-identical under compare.sh.
//
// IMPORTANT: type_info::name()/hash_code() are ABI-specific (Itanium vs MS) and
// would diverge host-vs-Windows, so this test never prints them — it compares
// type identity via `a.type() == typeid(T)` (the boolean result is identical on
// both targets) and reads values through the pointer form of any_cast.
//
// NB: the value forms of any_cast<T>() abort on a type mismatch (-fno-exceptions),
// so this test only value-casts the active type and uses the pointer form (which
// returns nullptr on mismatch) for the negative checks.

#include <stdio.h>

#include <any>
#include <utility>
#include <typeinfo>
#include <initializer_list>

namespace sprt::test {
namespace {

using std::any;
using std::any_cast;
using std::make_any;
using std::in_place_type;
using std::initializer_list;

// Larger than the 3-pointer SBO buffer -> stored on the heap (large handler).
struct Big {
	long v[5];
	Big(long s = 0) {
		for (int i = 0; i < 5; ++i) { v[i] = s + i; }
	}
};

// Constructible from an initializer_list, for the in_place + braced constructor.
struct Sum {
	int total;
	Sum(initializer_list<int> il) : total(0) {
		for (int x : il) { total += x; }
	}
};

} // namespace

void performAnyTest() {
	// --- 1. Empty any.
	{
		any a;
		printf("empty has=%d is_void=%d cast_null=%d\n", (int) a.has_value(),
				(int) (a.type() == typeid(void)), (int) (any_cast<int>(&a) == nullptr));
	}

	// --- 2. Small object (int): storage, type, cast (ptr + value), mismatch.
	{
		any a(42);
		printf("int has=%d is_int=%d ptr=%d value=%d mismatch_null=%d\n", (int) a.has_value(),
				(int) (a.type() == typeid(int)), *any_cast<int>(&a), any_cast<int>(a),
				(int) (any_cast<long>(&a) == nullptr));
	}

	// --- 3. Other small objects.
	{
		any a(3.5);
		printf("double is_double=%d val=%g\n", (int) (a.type() == typeid(double)),
				*any_cast<double>(&a));

		any b(in_place_type<const char *>, "hi");
		printf("cstr is_cstr=%d val=%s\n", (int) (b.type() == typeid(const char *)),
				*any_cast<const char *>(&b));
	}

	// --- 4. Large object (heap handler).
	{
		any a(in_place_type<Big>, 7L);
		auto *p = any_cast<Big>(&a);
		printf("big is_Big=%d v0=%ld v4=%ld\n", (int) (a.type() == typeid(Big)), p->v[0], p->v[4]);
	}

	// --- 5. initializer_list construction.
	{
		any a(in_place_type<Sum>, {1, 2, 3, 4});
		printf("init_list total=%d\n", any_cast<Sum>(&a)->total);
	}

	// --- 6. Copy makes an independent value (small + large).
	{
		any a(10);
		any b(a);
		*any_cast<int>(&a) = 99;
		printf("copy small a=%d b=%d\n", *any_cast<int>(&a), *any_cast<int>(&b));

		any c(in_place_type<Big>, 100L);
		any d(c);
		any_cast<Big>(&c)->v[0] = -1;
		printf("copy large c0=%ld d0=%ld\n", any_cast<Big>(&c)->v[0], any_cast<Big>(&d)->v[0]);
	}

	// --- 7. Move (source left empty), small + large.
	{
		any a(7);
		any b(std::move(a));
		printf("move small a_has=%d b=%d\n", (int) a.has_value(), *any_cast<int>(&b));

		any c(in_place_type<Big>, 5L);
		any d(std::move(c));
		printf("move large c_has=%d d0=%ld\n", (int) c.has_value(), any_cast<Big>(&d)->v[0]);
	}

	// --- 8. reset / emplace.
	{
		any a(1);
		a.reset();
		printf("reset has=%d is_void=%d\n", (int) a.has_value(), (int) (a.type() == typeid(void)));

		a.emplace<double>(2.5);
		printf("emplace is_double=%d val=%g\n", (int) (a.type() == typeid(double)),
				*any_cast<double>(&a));
	}

	// --- 9. swap.
	{
		any a(1);
		any b(2.5);
		std::swap(a, b);
		printf("swap a_is_double=%d b_is_int=%d a=%g b=%d\n", (int) (a.type() == typeid(double)),
				(int) (b.type() == typeid(int)), *any_cast<double>(&a), *any_cast<int>(&b));
	}

	// --- 10. assignment (value and any).
	{
		any a;
		a = 5;
		printf("assign_value is_int=%d val=%d\n", (int) (a.type() == typeid(int)),
				*any_cast<int>(&a));

		any b(1);
		any c(2.5);
		b = c;
		printf("assign_any is_double=%d val=%g\n", (int) (b.type() == typeid(double)),
				*any_cast<double>(&b));
	}

	// --- 11. make_any (small + large).
	{
		auto a = make_any<int>(77);
		auto b = make_any<Big>(3L);
		printf("make_any small=%d big_v0=%ld\n", *any_cast<int>(&a), any_cast<Big>(&b)->v[0]);
	}

	// --- 12. any_cast through a const any.
	{
		const any a(8);
		printf("const any_cast=%d value=%d\n", *any_cast<int>(&a), any_cast<int>(a));
	}
}

} // namespace sprt::test
