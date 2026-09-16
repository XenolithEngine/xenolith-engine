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

#include <future>
#include <system_error>

namespace sprt::test {

// future_errc is an error-code enum
static_assert(std::is_error_code_enum<std::future_errc>::value);

void performFutureTest() {
	// enum underlying values
	printf("future_status: ready=%d timeout=%d deferred=%d\n", (int)std::future_status::ready,
			(int)std::future_status::timeout, (int)std::future_status::deferred);
	printf("launch: async=%d deferred=%d\n", (int)std::launch::async, (int)std::launch::deferred);
	printf("future_errc: broken=%d retrieved=%d satisfied=%d no_state=%d\n",
			(int)std::future_errc::broken_promise, (int)std::future_errc::future_already_retrieved,
			(int)std::future_errc::promise_already_satisfied, (int)std::future_errc::no_state);

	// future_category name + per-value message strings (deterministic)
	const std::error_category &cat = std::future_category();
	printf("future_category: name=%s\n", cat.name());
	for (int e = 1; e <= 4; ++e) {
		std::error_code ec = std::make_error_code(static_cast<std::future_errc>(e));
		printf("  errc=%d value=%d message=%s\n", e, ec.value(), ec.message().c_str());
	}

	// make_error_condition from future_errc
	std::error_condition cond = std::make_error_condition(std::future_errc::no_state);
	printf("error_condition: value=%d category=%s\n", cond.value(), cond.category().name());

	// future_error object (constructed, never thrown): code + what()
	std::future_error fe(std::future_errc::promise_already_satisfied);
	printf("future_error: code=%d what=%s\n", fe.code().value(), fe.what());
}

} // namespace sprt::test
