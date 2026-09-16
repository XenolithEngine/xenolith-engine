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

// std::call_once (single-threaded behaviour). Regression test for the qonce fix:
// std::call_once wraps the user callable in a `mutable` lambda, which qonce must be
// able to invoke (previously it took the callback by const& and could not). Uses the
// <mutex> STL wrapper so it builds against the system library too.

#include <stdio.h>

#include <mutex>

namespace sprt::test {

void performCallOnceTest() {
	// Runs exactly once across repeated calls on the same flag.
	std::once_flag flag;
	int n = 0;
	std::call_once(flag, [&] { n += 10; });
	std::call_once(flag, [&] { n += 10; }); // must not run
	std::call_once(flag, [&] { n += 10; }); // must not run
	printf("call_once: n=%d (expect 10)\n", n);

	// Callable taking arguments.
	std::once_flag flag2;
	int sum = -1;
	std::call_once(flag2, [&](int a, int b) { sum = a + b; }, 3, 4);
	std::call_once(flag2, [&](int a, int b) { sum = a + b; }, 100, 200); // must not run
	printf("call_once args: sum=%d (expect 7)\n", sum);

	// Captureless lambda + run-count across multiple flags.
	int g = 0;
	auto inc = [&] { ++g; };
	std::once_flag flag3;
	std::call_once(flag3, inc);
	std::call_once(flag3, inc);
	std::once_flag flag4;
	std::call_once(flag4, inc);
	printf("call_once distinct flags: g=%d (expect 2)\n", g);
}

} // namespace sprt::test
