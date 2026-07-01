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

// std::function conformance. On the freestanding x86_64-pc-windows-msvc build
// <functional> is the sprt-backed STL header (std::function == sprt::__malloc_function);
// on the Linux host it is the system header. compare.sh diffs the two, so the sprt
// implementation is checked against the system reference. Only deterministic results
// are printed (call results, empty/target/target_type/swap booleans) — never
// addresses or hash values. Empty-call is NOT exercised: std::function throws
// bad_function_call there, which this -fno-exceptions runtime does not have.

#include <stdio.h>

#include <functional>
#include <utility>
#include <typeinfo>
#include <type_traits>

namespace sprt::test {

namespace {

int times2(int x) { return x * 2; }
int times3(int x) { return x * 3; }

struct Adder {
	int base;
	int operator()(int x) const { return x + base; }
};

// > 16-byte inline buffer -> forced onto the heap storage path
struct BigFunctor {
	long a, b, c, d, e;
	int operator()(int x) const { return x + (int) (a + b + c + d + e); }
};

static_assert(std::is_same_v<std::function<int(int)>::result_type, int>);
static_assert(!std::is_constructible_v<std::function<int(int)>, const char *>); // not invocable

} // namespace

void performFunctionTest() {
	using F = std::function<int(int)>;

	// construction + call: lambda / functor / function pointer / free-function lvalue
	F fl = [](int x) { return x + 1; };
	F ff = Adder {10};
	F fp = &times2;
	F fd = times3; // decays to a function pointer, like std::function
	printf("call: lam=%d fun=%d ptr=%d dec=%d\n", fl(5), ff(5), fp(5), fd(5));

	// empty state
	F e;
	printf("empty: bool=%d isnull=%d nonnull=%d\n", (int) (bool) e, (int) (e == nullptr),
			(int) (bool) fl);

	// copy is independent of the source
	F c1 = ff;
	printf("copy: %d\n", c1(7));

	// move leaves the source empty
	F m1 = std::move(c1);
	printf("move: dst=%d src_empty=%d\n", m1(7), (int) (c1 == nullptr));

	// assignment: functor / pointer / nullptr
	F a = &times2;
	a = Adder {100};
	printf("assign_fun: %d\n", a(1));
	a = &times3;
	printf("assign_ptr: %d\n", a(2));
	a = nullptr;
	printf("assign_null: empty=%d\n", (int) !a);

	// copy-assignment must COPY, not re-wrap: target_type stays the target's type
	F w1 = &times2;
	F w2;
	w2 = w1;
	printf("copywrap: same_type=%d calls=%d\n", (int) (w2.target_type() == w1.target_type()), w2(9));

	// swap between SOO and heap targets, member + ADL + self
	F s1 = Adder {1}; // SOO
	F s2 = BigFunctor {1, 2, 3, 4, 5}; // heap (sum 15)
	int b1 = s1(0), b2 = s2(0);
	s1.swap(s2);
	printf("swap_mem: s1=%d s2=%d ok=%d\n", s1(0), s2(0), (int) (s1(0) == b2 && s2(0) == b1));
	swap(s1, s2);
	printf("swap_adl: s1=%d s2=%d\n", s1(0), s2(0));
	s1.swap(s1);
	printf("swap_self: %d\n", s1(0));

	// target_type
	F tp = &times2;
	printf("ttype: is_ptr=%d empty_void=%d wrong=%d\n",
			(int) (tp.target_type() == typeid(int (*)(int))),
			(int) (e.target_type() == typeid(void)), (int) (tp.target_type() == typeid(double)));

	// target<T>: right type returns a usable pointer, wrong type returns null
	int (**pp)(int) = tp.target<int (*)(int)>();
	printf("target_ptr: found=%d calls=%d wrong_null=%d\n", (int) (pp != nullptr),
			pp ? (*pp)(21) : -1, (int) (tp.target<double>() == nullptr));

	// target<T> of a heap-stored functor, read its state back
	F fb = BigFunctor {2, 0, 0, 0, 0};
	const BigFunctor *bpc = fb.target<BigFunctor>();
	printf("target_big: found=%d a=%d\n", (int) (bpc != nullptr), bpc ? (int) bpc->a : -1);

	// reference_wrapper target
	F rw = std::ref(times2);
	printf("refwrap: %d\n", rw(6));

	// recursion through std::function
	std::function<int(int)> fact = [&fact](int n) -> int { return n <= 1 ? 1 : n * fact(n - 1); };
	printf("recurse: 5!=%d\n", fact(5));

	// void return + captured mutable state
	int counter = 0;
	std::function<void()> tick = [&counter] { ++counter; };
	tick();
	tick();
	tick();
	printf("void: counter=%d\n", counter);

	// CTAD from a function pointer
	std::function g = &times2;
	printf("ctad: val=%d same=%d\n", g(11),
			(int) std::is_same_v<decltype(g), std::function<int(int)>>);
}

} // namespace sprt::test
