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

#include <stdio.h>

#include <functional>
#include <type_traits>

namespace sprt::test {

namespace {

int sub(int a, int b) { return a - b; }
int add3(int a, int b, int c) { return a + b + c; }
int mul(int a, int b) { return a * b; }
void bump(int &x) { x += 100; }

struct Adder {
	int base;
	int addTo(int x) const { return base + x; }
};

// trait checks. The placeholder objects are `const` (inline constexpr), so decay
// away cv-qualification first — exactly as bind's own argument resolution does.
static_assert(std::is_placeholder_v<std::decay_t<decltype(std::placeholders::_1)>> == 1);
static_assert(std::is_placeholder_v<std::decay_t<decltype(std::placeholders::_3)>> == 3);
static_assert(std::is_placeholder_v<int> == 0);

} // namespace

void performBindTest() {
	using namespace std::placeholders;

	// argument selection and reordering
	auto f1 = std::bind(sub, _1, _2);
	auto f2 = std::bind(sub, _2, _1);
	printf("select: sub(_1,_2)(10,3)=%d sub(_2,_1)(10,3)=%d\n", f1(10, 3), f2(10, 3));

	// a fixed bound argument mixed with a placeholder
	auto f3 = std::bind(sub, 100, _1);
	printf("bound: sub(100,_1)(30)=%d\n", f3(30));

	// argument repetition: the same call argument used twice
	auto f4 = std::bind(add3, _1, _1, _2);
	printf("repeat: add3(_1,_1,_2)(5,1)=%d\n", f4(5, 1));

	// nested bind: inner result feeds the outer call
	auto f5 = std::bind(mul, std::bind(add3, _1, _2, 0), 2);
	printf("nested: mul(add3(_1,_2,0),2)(3,4)=%d\n", f5(3, 4));

	// reference_wrapper: the bound function mutates the caller's variable
	int counter = 5;
	auto f6 = std::bind(bump, std::ref(counter));
	f6();
	f6();
	printf("ref: counter=%d\n", counter);

	// pointer to member function, object passed as the first call argument
	Adder a{1'000};
	auto f7 = std::bind(&Adder::addTo, &a, _1);
	printf("memfn: a.addTo(_1)(7)=%d\n", f7(7));

	// is_bind_expression is true for a bind result
	printf("traits: is_bind_expr(f1)=%d is_placeholder(_2)=%d\n",
			(int)std::is_bind_expression_v<decltype(f1)>,
			(int)std::is_placeholder_v<std::decay_t<decltype(_2)>>);
}

} // namespace sprt::test
